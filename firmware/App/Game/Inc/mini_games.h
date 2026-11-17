#ifndef CONSOLE_MINI_GAMES_H
#define CONSOLE_MINI_GAMES_H
#include <stdbool.h>
#include <stdint.h>
#include "joystick_service.h"
#define SNAKE_COLS  20U
#define SNAKE_ROWS  13U
#define SNAKE_CELLS (SNAKE_COLS * SNAKE_ROWS)

typedef enum {
    MINI_SERVE,
    MINI_RUNNING,
    MINI_OVER,
    MINI_WON
} mini_phase_t;

typedef struct {
    uint16_t body[SNAKE_CELLS], length, food;
    uint8_t direction, next_direction, level;
    uint32_t score, best, random, last_ms, accumulated_ms;
    mini_phase_t phase;
} snake_model_t;

typedef struct {
    int32_t paddle_fp, ball_x_fp, ball_y_fp, vx, vy;
    uint8_t bricks[40], lives, level;
    uint32_t score, best, last_ms;
    mini_phase_t phase;
} breakout_model_t;

void snake_init(snake_model_t *s, uint32_t now, uint32_t best);
bool snake_process(snake_model_t *s, uint32_t now, const joystick_input_t *in, bool action);
void breakout_init(breakout_model_t *b, uint32_t now, uint32_t best);
bool breakout_process(breakout_model_t *b, uint32_t now, const joystick_input_t *in, bool action);
#endif
