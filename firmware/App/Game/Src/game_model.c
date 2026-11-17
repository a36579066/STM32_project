#include "game_model.h"

#include <stddef.h>
#include <string.h>

_Static_assert(GAME_GROUND_Y > GAME_HERO_HEIGHT + GAME_JUMP_HEIGHT + GAME_HUD_HEIGHT,
               "Jump must stay below the HUD");

static uint32_t random_next(game_model_t *g)
{
    uint32_t x = g->random_state;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    g->random_state = x;
    return x;
}

static void add_score(game_model_t *g, uint32_t points)
{
    g->score = (points > GAME_MAX_SCORE - g->score) ? GAME_MAX_SCORE : g->score + points;
}

static void toast(game_model_t *g, game_toast_t message)
{
    g->toast = message;
    g->toast_ms = 800U;
}

static void add_object(game_model_t *g, game_object_kind_t kind, int16_t x, int16_t y, uint8_t w,
                       uint8_t h)
{
    for (uint32_t i = 0U; i < GAME_OBJECT_CAPACITY; ++i) {
        if (!g->objects[i].active) {
            g->objects[i] = (game_object_t){
                .x_fp = (int32_t)x * GAME_FP_ONE,
                .y = y,
                .width = w,
                .height = h,
                .kind = kind,
                .active = true,
            };
            return;
        }
    }
}

static void spawn_wave(game_model_t *g)
{
    /* Introduce one hazard at a time, then mix the three learned patterns. */
    const uint32_t pattern = (g->waves < 3U) ? g->waves : random_next(g) % 3U;
    ++g->waves;
    if (pattern == 2U) {
        add_object(g, GAME_DRONE, 340, 148, 28U, 16U);
        for (int16_t i = 0; i < 3; ++i) {
            add_object(g, GAME_COIN, (int16_t)(324 + i * 30), 178, 10U, 12U);
        }
    } else {
        const bool crate = pattern == 1U;
        const uint8_t height = crate ? 30U : 20U;
        add_object(g, crate ? GAME_CRATE : GAME_SPIKES, 340, (int16_t)(GAME_GROUND_Y - height),
                   crate ? 24U : 26U, height);
        add_object(g, GAME_COIN, 324, 142, 10U, 12U);
        add_object(g, GAME_COIN, 354, 120, 10U, 12U);
        add_object(g, GAME_COIN, 384, 142, 10U, 12U);
    }
    g->spawn_ms = 1500U - (uint32_t)(g->level - 1U) * 55U + random_next(g) % 260U;
}

static bool overlaps(int32_t ax, int32_t ay, int32_t aw, int32_t ah, int32_t bx, int32_t by,
                     int32_t bw, int32_t bh)
{
    return (ax < bx + bw) && (ax + aw > bx) && (ay < by + bh) && (ay + ah > by);
}

static void collect_coin(game_model_t *g)
{
    if ((g->combo == 0U) || ((uint32_t)(g->run_ms - g->last_coin_ms) > GAME_COMBO_WINDOW_MS)) {
        g->combo = 1U;
    } else if (g->combo < GAME_MAX_COMBO) {
        ++g->combo;
    }
    g->last_coin_ms = g->run_ms;
    ++g->coins;
    add_score(g, 10U * g->combo);
    if ((g->coins % GAME_COINS_PER_SHIELD == 0U) && !g->shield) {
        g->shield = true;
        toast(g, GAME_TOAST_SHIELD);
    } else if (g->combo >= 3U) {
        toast(g, GAME_TOAST_COMBO);
    }
}

static void take_hit(game_model_t *g)
{
    g->combo = 0U;
    g->invulnerable_ms = GAME_HIT_GRACE_MS;
    if (g->shield) {
        g->shield = false;
        toast(g, GAME_TOAST_BLOCKED);
        return;
    }
    if (g->lives > 0U)
        --g->lives;
    toast(g, GAME_TOAST_HIT);
    if (g->lives == 0U) {
        g->mode = GAME_OVER;
        if (g->score > g->best_score)
            g->best_score = g->score;
    }
}

static void start_round(game_model_t *g, uint32_t now_ms)
{
    const uint32_t best = g->best_score;
    const uint32_t seed = random_next(g);
    const uint32_t sequence = g->previous_press_sequence;
    const bool held = g->previous_pressed;
    memset(g, 0, sizeof(*g));
    g->best_score = best;
    g->random_state = seed;
    g->previous_press_sequence = sequence;
    g->previous_pressed = held;
    g->mode = GAME_RUNNING;
    g->hero_x_fp = 64 * GAME_FP_ONE;
    g->hero_y = GAME_GROUND_Y - GAME_HERO_HEIGHT;
    g->lives = GAME_START_LIVES;
    g->level = 1U;
    g->speed = 90U;
    g->spawn_ms = 750U;
    g->last_update_ms = now_ms;
}

void game_model_init(game_model_t *g, uint32_t now_ms)
{
    memset(g, 0, sizeof(*g));
    g->random_state = now_ms ^ 0x91E10DA5U;
    if (g->random_state == 0U)
        g->random_state = 1U;
    g->mode = GAME_READY;
    g->hero_x_fp = 48 * GAME_FP_ONE;
    g->hero_y = GAME_GROUND_Y - GAME_HERO_HEIGHT;
    g->lives = GAME_START_LIVES;
    g->level = 1U;
    g->speed = 90U;
    g->last_update_ms = now_ms;
}

