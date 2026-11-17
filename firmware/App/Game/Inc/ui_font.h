#ifndef ARCADE_UI_FONT_H
#define ARCADE_UI_FONT_H
#include <stdbool.h>
#include <stdint.h>
uint8_t ui_glyph_row(char c, unsigned row);
bool ui_text_pixel(int x, int y, int left, int top, unsigned scale, const char *text);
void ui_number(char *out, uint32_t value, unsigned digits);
#endif
