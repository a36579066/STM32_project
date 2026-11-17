#ifndef HOST_HARDWARE_H
#define HOST_HARDWARE_H
/* Use ST's actual register layouts/bit masks, with RAM-backed instances. */
#include "stm32f446xx.h"
extern RCC_TypeDef host_rcc;
extern ADC_Common_TypeDef host_adc_common;
extern ADC_TypeDef host_adc1;
extern DMA_TypeDef host_dma2;
extern DMA_TypeDef host_dma1;
extern DMA_Stream_TypeDef host_stream6;
extern I2C_TypeDef host_i2c1;
extern DMA_Stream_TypeDef host_stream0, host_stream3, host_stream4;
extern TIM_TypeDef host_tim3;
extern SPI_TypeDef host_spi1;
extern USART_TypeDef host_usart2;
extern GPIO_TypeDef host_gpioa;
extern DBGMCU_TypeDef host_dbgmcu;
#undef RCC
#undef ADC
#undef ADC1
#undef DMA2
#undef DMA1
#undef DMA1_Stream6
#undef I2C1
#undef DMA2_Stream0
#undef DMA2_Stream3
#undef DMA2_Stream4
#undef TIM3
#undef SPI1
#undef USART2
#undef GPIOA
#undef DBGMCU
#define RCC          (&host_rcc)
#define ADC          (&host_adc_common)
#define ADC1         (&host_adc1)
#define DMA2         (&host_dma2)
#define DMA1         (&host_dma1)
#define DMA1_Stream6 (&host_stream6)
#define I2C1         (&host_i2c1)
#define DMA2_Stream0 (&host_stream0)
#define DMA2_Stream3 (&host_stream3)
#define DMA2_Stream4 (&host_stream4)
#define TIM3         (&host_tim3)
#define SPI1         (&host_spi1)
#define USART2       (&host_usart2)
#define GPIOA        (&host_gpioa)
#define DBGMCU       (&host_dbgmcu)
#endif
