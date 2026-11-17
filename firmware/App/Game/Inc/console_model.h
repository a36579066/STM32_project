#ifndef ARCADE_CONSOLE_MODEL_H
#define ARCADE_CONSOLE_MODEL_H
#include "game_model.h"
#include "mini_games.h"

typedef enum {
    CONSOLE_MENU,
    CONSOLE_PLAYING,
    CONSOLE_PAUSED
} console_mode_t;

typedef enum {
    ARCADE_COIN_DASH,
    ARCADE_SNAKE,
    ARCADE_BREAKOUT,
    ARCADE_GAME_COUNT
} arcade_game_t;

typedef struct {
    console_mode_t mode;
    arcade_game_t selected;
    uint8_t pause_item;
    uint32_t score, best;
    uint16_t detail;
    uint8_t lives, level;
    bool over, ready;
} console_status_t;

typedef struct {
    console_mode_t mode;
    arcade_game_t selected;
    uint8_t pause_item;
    uint32_t best[ARCADE_GAME_COUNT];
    uint32_t a_sequence, b_sequence, menu_sequence, nav_ms, frame_ms;
    int8_t nav_direction;
    bool a_held, b_held, menu_held;
    game_model_t runner;
    snake_model_t snake;
    breakout_model_t breakout;
} console_model_t;

const char *console_game_name(arcade_game_t game);
void console_model_init(console_model_t *c, uint32_t now);
bool console_model_process(console_model_t *c, uint32_t now, const joystick_input_t *input);
void console_model_status(const console_model_t *c, console_status_t *status);
#endif
