#include "st7789.h"

#include <stddef.h>

#include "spi.h"

#define ST7789_CMD_SWRESET 0x01U
#define ST7789_CMD_SLPOUT  0x11U
#define ST7789_CMD_NORON   0x13U
#define ST7789_CMD_INVON   0x21U
#define ST7789_CMD_DISPON  0x29U
#define ST7789_CMD_CASET   0x2AU
#define ST7789_CMD_RASET   0x2BU
#define ST7789_CMD_RAMWR   0x2CU
#define ST7789_CMD_RAMWRC  0x3CU
#define ST7789_CMD_MADCTL  0x36U
#define ST7789_CMD_COLMOD  0x3AU

#define ST7789_COLOR_CHUNK_BYTES 4096U

static bool st7789_control_is_valid(const st7789_bus_control_t *control)
{
    return (control != NULL) && (control->cs_set != NULL) && (control->dc_set != NULL);
}

static spi_status_t st7789_set_address_window(const st7789_bus_control_t *control, uint16_t x,
                                              uint16_t y, uint16_t width, uint16_t height)
{
    const uint16_t x_end = (uint16_t)(x + width - 1U);
    const uint16_t y_end = (uint16_t)(y + height - 1U);
    const uint8_t columns[4] = {(uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(x_end >> 8),
                                (uint8_t)x_end};
    const uint8_t rows[4] = {(uint8_t)(y >> 8), (uint8_t)y, (uint8_t)(y_end >> 8), (uint8_t)y_end};

    spi_status_t status =
        st7789_write_command_data(control, ST7789_CMD_CASET, columns, sizeof(columns));
    if (status != SPI_STATUS_OK) {
        return status;
    }

    return st7789_write_command_data(control, ST7789_CMD_RASET, rows, sizeof(rows));
}

static spi_status_t st7789_write_pixels(const st7789_bus_control_t *control, const uint8_t *pixels,
                                        uint32_t byte_count, bool repeat_buffer)
{
    uint32_t remaining = byte_count;
    bool first_chunk = true;

    while (remaining > 0U) {
        const uint16_t chunk_length =
            (remaining > ST7789_COLOR_CHUNK_BYTES) ? ST7789_COLOR_CHUNK_BYTES : (uint16_t)remaining;
        const uint8_t command = first_chunk ? ST7789_CMD_RAMWR : ST7789_CMD_RAMWRC;
        spi_status_t status = st7789_write_command_data(control, command, pixels, chunk_length);
        if (status != SPI_STATUS_OK) {
            return status;
        }

        if (!repeat_buffer) {
            pixels += chunk_length;
        }
        remaining -= chunk_length;
        first_chunk = false;
    }

    return SPI_STATUS_OK;
}

spi_status_t st7789_write_command_data(const st7789_bus_control_t *control, uint8_t command,
                                       const uint8_t *data, uint16_t length)
{
    if (!st7789_control_is_valid(control) || ((length > 0U) && (data == NULL))) {
        return SPI_STATUS_INVALID_ARGUMENT;
    }

    control->cs_set(true);
    control->dc_set(false);
    spi_status_t status = spi1_transfer(command, NULL);

    if ((status == SPI_STATUS_OK) && (length > 0U)) {
        control->dc_set(true);
        status = spi1_write_dma(data, length);
    }

    /* Release CS even if SPI or DMA reports an error. */
    control->cs_set(false);
    return status;
}

spi_status_t st7789_init(const st7789_bus_control_t *control)
{
    if (!st7789_control_is_valid(control) || (control->reset_set == NULL) ||
        (control->delay_ms == NULL)) {
        return SPI_STATUS_INVALID_ARGUMENT;
    }

    control->cs_set(false);
    control->reset_set(false);
    control->delay_ms(10U);
    control->reset_set(true);
    control->delay_ms(120U);

    spi_status_t status = st7789_write_command_data(control, ST7789_CMD_SWRESET, NULL, 0U);
    if (status != SPI_STATUS_OK) {
        return status;
    }
    control->delay_ms(150U);

    status = st7789_write_command_data(control, ST7789_CMD_SLPOUT, NULL, 0U);
    if (status != SPI_STATUS_OK) {
        return status;
    }
    control->delay_ms(120U);

    const uint8_t pixel_format = 0x55U; /* 16-bit RGB565 over 4-wire SPI. */
    status = st7789_write_command_data(control, ST7789_CMD_COLMOD, &pixel_format, 1U);
    if (status != SPI_STATUS_OK) {
        return status;
    }
    control->delay_ms(10U);

    const uint8_t memory_access = 0x68U; /* MX | MV | BGR: 90-degree clockwise landscape. */
    status = st7789_write_command_data(control, ST7789_CMD_MADCTL, &memory_access, 1U);
    if (status != SPI_STATUS_OK) {
        return status;
    }

    status = st7789_write_command_data(control, ST7789_CMD_INVON, NULL, 0U);
    if (status != SPI_STATUS_OK) {
        return status;
    }
    control->delay_ms(10U);

    status = st7789_write_command_data(control, ST7789_CMD_NORON, NULL, 0U);
    if (status != SPI_STATUS_OK) {
        return status;
    }
    control->delay_ms(10U);

    status = st7789_write_command_data(control, ST7789_CMD_DISPON, NULL, 0U);
    if (status != SPI_STATUS_OK) {
        return status;
    }
    control->delay_ms(20U);

    return SPI_STATUS_OK;
}

spi_status_t st7789_fill_rect(const st7789_bus_control_t *control, uint16_t x, uint16_t y,
                              uint16_t width, uint16_t height, uint16_t color)
{
    if (!st7789_control_is_valid(control) || (width == 0U) || (height == 0U) ||
        ((uint32_t)x + width > ST7789_WIDTH) || ((uint32_t)y + height > ST7789_HEIGHT)) {
        return SPI_STATUS_INVALID_ARGUMENT;
    }

    spi_status_t status = st7789_set_address_window(control, x, y, width, height);
    if (status != SPI_STATUS_OK) {
        return status;
    }

    static uint8_t color_chunk[ST7789_COLOR_CHUNK_BYTES];
    for (uint32_t i = 0U; i < ST7789_COLOR_CHUNK_BYTES; i += 2U) {
        color_chunk[i] = (uint8_t)(color >> 8);
        color_chunk[i + 1U] = (uint8_t)color;
    }

    return st7789_write_pixels(control, color_chunk, (uint32_t)width * height * 2U, true);
}

spi_status_t st7789_draw_rgb565(const st7789_bus_control_t *control, uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height, const uint8_t *pixels_be)
{
    if (!st7789_control_is_valid(control) || (pixels_be == NULL) || (width == 0U) ||
        (height == 0U) || ((uint32_t)x + width > ST7789_WIDTH) ||
        ((uint32_t)y + height > ST7789_HEIGHT)) {
        return SPI_STATUS_INVALID_ARGUMENT;
    }

    spi_status_t status = st7789_set_address_window(control, x, y, width, height);
    if (status != SPI_STATUS_OK) {
        return status;
    }

    return st7789_write_pixels(control, pixels_be, (uint32_t)width * height * 2U, false);
}
