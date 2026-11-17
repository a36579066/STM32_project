#include "game_sprite.h"

#include "game_config.h"

#define HERO_SOURCE_WIDTH  12U
#define HERO_SOURCE_HEIGHT 16U
#define HERO_SCALE         2U

/* Original 12x16 helper-bot pixel art, enlarged 2x when rendered. */
static const char hero_pose[2][HERO_SOURCE_HEIGHT][HERO_SOURCE_WIDTH + 1U] = {
    {
        ".....Y......",
        ".....D......",
        "...DDDDD....",
        "..DCCCCCD...",
        ".DCCCCCCCD..",
        ".DCCLLCCCD..",
        "..DCCVVCCD..",
        "..DCCCCCD...",
        "...DDDDD....",
        "..DYYYYD....",
        ".DOOOOOD....",
        ".DOOGGOOD...",
        ".DOOOOOOD...",
        "..D....D....",
        ".DB....BD...",
        ".DB....BD...",
    },
    {
        ".....Y......",
        ".....D......",
        "...DDDDD....",
        "..DCCCCCD...",
        ".DCCCCCCCD..",
        ".DCCLLCCCD..",
        "..DCCVVCCD..",
        "..DCCCCCD...",
        "...DDDDD....",
        "..DYYYYD....",
        ".DOOOOOD....",
        ".DOOGGOOD...",
        ".DOOOOOOD...",
        "..D....D....",
        "..DB..BD....",
        ".DB....BD...",
    },
};

static uint16_t palette_color(char pixel, uint16_t background)
{
    switch (pixel) {
    case 'D':
        return 0x18E3U;
    case 'C':
        return 0x07FFU;
    case 'V':
        return 0x001FU;
    case 'L':
        return 0xAFE5U;
    case 'Y':
        return 0xFFE0U;
    case 'O':
        return 0xFD20U;
    case 'G':
        return 0x07E0U;
    case 'B':
        return 0x4208U;
    default:
        return background;
    }
}

uint16_t game_sprite_pixel(uint16_t x, uint16_t y, uint8_t pose, bool face_left,
                           uint16_t background)
{
    if (x >= GAME_HERO_WIDTH || y >= GAME_HERO_HEIGHT)
        return background;
    uint16_t source_x = x / HERO_SCALE;
    if (face_left)
        source_x = HERO_SOURCE_WIDTH - 1U - source_x;
    return palette_color(hero_pose[pose & 1U][y / HERO_SCALE][source_x], background);
}
