#include "cpu.h"

#include "stm32f446xx.h"

void cpu_init(void)
{
    /* Keep WFI in ordinary Sleep and retain HCLK for the debug interface.
       CPU still waits for IRQs; this development setting increases sleep
       power compared with turning the debug clock off (RM0390 DBG_SLEEP). */
    SCB->SCR &= ~(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk);
    DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP;
}

uint32_t cpu_irq_save(void)
{
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    return mask;
}

void cpu_irq_restore(uint32_t mask)
{
    __set_PRIMASK(mask);
}

void cpu_wait_for_interrupt(void)
{
    __DSB();
    __WFI();
    __ISB();
}
