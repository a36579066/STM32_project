#include "console_scene.h"
#include <stddef.h>
#include <string.h>
#include "display_service.h"
#include "game_scene.h"
#include "ui_font.h"
#define BG    0x0842U
#define WHITE 0xEFFFU
#define MUTED 0x8492U
#define CYAN  0x07FFU
#define GOLD  0xFFE4U
#define GREEN 0x67EDU
#define PINK  0xF9AEU

typedef struct {
    int x, y, width;
    unsigned scale;
    const char *text;
    uint16_t color;
} label_t;

typedef struct {
    const console_model_t *c;
    console_status_t s;
    label_t labels[16];
    unsigned count;
    uint16_t row_labels[240];
    char score[6], best[6], level[2], lives[2];
    uint8_t snake[SNAKE_CELLS];
} view_t;

static view_t current_view, old_view;
static uint8_t tile[32 * 16 * 2];
static bool dirty_tiles[15][10];

static void mark(int x, int y, int w, int h)
{
    int right = x + w - 1, bottom = y + h - 1;
    if (right < 0 || bottom < 0 || x >= 320 || y >= 240)
        return;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (right > 319)
        right = 319;
    if (bottom > 239)
        bottom = 239;
    for (int row = y / 16; row <= bottom / 16; ++row)
        for (int col = x / 32; col <= right / 32; ++col)
            dirty_tiles[row][col] = true;
}

static void label(view_t *v, int x, int y, unsigned scale, const char *s, uint16_t color)
{
    unsigned index = v->count++;
    v->labels[index] = (label_t){x, y, (int)(strlen(s) * 6U * scale), scale, s, color};
    for (int row = y; row < y + (int)(7U * scale) && row < 240; ++row)
        if (row >= 0)
            v->row_labels[row] |= 1U << index;
}

static void prepare(view_t *v, const console_model_t *c)
{
    memset(v, 0, sizeof(*v));
    v->c = c;
    console_model_status(c, &v->s);
    ui_number(v->score, v->s.score, 5);
    ui_number(v->best, v->s.best, 5);
    ui_number(v->level, v->s.level, 1);
    ui_number(v->lives, v->s.lives, 1);
    if (c->mode != CONSOLE_PLAYING) {
        label(v, 22, 18, 3, c->mode == CONSOLE_MENU ? "POCKET ARCADE" : "GAME PAUSED", CYAN);
        label(v, 24, 49, 1,
              c->mode == CONSOLE_MENU ? "THREE GAMES - ONE CONTROLLER"
                                      : console_game_name(c->selected),
              MUTED);
        static const char *const pause[] = {"RESUME", "RESTART", "GAME LIST"};
        for (unsigned i = 0; i < 3; ++i)
            label(v, 42, 84 + 36 * i, 2,
                  c->mode == CONSOLE_MENU ? console_game_name((arcade_game_t)i) : pause[i], WHITE);
        label(v, 24, 201, 1, "STICK UP/DOWN   A SELECT", GOLD);
        label(v, 24, 218, 1,
              c->mode == CONSOLE_MENU ? "MENU: PAUSE DURING ANY GAME" : "B OR MENU: RESUME", MUTED);
        return;
    }
    label(v, 8, 9, 2, console_game_name(c->selected), CYAN);
    label(v, 204, 9, 1, "SCORE", MUTED);
    label(v, 242, 9, 1, v->score, WHITE);
    label(v, 8, 29, 1, "BEST", MUTED);
    label(v, 38, 29, 1, v->best, GOLD);
    label(v, 104, 29, 1, "LV", MUTED);
    label(v, 122, 29, 1, v->level, WHITE);
    label(v, 160, 29, 1, "LIVES", MUTED);
    label(v, 196, 29, 1, v->lives, PINK);
    if (c->selected == ARCADE_SNAKE) {
        for (unsigned i = 0; i < c->snake.length; ++i)
            v->snake[c->snake.body[i]] = i == 0 ? 2 : 1;
        if (c->snake.phase != MINI_WON)
            v->snake[c->snake.food] = 3;
    }
    label(v, 12, 228, 1,
          c->selected == ARCADE_SNAKE ? "STICK TURN  B BOOST  MENU PAUSE"
                                      : "STICK MOVE  B SLOW   MENU PAUSE",
          MUTED);
    if (v->s.over) {
        label(v, 82, 116, 2,
              c->selected == ARCADE_SNAKE && c->snake.phase == MINI_WON ? "YOU WIN" : "GAME OVER",
              GOLD);
        label(v, 67, 145, 1, "A RETRY   B GAME LIST", WHITE);
    } else if (c->selected == ARCADE_BREAKOUT && c->breakout.phase == MINI_SERVE)
        label(v, 70, 159, 2, "A TO SERVE", GOLD);
}

