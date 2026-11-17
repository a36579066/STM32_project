#ifndef CONSOLE_GAME_SPRITE_H
#define CONSOLE_GAME_SPRITE_H

#include <stdbool.h>
#include <stdint.h>
#include "game_config.h"

uint16_t game_sprite_pixel(uint16_t x, uint16_t y, uint8_t pose, bool face_left,
                           uint16_t background);

#endif /* CONSOLE_GAME_SPRITE_H */
