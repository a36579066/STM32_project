#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "adc.h"
#include "cpu.h"
#include "joystick_service.h"
#include "spi.h"
#include "timebase.h"
#include "uart.h"
#include "i2c.h"
#ifdef HOST_RTOS_IO
#include "FreeRTOS.h"
#include "task.h"
#include "io_wait.h"
#endif

RCC_TypeDef host_rcc;
ADC_Common_TypeDef host_adc_common;
ADC_TypeDef host_adc1;
DMA_TypeDef host_dma2;
DMA_TypeDef host_dma1;
DMA_Stream_TypeDef host_stream6;
I2C_TypeDef host_i2c1;
DMA_Stream_TypeDef host_stream0, host_stream3, host_stream4;
TIM_TypeDef host_tim3;
SPI_TypeDef host_spi1;
USART_TypeDef host_usart2;
GPIO_TypeDef host_gpioa;
DBGMCU_TypeDef host_dbgmcu;
SCB_Type host_scb;

static bool enabled[128], pending[128];
static uint32_t irq_priority[128];
static uint32_t primask, ipsr, wall_ms, sleeps, adc_samples, dma_phase;
static bool tick_pending, samples_enabled = true, sw_pressed;
static bool spi_enabled = true, dma_error, uart_enabled = true;
static bool b_pressed, menu_pressed, i2c_enabled = true, i2c_nack, i2c_dma_error, i2c_bus_error;
static unsigned i2c_phase, i2c_completions;
static uint16_t raw_x = 2048U, raw_y = 2050U;
static char transmitted[10000];
static size_t transmitted_count;

static void set_readonly(volatile const uint32_t *reg, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)reg = value;
}

static void clear_dma_flags(void)
{
    set_readonly(&DMA2->LISR, DMA2->LISR & ~DMA2->LIFCR);
    set_readonly(&DMA2->HISR, DMA2->HISR & ~DMA2->HIFCR);
    DMA2->LIFCR = 0U;
    DMA2->HIFCR = 0U;
    set_readonly(&DMA1->HISR, DMA1->HISR & ~DMA1->HIFCR);
    DMA1->HIFCR = 0;
}

static void dispatch(void)
{
    if (primask != 0U)
        return;
    if (tick_pending) {
        tick_pending = false;
        timebase_tick_isr();
    }
    const IRQn_Type order[] = {ADC_IRQn,          DMA2_Stream4_IRQn, DMA2_Stream0_IRQn,
                               DMA2_Stream3_IRQn, SPI1_IRQn,         USART2_IRQn,
                               I2C1_EV_IRQn,      I2C1_ER_IRQn,      DMA1_Stream6_IRQn};
    for (size_t i = 0U; i < sizeof(order) / sizeof(order[0]); ++i) {
        const IRQn_Type irq = order[i];
        if (!enabled[irq] || !pending[irq])
            continue;
        pending[irq] = false;
        ipsr = (uint32_t)irq + 16U;
        switch (irq) {
        case ADC_IRQn:
            adc1_irq_handler();
            break;
        case DMA2_Stream4_IRQn:
            adc1_dma_irq_handler();
            break;
        case DMA2_Stream0_IRQn:
            spi1_dma_rx_irq_handler();
            break;
        case DMA2_Stream3_IRQn:
            spi1_dma_tx_irq_handler();
            break;
        case I2C1_EV_IRQn:
            i2c1_event_irq_handler();
            I2C1->SR1 = 0;
            break;
        case I2C1_ER_IRQn:
            i2c1_error_irq_handler();
            break;
        case DMA1_Stream6_IRQn:
            i2c1_dma_tx_irq_handler();
            break;
        case SPI1_IRQn:
            spi1_irq_handler();
            SPI1->SR &= ~SPI_SR_RXNE; /* Reading DR consumes RXNE. */
            break;
        case USART2_IRQn:
            uart_irq_handler();
            if ((USART2->CR1 & USART_CR1_TXEIE) != 0U) {
                assert(transmitted_count < sizeof(transmitted));
                transmitted[transmitted_count++] = (char)USART2->DR;
            }
            break;
        default:
            assert(false);
        }
        ipsr = 0U;
        clear_dma_flags();
    }
}

uint32_t __get_PRIMASK(void)
{
    return primask;
}

