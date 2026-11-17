#ifndef CONSOLE_GAME_MODEL_H
#define CONSOLE_GAME_MODEL_H

#include <stdbool.h>
#include <stdint.h>
#include "game_config.h"
#include "joystick_service.h"

typedef enum {
    GAME_READY,
    GAME_RUNNING,
    GAME_OVER
} game_mode_t;

typedef enum {
    GAME_COIN,
    GAME_CRATE,
    GAME_SPIKES,
    GAME_DRONE
} game_object_kind_t;

typedef enum {
    GAME_TOAST_NONE,
    GAME_TOAST_COMBO,
    GAME_TOAST_SHIELD,
    GAME_TOAST_BLOCKED,
    GAME_TOAST_HIT,
    GAME_TOAST_LEVEL
} game_toast_t;

typedef struct {
    int32_t x_fp;
    int16_t y;
    uint8_t width;
    uint8_t height;
    game_object_kind_t kind;
    bool active;
    bool passed;
} game_object_t;

typedef struct {
    game_mode_t mode;
    int32_t hero_x_fp;
    int16_t hero_y;
    uint32_t score;
    uint32_t best_score; /* RAM only; retained across rounds, not power cycles. */
    uint32_t coins;
    uint32_t distance_fp;
    uint32_t run_ms;
    uint32_t last_update_ms;
    uint32_t jump_started_ms;
    uint32_t jump_requested_ms;
    uint32_t last_coin_ms;
    uint32_t invulnerable_ms;
    uint32_t toast_ms;
    uint32_t spawn_ms;
    uint32_t waves;
    uint32_t random_state;
    uint32_t previous_press_sequence;
    uint16_t speed;
    uint8_t lives;
    uint8_t level;
    uint8_t combo;
    bool shield;
    bool jumping;
    bool jump_pending;
    bool previous_pressed;
    game_toast_t toast;
    game_object_t objects[GAME_OBJECT_CAPACITY];
} game_model_t;

void game_model_init(game_model_t *model, uint32_t now_ms);
/* Captures input on every call; simulates at most once per display interval.
   Returns true when the renderer needs an updated frame. No hardware access. */
bool game_model_process(game_model_t *model, uint32_t now_ms, const joystick_input_t *input);

#endif
