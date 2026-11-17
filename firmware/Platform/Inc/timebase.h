#ifndef CONSOLE_TIMEBASE_H
#define CONSOLE_TIMEBASE_H

#include <stdbool.h>
#include <stdint.h>

/* Configure a 1 ms CMSIS SysTick from the current HCLK frequency. */
bool timebase_init(uint32_t hclk_hz);
uint32_t timebase_millis(void);
void timebase_delay_ms(uint32_t milliseconds);

/* Called only by SysTick_Handler. */
void timebase_tick_isr(void);

#endif /* CONSOLE_TIMEBASE_H */
