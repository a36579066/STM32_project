#include "timebase.h"

#include "cpu.h"
#include "io_wait.h"
#include "stm32f446xx.h"

static volatile uint32_t system_milliseconds;

bool timebase_init(uint32_t hclk_hz)
{
    system_milliseconds = 0U;
    if ((hclk_hz < 1000U) || (SysTick_Config(hclk_hz / 1000U) != 0U)) {
        return false;
    }
    return true;
}

uint32_t timebase_millis(void)
{
    return system_milliseconds;
}

void timebase_delay_ms(uint32_t milliseconds)
{
    if (io_delay_ms(milliseconds))
        return;
    const uint32_t start = timebase_millis();
    while ((uint32_t)(timebase_millis() - start) < milliseconds) {
        const uint32_t mask = cpu_irq_save();
        if ((uint32_t)(timebase_millis() - start) < milliseconds) {
            cpu_wait_for_interrupt();
        }
        cpu_irq_restore(mask);
    }
}

void timebase_tick_isr(void)
{
    ++system_milliseconds;
}
