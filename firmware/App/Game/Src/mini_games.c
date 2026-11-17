#include "mini_games.h"
#include <string.h>

static uint32_t elapsed(uint32_t now, uint32_t *last)
{
    uint32_t dt = now - *last;
    *last = now;
    return dt > 60U ? 60U : dt;
}

static void food(snake_model_t *s)
{
    s->random ^= s->random << 13;
    s->random ^= s->random >> 17;
    s->random ^= s->random << 5;
    uint16_t cell = s->random % SNAKE_CELLS;
    for (uint32_t n = 0; n < SNAKE_CELLS; ++n, cell = (cell + 1U) % SNAKE_CELLS) {
        bool free = true;
        for (uint32_t i = 0; i < s->length; ++i)
            if (s->body[i] == cell) {
                free = false;
                break;
            }
        if (free) {
            s->food = cell;
            return;
        }
    }
    s->phase = MINI_WON;
}

void snake_init(snake_model_t *s, uint32_t now, uint32_t best)
{
    memset(s, 0, sizeof(*s));
    s->length = 3;
    s->body[0] = 6 * SNAKE_COLS + 8;
    s->body[1] = s->body[0] - 1;
    s->body[2] = s->body[0] - 2;
    s->direction = 1;
    s->next_direction = 1;
    s->level = 1;
    s->best = best;
    s->random = now ^ 0x713AC895U;
    if (!s->random)
        s->random = 1;
    s->last_ms = now;
    s->phase = MINI_RUNNING;
    food(s);
}

bool snake_process(snake_model_t *s, uint32_t now, const joystick_input_t *in, bool action)
{
    if (s->phase != MINI_RUNNING) {
        if (action) {
            snake_init(s, now, s->best);
            return true;
        }
        return false;
    }
    int direction = -1;
    if (in->x > 550)
        direction = 1;
    else if (in->x < -550)
        direction = 3;
    else if (in->y > 550)
        direction = 2;
    else if (in->y < -550)
        direction = 0;
    if (direction >= 0 && ((direction + 2) % 4) != s->direction)
        s->next_direction = (uint8_t)direction;
    s->accumulated_ms += elapsed(now, &s->last_ms);
    uint32_t period = 180U - 12U * s->level;
    if (in->b_pressed)
        period /= 2U;
    if (s->accumulated_ms < period)
        return false;
    s->accumulated_ms -= period;
    s->direction = s->next_direction;
    int x = s->body[0] % SNAKE_COLS, y = s->body[0] / SNAKE_COLS;
    if (s->direction == 0)
        --y;
    else if (s->direction == 1)
        ++x;
    else if (s->direction == 2)
        ++y;
    else
        --x;
    if (x < 0 || x >= (int)SNAKE_COLS || y < 0 || y >= (int)SNAKE_ROWS) {
        s->phase = MINI_OVER;
        return true;
    }
    uint16_t cell = (uint16_t)(y * SNAKE_COLS + x);
    bool grow = cell == s->food;
    uint32_t occupied = s->length - (grow ? 0U : 1U); /* A moving tail vacates its cell. */
    for (uint32_t i = 0; i < occupied; ++i)
        if (s->body[i] == cell) {
            s->phase = MINI_OVER;
            return true;
        }
    if (grow) {
        ++s->length;
        s->score += 10;
        if (s->score > s->best)
            s->best = s->score;
        s->level = 1U + s->score / 50U;
        if (s->level > 9)
            s->level = 9;
    }
    for (uint32_t i = s->length - 1; i > 0; --i)
        s->body[i] = s->body[i - 1];
    s->body[0] = cell;
    if (grow)
        food(s);
    return true;
}

static void serve(breakout_model_t *b)
{
    b->phase = MINI_SERVE;
    b->ball_x_fp = b->paddle_fp + 24 * 256;
    b->ball_y_fp = 210 * 256;
    b->vx = 120 + 10 * b->level;
    b->vy = -(160 + 10 * b->level);
}

