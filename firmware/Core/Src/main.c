#include "main.h"

#include "app.h"
#include "cpu.h"
#include "timebase.h"
#include "uart.h"

void SystemClock_Config(void);

int main(void)
{
    cpu_init(); /* Configure debug-friendly Sleep before any WFI/error path. */
    HAL_Init();
    SystemClock_Config();

    SystemCoreClockUpdate();
    NVIC_SetPriorityGrouping(0U);

    if (!timebase_init(SystemCoreClock)) {
        Error_Handler();
    }

    /* USART2 is on APB1: HCLK 180 MHz / APB1 divider 4 = 45 MHz. */
    uart_init(SystemCoreClock / 4U, 115200U);
    uart_write("Clock ready: SYSCLK=180 MHz, PCLK1=45 MHz, PCLK2=90 MHz\n");
    uart_write("UART ready; starting FreeRTOS interrupt app\n");
    uart_write("Sleep debug clock enabled\n");

    if (!app_init()) {
        uart_write("ERROR: application initialization failed\n");
        Error_Handler();
    }
    app_start();
    Error_Handler(); /* A successfully started scheduler does not return. */
}

/* HSI -> PLL: SYSCLK/HCLK=180 MHz, PCLK1=45 MHz, PCLK2=90 MHz. */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clocks = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    oscillator.HSIState = RCC_HSI_ON;
    oscillator.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    oscillator.PLL.PLLState = RCC_PLL_ON;
    oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    oscillator.PLL.PLLM = 8U;
    oscillator.PLL.PLLN = 180U;
    oscillator.PLL.PLLP = RCC_PLLP_DIV2;
    oscillator.PLL.PLLQ = 7U;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        Error_Handler();
    }

    clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clocks.APB1CLKDivider = RCC_HCLK_DIV4;
    clocks.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    (void)uart_flush(200U);
    __disable_irq();
    for (;;) { __WFI(); }
}
