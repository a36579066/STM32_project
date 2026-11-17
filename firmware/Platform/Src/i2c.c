#include "i2c.h"
#include <stdbool.h>
#include <stddef.h>
#include "cpu.h"
#include "io_wait.h"
#include "stm32f446xx.h"
#include "timebase.h"

#define TX_FLAGS                                                                                   \
    (DMA_HISR_FEIF6 | DMA_HISR_DMEIF6 | DMA_HISR_TEIF6 | DMA_HISR_HTIF6 | DMA_HISR_TCIF6)
#define TX_ERRORS  (DMA_HISR_FEIF6 | DMA_HISR_DMEIF6 | DMA_HISR_TEIF6)
#define BUS_ERRORS (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR | I2C_SR1_TIMEOUT)
#define REQUESTS   (I2C_CR2_DMAEN | I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN)
static volatile bool active, complete, dma_done;
static volatile i2c_status_t result;
static uint8_t address;
static uint16_t count;
static uint32_t saved_pclk, saved_bus;

static void configure_timing(void)
{
    const uint32_t mhz = saved_pclk / 1000000U;
    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0U;
    I2C1->CR2 = mhz;
    if (saved_bus <= 100000U) {
        I2C1->CCR = (saved_pclk + 2U * saved_bus - 1U) / (2U * saved_bus);
        I2C1->TRISE = mhz + 1U;
    } else {
        I2C1->CCR = I2C_CCR_FS | ((saved_pclk + 3U * saved_bus - 1U) / (3U * saved_bus));
        I2C1->TRISE = mhz * 300U / 1000U + 1U;
    }
    I2C1->CR1 = I2C_CR1_PE;
}

static bool bus_idle(uint32_t timeout_ms)
{
    const uint32_t start = timebase_millis();
    for (;;) {
        const uint32_t mask = cpu_irq_save();
        const bool idle = (I2C1->SR2 & I2C_SR2_BUSY) == 0U && (I2C1->CR1 & I2C_CR1_STOP) == 0U &&
                          (DMA1_Stream6->CR & DMA_SxCR_EN) == 0U;
        const bool expired = (uint32_t)(timebase_millis() - start) >= timeout_ms;
        if (!idle && !expired)
            io_wait_locked(IO_WAIT_I2C, mask, 1U);
        else
            cpu_irq_restore(mask);
        if (idle || expired)
            return idle;
    }
}

i2c_status_t i2c1_init(uint32_t pclk1_hz, uint32_t bus_hz)
{
    if (pclk1_hz < 4000000U || pclk1_hz > 50000000U || pclk1_hz % 1000000U != 0U ||
        bus_hz < 10000U || bus_hz > 400000U)
        return I2C_STATUS_INVALID_ARGUMENT;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    if (active || (DMA1_Stream6->CR & DMA_SxCR_EN) != 0U)
        return I2C_STATUS_BUSY;
    saved_pclk = pclk1_hz;
    saved_bus = bus_hz;
    configure_timing();
    DMA1_Stream6->CR = DMA_SxCR_CHSEL_0 | DMA_SxCR_DIR_0 | DMA_SxCR_MINC | DMA_SxCR_TCIE |
                       DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    DMA1_Stream6->PAR = (uint32_t)(uintptr_t)&I2C1->DR;
    DMA1_Stream6->FCR = DMA_SxFCR_FEIE;
    DMA1->HIFCR = TX_FLAGS;
    const IRQn_Type irqs[] = {I2C1_EV_IRQn, I2C1_ER_IRQn, DMA1_Stream6_IRQn};
    for (uint32_t i = 0; i < 3U; ++i) {
        NVIC_ClearPendingIRQ(irqs[i]);
        NVIC_SetPriority(irqs[i], IO_IRQ_PRIORITY_I2C);
        NVIC_EnableIRQ(irqs[i]);
    }
    return I2C_STATUS_OK;
}

static void finish(i2c_status_t status, bool stop)
{
    I2C1->CR2 &= ~REQUESTS;
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    if (stop)
        I2C1->CR1 |= I2C_CR1_STOP;
    result = status;
    complete = true;
    io_wake_from_isr(IO_WAIT_I2C);
}

