#include "spi.h"

#include <stddef.h>
#include "cpu.h"
#include "io_wait.h"
#include "timebase.h"

#define SPI1_TIMEOUT_MS     10U
#define SPI1_DMA_TIMEOUT_MS 1000U
#define SPI1_DMA_RX_FLAGS                                                                          \
    (DMA_LISR_FEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_TEIF0 | DMA_LISR_HTIF0 | DMA_LISR_TCIF0)
#define SPI1_DMA_TX_FLAGS                                                                          \
    (DMA_LISR_FEIF3 | DMA_LISR_DMEIF3 | DMA_LISR_TEIF3 | DMA_LISR_HTIF3 | DMA_LISR_TCIF3)
#define SPI1_DMA_RX_ERRORS (DMA_LISR_FEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_TEIF0)
#define SPI1_DMA_TX_ERRORS (DMA_LISR_FEIF3 | DMA_LISR_DMEIF3 | DMA_LISR_TEIF3)

static volatile uint8_t spi1_dma_rx_sink;
static volatile uint8_t received_byte;
static volatile bool byte_complete;
static volatile uint32_t dma_complete;
static volatile spi_status_t transfer_status;
static bool transfer_active;

/* The uncommon register settling/stream-disable waits also sleep. SysTick
   supplies the timeout wakeup if the peripheral never reaches its idle state. */
static bool wait_register(volatile uint32_t *reg, uint32_t flag, bool expected_set,
                          uint32_t timeout_ms)
{
    const uint32_t start = timebase_millis();
    for (;;) {
        const uint32_t mask = cpu_irq_save();
        const bool ready = ((*reg & flag) != 0U) == expected_set;
        const bool expired = (uint32_t)(timebase_millis() - start) >= timeout_ms;
        if (!ready && !expired) {
            /* BSY/stream disable have no dedicated completion interrupt. */
            io_wait_locked(IO_WAIT_SPI, mask, 1U);
        } else
            cpu_irq_restore(mask);
        if (ready || expired) {
            return ready;
        }
    }
}

static spi_status_t wait_transfer(bool dma, uint32_t timeout_ms)
{
    const uint32_t start = timebase_millis();
    for (;;) {
        const uint32_t mask = cpu_irq_save();
        const spi_status_t status = transfer_status;
        const bool done = dma ? (dma_complete == 3U) : byte_complete;
        const bool expired = (uint32_t)(timebase_millis() - start) >= timeout_ms;
        /* Check under PRIMASK before WFI: completion cannot be consumed in the
           gap and leave us asleep until an unrelated SysTick. */
        if (!done && (status == SPI_STATUS_OK) && !expired) {
            io_wait_locked(IO_WAIT_SPI, mask, timeout_ms - (uint32_t)(timebase_millis() - start));
        } else
            cpu_irq_restore(mask);
        if (status != SPI_STATUS_OK)
            return status;
        if (done)
            return SPI_STATUS_OK;
        if (expired)
            return SPI_STATUS_TIMEOUT;
    }
}

static void clear_rx_state(void)
{
    const uint32_t status = SPI1->SR;
    if ((status & (SPI_SR_OVR | SPI_SR_RXNE)) != 0U) {
        (void)*(__IO uint8_t *)&SPI1->DR;
        (void)SPI1->SR;
    }
}

void spi1_init(uint32_t baud_prescaler, bool cpol, bool cpha)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | ((baud_prescaler & 7U) << 3U) |
                (cpol ? SPI_CR1_CPOL : 0U) | (cpha ? SPI_CR1_CPHA : 0U);
    SPI1->CR2 = 0U;
    transfer_active = false;
    NVIC_ClearPendingIRQ(SPI1_IRQn);
    NVIC_SetPriority(SPI1_IRQn, IO_IRQ_PRIORITY_SPI);
    NVIC_EnableIRQ(SPI1_IRQn);
}

