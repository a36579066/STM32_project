#ifndef CONSOLE_I2C_H
#define CONSOLE_I2C_H
#include <stdint.h>

typedef enum {
    I2C_STATUS_OK,
    I2C_STATUS_NACK,
    I2C_STATUS_TIMEOUT,
    I2C_STATUS_BUSY,
    I2C_STATUS_DMA_ERROR,
    I2C_STATUS_BUS_ERROR,
    I2C_STATUS_INVALID_ARGUMENT,
} i2c_status_t;

/* I2C1: PB8/PB9 configured by Board. DMA1 Stream 6 Channel 1 handles TX. */
i2c_status_t i2c1_init(uint32_t pclk1_hz, uint32_t bus_hz);
/* addr7 is an unshifted 7-bit address. A zero-length write probes ACK. */
i2c_status_t i2c1_write(uint8_t addr7, const uint8_t *data, uint16_t length, uint32_t timeout_ms);
void i2c1_event_irq_handler(void);
void i2c1_error_irq_handler(void);
void i2c1_dma_tx_irq_handler(void);
const char *i2c_status_name(i2c_status_t status);
#endif
