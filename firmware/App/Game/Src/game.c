#include "game.h"

#include <stddef.h>
#include "game_model.h"
#include "game_scene.h"

static game_model_t world;
static game_model_t drawn;
static bool ready;

bool game_init(uint32_t now_ms)
{
    game_model_init(&world, now_ms);
    ready = game_scene_render(&world, NULL);
    if (ready)
        drawn = world;
    return ready;
}

bool game_process(uint32_t now_ms, const joystick_input_t *input)
{
    if (!ready || (input == NULL))
        return true;
    if (!game_model_process(&world, now_ms, input))
        return true;
    if (!game_scene_render(&world, &drawn)) {
        ready = false;
        return false;
    }
    drawn = world;
    return true;
}