static uint16_t pixel(const view_t *v, int x, int y)
{
    const console_model_t *c = v->c;
    uint16_t color = BG;
    if (c->mode != CONSOLE_PLAYING) {
        unsigned selected = c->mode == CONSOLE_MENU ? (unsigned)c->selected : c->pause_item;
        int top = 74 + 36 * selected;
        if (x >= 24 && x < 296 && y >= top && y < top + 30)
            color = 0x1210;
        if (x >= 24 && x < 28 && y >= top && y < top + 30)
            color = CYAN;
        if (y == 64 && x >= 24 && x < 296)
            color = 0x29A7;
    } else if (c->selected == ARCADE_SNAKE) {
        if (x >= 38 && x < 282 && y >= 46 && y < 206)
            color = MUTED;
        if (x >= 40 && x < 280 && y >= 48 && y < 204) {
            int localx = x - 40, localy = y - 48;
            uint8_t kind = v->snake[(localy / 12) * SNAKE_COLS + localx / 12];
            color = BG;
            if (localx % 12 != 11 && localy % 12 != 11) {
                if (kind == 1)
                    color = GREEN;
                else if (kind == 2)
                    color = CYAN;
                else if (kind == 3 && localx % 12 >= 3 && localx % 12 <= 8 && localy % 12 >= 3 &&
                         localy % 12 <= 8)
                    color = GOLD;
            }
        }
    } else {
        const breakout_model_t *b = &c->breakout;
        if ((x == 0 || x == 319) && y >= 39 && y < 224)
            color = MUTED;
        if (y == 39)
            color = MUTED;
        if (y >= 52 && y < 112 && x >= 6 && x < 313) {
            unsigned col = (unsigned)(x - 6) / 31, row = (unsigned)(y - 52) / 15;
            if (col < 10 && row < 4 && (x - 6) % 31 < 28 && (y - 52) % 15 < 10 &&
                b->bricks[row * 10 + col]) {
                static const uint16_t colors[] = {PINK, GOLD, GREEN, CYAN};
                color = colors[row];
            }
        }
        if (y >= 214 && y < 222 && x >= b->paddle_fp / 256 && x < b->paddle_fp / 256 + 48)
            color = CYAN;
        int dx = x - b->ball_x_fp / 256, dy = y - b->ball_y_fp / 256;
        if (dx * dx + dy * dy <= 9)
            color = WHITE;
    }
    if (v->s.over && c->mode == CONSOLE_PLAYING && x >= 50 && x < 270 && y >= 104 && y < 163)
        color = 0x1210;
    uint16_t labels = v->row_labels[y];
    while (labels) {
        unsigned i = (unsigned)__builtin_ctz((unsigned)labels);
        labels &= labels - 1U;
        const label_t *l = &v->labels[i];
        if (x < l->x || x >= l->x + l->width)
            continue;
        unsigned local = (unsigned)(x - l->x) / l->scale, col = local % 6;
        if (col < 5 &&
            (ui_glyph_row(l->text[local / 6], (unsigned)(y - l->y) / l->scale) & (1U << (4 - col))))
            color = l->color;
    }
    return color;
}

