#include "oled.h"
#include <stddef.h>
#include <string.h>
static uint8_t address, column_offset;
static bool valid[8];
static uint8_t previous[1024];

static i2c_status_t commands(const uint8_t *data, uint16_t size)
{
    uint8_t packet[32];
    packet[0] = 0;
    if (size > sizeof(packet) - 1U)
        return I2C_STATUS_INVALID_ARGUMENT;
    memcpy(packet + 1, data, size);
    return i2c1_write(address, packet, size + 1, 40);
}

uint8_t oled_address(void)
{
    return address;
}

i2c_status_t oled_init(oled_controller_t controller)
{
    if (controller != OLED_SSD1306 && controller != OLED_SH1106)
        return I2C_STATUS_INVALID_ARGUMENT;
    address = 0;
    memset(valid, 0, sizeof(valid));
    i2c_status_t status = I2C_STATUS_NACK;
    for (uint8_t candidate = 0x3C; candidate <= 0x3D; ++candidate) {
        status = i2c1_write(candidate, NULL, 0, 30);
        if (status == I2C_STATUS_OK) {
            address = candidate;
            break;
        }
    }
    if (!address)
        return status;
    column_offset = controller == OLED_SH1106 ? 2 : 0;
    const uint8_t setup[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40};
    status = commands(setup, sizeof(setup));
    if (status != I2C_STATUS_OK)
        return status;
    const uint8_t ssd[] = {0x8D, 0x14, 0x20, 0x02};
    const uint8_t sh[] = {0xAD, 0x8B};
    status = controller == OLED_SH1106 ? commands(sh, sizeof(sh)) : commands(ssd, sizeof(ssd));
    if (status != I2C_STATUS_OK)
        return status;
    const uint8_t common[] = {0xA1, 0xC8, 0xDA, 0x12, 0x81, 0x8F,
                              0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6};
    status = commands(common, sizeof(common));
    if (status != I2C_STATUS_OK)
        return status;
    static const uint8_t blank[1024] = {0};
    status = oled_present(blank);
    if (status != I2C_STATUS_OK)
        return status;
    const uint8_t on = 0xAF;
    return commands(&on, 1);
}

i2c_status_t oled_present(const uint8_t pixels[1024])
{
    if (!address || !pixels)
        return I2C_STATUS_INVALID_ARGUMENT;
    for (unsigned page = 0; page < 8; ++page) {
        const uint8_t *src = pixels + page * 128;
        if (valid[page] && memcmp(src, previous + page * 128, 128) == 0)
            continue;
        const uint8_t position[] = {(uint8_t)(0xB0 | page), column_offset, 0x10};
        i2c_status_t status = commands(position, sizeof(position));
        if (status == I2C_STATUS_OK) {
            uint8_t packet[129];
            packet[0] = 0x40;
            memcpy(packet + 1, src, 128);
            status = i2c1_write(address, packet, sizeof(packet), 40);
        }
        if (status != I2C_STATUS_OK) {
            memset(valid, 0, sizeof(valid));
            return status;
        }
        memcpy(previous + page * 128, src, 128);
        valid[page] = true;
    }
    return I2C_STATUS_OK;
}