i2c_status_t i2c1_write(uint8_t addr7, const uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    if (addr7 < 8U || addr7 > 0x77U || (length > 0U && data == NULL) || timeout_ms == 0U ||
        saved_pclk == 0U)
        return I2C_STATUS_INVALID_ARGUMENT;
    if (active || __get_IPSR() != 0U || __get_PRIMASK() != 0U)
        return I2C_STATUS_BUSY;
    if (!bus_idle(timeout_ms)) {
        configure_timing();
        return I2C_STATUS_BUSY;
    }
    I2C1->CR2 &= ~REQUESTS;
    DMA1->HIFCR = TX_FLAGS;
    I2C1->SR1 &= ~BUS_ERRORS;
    DMA1_Stream6->M0AR = (uint32_t)(uintptr_t)data;
    DMA1_Stream6->NDTR = length;
    address = addr7;
    count = length;
    result = I2C_STATUS_OK;
    complete = false;
    dma_done = false;
    active = true;
    const uint32_t start = timebase_millis();
    I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;
    I2C1->CR1 |= I2C_CR1_START;
    for (;;) {
        const uint32_t mask = cpu_irq_save();
        const bool done = complete;
        const uint32_t elapsed = (uint32_t)(timebase_millis() - start);
        const bool expired = elapsed >= timeout_ms;
        if (!done && !expired)
            io_wait_locked(IO_WAIT_I2C, mask, timeout_ms - elapsed);
        else
            cpu_irq_restore(mask);
        if (done)
            break;
        if (expired) {
            result = I2C_STATUS_TIMEOUT;
            break;
        }
    }
    I2C1->CR2 &= ~REQUESTS;
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    if (!complete)
        I2C1->CR1 |= I2C_CR1_STOP;
    if (!bus_idle(10U)) {
        if (result == I2C_STATUS_OK)
            result = I2C_STATUS_TIMEOUT;
        configure_timing();
    }
    DMA1->HIFCR = TX_FLAGS;
    active = false;
    return result;
}

void i2c1_event_irq_handler(void)
{
    const uint32_t status = I2C1->SR1;
    if (!active || complete)
        return;
    if (status & I2C_SR1_SB) {
        I2C1->DR = (uint32_t)address << 1U;
    } else if (status & I2C_SR1_ADDR) {
        (void)I2C1->SR2; /* SR1 then SR2 clears ADDR. */
        if (count == 0U)
            finish(I2C_STATUS_OK, true);
        else {
            /* Suppress BTF while DMA is still active. It is re-enabled at TC. */
            I2C1->CR2 &= ~I2C_CR2_ITEVTEN;
            DMA1_Stream6->CR |= DMA_SxCR_EN;
            I2C1->CR2 |= I2C_CR2_DMAEN;
        }
    } else if ((status & I2C_SR1_BTF) && dma_done)
        finish(I2C_STATUS_OK, true);
}

void i2c1_error_irq_handler(void)
{
    const uint32_t status = I2C1->SR1 & BUS_ERRORS;
    I2C1->SR1 &= ~status;
    if (!active || complete)
        return;
    finish((status & I2C_SR1_AF) ? I2C_STATUS_NACK : I2C_STATUS_BUS_ERROR,
           (status & I2C_SR1_ARLO) == 0U);
}

void i2c1_dma_tx_irq_handler(void)
{
    const uint32_t flags = DMA1->HISR & TX_FLAGS;
    DMA1->HIFCR = flags;
    if (!active || complete)
        return;
    if (flags & TX_ERRORS)
        finish(I2C_STATUS_DMA_ERROR, true);
    else if (flags & DMA_HISR_TCIF6) {
        I2C1->CR2 &= ~I2C_CR2_DMAEN;
        dma_done = true;
        I2C1->CR2 |= I2C_CR2_ITEVTEN; /* BTF confirms the final ACK, before STOP. */
    }
}

const char *i2c_status_name(i2c_status_t status)
{
    switch (status) {
    case I2C_STATUS_OK:
        return "OK";
    case I2C_STATUS_NACK:
        return "NACK";
    case I2C_STATUS_TIMEOUT:
        return "timeout";
    case I2C_STATUS_BUSY:
        return "bus busy";
    case I2C_STATUS_DMA_ERROR:
        return "DMA error";
    case I2C_STATUS_BUS_ERROR:
        return "I2C bus error";
    default:
        return "invalid argument";
    }
}
