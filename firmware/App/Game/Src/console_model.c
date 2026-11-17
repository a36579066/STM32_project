#include "console_model.h"
#include <string.h>

const char *console_game_name(arcade_game_t game)
{
    static const char *const names[] = {"COIN DASH", "SNAKE", "BREAKOUT"};
    return game < ARCADE_GAME_COUNT ? names[game] : "ARCADE";
}

void console_model_init(console_model_t *c, uint32_t now)
{
    memset(c, 0, sizeof(*c));
    c->mode = CONSOLE_MENU;
    c->frame_ms = now;
}

static bool edge(bool held, uint32_t seq, uint32_t at, bool *oldheld, uint32_t *oldseq,
                 uint32_t now)
{
    bool changed = seq != *oldseq;
    bool hit = (changed && (uint32_t)(now - at) <= 150U) || (!changed && held && !*oldheld);
    *oldheld = held;
    *oldseq = seq;
    return hit;
}

static void synchronize(console_model_t *c, uint32_t now, const joystick_input_t *in)
{
    c->frame_ms = now;
    c->runner.last_update_ms = now;
    c->runner.previous_press_sequence = in->press_sequence;
    c->runner.previous_pressed = in->pressed;
    c->runner.jump_pending = false;
    c->snake.last_ms = now;
    c->breakout.last_ms = now;
}

static void start(console_model_t *c, uint32_t now, const joystick_input_t *in)
{
    if (c->selected == ARCADE_COIN_DASH) {
        game_model_init(&c->runner, now);
        c->runner.best_score = c->best[ARCADE_COIN_DASH];
        joystick_input_t confirm = *in;
        confirm.pressed = true;
        confirm.pressed_at_ms = now;
        confirm.press_sequence = 1;
        (void)game_model_process(&c->runner, now, &confirm);
    } else if (c->selected == ARCADE_SNAKE)
        snake_init(&c->snake, now, c->best[ARCADE_SNAKE]);
    else
        breakout_init(&c->breakout, now, c->best[ARCADE_BREAKOUT]);
    c->mode = CONSOLE_PLAYING;
    synchronize(c, now, in);
}

static int navigate(console_model_t *c, uint32_t now, int y)
{
    int direction = y > 600 ? 1 : (y < -600 ? -1 : 0);
    if (y > -300 && y < 300)
        c->nav_direction = 0;
    if (!direction)
        return 0;
    if (direction != c->nav_direction) {
        c->nav_direction = (int8_t)direction;
        c->nav_ms = now + 350U;
        return direction;
    }
    if ((int32_t)(now - c->nav_ms) >= 0) {
        c->nav_ms = now + 140U;
        return direction;
    }
    return 0;
}

bool console_model_process(console_model_t *c, uint32_t now, const joystick_input_t *in)
{
    const bool a =
        edge(in->pressed, in->press_sequence, in->pressed_at_ms, &c->a_held, &c->a_sequence, now);
    const bool b = edge(in->b_pressed, in->b_press_sequence, in->b_pressed_at_ms, &c->b_held,
                        &c->b_sequence, now);
    const bool menu = edge(in->menu_pressed, in->menu_press_sequence, in->menu_pressed_at_ms,
                           &c->menu_held, &c->menu_sequence, now);
    if (menu && c->mode == CONSOLE_PLAYING) {
        c->mode = CONSOLE_PAUSED;
        c->pause_item = 0;
        c->nav_direction = 0;
        return true;
    }
    if (c->mode == CONSOLE_PAUSED && (b || menu)) {
        c->mode = CONSOLE_PLAYING;
        synchronize(c, now, in);
        return true;
    }
    if (c->mode != CONSOLE_PLAYING) {
        int nav = navigate(c, now, in->y);
        if (nav) {
            if (c->mode == CONSOLE_MENU)
                c->selected =
                    (arcade_game_t)((c->selected + ARCADE_GAME_COUNT + nav) % ARCADE_GAME_COUNT);
            else
                c->pause_item = (uint8_t)((c->pause_item + 3 + nav) % 3);
        }
        if (a) {
            if (c->mode == CONSOLE_MENU || c->pause_item == 1)
                start(c, now, in);
            else if (c->pause_item == 0) {
                c->mode = CONSOLE_PLAYING;
                synchronize(c, now, in);
            } else {
                c->mode = CONSOLE_MENU;
                c->nav_direction = 0;
            }
            return true;
        }
        return nav != 0;
    }
    bool dirty;
    if (c->selected == ARCADE_COIN_DASH) {
        dirty = game_model_process(&c->runner, now, in);
        c->best[0] = c->runner.best_score;
    } else {
        /* Retain an A edge until a 20 ms frame; mini games have no own input buffer. */
        if (a || (uint32_t)(now - c->frame_ms) >= 20U) {
            if (c->selected == ARCADE_SNAKE) {
                dirty = snake_process(&c->snake, now, in, a);
                c->best[1] = c->snake.best;
            } else {
                dirty = breakout_process(&c->breakout, now, in, a);
                c->best[2] = c->breakout.best;
            }
            c->frame_ms = now;
        } else
            dirty = false;
    }
    console_status_t status;
    console_model_status(c, &status);
    if (b && status.over) {
        c->mode = CONSOLE_MENU;
        c->nav_direction = 0;
        return true;
    }
    return dirty;
}

void console_model_status(const console_model_t *c, console_status_t *s)
{
    memset(s, 0, sizeof(*s));
    s->ready = true;
    s->mode = c->mode;
    s->selected = c->selected;
    s->pause_item = c->pause_item;
    s->best = c->best[c->selected];
    if (c->selected == ARCADE_COIN_DASH) {
        s->score = c->runner.score;
        s->lives = c->runner.lives;
        s->level = c->runner.level;
        s->detail = (uint16_t)c->runner.coins;
        s->over = c->runner.mode == GAME_OVER;
    } else if (c->selected == ARCADE_SNAKE) {
        s->score = c->snake.score;
        s->lives = c->snake.phase == MINI_RUNNING;
        s->level = c->snake.level;
        s->detail = c->snake.length;
        s->over = c->snake.phase >= MINI_OVER;
    } else {
        s->score = c->breakout.score;
        s->lives = c->breakout.lives;
        s->level = c->breakout.level;
        for (unsigned i = 0; i < 40; ++i)
            s->detail += c->breakout.bricks[i] != 0;
        s->over = c->breakout.phase == MINI_OVER;
    }
}