bool console_scene_render(const console_model_t *c, const console_model_t *previous)
{
    bool compatible = previous && previous->mode == c->mode &&
                      (c->mode == CONSOLE_MENU || previous->selected == c->selected);
    if (c->mode == CONSOLE_PLAYING && c->selected == ARCADE_COIN_DASH)
        return game_scene_render(&c->runner, compatible ? &previous->runner : NULL);
    prepare(&current_view, c);
    if (compatible)
        prepare(&old_view, previous);
    memset(dirty_tiles, 0, sizeof(dirty_tiles));
    if (!compatible)
        mark(0, 0, 320, 240);
    else if (c->mode != CONSOLE_PLAYING) {
        unsigned selected = c->mode == CONSOLE_MENU ? (unsigned)c->selected : c->pause_item;
        unsigned old_selected =
            c->mode == CONSOLE_MENU ? (unsigned)previous->selected : previous->pause_item;
        if (selected != old_selected) {
            mark(24, 74 + 36 * selected, 272, 30);
            mark(24, 74 + 36 * old_selected, 272, 30);
        }
    } else {
        const console_status_t *s = &current_view.s, *old = &old_view.s;
        if (s->score != old->score || s->best != old->best || s->level != old->level ||
            s->lives != old->lives)
            mark(0, 0, 320, 39);
        if (s->over != old->over)
            mark(50, 104, 220, 70);
        if (c->selected == ARCADE_SNAKE) {
            for (unsigned cell = 0; cell < SNAKE_CELLS; ++cell)
                if (current_view.snake[cell] != old_view.snake[cell])
                    mark(40 + 12 * (cell % SNAKE_COLS), 48 + 12 * (cell / SNAKE_COLS), 12, 12);
        } else {
            const breakout_model_t *b = &c->breakout, *old_b = &previous->breakout;
            if (b->ball_x_fp != old_b->ball_x_fp || b->ball_y_fp != old_b->ball_y_fp) {
                mark(b->ball_x_fp / 256 - 3, b->ball_y_fp / 256 - 3, 7, 7);
                mark(old_b->ball_x_fp / 256 - 3, old_b->ball_y_fp / 256 - 3, 7, 7);
            }
            if (b->paddle_fp != old_b->paddle_fp) {
                mark(b->paddle_fp / 256, 214, 48, 8);
                mark(old_b->paddle_fp / 256, 214, 48, 8);
            }
            for (unsigned i = 0; i < 40; ++i)
                if (b->bricks[i] != old_b->bricks[i])
                    mark(6 + 31 * (i % 10), 52 + 15 * (i / 10), 28, 10);
            if (b->phase != old_b->phase)
                mark(50, 104, 220, 70);
        }
    }
    /* Compare composed tiles against the last actually displayed state. This
       also erases overlays and survives a one-element queue dropping frames. */
    for (int top = 0; top < 240; top += 16)
        for (int left = 0; left < 320; left += 32) {
            if (!dirty_tiles[top / 16][left / 32])
                continue;
            bool changed = !compatible;
            unsigned p = 0;
            for (int y = top; y < top + 16; ++y)
                for (int x = left; x < left + 32; ++x) {
                    uint16_t color = pixel(&current_view, x, y);
                    if (compatible && !changed && color != pixel(&old_view, x, y))
                        changed = true;
                    tile[p++] = (uint8_t)(color >> 8);
                    tile[p++] = (uint8_t)color;
                }
            if (changed && !display_service_draw_rgb565(left, top, 32, 16, tile))
                return false;
        }
    return true;
}

static void oled_text(uint8_t *pixels, int x, int y, const char *text, bool inverse)
{
    for (unsigned i = 0; text[i] && x + (int)(i * 6) < 128; ++i) {
        for (unsigned row = 0; row < 7; ++row)
            for (unsigned col = 0; col < 6; ++col) {
                int px = x + (int)(i * 6 + col), py = y + (int)row;
                if (px < 0 || px >= 128 || py < 0 || py >= 64)
                    continue;
                bool on = col < 5 && (ui_glyph_row(text[i], row) & (1U << (4 - col)));
                if (on != inverse)
                    pixels[px + (py / 8) * 128] |= 1U << (py % 8);
                else
                    pixels[px + (py / 8) * 128] &= ~(1U << (py % 8));
            }
    }
}

void console_oled_render(const console_status_t *s, uint8_t pixels[1024])
{
    memset(pixels, 0, 1024);
    if (!s->ready) {
        oled_text(pixels, 4, 12, "POCKET ARCADE", false);
        oled_text(pixels, 4, 32, "WAIT INPUT / TFT", false);
        return;
    }
    oled_text(pixels, 2, 0,
              s->mode == CONSOLE_MENU ? "POCKET ARCADE" : console_game_name(s->selected), false);
    if (s->mode != CONSOLE_PLAYING) {
        static const char *const pause[] = {"RESUME", "RESTART", "GAME LIST"};
        unsigned selected = s->mode == CONSOLE_MENU ? (unsigned)s->selected : s->pause_item;
        for (unsigned i = 0; i < 3; ++i) {
            bool active = i == selected;
            int y = 14 + 12 * i;
            if (active)
                for (int py = y - 1; py < y + 9; ++py)
                    for (int x = 0; x < 128; ++x)
                        pixels[x + (py / 8) * 128] |= 1U << (py % 8);
            oled_text(pixels, 6, y,
                      s->mode == CONSOLE_MENU ? console_game_name((arcade_game_t)i) : pause[i],
                      active);
        }
        oled_text(pixels, 2, 56,
                  s->mode == CONSOLE_MENU ? "A SELECT  STICK NAV" : "A OK  B/MENU RESUME", false);
    } else {
        char number[6];
        oled_text(pixels, 2, 14, "SCORE", false);
        ui_number(number, s->score, 5);
        oled_text(pixels, 50, 14, number, false);
        oled_text(pixels, 2, 26, "BEST", false);
        ui_number(number, s->best, 5);
        oled_text(pixels, 50, 26, number, false);
        oled_text(pixels, 2, 38, "LIVES", false);
        ui_number(number, s->lives, 1);
        oled_text(pixels, 38, 38, number, false);
        oled_text(pixels, 62, 38, "LV", false);
        ui_number(number, s->level, 1);
        oled_text(pixels, 86, 38, number, false);
        oled_text(pixels, 2, 56, s->over ? "A RETRY  B GAME LIST" : "MENU PAUSE / RETURN", false);
    }
}
