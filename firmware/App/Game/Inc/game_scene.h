#ifndef CONSOLE_GAME_SCENE_H
#define CONSOLE_GAME_SCENE_H

#include <stdbool.h>
#include "game_model.h"

/* Recompose clipped dirty rectangles with a small RGB565 staging buffer. */
bool game_scene_render(const game_model_t *world, const game_model_t *previous);

#endif /* CONSOLE_GAME_SCENE_H */
