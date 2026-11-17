#ifndef CONSOLE_BOARD_H
#define CONSOLE_BOARD_H

#include <stdbool.h>

/* Call after the core and system clock initialization in main.c. */
void board_init(void);
/* Bring-up LED only. PA5 must be released before SPI1 owns it as TFT SCK. */
void board_led_set(bool on);

/*====================================================================================
 TFT pin control functions. The ST7789 is a 3-wire SPI device with a separate CS pin 
 and a D/C pin to select command or data bytes. The TFT reset pin is also controlled 
 by the Board layer.
======================================================================================*/
/* Configure the Nucleo pins used by the ST7789 TFT. */
void board_tft_pins_init(void);
/* `active` selects the active-low TFT CS pin. */
void board_tft_cs_set(bool active);
/* true selects pixel/parameter data; false selects a command byte. */
void board_tft_dc_set(bool data_mode);
/* Set the physical level on the active-low TFT reset pin. */
void board_tft_reset_set(bool high);

void board_button_init(void);

bool board_button_is_pressed(void);

/* KY-023 SW: PB0 with pull-up; pressing connects the input to GND. */
void board_joystick_init(void);
bool board_joystick_button_is_pressed(void);
bool board_game_b_is_pressed(void);
bool board_game_menu_is_pressed(void);
void board_oled_pins_init(void);

#endif /* CONSOLE_BOARD_H */
