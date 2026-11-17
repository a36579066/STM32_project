#include "display_service.h"

#include <stddef.h>

#include "board.h"
#include "spi.h"
#include "st7789.h"

#define TFT_BENCHMARK_FRAMES 10U

_Static_assert(DISPLAY_SERVICE_WIDTH == ST7789_WIDTH, "Display and controller widths must agree");
_Static_assert(DISPLAY_SERVICE_HEIGHT == ST7789_HEIGHT,
               "Display and controller heights must agree");

static st7789_bus_control_t tft_control = {
    .cs_set = board_tft_cs_set,
    .dc_set = board_tft_dc_set,
    .reset_set = board_tft_reset_set,
    .delay_ms = NULL,
};
static display_service_now_ms_fn service_now_ms;
static display_service_error_t last_error;

static bool record_display_result(spi_status_t status, const char *operation, uint16_t x,
                                  uint16_t y, uint16_t width, uint16_t height)
{
    if (status == SPI_STATUS_OK) {
        return true;
    }

    const char *reason;
    switch (status) {
    case SPI_STATUS_TIMEOUT:
        reason = "timeout";
        break;
    case SPI_STATUS_INVALID_ARGUMENT:
        reason = "invalid argument";
        break;
    case SPI_STATUS_BUSY:
        reason = "busy";
        break;
    case SPI_STATUS_DMA_ERROR:
        reason = "DMA error";
        break;
    case SPI_STATUS_BUS_ERROR:
        reason = "SPI bus error";
        break;
    default:
        reason = "unknown error";
        break;
    }
    last_error = (display_service_error_t){
        .operation = operation,
        .reason = reason,
        .x = x,
        .y = y,
        .width = width,
        .height = height,
    };
    return false;
}

const display_service_error_t *display_service_last_error(void)
{
    return (last_error.operation == NULL) ? NULL : &last_error;
}

bool display_service_init(display_service_delay_fn delay_ms, display_service_now_ms_fn now_ms)
{
    last_error = (display_service_error_t){0};
    if ((delay_ms == NULL) || (now_ms == NULL)) {
        return record_display_result(SPI_STATUS_INVALID_ARGUMENT, "init", 0U, 0U, 0U, 0U);
    }
    tft_control.delay_ms = delay_ms;
    service_now_ms = now_ms;
    return record_display_result(st7789_init(&tft_control), "init", 0U, 0U, 0U, 0U);
}

bool display_service_run_startup_benchmark(display_benchmark_result_t *result)
{
    static const uint16_t test_colors[TFT_BENCHMARK_FRAMES] = {
        0x0000U, 0xFFFFU, 0xF800U, 0x07E0U, 0x001FU, 0xFFE0U, 0x07FFU, 0xF81FU, 0x8410U, 0xFD20U,
    };

    if ((result == NULL) || (service_now_ms == NULL)) {
        return false;
    }
    const uint32_t start_ms = service_now_ms();
    for (uint32_t i = 0U; i < TFT_BENCHMARK_FRAMES; ++i) {
        if (!display_service_fill_rect(0U, 0U, DISPLAY_SERVICE_WIDTH, DISPLAY_SERVICE_HEIGHT,
                                       test_colors[i])) {
            return false;
        }
    }

    const uint32_t elapsed_ms = (uint32_t)(service_now_ms() - start_ms);
    if (elapsed_ms == 0U) {
        return false;
    }

    result->elapsed_ms = elapsed_ms;
    result->average_upload_tenths_ms =
        (elapsed_ms * 10U + (TFT_BENCHMARK_FRAMES / 2U)) / TFT_BENCHMARK_FRAMES;
    result->upload_rate_tenths_fps =
        ((TFT_BENCHMARK_FRAMES * 10000U) + (elapsed_ms / 2U)) / elapsed_ms;
    return true;
}

bool display_service_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                               uint16_t color)
{
    return record_display_result(st7789_fill_rect(&tft_control, x, y, width, height, color),
                                 "fill_rect", x, y, width, height);
}

bool display_service_draw_rgb565(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                 const uint8_t *pixels_be)
{
    return record_display_result(st7789_draw_rgb565(&tft_control, x, y, width, height, pixels_be),
                                 "draw_rgb565", x, y, width, height);
}
