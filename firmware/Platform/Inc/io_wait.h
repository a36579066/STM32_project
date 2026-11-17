#ifndef CONSOLE_IO_WAIT_H
#define CONSOLE_IO_WAIT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    IO_WAIT_SPI,
    IO_WAIT_UART,
    IO_WAIT_ADC,
    IO_WAIT_I2C,
    IO_WAIT_COUNT
} io_wait_source_t;

/* IRQs that notify FreeRTOS tasks must be at or below its syscall ceiling. */
#define IO_IRQ_PRIORITY_ADC  5U
#define IO_IRQ_PRIORITY_SPI  6U
#define IO_IRQ_PRIORITY_UART 7U
#define IO_IRQ_PRIORITY_I2C  6U

/* Called with PRIMASK set, after checking a completion predicate. Registers
   the waiter before unmasking IRQs, and restores saved_mask before returning. */
void io_wait_locked(io_wait_source_t source, uint32_t saved_mask, uint32_t timeout_ms);
void io_wake_from_isr(io_wait_source_t source);

/* Returns false before the scheduler starts; the caller then uses its timebase. */
bool io_delay_ms(uint32_t milliseconds);

#endif