bool game_model_process(game_model_t *g, uint32_t now_ms, const joystick_input_t *input)
{
    if ((g == NULL) || (input == NULL))
        return false;
    const bool retained = input->press_sequence != g->previous_press_sequence;
    const uint32_t request_ms = retained ? input->pressed_at_ms : now_ms;
    const bool press = (retained || (input->pressed && !g->previous_pressed)) &&
                       ((uint32_t)(now_ms - request_ms) <= GAME_JUMP_BUFFER_MS);
    g->previous_press_sequence = input->press_sequence;
    g->previous_pressed = input->pressed;
    if (g->mode != GAME_RUNNING) {
        if (press) {
            start_round(g, now_ms);
            return true; /* Start/retry consumes this press; it does not auto-jump. */
        }
        return false;
    }
    if (press) {
        g->jump_pending = true;
        g->jump_requested_ms = request_ms;
    }
    uint32_t dt = (uint32_t)(now_ms - g->last_update_ms);
    if (dt < GAME_FRAME_INTERVAL_MS)
        return false;
    g->last_update_ms = now_ms;
    /* Bound catch-up motion after an unusually slow transfer; no teleporting
       through narrow hazards/coins. Input age still uses real milliseconds. */
    if (dt > 60U)
        dt = 60U;
    g->run_ms += dt;
    g->invulnerable_ms = (g->invulnerable_ms > dt) ? g->invulnerable_ms - dt : 0U;
    g->toast_ms = (g->toast_ms > dt) ? g->toast_ms - dt : 0U;
    if (g->toast_ms == 0U)
        g->toast = GAME_TOAST_NONE;
    if ((g->combo > 0U) && ((uint32_t)(g->run_ms - g->last_coin_ms) > GAME_COMBO_WINDOW_MS)) {
        g->combo = 0U;
    }
    uint32_t level = 1U + g->run_ms / GAME_LEVEL_DURATION_MS;
    if (level > GAME_MAX_LEVEL)
        level = GAME_MAX_LEVEL;
    if (level != g->level) {
        g->level = (uint8_t)level;
        toast(g, GAME_TOAST_LEVEL);
    }
    g->speed = (uint16_t)(90U + (g->level - 1U) * 10U);

    g->hero_x_fp += (int32_t)((int64_t)input->x * GAME_HERO_SPEED * dt * GAME_FP_ONE / 1000000);
    if (g->hero_x_fp < (int32_t)(GAME_HERO_MIN_X * GAME_FP_ONE))
        g->hero_x_fp = GAME_HERO_MIN_X * GAME_FP_ONE;
    if (g->hero_x_fp > (int32_t)(GAME_HERO_MAX_X * GAME_FP_ONE))
        g->hero_x_fp = GAME_HERO_MAX_X * GAME_FP_ONE;

    uint32_t lift = 0U;
    if (g->jumping) {
        const uint32_t t = (uint32_t)(g->run_ms - g->jump_started_ms);
        if (t >= GAME_JUMP_DURATION_MS) {
            g->jumping = false;
        } else {
            lift = 4U * GAME_JUMP_HEIGHT * t * (GAME_JUMP_DURATION_MS - t) /
                   (GAME_JUMP_DURATION_MS * GAME_JUMP_DURATION_MS);
        }
    }
    if (g->jump_pending && ((uint32_t)(now_ms - g->jump_requested_ms) > GAME_JUMP_BUFFER_MS)) {
        g->jump_pending = false;
    }
    if (g->jump_pending && !g->jumping) {
        g->jump_pending = false;
        g->jumping = true;
        g->jump_started_ms = g->run_ms;
        lift = 0U;
    }
    g->hero_y = (int16_t)(GAME_GROUND_Y - GAME_HERO_HEIGHT - lift);

    const uint32_t old_distance_points = g->distance_fp / (16U * GAME_FP_ONE);
    const uint32_t scroll = g->speed * dt * GAME_FP_ONE / 1000U;
    g->distance_fp += scroll;
    add_score(g, g->distance_fp / (16U * GAME_FP_ONE) - old_distance_points);
    const int32_t hero_x = g->hero_x_fp / GAME_FP_ONE;
    for (uint32_t i = 0U; i < GAME_OBJECT_CAPACITY; ++i) {
        game_object_t *o = &g->objects[i];
        if (!o->active)
            continue;
        o->x_fp -= (int32_t)scroll;
        const int32_t x = o->x_fp / GAME_FP_ONE;
        if (x + o->width <= 0) {
            o->active = false;
            continue;
        }
        if (o->kind == GAME_COIN) {
            if (overlaps(hero_x + 3, g->hero_y + 2, GAME_HERO_WIDTH - 6, GAME_HERO_HEIGHT - 4, x,
                         o->y, o->width, o->height)) {
                o->active = false;
                collect_coin(g);
            }
        } else {
            if ((g->invulnerable_ms == 0U) &&
                overlaps(hero_x + 5, g->hero_y + 4, GAME_HERO_WIDTH - 10, GAME_HERO_HEIGHT - 8,
                         x + 3, o->y + 2, o->width - 6, o->height - 2)) {
                o->passed = true;
                take_hit(g);
                if (g->mode == GAME_OVER)
                    break;
            }
            if (!o->passed && (x + o->width < hero_x + 5)) {
                o->passed = true;
                add_score(g, 5U);
            }
        }
    }
    if (g->mode == GAME_RUNNING) {
        if (g->spawn_ms > dt)
            g->spawn_ms -= dt;
        else
            spawn_wave(g);
    }
    return true;
}
