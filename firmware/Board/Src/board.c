#include "board.h"

#include "board_pins.h"
#include "gpio.h"
#include <stm32f446xx.h>

void board_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;

    gpio_config_output(BOARD_LED_PORT, BOARD_LED_PIN, BOARD_LED_ACTIVE_HIGH == 0U);
}

void board_tft_pins_init(void)
{
    const uint32_t sck_shift = 2U * BOARD_TFT_SCK_PIN;
    const uint32_t mosi_shift = 2U * BOARD_TFT_MOSI_PIN;
    const uint32_t spi_pins = (1UL << BOARD_TFT_SCK_PIN) | (1UL << BOARD_TFT_MOSI_PIN);

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;

    /* PA5 and PA7 Alternate Function mode. */
    GPIOA->MODER &= ~((3UL << sck_shift) | (3UL << mosi_shift));
    GPIOA->MODER |= (2UL << sck_shift) | (2UL << mosi_shift);
    GPIOA->OTYPER &= ~spi_pins; /* Push-pull. */
    GPIOA->OSPEEDR &= ~((3UL << sck_shift) | (3UL << mosi_shift));
    GPIOA->OSPEEDR |=
        (2UL << sck_shift) | (2UL << mosi_shift); /* High GPIO speed for 11.25 MHz SPI1. */
    GPIOA->PUPDR &= ~((3UL << sck_shift) | (3UL << mosi_shift)); /* No pulls. */

    /* AF5 selects SPI1_SCK on PA5 and SPI1_MOSI on PA7. */
    GPIOA->AFR[0] &= ~((0xFUL << (4U * BOARD_TFT_SCK_PIN)) | (0xFUL << (4U * BOARD_TFT_MOSI_PIN)));
    GPIOA->AFR[0] |= (5UL << (4U * BOARD_TFT_SCK_PIN)) | (5UL << (4U * BOARD_TFT_MOSI_PIN));

    /* CS inactive-high, D/C command-low, and RESET inactive-high. */
    gpio_config_output(BOARD_TFT_CS_PORT, BOARD_TFT_CS_PIN, true);
    gpio_config_output(BOARD_TFT_DC_PORT, BOARD_TFT_DC_PIN, false);
    gpio_config_output(BOARD_TFT_RESET_PORT, BOARD_TFT_RESET_PIN, true);
}

void board_tft_cs_set(bool active)
{
    gpio_write(BOARD_TFT_CS_PORT, BOARD_TFT_CS_PIN, !active);
}

void board_tft_dc_set(bool data_mode)
{
    gpio_write(BOARD_TFT_DC_PORT, BOARD_TFT_DC_PIN, data_mode);
}

void board_tft_reset_set(bool high)
{
    gpio_write(BOARD_TFT_RESET_PORT, BOARD_TFT_RESET_PIN, high);
}

void board_button_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR; // make sure clock enable is effective before configuring the pin

    gpio_config_input(BOARD_BUTTON_PORT, BOARD_BUTTON_PIN);

    // Enable SYSCFG clock
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;

    // Clear EXTI13 selection bits [7:4]
    SYSCFG->EXTICR[3] &= ~(0xFU << 4);
    // Select GPIOC for EXTI13
    SYSCFG->EXTICR[3] |= (2U << 4);

    // Experiment mode: detect press edges only; debounce is intentionally omitted.
    EXTI->RTSR &= ~EXTI_RTSR_TR13;
    EXTI->FTSR |= EXTI_FTSR_TR13;
    EXTI->PR = EXTI_PR_PR13; // Clear pending by writing 1.

    // Set EXTI15_10_IRQn priority to 5 and enable it.
    NVIC_SetPriority(EXTI15_10_IRQn, 5);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    EXTI->IMR |= EXTI_IMR_MR13; // Unmask EXTI13 after configuration.
}

void board_led_set(bool on)
{
    const bool high = BOARD_LED_ACTIVE_HIGH != 0U ? on : !on;

    gpio_write(BOARD_LED_PORT, BOARD_LED_PIN, high);
}

bool board_button_is_pressed(void)
{
    // low is pressed, high is released
    return !gpio_read(BOARD_BUTTON_PORT, BOARD_BUTTON_PIN);
}

void board_joystick_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;

    const uint32_t x_shift = 2U * BOARD_JOYSTICK_X_PIN;
    const uint32_t y_shift = 2U * BOARD_JOYSTICK_Y_PIN;
    GPIOA->MODER |= (3UL << x_shift) | (3UL << y_shift); /* Analog. */
    GPIOA->PUPDR &= ~((3UL << x_shift) | (3UL << y_shift));

    const uint32_t shift = 2U * BOARD_JOYSTICK_SW_PIN;
    const uint32_t pin = 1UL << BOARD_JOYSTICK_SW_PIN;
    GPIOB->MODER &= ~(3UL << shift); /* Input. */
    GPIOB->OTYPER &= ~pin;
    GPIOB->OSPEEDR &= ~(3UL << shift);
    GPIOB->PUPDR = (GPIOB->PUPDR & ~(3UL << shift)) | (1UL << shift);

    GPIOB->MODER &= ~(3UL << (2U * BOARD_GAME_B_PIN));
    GPIOB->PUPDR =
        (GPIOB->PUPDR & ~(3UL << (2U * BOARD_GAME_B_PIN))) | (1UL << (2U * BOARD_GAME_B_PIN));
    GPIOA->MODER &= ~(3UL << (2U * BOARD_GAME_MENU_PIN));
    GPIOA->PUPDR =
        (GPIOA->PUPDR & ~(3UL << (2U * BOARD_GAME_MENU_PIN))) | (1UL << (2U * BOARD_GAME_MENU_PIN));
}

bool board_joystick_button_is_pressed(void)
{
    return (GPIOB->IDR & (1UL << BOARD_JOYSTICK_SW_PIN)) == 0U;
}

bool board_game_b_is_pressed(void)
{
    return (GPIOB->IDR & (1UL << BOARD_GAME_B_PIN)) == 0U;
}

bool board_game_menu_is_pressed(void)
{
    return (GPIOA->IDR & (1UL << BOARD_GAME_MENU_PIN)) == 0U;
}

void board_oled_pins_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    const uint32_t pins = (1UL << BOARD_OLED_SCL_PIN) | (1UL << BOARD_OLED_SDA_PIN);
    const uint32_t fields = (3UL << (2U * BOARD_OLED_SCL_PIN)) | (3UL << (2U * BOARD_OLED_SDA_PIN));
    GPIOB->MODER = (GPIOB->MODER & ~fields) | (2UL << (2U * BOARD_OLED_SCL_PIN)) |
                   (2UL << (2U * BOARD_OLED_SDA_PIN));
    GPIOB->OTYPER |= pins; /* I2C is open drain. Module pull-ups should go to 3V3. */
    GPIOB->OSPEEDR = (GPIOB->OSPEEDR & ~fields) | (1UL << (2U * BOARD_OLED_SCL_PIN)) |
                     (1UL << (2U * BOARD_OLED_SDA_PIN));
    GPIOB->PUPDR = (GPIOB->PUPDR & ~fields) | (1UL << (2U * BOARD_OLED_SCL_PIN)) |
                   (1UL << (2U * BOARD_OLED_SDA_PIN));
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~0xFFUL) | 0x44UL;
}
