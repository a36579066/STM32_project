#ifndef CONSOLE_SPI_H
#define CONSOLE_SPI_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx.h"

typedef enum {
    SPI_STATUS_OK = 0,
    SPI_STATUS_TIMEOUT,
    SPI_STATUS_INVALID_ARGUMENT,
    SPI_STATUS_BUSY,
    SPI_STATUS_DMA_ERROR,
    SPI_STATUS_BUS_ERROR
} spi_status_t;

/* Configure SPI1 peripheral registers; the Board layer owns GPIO pin muxing. */
void spi1_init(uint32_t baud_prescaler, bool cpol, bool cpha);

/* Foreground only, IRQs enabled. Waits sleep until SPI/DMA IRQ completion.
   Send one byte and consume the simultaneously received byte. */
spi_status_t spi1_transfer(uint8_t tx, uint8_t *rx);

/* Configure DMA2 streams used by SPI1. Call once after spi1_init(). */
void spi1_dma_init(void);

/* Send a byte buffer with DMA; the caller controls the display's CS pin. */
spi_status_t spi1_write_dma(const uint8_t *data, uint16_t length);

void spi1_irq_handler(void);
void spi1_dma_rx_irq_handler(void);
void spi1_dma_tx_irq_handler(void);

#endif /* CONSOLE_SPI_H */
