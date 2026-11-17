#ifndef ARCADE_OLED_CONFIG_H
#define ARCADE_OLED_CONFIG_H
#include "oled.h"
/* Unknown 0.96-inch 128x64 module: start with SSD1306.
   If ACK succeeds but the image is shifted/incorrect, try OLED_SH1106. */
#define APP_OLED_CONTROLLER OLED_SSD1306
#define APP_OLED_BUS_HZ     100000U
#endif