spi_status_t spi1_transfer(uint8_t tx, uint8_t *rx)
{
    if (transfer_active || (__get_PRIMASK() != 0U) || (__get_IPSR() != 0U)) {
        return SPI_STATUS_BUSY;
    }
    transfer_active = true;
    spi_status_t status = SPI_STATUS_TIMEOUT;
    if (wait_register(&SPI1->SR, SPI_SR_TXE, true, SPI1_TIMEOUT_MS) &&
        wait_register(&SPI1->SR, SPI_SR_BSY, false, SPI1_TIMEOUT_MS)) {
        clear_rx_state();
        byte_complete = false;
        transfer_status = SPI_STATUS_OK;
        SPI1->CR1 |= SPI_CR1_SPE;
        SPI1->CR2 |= SPI_CR2_RXNEIE | SPI_CR2_ERRIE;
        *(__IO uint8_t *)&SPI1->DR = tx;
        status = wait_transfer(false, SPI1_TIMEOUT_MS);
    }
    SPI1->CR2 &= ~(SPI_CR2_RXNEIE | SPI_CR2_ERRIE);
    if ((status == SPI_STATUS_OK) &&
        !wait_register(&SPI1->SR, SPI_SR_BSY, false, SPI1_TIMEOUT_MS)) {
        status = SPI_STATUS_TIMEOUT;
    }
    if ((status == SPI_STATUS_OK) && (rx != NULL)) {
        *rx = received_byte;
    }
    if (status != SPI_STATUS_OK) {
        SPI1->CR1 &= ~SPI_CR1_SPE;
        clear_rx_state();
    }
    transfer_active = false;
    return status;
}

void spi1_dma_init(void)
{
    const uint32_t channel_3 = DMA_SxCR_CHSEL_0 | DMA_SxCR_CHSEL_1;
    const uint32_t interrupts = DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC->AHB1ENR;
    DMA2_Stream0->CR = channel_3 | interrupts | DMA_SxCR_PL_0;
    DMA2_Stream0->PAR = (uint32_t)(uintptr_t)&SPI1->DR;
    DMA2_Stream0->M0AR = (uint32_t)(uintptr_t)&spi1_dma_rx_sink;
    DMA2_Stream0->FCR = DMA_SxFCR_FEIE;
    DMA2_Stream3->CR = channel_3 | interrupts | DMA_SxCR_PL_0 | DMA_SxCR_DIR_0 | DMA_SxCR_MINC;
    DMA2_Stream3->PAR = (uint32_t)(uintptr_t)&SPI1->DR;
    DMA2_Stream3->FCR = DMA_SxFCR_FEIE;
    DMA2->LIFCR = SPI1_DMA_RX_FLAGS | SPI1_DMA_TX_FLAGS;
    NVIC_ClearPendingIRQ(DMA2_Stream0_IRQn);
    NVIC_ClearPendingIRQ(DMA2_Stream3_IRQn);
    NVIC_SetPriority(DMA2_Stream0_IRQn, IO_IRQ_PRIORITY_SPI);
    NVIC_SetPriority(DMA2_Stream3_IRQn, IO_IRQ_PRIORITY_SPI);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    NVIC_EnableIRQ(DMA2_Stream3_IRQn);
}

