#ifndef CONSOLE_ST7789_H
#define CONSOLE_ST7789_H

#include <stdbool.h>
#include <stdint.h>

#include "spi.h"

#define ST7789_WIDTH  320U
#define ST7789_HEIGHT 240U

typedef struct {
    void (*cs_set)(bool active);
    void (*dc_set)(bool data_mode);
    void (*reset_set)(bool high);
    void (*delay_ms)(uint32_t ms);
} st7789_bus_control_t;

/* Send one ST7789 command and its optional parameter bytes as one CS transaction. */
spi_status_t st7789_write_command_data(const st7789_bus_control_t *control, uint8_t command,
                                       const uint8_t *data, uint16_t length);

/* Reset and initialize the controller for a 320x240 landscape RGB565 panel. */
spi_status_t st7789_init(const st7789_bus_control_t *control);

/* Fill a bounded rectangle with one RGB565 color; x/y are the start, width/height are counts. */
spi_status_t st7789_fill_rect(const st7789_bus_control_t *control, uint16_t x, uint16_t y,
                              uint16_t width, uint16_t height, uint16_t color);

/* Draw row-major RGB565 pixels; each pixel is stored high byte first. */
spi_status_t st7789_draw_rgb565(const st7789_bus_control_t *control, uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height, const uint8_t *pixels_be);

#endif /* CONSOLE_ST7789_H */