void breakout_init(breakout_model_t *b, uint32_t now, uint32_t best)
{
    memset(b, 0, sizeof(*b));
    memset(b->bricks, 1, sizeof(b->bricks));
    b->paddle_fp = 136 * 256;
    b->lives = 3;
    b->level = 1;
    b->best = best;
    b->last_ms = now;
    serve(b);
}

static void breakout_step(breakout_model_t *b, uint32_t dt)
{
    int32_t oldx = b->ball_x_fp, oldy = b->ball_y_fp;
    b->ball_x_fp += b->vx * (int32_t)dt * 256 / 1000;
    b->ball_y_fp += b->vy * (int32_t)dt * 256 / 1000;
    if (b->ball_x_fp < 3 * 256) {
        b->ball_x_fp = 3 * 256;
        if (b->vx < 0)
            b->vx = -b->vx;
    }
    if (b->ball_x_fp > 317 * 256) {
        b->ball_x_fp = 317 * 256;
        if (b->vx > 0)
            b->vx = -b->vx;
    }
    if (b->ball_y_fp < 40 * 256) {
        b->ball_y_fp = 40 * 256;
        if (b->vy < 0)
            b->vy = -b->vy;
    }
    int x = b->ball_x_fp / 256, y = b->ball_y_fp / 256, paddle = b->paddle_fp / 256;
    if (b->vy > 0 && oldy <= 211 * 256 && y >= 211 && x + 3 >= paddle && x - 3 <= paddle + 48) {
        b->ball_y_fp = 211 * 256;
        b->vy = -(170 + 10 * b->level);
        b->vx = (x - (paddle + 24)) * 8;
        if (b->vx >= 0 && b->vx < 40)
            b->vx = 40;
        if (b->vx < 0 && b->vx > -40)
            b->vx = -40;
    }
    for (uint32_t i = 0; i < 40; ++i) {
        if (!b->bricks[i])
            continue;
        int left = 6 + 31 * (i % 10), top = 52 + 15 * (i / 10);
        if (x + 3 < left || x - 3 >= left + 28 || y + 3 < top || y - 3 >= top + 10)
            continue;
        b->bricks[i] = 0;
        b->score += 10;
        if (b->score > b->best)
            b->best = b->score;
        if (oldy + 3 * 256 <= top * 256 || oldy - 3 * 256 >= (top + 10) * 256) {
            b->vy = -b->vy;
            b->ball_y_fp = (oldy < top * 256 ? top - 3 : top + 13) * 256;
        } else {
            b->vx = -b->vx;
            b->ball_x_fp = (oldx < left * 256 ? left - 3 : left + 31) * 256;
        }
        break;
    }
    if (b->ball_y_fp > 230 * 256) {
        if (--b->lives == 0)
            b->phase = MINI_OVER;
        else
            serve(b);
    }
    bool any = false;
    for (uint32_t i = 0; i < 40; ++i)
        any |= b->bricks[i] != 0;
    if (!any) {
        if (b->level < 9)
            ++b->level;
        memset(b->bricks, 1, sizeof(b->bricks));
        serve(b);
    }
}

bool breakout_process(breakout_model_t *b, uint32_t now, const joystick_input_t *in, bool action)
{
    uint32_t dt = elapsed(now, &b->last_ms);
    if (b->phase == MINI_OVER) {
        if (action) {
            breakout_init(b, now, b->best);
            return true;
        }
        return false;
    }
    int32_t previous = b->paddle_fp;
    b->paddle_fp += (int32_t)((int64_t)in->x * 220 * dt * 256 / 1000000);
    if (b->paddle_fp < 6 * 256)
        b->paddle_fp = 6 * 256;
    if (b->paddle_fp > 266 * 256)
        b->paddle_fp = 266 * 256;
    if (b->phase == MINI_SERVE) {
        b->ball_x_fp = b->paddle_fp + 24 * 256;
        if (action)
            b->phase = MINI_RUNNING;
        return action || previous != b->paddle_fp;
    }
    if (in->b_pressed)
        dt = (dt + 1U) / 2U;
    while (dt && b->phase == MINI_RUNNING) {
        uint32_t step = dt > 10U ? 10U : dt;
        breakout_step(b, step);
        dt -= step;
    }
    return true;
}
