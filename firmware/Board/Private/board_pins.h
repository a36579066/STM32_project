#ifndef CONSOLE_BOARD_PINS_H
#define CONSOLE_BOARD_PINS_H

#include "stm32f4xx.h"

/* Physical wiring belongs here; application code uses board.h instead. */
#define BOARD_LED_PORT        GPIOA
#define BOARD_LED_PIN         5U
#define BOARD_LED_ACTIVE_HIGH 1U

/* NUCLEO-F446RE B1 is active-low on PC13 and routed to EXTI13. */
#define BOARD_BUTTON_PORT GPIOC
#define BOARD_BUTTON_PIN  13U

/* KY-023 joystick on NUCLEO-F446RE. */
#define BOARD_JOYSTICK_X_PORT  GPIOA
#define BOARD_JOYSTICK_X_PIN   0U /* PA0 / A0 / ADC1_IN0 */
#define BOARD_JOYSTICK_Y_PORT  GPIOA
#define BOARD_JOYSTICK_Y_PIN   1U /* PA1 / A1 / ADC1_IN1 */
#define BOARD_JOYSTICK_SW_PORT GPIOB
#define BOARD_JOYSTICK_SW_PIN  0U /* PB0 / A3, active-low switch */

#define BOARD_GAME_B_PIN    5U /* PB5 / D4, active low */
#define BOARD_GAME_MENU_PIN 8U /* PA8 / D7, active low */
#define BOARD_OLED_SCL_PIN  8U /* PB8 / D15 / I2C1 AF4 */
#define BOARD_OLED_SDA_PIN  9U /* PB9 / D14 / I2C1 AF4 */

/* ST7789 wiring on NUCLEO-F446RE. */
#define BOARD_TFT_SCK_PIN    5U /* PA5 / Arduino D13 / SPI1_SCK */
#define BOARD_TFT_MOSI_PIN   7U /* PA7 / Arduino D11 / SPI1_MOSI */
#define BOARD_TFT_CS_PORT    GPIOB
#define BOARD_TFT_CS_PIN     6U /* PB6 / Arduino D10 */
#define BOARD_TFT_DC_PORT    GPIOC
#define BOARD_TFT_DC_PIN     7U /* PC7 / Arduino D9 */
#define BOARD_TFT_RESET_PORT GPIOA
#define BOARD_TFT_RESET_PIN  9U /* PA9 / Arduino D8 */

/* Planned peripheral wiring is recorded in docs/wiring.md. */

#endif /* CONSOLE_BOARD_PINS_H */