spi_status_t spi1_write_dma(const uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length == 0U)) {
        return SPI_STATUS_INVALID_ARGUMENT;
    }
    if (transfer_active || (__get_PRIMASK() != 0U) || (__get_IPSR() != 0U) ||
        ((DMA2_Stream0->CR & DMA_SxCR_EN) != 0U) || ((DMA2_Stream3->CR & DMA_SxCR_EN) != 0U)) {
        return SPI_STATUS_BUSY;
    }
    transfer_active = true;
    if (!wait_register(&SPI1->SR, SPI_SR_TXE, true, SPI1_TIMEOUT_MS) ||
        !wait_register(&SPI1->SR, SPI_SR_BSY, false, SPI1_TIMEOUT_MS)) {
        transfer_active = false;
        return SPI_STATUS_TIMEOUT;
    }
    clear_rx_state();
    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR2 &= ~(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN | SPI_CR2_RXNEIE);
    DMA2->LIFCR = SPI1_DMA_RX_FLAGS | SPI1_DMA_TX_FLAGS;
    NVIC_ClearPendingIRQ(DMA2_Stream0_IRQn);
    NVIC_ClearPendingIRQ(DMA2_Stream3_IRQn);
    dma_complete = 0U;
    transfer_status = SPI_STATUS_OK;
    DMA2_Stream0->NDTR = length;
    DMA2_Stream3->NDTR = length;
    DMA2_Stream3->M0AR = (uint32_t)(uintptr_t)data;

    /* RX request -> streams -> TX request -> SPI, as in RM0390. */
    SPI1->CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_ERRIE;
    DMA2_Stream0->CR |= DMA_SxCR_EN;
    DMA2_Stream3->CR |= DMA_SxCR_EN;
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
    SPI1->CR1 |= SPI_CR1_SPE;
    spi_status_t status = wait_transfer(true, SPI1_DMA_TIMEOUT_MS);

    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    DMA2_Stream3->CR &= ~DMA_SxCR_EN;
    const bool rx_disabled =
        wait_register(&DMA2_Stream0->CR, DMA_SxCR_EN, false, SPI1_DMA_TIMEOUT_MS);
    const bool tx_disabled =
        wait_register(&DMA2_Stream3->CR, DMA_SxCR_EN, false, SPI1_DMA_TIMEOUT_MS);
    if (!rx_disabled || !tx_disabled ||
        !wait_register(&SPI1->SR, SPI_SR_TXE, true, SPI1_TIMEOUT_MS) ||
        !wait_register(&SPI1->SR, SPI_SR_BSY, false, SPI1_TIMEOUT_MS)) {
        status = SPI_STATUS_TIMEOUT;
    }
    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR2 &= ~(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN | SPI_CR2_ERRIE);
    clear_rx_state();
    DMA2->LIFCR = SPI1_DMA_RX_FLAGS | SPI1_DMA_TX_FLAGS;
    transfer_active = false;
    return status;
}

void spi1_irq_handler(void)
{
    const uint32_t status = SPI1->SR;
    if ((status & (SPI_SR_OVR | SPI_SR_MODF | SPI_SR_FRE)) != 0U) {
        clear_rx_state();
        if ((status & SPI_SR_MODF) != 0U) {
            SPI1->CR1 |= SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI;
        }
        transfer_status = SPI_STATUS_BUS_ERROR;
        SPI1->CR2 &= ~(SPI_CR2_RXNEIE | SPI_CR2_ERRIE);
    } else if (((status & SPI_SR_RXNE) != 0U) && ((SPI1->CR2 & SPI_CR2_RXNEIE) != 0U)) {
        received_byte = *(__IO uint8_t *)&SPI1->DR;
        byte_complete = true;
        SPI1->CR2 &= ~SPI_CR2_RXNEIE;
    }
    io_wake_from_isr(IO_WAIT_SPI);
}

void spi1_dma_rx_irq_handler(void)
{
    const uint32_t flags = DMA2->LISR & SPI1_DMA_RX_FLAGS;
    DMA2->LIFCR = flags;
    if ((flags & SPI1_DMA_RX_ERRORS) != 0U)
        transfer_status = SPI_STATUS_DMA_ERROR;
    if ((flags & DMA_LISR_TCIF0) != 0U)
        dma_complete |= 1U;
    io_wake_from_isr(IO_WAIT_SPI);
}

void spi1_dma_tx_irq_handler(void)
{
    const uint32_t flags = DMA2->LISR & SPI1_DMA_TX_FLAGS;
    DMA2->LIFCR = flags;
    if ((flags & SPI1_DMA_TX_ERRORS) != 0U)
        transfer_status = SPI_STATUS_DMA_ERROR;
    if ((flags & DMA_LISR_TCIF3) != 0U)
        dma_complete |= 2U;
    io_wake_from_isr(IO_WAIT_SPI);
}
