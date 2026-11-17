#ifndef HOST_CORE_CM4_H
#define HOST_CORE_CM4_H
#include <stdint.h>
#define __I  volatile const
#define __O  volatile
#define __IO volatile

typedef struct {
    volatile uint32_t SCR;
} SCB_Type;

extern SCB_Type host_scb;
#define SCB                     (&host_scb)
#define SCB_SCR_SLEEPDEEP_Msk   (1UL << 2U)
#define SCB_SCR_SLEEPONEXIT_Msk (1UL << 1U)
uint32_t __get_PRIMASK(void);
uint32_t __get_IPSR(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t mask);
void __WFI(void);
void NVIC_ClearPendingIRQ(IRQn_Type irq);
void NVIC_SetPriority(IRQn_Type irq, uint32_t priority);
void NVIC_EnableIRQ(IRQn_Type irq);
void NVIC_DisableIRQ(IRQn_Type irq);

static inline void __DSB(void)
{
}

static inline void __ISB(void)
{
}

static inline uint32_t SysTick_Config(uint32_t ticks)
{
    return ticks == 0U;
}
#endif
