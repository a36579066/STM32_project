#ifndef ARCADE_OLED_H
#define ARCADE_OLED_H
#include <stdbool.h>
#include <stdint.h>
#include "i2c.h"

typedef enum {
    OLED_SSD1306,
    OLED_SH1106
} oled_controller_t;

/* No ACK-based controller identification is possible. Choose in oled_config.h. */
i2c_status_t oled_init(oled_controller_t controller);
i2c_status_t oled_present(const uint8_t pixels[1024]);
uint8_t oled_address(void);
#endif
