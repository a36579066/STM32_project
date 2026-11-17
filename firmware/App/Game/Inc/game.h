#ifndef CONSOLE_GAME_H
#define CONSOLE_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "joystick_service.h"

bool game_init(uint32_t now_ms);
bool game_process(uint32_t now_ms, const joystick_input_t *input);

#endif /* CONSOLE_GAME_H */
