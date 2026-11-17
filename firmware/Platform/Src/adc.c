#include "adc.h"

#include <stddef.h>
#include "stm32f446xx.h"
#include "timebase.h"
#include "io_wait.h"

#define ADC_DMA_FLAGS                                                                              \
    (DMA_HISR_FEIF4 | DMA_HISR_DMEIF4 | DMA_HISR_TEIF4 | DMA_HISR_HTIF4 | DMA_HISR_TCIF4)
#define ADC_DMA_ERRORS (DMA_HISR_FEIF4 | DMA_HISR_DMEIF4 | DMA_HISR_TEIF4)

/* Each half contains one complete X/Y pair. DMA writes the other half while
   the ISR consumes the completed half. F446 SRAM has no data cache. */
static volatile uint16_t adc_pairs[4];
static adc1_sample_fn sample_callback;
static volatile adc_status_t current_status;

adc_status_t adc1_start(uint32_t timer_clock_hz, adc1_sample_fn callback)
{
    if ((callback == NULL) || (timer_clock_hz < 1000000U) || (timer_clock_hz % 1000000U != 0U) ||
        (timer_clock_hz / 1000000U > 65536U)) {
        return ADC_STATUS_INVALID_ARGUMENT;
    }

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;
    if ((DMA2_Stream4->CR & DMA_SxCR_EN) != 0U) {
        return ADC_STATUS_BUSY;
    }

    /* Load the timer prescaler before enabling the ADC external trigger. */
    TIM3->CR1 = TIM_CR1_ARPE;
    TIM3->CR2 = 0U;
    TIM3->DIER = 0U;
    TIM3->PSC = timer_clock_hz / 1000000U - 1U;
    TIM3->ARR = 10000U - 1U; /* 1 MHz counter / 10000 = 100 Hz. */
    TIM3->CNT = 0U;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    TIM3->CR2 = TIM_CR2_MMS_1; /* Update event becomes TRGO. */

    ADC1->CR2 = 0U;
    ADC->CCR = (ADC->CCR & ~ADC_CCR_ADCPRE) | ADC_CCR_ADCPRE_0;
    ADC1->SMPR2 = (6UL << 0U) | (6UL << 3U); /* 144 cycles, 22.5 MHz. */
    ADC1->SQR1 = ADC_SQR1_L_0;               /* Two regular conversions per trigger. */
    ADC1->SQR2 = 0U;
    ADC1->SQR3 = 1UL << 5U; /* SQ1=IN0 (X), SQ2=IN1 (Y). */
    ADC1->CR1 = ADC_CR1_SCAN | ADC_CR1_OVRIE;
    ADC1->SR = 0U;

    DMA2_Stream4->CR = DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 |
                       DMA_SxCR_PL_1 | DMA_SxCR_HTIE | DMA_SxCR_TCIE | DMA_SxCR_TEIE |
                       DMA_SxCR_DMEIE;
    DMA2_Stream4->FCR = DMA_SxFCR_FEIE;
    DMA2_Stream4->PAR = (uint32_t)(uintptr_t)&ADC1->DR;
    DMA2_Stream4->M0AR = (uint32_t)(uintptr_t)adc_pairs;
    DMA2_Stream4->NDTR = 4U;
    DMA2->HIFCR = ADC_DMA_FLAGS;
    sample_callback = callback;
    current_status = ADC_STATUS_OK;

    NVIC_ClearPendingIRQ(DMA2_Stream4_IRQn);
    NVIC_ClearPendingIRQ(ADC_IRQn);
    NVIC_SetPriority(DMA2_Stream4_IRQn, IO_IRQ_PRIORITY_ADC);
    NVIC_SetPriority(ADC_IRQn, IO_IRQ_PRIORITY_ADC);
    NVIC_EnableIRQ(DMA2_Stream4_IRQn);
    NVIC_EnableIRQ(ADC_IRQn);
    DMA2_Stream4->CR |= DMA_SxCR_EN;
    /* EXTSEL=1000: TIM3 TRGO. Each rising edge starts the whole scan. */
    ADC1->CR2 = ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_EXTSEL_3 | ADC_CR2_EXTEN_0 | ADC_CR2_ADON;
    timebase_delay_ms(1U); /* Sleep through ADC power-up stabilization. */
    TIM3->CR1 |= TIM_CR1_CEN;
    return ADC_STATUS_OK;
}

void adc1_stop(void)
{
    TIM3->CR1 &= ~TIM_CR1_CEN;
    ADC1->CR2 &= ~(ADC_CR2_EXTEN | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_ADON);
    ADC1->CR1 &= ~ADC_CR1_OVRIE;
    DMA2_Stream4->CR &= ~DMA_SxCR_EN;
    NVIC_DisableIRQ(DMA2_Stream4_IRQn);
    NVIC_DisableIRQ(ADC_IRQn);
    DMA2->HIFCR = ADC_DMA_FLAGS;
}

adc_status_t adc1_status(void)
{
    return current_status;
}

void adc1_dma_irq_handler(void)
{
    const uint32_t flags = DMA2->HISR & ADC_DMA_FLAGS;
    DMA2->HIFCR = flags;
    if ((flags & ADC_DMA_ERRORS) != 0U) {
        current_status = ADC_STATUS_DMA_ERROR;
        adc1_stop();
        io_wake_from_isr(IO_WAIT_ADC);
        return;
    }
    if ((flags & (DMA_HISR_HTIF4 | DMA_HISR_TCIF4)) == (DMA_HISR_HTIF4 | DMA_HISR_TCIF4)) {
        /* IRQs were delayed for an entire pair interval; do not publish a
           potentially overwritten pair as a coherent sample. */
        current_status = ADC_STATUS_OVERRUN;
        adc1_stop();
        io_wake_from_isr(IO_WAIT_ADC);
        return;
    }
    if ((flags & (DMA_HISR_HTIF4 | DMA_HISR_TCIF4)) != 0U) {
        const uint32_t offset = ((flags & DMA_HISR_HTIF4) != 0U) ? 0U : 2U;
        sample_callback(adc_pairs[offset], adc_pairs[offset + 1U], timebase_millis());
        io_wake_from_isr(IO_WAIT_ADC);
    }
}

void adc1_irq_handler(void)
{
    if ((ADC1->SR & ADC_SR_OVR) != 0U) {
        current_status = ADC_STATUS_OVERRUN;
        adc1_stop();
        ADC1->SR = 0U;
        io_wake_from_isr(IO_WAIT_ADC);
    }
}