uint32_t __get_IPSR(void)
{
    return ipsr;
}

void __disable_irq(void)
{
    primask = 1U;
}

void __set_PRIMASK(uint32_t mask)
{
    primask = mask;
    dispatch();
}

void NVIC_ClearPendingIRQ(IRQn_Type irq)
{
    pending[irq] = false;
}

void NVIC_SetPriority(IRQn_Type irq, uint32_t priority)
{
    irq_priority[irq] = priority;
}

void NVIC_EnableIRQ(IRQn_Type irq)
{
    enabled[irq] = true;
}

void NVIC_DisableIRQ(IRQn_Type irq)
{
    enabled[irq] = false;
}

void board_joystick_init(void)
{
}

bool board_joystick_button_is_pressed(void)
{
    return sw_pressed;
}

bool board_game_b_is_pressed(void)
{
    return b_pressed;
}

bool board_game_menu_is_pressed(void)
{
    return menu_pressed;
}

/* One deterministic hardware tick per sleep. This models IRQ ordering, flag
   acknowledgement and progress while asleep, not cycle-accurate bus timing. */
void __WFI(void)
{
    assert(ipsr == 0U);
    assert(++sleeps < 30000U); /* Detect a driver deadlock. */
    ++wall_ms;
    tick_pending = true;
    clear_dma_flags();
    if (I2C1->CR1 & I2C_CR1_STOP) {
        I2C1->CR1 &= ~I2C_CR1_STOP;
        I2C1->SR2 = 0;
        i2c_phase = 0;
    }
    if (i2c_enabled) {
        if (I2C1->CR1 & I2C_CR1_START) {
            I2C1->CR1 &= ~I2C_CR1_START;
            I2C1->SR2 = I2C_SR2_BUSY | I2C_SR2_MSL;
            I2C1->SR1 = I2C_SR1_SB;
            pending[I2C1_EV_IRQn] = true;
            i2c_phase = 1;
        } else if (i2c_phase == 1) {
            I2C1->SR1 = i2c_bus_error ? I2C_SR1_BERR : i2c_nack ? I2C_SR1_AF : I2C_SR1_ADDR;
            pending[i2c_nack || i2c_bus_error ? I2C1_ER_IRQn : I2C1_EV_IRQn] = true;
            i2c_phase = 2;
        } else if (i2c_phase == 2 && (DMA1_Stream6->CR & DMA_SxCR_EN)) {
            assert((I2C1->CR2 & I2C_CR2_DMAEN) != 0);
            assert((I2C1->CR2 & I2C_CR2_ITEVTEN) == 0);
            DMA1_Stream6->CR &= ~DMA_SxCR_EN;
            set_readonly(&DMA1->HISR,
                         DMA1->HISR | (i2c_dma_error ? DMA_HISR_TEIF6 : DMA_HISR_TCIF6));
            pending[DMA1_Stream6_IRQn] = true;
            i2c_phase = 3;
        } else if (i2c_phase == 3 && (I2C1->CR2 & I2C_CR2_ITEVTEN)) {
            assert((I2C1->CR2 & I2C_CR2_DMAEN) == 0);
            I2C1->SR1 = I2C_SR1_BTF;
            pending[I2C1_EV_IRQn] = true;
            ++i2c_completions;
            i2c_phase = 4;
        }
    }
    if (samples_enabled && ((TIM3->CR1 & TIM_CR1_CEN) != 0U) &&
        ((DMA2_Stream4->CR & DMA_SxCR_EN) != 0U) && (wall_ms % 10U == 0U)) {
        uint16_t *buffer = (uint16_t *)(uintptr_t)DMA2_Stream4->M0AR;
        const bool first = DMA2_Stream4->NDTR == 4U;
        const uint32_t offset = first ? 0U : 2U;
        buffer[offset] = raw_x;
        buffer[offset + 1U] = raw_y;
        DMA2_Stream4->NDTR = first ? 2U : 4U;
        set_readonly(&DMA2->HISR, DMA2->HISR | (first ? DMA_HISR_HTIF4 : DMA_HISR_TCIF4));
        pending[DMA2_Stream4_IRQn] = true;
        ++adc_samples;
    }
    if (spi_enabled && ((SPI1->CR1 & SPI_CR1_SPE) != 0U)) {
        if ((SPI1->CR2 & SPI_CR2_RXNEIE) != 0U) {
            SPI1->DR = 0xA5U;
            SPI1->SR = SPI_SR_RXNE | SPI_SR_TXE;
            pending[SPI1_IRQn] = true;
        } else if ((SPI1->CR2 & SPI_CR2_TXDMAEN) != 0U) {
            if (dma_error) {
                set_readonly(&DMA2->LISR, DMA2->LISR | DMA_LISR_TEIF3);
                pending[DMA2_Stream3_IRQn] = true;
            } else if (dma_phase++ == 0U) {
                DMA2_Stream3->CR &= ~DMA_SxCR_EN;
                set_readonly(&DMA2->LISR, DMA2->LISR | DMA_LISR_TCIF3);
                pending[DMA2_Stream3_IRQn] = true;
            } else {
                DMA2_Stream0->CR &= ~DMA_SxCR_EN;
                set_readonly(&DMA2->LISR, DMA2->LISR | DMA_LISR_TCIF0);
                pending[DMA2_Stream0_IRQn] = true;
                dma_phase = 0U;
            }
        }
    }
    if (uart_enabled && ((USART2->CR1 & (USART_CR1_TXEIE | USART_CR1_TCIE)) != 0U)) {
        USART2->SR = USART_SR_TXE | USART_SR_TC;
        pending[USART2_IRQn] = true;
    }
    dispatch();
}

