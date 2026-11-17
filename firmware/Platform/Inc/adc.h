#ifndef CONSOLE_ADC_H
#define CONSOLE_ADC_H

#include <stdint.h>

typedef enum {
    ADC_STATUS_OK = 0,
    ADC_STATUS_INVALID_ARGUMENT,
    ADC_STATUS_BUSY,
    ADC_STATUS_DMA_ERROR,
    ADC_STATUS_OVERRUN
} adc_status_t;

/* Called at 100 Hz in DMA IRQ context; keep it short and never wait/print. */
typedef void (*adc1_sample_fn)(uint16_t x, uint16_t y, uint32_t sampled_ms);

/* TIM3 TRGO -> ADC1 IN0/IN1 scan -> DMA2 Stream 4, Channel 0. */
adc_status_t adc1_start(uint32_t timer_clock_hz, adc1_sample_fn callback);
void adc1_stop(void);
adc_status_t adc1_status(void);
void adc1_dma_irq_handler(void);
void adc1_irq_handler(void);

#endif
