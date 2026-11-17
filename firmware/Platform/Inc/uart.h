#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx.h"

void uart_init(uint32_t pclk1_hz, uint32_t baud_rate);
void uart_putc(char c);
void uart_write(const char *s);
/* Drain queued output before entering a fatal handler that masks interrupts. */
bool uart_flush(uint32_t timeout_ms);
uint32_t uart_dropped_bytes(void);
void uart_irq_handler(void);

#endif /* UART_H */
