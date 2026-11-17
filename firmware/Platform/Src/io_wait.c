/* Bare-metal backend retained for the original peripheral host regressions.
   The RTOS firmware links RTOS/Src/io_wait_rtos.c instead. */
#include "io_wait.h"
#include "cpu.h"

void io_wait_locked(io_wait_source_t source, uint32_t saved_mask, uint32_t timeout_ms)
{
    (void)source;
    (void)timeout_ms;
    cpu_wait_for_interrupt();
    cpu_irq_restore(saved_mask);
}

void io_wake_from_isr(io_wait_source_t source)
{
    (void)source;
}

bool io_delay_ms(uint32_t milliseconds)
{
    (void)milliseconds;
    return false;
}
