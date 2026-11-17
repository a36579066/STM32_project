#ifndef ARCADE_CONSOLE_SCENE_H
#define ARCADE_CONSOLE_SCENE_H
#include "console_model.h"
bool console_scene_render(const console_model_t *c, const console_model_t *previous);
/* SSD1306/SH1106 page layout: byte = x + (y/8)*128, bit = y%8. */
void console_oled_render(const console_status_t *s, uint8_t pixels[1024]);
#endif