static void advance(uint32_t ms)
{
    for (uint32_t i = 0U; i < ms; ++i)
        cpu_wait_for_interrupt();
}

#ifdef HOST_RTOS_IO
static BaseType_t scheduler_state;
static StaticTask_t simulated_task;
static uint32_t blocked_waits, notify_gives, timed_out_waits;

BaseType_t xTaskGetSchedulerState(void)
{
    return scheduler_state;
}

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    return &simulated_task;
}

void host_port_yield(BaseType_t woken)
{
    (void)woken;
}

void vTaskDelay(TickType_t ticks)
{
    assert(primask == 0U && ipsr == 0U);
    advance(ticks);
}

uint32_t ulTaskNotifyTakeIndexed(UBaseType_t index, BaseType_t clear, TickType_t timeout)
{
    assert(index == 1U && clear == pdTRUE && primask == 0U && ipsr == 0U);
    ++blocked_waits;
    while (simulated_task.notification[index] == 0U && timeout-- > 0U)
        cpu_wait_for_interrupt();
    const uint32_t count = simulated_task.notification[index];
    simulated_task.notification[index] = 0U;
    if (count == 0U)
        ++timed_out_waits;
    return count;
}

void vTaskNotifyGiveIndexedFromISR(TaskHandle_t task, UBaseType_t index, BaseType_t *woken)
{
    assert(ipsr >= 16U && irq_priority[ipsr - 16U] >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    assert(task == &simulated_task && index == 1U);
    ++task->notification[index];
    ++notify_gives;
    *woken = pdTRUE;
}
#endif

int main(void)
{
    /* Recover ordinary Sleep even if an earlier debug session left SCR bits;
       retain existing debug options while enabling Sleep debug clocks. */
    SCB->SCR = SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk;
    DBGMCU->CR = DBGMCU_CR_TRACE_IOEN;
    cpu_init();
    assert((SCB->SCR & (SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk)) == 0U);
    assert((DBGMCU->CR & DBGMCU_CR_DBG_SLEEP) != 0U);
    assert((DBGMCU->CR & DBGMCU_CR_TRACE_IOEN) != 0U);
    assert(timebase_init(180000000U));
#ifdef HOST_RTOS_IO
    assert(!io_delay_ms(1U)); /* Bootstrap retains the original WFI backend. */
    scheduler_state = taskSCHEDULER_RUNNING;
    simulated_task.notification[0] = 3U; /* Startup bits must survive I/O waits. */
    assert(io_delay_ms(2U));
#endif
    assert(joystick_service_init(90000000U));
    assert(adc_samples == 32U);
    assert(TIM3->PSC == 89U && TIM3->ARR == 9999U);
    assert((ADC1->CR2 & ADC_CR2_EXTSEL) == ADC_CR2_EXTSEL_3);
    assert((ADC1->CR2 & ADC_CR2_CONT) == 0U);
    assert((ADC1->SQR1 & ADC_SQR1_L) == ADC_SQR1_L_0);
    assert((DMA2_Stream4->CR & DMA_SxCR_CIRC) != 0U);
    joystick_input_t input;
    assert(joystick_service_read(&input) && input.x == 0 && input.y == 0);
    raw_x = 4095U;
    raw_y = 0U;
    advance(10U);
    assert(joystick_service_read(&input) && input.x == 1000 && input.y == -1000);

    /* SW is debounced/counted without foreground reads, even after release. */
    sw_pressed = true;
    advance(20U);
    sw_pressed = false;
    advance(40U);
    assert(joystick_service_read(&input) && input.press_sequence == 0U);
    sw_pressed = true;
    advance(50U);
    sw_pressed = false;
    advance(50U);
    assert(joystick_service_read(&input));
    assert(!input.pressed && input.press_sequence == 1U);
    b_pressed = true;
    menu_pressed = true;
    advance(20);
    b_pressed = false;
    menu_pressed = false;
    advance(40);
    assert(joystick_service_read(&input) && input.b_press_sequence == 0 &&
           input.menu_press_sequence == 0);
    b_pressed = true;
    menu_pressed = true;
    advance(50);
    b_pressed = false;
    menu_pressed = false;
    advance(50);
    assert(joystick_service_read(&input) && !input.b_pressed && !input.menu_pressed);
    assert(input.b_press_sequence == 1 && input.menu_press_sequence == 1);
    assert(input.press_sequence == 1); /* Independent A/B/MENU counters. */

    spi1_init(2U, false, false);
    spi1_dma_init();
#ifdef HOST_RTOS_IO
    /* Force completion in the exact unmask-before-Take window. */
    const uint32_t before_race = wall_ms;
    const uint32_t race_mask = cpu_irq_save();
    SPI1->CR2 |= SPI_CR2_RXNEIE;
    SPI1->SR = SPI_SR_TXE | SPI_SR_RXNE;
    pending[SPI1_IRQn] = true;
    io_wait_locked(IO_WAIT_SPI, race_mask, 10U);
    assert(wall_ms == before_race && simulated_task.notification[1] == 0U);
#endif
    SPI1->SR = SPI_SR_TXE;
    const uint32_t adc_config = DMA2_Stream4->CR;
    uint8_t rx;
    assert(spi1_transfer(0x36U, &rx) == SPI_STATUS_OK && rx == 0xA5U);
    static uint8_t pixels[4096];
    const uint32_t before_dma = timebase_millis();
    const uint32_t samples_before_dma = adc_samples;
    for (uint32_t i = 0U; i < 50U; ++i) {
        assert(spi1_write_dma(pixels, sizeof(pixels)) == SPI_STATUS_OK);
    }
    assert((uint32_t)(timebase_millis() - before_dma) == 100U);
    assert(adc_samples > samples_before_dma);
    assert(DMA2_Stream4->CR == adc_config);
    assert(joystick_service_read(&input));
    dma_error = true;
    assert(spi1_write_dma(pixels, 8U) == SPI_STATUS_DMA_ERROR);
    dma_error = false;
    dma_phase = 0U;
    spi_enabled = false;
    assert(spi1_transfer(0U, NULL) == SPI_STATUS_TIMEOUT);
    assert(spi1_write_dma(pixels, 8U) == SPI_STATUS_TIMEOUT);
    spi_enabled = true;
    assert(spi1_write_dma(pixels, 8U) == SPI_STATUS_OK);

    /* Real I2C driver: ACK probe, DMA payload, final BTF, errors and recovery.
       Unrelated DMA1 flags and the ADC stream must survive every transfer. */
    assert(i2c1_init(45000000U, 100000U) == I2C_STATUS_OK);
    assert(I2C1->CR2 == 45 && I2C1->CCR == 225 && I2C1->TRISE == 46);
    assert((DMA1_Stream6->CR & DMA_SxCR_CHSEL) == DMA_SxCR_CHSEL_0);
    assert(i2c1_write(0x3C, NULL, 0, 30) == I2C_STATUS_OK);
    set_readonly(&DMA1->HISR, DMA_HISR_TCIF5);
    assert(i2c1_write(0x3C, pixels, 129, 40) == I2C_STATUS_OK && i2c_completions == 1);
    assert((DMA1->HISR & DMA_HISR_TCIF5) != 0 && DMA2_Stream4->CR == adc_config);
    i2c_nack = true;
    assert(i2c1_write(0x3C, NULL, 0, 30) == I2C_STATUS_NACK);
    i2c_nack = false;
    i2c_dma_error = true;
    assert(i2c1_write(0x3C, pixels, 129, 40) == I2C_STATUS_DMA_ERROR);
    i2c_dma_error = false;
    i2c_bus_error = true;
    assert(i2c1_write(0x3C, NULL, 0, 30) == I2C_STATUS_BUS_ERROR);
    i2c_bus_error = false;
    i2c_enabled = false;
    assert(i2c1_write(0x3C, pixels, 129, 5) == I2C_STATUS_TIMEOUT);
    i2c_enabled = true;
    assert(i2c1_write(0x3D, pixels, 129, 40) == I2C_STATUS_OK);
    assert(i2c1_write(0x3C, NULL, 1, 30) == I2C_STATUS_INVALID_ARGUMENT);
    assert(irq_priority[I2C1_EV_IRQn] == 6 && irq_priority[DMA1_Stream6_IRQn] == 6);

    uart_init(45000000U, 115200U);
    uart_write("JOY\n");
    assert(transmitted_count == 0U); /* Enqueue does not wait for the wire. */
    assert(uart_flush(100U));
    assert(transmitted_count == 5U && memcmp(transmitted, "JOY\r\n", 5U) == 0);
    transmitted_count = 0U;
    for (uint32_t i = 0U; i < 2400U; ++i)
        uart_putc((char)('a' + i % 26U));
    assert(uart_flush(2000U));
    assert(transmitted_count == 2400U && uart_dropped_bytes() == 0U);
    for (uint32_t i = 0U; i < 2400U; ++i)
        assert(transmitted[i] == (char)('a' + i % 26U));
    uart_enabled = false;
    uart_write("timeout");
    assert(!uart_flush(5U));
    uart_enabled = true;
    assert(uart_flush(100U));

    /* A stalled ADC fails closed instead of returning stale sensor data. */
    samples_enabled = false;
    advance(60U);
    assert(!joystick_service_read(&input));
    adc1_stop();
    assert(!joystick_service_init(90000000U)); /* Calibration timeout sleeps. */
    samples_enabled = true;
    assert(joystick_service_init(90000000U));
    set_readonly(&DMA2->HISR, DMA_HISR_TEIF4);
    adc1_dma_irq_handler();
    assert(adc1_status() == ADC_STATUS_DMA_ERROR);
    assert((TIM3->CR1 & TIM_CR1_CEN) == 0U);
    assert(!joystick_service_read(&input));
    assert(joystick_service_init(90000000U));
    ADC1->SR = ADC_SR_OVR;
    adc1_irq_handler();
    assert(adc1_status() == ADC_STATUS_OVERRUN);
    assert((TIM3->CR1 & TIM_CR1_CEN) == 0U);
    assert(joystick_service_init(90000000U));
    set_readonly(&DMA2->HISR, DMA_HISR_HTIF4 | DMA_HISR_TCIF4);
    adc1_dma_irq_handler();
    assert(adc1_status() == ADC_STATUS_OVERRUN);
#ifdef HOST_RTOS_IO
    assert(blocked_waits > 0U && notify_gives > 0U && timed_out_waits > 0U);
    assert(simulated_task.notification[0] == 3U);
    assert(irq_priority[ADC_IRQn] == IO_IRQ_PRIORITY_ADC);
    assert(irq_priority[SPI1_IRQn] == IO_IRQ_PRIORITY_SPI);
    assert(irq_priority[USART2_IRQn] == IO_IRQ_PRIORITY_UART);
    printf("PASS: RTOS I/O adapter: %u waits, %u IRQ wakes, %u timeouts; "
           "unmask/notification race and startup-index isolation\n",
           blocked_waits, notify_gives, timed_out_waits);
#endif
    printf(
        "PASS: timer/ADC circular DMA, coherent input, retained SW presses, "
        "SPI IRQ completion/error/timeout/recovery, UART queue/CRLF/wrap, "
        "I2C ACK/DMA/BTF/NACK/error/timeout/recovery; ADC stale/error/timeout; %u simulated sleeps\n",
        sleeps);
    return 0;
}
