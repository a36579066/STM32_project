#ifndef CONSOLE_DISPLAY_SERVICE_H
#define CONSOLE_DISPLAY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_SERVICE_WIDTH  320U
#define DISPLAY_SERVICE_HEIGHT 240U

typedef struct {
    uint32_t elapsed_ms;
    uint32_t average_upload_tenths_ms;
    uint32_t upload_rate_tenths_fps;
} display_benchmark_result_t;

typedef struct {
    const char *operation;
    const char *reason;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} display_service_error_t;

typedef void (*display_service_delay_fn)(uint32_t milliseconds);
typedef uint32_t (*display_service_now_ms_fn)(void);

/* Bind the ST7789 driver to board pins and initialize its controller. */
bool display_service_init(display_service_delay_fn delay_ms, display_service_now_ms_fn now_ms);

/* Measure uploads without owning the UART or choosing how to report results. */
bool display_service_run_startup_benchmark(display_benchmark_result_t *result);

/* Last failed display call, or NULL since init. Successful calls preserve it. */
const display_service_error_t *display_service_last_error(void);

/* App-level drawing operations; coordinates use pixels and sizes use counts. */
bool display_service_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                               uint16_t color);
bool display_service_draw_rgb565(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                 const uint8_t *pixels_be);

#endif /* CONSOLE_DISPLAY_SERVICE_H */
