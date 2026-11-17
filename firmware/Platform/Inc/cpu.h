#ifndef CONSOLE_CPU_H
#define CONSOLE_CPU_H

#include <stdint.h>

/* Ordinary Sleep with debug clocks retained, so SWD can attach during WFI. */
void cpu_init(void);

/* Restore the returned PRIMASK after a short critical section. */
uint32_t cpu_irq_save(void);
void cpu_irq_restore(uint32_t mask);

/* Sleep until an interrupt is pending. Peripheral clocks continue running.
   When waiting on a flag, mask IRQs, recheck it, sleep, then restore PRIMASK. */
void cpu_wait_for_interrupt(void);

#endif
