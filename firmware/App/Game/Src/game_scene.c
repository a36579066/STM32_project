#include "game_scene.h"

#include <stddef.h>
#include "display_service.h"
#include "game_sprite.h"

#define C_WHITE        0xEFFFU
#define C_MUTED        0x8492U
#define C_CYAN         0x07FFU
#define C_GOLD         0xFFE4U
#define C_PINK         0xF9AEU
#define C_PANEL        0x0842U
#define TILE_PIXELS    1280U
#define DIRTY_CAPACITY 64U

typedef struct {
    int16_t x, y, w, h;
} rect_t;

typedef struct {
    const game_model_t *g;
    char score[6], best[6], level[3], coins[5], combo[2], speed[4], charge[2];
    uint8_t visible[GAME_OBJECT_CAPACITY];
    uint8_t visible_count;
} render_t;

static uint8_t pixels[TILE_PIXELS * 2U];
static rect_t dirty[DIRTY_CAPACITY];
static uint32_t dirty_count;

/* Original 5x7 bitmap glyphs: digits, then A-Z. */
static const uint8_t glyphs[36][7] = {
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},
    {30, 1, 1, 14, 1, 1, 30},     {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 1, 14},   {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31},
    {31, 16, 16, 30, 16, 16, 16}, {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14},      {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17},
    {16, 16, 16, 16, 16, 16, 31}, {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
    {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13},
    {30, 17, 17, 30, 20, 18, 17}, {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10},
    {17, 17, 10, 4, 10, 17, 17},  {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},
};

static void number(char *out, uint32_t value, uint32_t digits)
{
    uint32_t limit = 1U;
    for (uint32_t i = 0U; i < digits; ++i)
        limit *= 10U;
    if (value >= limit)
        value = limit - 1U;
    out[digits] = '\0';
    while (digits > 0U) {
        out[--digits] = (char)('0' + value % 10U);
        value /= 10U;
    }
}

static uint8_t glyph_row(char c, uint32_t row)
{
    if ((c >= '0') && (c <= '9'))
        return glyphs[c - '0'][row];
    if ((c >= 'A') && (c <= 'Z'))
        return glyphs[10 + c - 'A'][row];
    if (c == ':')
        return ((row == 2U) || (row == 5U)) ? 4U : 0U;
    if (c == '/')
        return (uint8_t)(1U << (row < 5U ? row : 4U));
    if (c == '-')
        return row == 3U ? 14U : 0U;
    if (c == '=')
        return row == 2U || row == 4U ? 14U : 0U;
    if (c == '!')
        return row < 5U || row == 6U ? 4U : 0U;
    return 0U;
}

static uint16_t text_pixel(uint16_t bg, int16_t x, int16_t y, int16_t left, int16_t top,
                           uint8_t scale, const char *text, uint16_t color)
{
    if ((x < left) || (y < top) || (y >= top + 7 * scale))
        return bg;
    const uint32_t index = (uint32_t)(x - left) / (6U * scale);
    /* Strings are short; bound the index before indexing the text buffer. */
    for (uint32_t i = 0U; i <= index; ++i)
        if (text[i] == '\0')
            return bg;
    const uint32_t column = (uint32_t)(x - left) / scale % 6U;
    if (column >= 5U)
        return bg;
    const uint32_t row = (uint32_t)(y - top) / scale;
    return (glyph_row(text[index], row) & (1U << (4U - column))) != 0U ? color : bg;
}

static bool intersects(rect_t a, rect_t b)
{
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

static rect_t united(rect_t a, rect_t b)
{
    const int16_t l = a.x < b.x ? a.x : b.x;
    const int16_t t = a.y < b.y ? a.y : b.y;
    const int16_t r = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w;
    const int16_t bottom = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
    return (rect_t){l, t, (int16_t)(r - l), (int16_t)(bottom - t)};
}

static rect_t object_rect(const game_object_t *o)
{
    return (rect_t){(int16_t)(o->x_fp / GAME_FP_ONE), o->y, o->width, o->height};
}

static void add_dirty(rect_t r)
{
    int16_t right = (int16_t)(r.x + r.w);
    int16_t bottom = (int16_t)(r.y + r.h);
    if (r.x < 0)
        r.x = 0;
    if (r.y < 0)
        r.y = 0;
    if (right > (int16_t)DISPLAY_SERVICE_WIDTH)
        right = DISPLAY_SERVICE_WIDTH;
    if (bottom > (int16_t)DISPLAY_SERVICE_HEIGHT)
        bottom = DISPLAY_SERVICE_HEIGHT;
    r.w = (int16_t)(right - r.x);
    r.h = (int16_t)(bottom - r.y);
    if ((r.w <= 0) || (r.h <= 0))
        return;
    for (uint32_t i = 0U; i < dirty_count; ++i) {
        const rect_t u = united(dirty[i], r);
        if (intersects(dirty[i], r) &&
            ((int32_t)u.w * u.h <= (int32_t)dirty[i].w * dirty[i].h + (int32_t)r.w * r.h + 64)) {
            dirty[i] = u;
            return;
        }
    }
    if (dirty_count < DIRTY_CAPACITY) {
        dirty[dirty_count++] = r;
    } else {
        /* Never drop a dirty region if a future object budget grows. */
        dirty_count = 1U;
        dirty[0] = (rect_t){0, 0, DISPLAY_SERVICE_WIDTH, DISPLAY_SERVICE_HEIGHT};
    }
}

static uint16_t background_pixel(const game_model_t *g, int16_t x, int16_t y)
{
    if (y >= 214)
        return C_PANEL;
    if (y >= (int16_t)GAME_GROUND_Y) {
        if (y == GAME_GROUND_Y)
            return C_CYAN;
        if (y < (int16_t)GAME_GROUND_Y + 3)
            return 0x22F0U;
        if ((y >= 204) && (y < 208)) {
            const uint32_t phase = ((g->distance_fp / (4U * GAME_FP_ONE)) & 7U) * 4U;
            return (((uint32_t)x + phase) % 32U < 12U) ? 0x35B8U : 0x10A5U;
        }
        return 0x10A5U;
    }
    uint16_t c = (y < 78) ? 0x0843U : ((y < 140) ? 0x10A6U : 0x1949U);
    if ((y > 43) && (y < 100) && ((((uint32_t)x * 37U + (uint32_t)y * 83U) % 571U) == 0U))
        c = 0x74F6U;
    const int16_t building = (int16_t)(x / 24);
    const int16_t roof = (int16_t)(112 + (building * 17) % 43);
    if ((y >= roof) && (x % 24 < 21)) {
        c = 0x0863U;
        if ((x % 24 >= 5) && (x % 24 <= 9) && (y % 13 >= 3) && (y % 13 <= 5))
            c = 0x1B2DU;
        if ((x % 24 >= 14) && (x % 24 <= 17) && (y % 17 >= 4) && (y % 17 <= 6))
            c = 0x234CU;
    }
    return c;
}

static uint16_t hud_pixel(const render_t *r, int16_t x, int16_t y)
{
    uint16_t c = y == 35 ? 0x2492U : C_PANEL;
    c = text_pixel(c, x, y, 10, 4, 1, "SCORE", C_MUTED);
    c = text_pixel(c, x, y, 10, 16, 2, r->score, C_GOLD);
    c = text_pixel(c, x, y, 88, 4, 1, "BEST", C_MUTED);
    c = text_pixel(c, x, y, 88, 16, 2, r->best, C_WHITE);
    c = text_pixel(c, x, y, 168, 4, 1, "LEVEL", C_MUTED);
    c = text_pixel(c, x, y, 174, 16, 2, r->level, C_CYAN);
    /* Shield outline and three hearts communicate power/lives without text. */
    const int16_t sx = x - 216, sy = y - 14;
    if ((sx >= 0) && (sx < 14) && (sy >= 0) && (sy < 16) &&
        ((sy < 10) || ((sx >= sy - 9) && (sx <= 22 - sy)))) {
        c = r->g->shield ? C_CYAN : 0x21AAU;
        if (sx > 2 && sx < 11 && sy > 2 && sy < 9)
            c = r->g->shield ? 0x2C33U : C_PANEL;
    }
    for (int16_t i = 0; i < 3; ++i) {
        const int16_t hx = (int16_t)(x - (246 + i * 22)), hy = y - 15;
        if ((hx < 0) || (hx >= 14) || (hy < 0) || (hy >= 12))
            continue;
        const bool shape = (hy < 4) ? ((hx >= 1 && hx <= 5) || (hx >= 8 && hx <= 12))
                                    : (hx >= hy - 4 && hx <= 17 - hy);
        if (shape)
            c = i < r->g->lives ? C_PINK : 0x31A8U;
    }
    return c;
}

static uint16_t footer_pixel(const render_t *r, int16_t x, int16_t y)
{
    uint16_t c = C_PANEL;
    c = text_pixel(c, x, y, 10, 217, 1, "COINS", C_MUTED);
    c = text_pixel(c, x, y, 46, 217, 1, r->coins, C_GOLD);
    c = text_pixel(c, x, y, 100, 217, 1, "COMBO X", C_MUTED);
    c = text_pixel(c, x, y, 142, 217, 1, r->combo, C_GOLD);
    c = text_pixel(c, x, y, 196, 217, 1, "SPD", C_MUTED);
    c = text_pixel(c, x, y, 220, 217, 1, r->speed, C_CYAN);
    c = text_pixel(c, x, y, 270, 217, 1, r->charge, C_CYAN);
    c = text_pixel(c, x, y, 276, 217, 1, "/10", C_MUTED);
    c = text_pixel(c, x, y, 44, 231, 1, "STICK: MOVE   BUTTON: JUMP", 0x6BB0U);
    return c;
}

static uint16_t object_pixel(uint16_t c, const game_object_t *o, int16_t x, int16_t y,
                             uint32_t run_ms)
{
    x = (int16_t)(x - o->x_fp / GAME_FP_ONE);
    y = (int16_t)(y - o->y);
    if ((x < 0) || (y < 0) || (x >= o->width) || (y >= o->height))
        return c;
    if (o->kind == GAME_COIN) {
        const int16_t dx = x - 4, dy = y - 5;
        if (dx * dx * 2 + dy * dy > 34)
            return c;
        c = dx * dx * 2 + dy * dy > 18 ? 0xFCA0U : C_GOLD;
        if (x == 4 && y >= 3 && y <= 7)
            c = ((run_ms / 120U) & 1U) ? C_WHITE : 0xFDC0U;
        return c;
    }
    if (o->kind == GAME_SPIKES) {
        const int16_t center = o->width / 2;
        if ((x < center - y * 2 / 3) || (x > center + y * 2 / 3))
            return c;
        return (x <= center - y * 2 / 3 + 1 || x >= center + y * 2 / 3 - 1) ? 0xFF95U : 0xF985U;
    }
    if (o->kind == GAME_CRATE) {
        if (x < 2 || x >= o->width - 2 || y < 2 || y >= o->height - 2)
            return 0xFDA0U;
        if ((y < 6 || y >= o->height - 6) && ((x + y) % 10 < 5))
            return 0x18C4U;
        return (x == y - 2 || x + y == o->width + 2) ? C_GOLD : 0xB389U;
    }
    if ((y < 3) && ((x >= 3 && x <= 10) || (x >= 18 && x <= 25))) {
        return ((run_ms / 60U) & 1U) ? C_WHITE : C_CYAN;
    }
    if (x >= 4 && x < 24 && y >= 4 && y < 14) {
        if (x >= 17 && x <= 21 && y >= 7 && y <= 9)
            return C_PINK;
        return y < 7 ? 0xB6FEU : 0x3C55U;
    }
    return (y >= 7 && y <= 10) ? C_CYAN : c;
}

static uint16_t overlay_pixel(uint16_t c, const render_t *r, int16_t x, int16_t y)
{
    if (r->g->mode == GAME_READY) {
        if (x < 40 || x >= 280 || y < 46 || y >= 164)
            return c;
        c = x < 42 || x >= 278 || y < 48 || y >= 162 ? 0x2513U : C_PANEL;
        c = text_pixel(c, x, y, 79, 60, 3, "COIN DASH", C_CYAN);
        c = text_pixel(c, x, y, 112, 87, 1, "ROBOT NIGHT RUN", C_GOLD);
        c = text_pixel(c, x, y, 55, 105, 1, "JUMP CRATES - WALK UNDER DRONES", C_WHITE);
        c = text_pixel(c, x, y, 106, 118, 1, "10 COINS = SHIELD", C_MUTED);
        c = text_pixel(c, x, y, 88, 140, 2, "PRESS BUTTON", C_GOLD);
    } else if (r->g->mode == GAME_OVER) {
        if (x < 54 || x >= 266 || y < 62 || y >= 166)
            return c;
        c = x < 56 || x >= 264 || y < 64 || y >= 164 ? C_PINK : C_PANEL;
        c = text_pixel(c, x, y, 112, 74, 2, "RUN OVER", C_PINK);
        c = text_pixel(c, x, y, 80, 104, 1, "SCORE", C_MUTED);
        c = text_pixel(c, x, y, 134, 100, 2, r->score, C_GOLD);
        c = text_pixel(c, x, y, 80, 124, 1, "BEST", C_MUTED);
        c = text_pixel(c, x, y, 134, 120, 2, r->best, C_WHITE);
        c = text_pixel(c, x, y, 100, 148, 1, "BUTTON: RETRY", C_CYAN);
    } else if (r->g->toast_ms > 0U && x >= 80 && x < 240 && y >= 48 && y < 67) {
        c = C_PANEL;
        const char *label = "NICE!";
        uint16_t color = C_GOLD;
        switch (r->g->toast) {
        case GAME_TOAST_COMBO:
            label = "COIN COMBO!";
            break;
        case GAME_TOAST_SHIELD:
            label = "SHIELD READY";
            color = C_CYAN;
            break;
        case GAME_TOAST_BLOCKED:
            label = "SHIELD BLOCK!";
            color = C_CYAN;
            break;
        case GAME_TOAST_HIT:
            label = "OUCH! KEEP GOING";
            color = C_PINK;
            break;
        case GAME_TOAST_LEVEL:
            label = "LEVEL UP!";
            color = C_CYAN;
            break;
        default:
            break;
        }
        c = text_pixel(c, x, y, 96, 54, 1, label, color);
    }
    return c;
}

static uint16_t scene_pixel(const render_t *r, int16_t x, int16_t y)
{
    if (y < (int16_t)GAME_HUD_HEIGHT)
        return hud_pixel(r, x, y);
    if (y >= 214)
        return footer_pixel(r, x, y);
    uint16_t c = background_pixel(r->g, x, y);
    for (uint32_t i = 0U; i < r->visible_count; ++i) {
        c = object_pixel(c, &r->g->objects[r->visible[i]], x, y, r->g->run_ms);
    }
    const int16_t hx = (int16_t)(x - r->g->hero_x_fp / GAME_FP_ONE), hy = y - r->g->hero_y;
    /* Shield ownership is shown in the HUD; keep the hero free of a box. */
    const bool visible = (r->g->invulnerable_ms == 0U) || ((r->g->run_ms / 80U) & 1U) == 0U;
    if (visible && hx >= 0 && hx < (int16_t)GAME_HERO_WIDTH && hy >= 0 &&
        hy < (int16_t)GAME_HERO_HEIGHT) {
        c = game_sprite_pixel((uint16_t)hx, (uint16_t)hy, (uint8_t)((r->g->run_ms / 100U) & 1U),
                              false, c);
    }
    return overlay_pixel(c, r, x, y);
}

static bool render_rect(render_t *r, rect_t area)
{
    const uint16_t rows_per_chunk = (uint16_t)(TILE_PIXELS / (uint16_t)area.w);
    for (int16_t y = area.y; y < area.y + area.h;) {
        const uint16_t rows =
            (uint16_t)(((area.y + area.h - y) < rows_per_chunk) ? (area.y + area.h - y)
                                                                : rows_per_chunk);
        const rect_t chunk = {area.x, y, area.w, (int16_t)rows};
        r->visible_count = 0U;
        for (uint32_t i = 0U; i < GAME_OBJECT_CAPACITY; ++i) {
            if (r->g->objects[i].active && intersects(chunk, object_rect(&r->g->objects[i]))) {
                r->visible[r->visible_count++] = (uint8_t)i;
            }
        }
        uint32_t offset = 0U;
        for (int16_t row = y; row < y + rows; ++row) {
            for (int16_t x = area.x; x < area.x + area.w; ++x) {
                const uint16_t color = scene_pixel(r, x, row);
                pixels[offset++] = (uint8_t)(color >> 8U);
                pixels[offset++] = (uint8_t)color;
            }
        }
        if (!display_service_draw_rgb565((uint16_t)area.x, (uint16_t)y, (uint16_t)area.w, rows,
                                         pixels))
            return false;
        y = (int16_t)(y + rows);
    }
    return true;
}

bool game_scene_render(const game_model_t *g, const game_model_t *old)
{
    if (g == NULL)
        return false;
    render_t r = {.g = g};
    number(r.score, g->score, 5U);
    number(r.best, g->best_score, 5U);
    number(r.level, g->level, 2U);
    number(r.coins, g->coins, 4U);
    number(r.combo, g->combo, 1U);
    number(r.speed, g->speed, 3U);
    number(r.charge, g->coins % GAME_COINS_PER_SHIELD, 1U);
    dirty_count = 0U;
    if (old == NULL || old->mode != g->mode) {
        add_dirty((rect_t){0, 0, DISPLAY_SERVICE_WIDTH, DISPLAY_SERVICE_HEIGHT});
    } else {
        const rect_t hero = {(int16_t)(g->hero_x_fp / GAME_FP_ONE - 3), (int16_t)(g->hero_y - 3),
                             30, 38};
        const rect_t previous = {(int16_t)(old->hero_x_fp / GAME_FP_ONE - 3),
                                 (int16_t)(old->hero_y - 3), 30, 38};
        add_dirty(united(hero, previous));
        for (uint32_t i = 0U; i < GAME_OBJECT_CAPACITY; ++i) {
            const game_object_t *a = &old->objects[i], *b = &g->objects[i];
            if (a->active && b->active && (a->x_fp - b->x_fp < 32 * GAME_FP_ONE) &&
                (b->x_fp - a->x_fp < 32 * GAME_FP_ONE)) {
                add_dirty(united(object_rect(a), object_rect(b)));
            } else {
                if (a->active)
                    add_dirty(object_rect(a));
                if (b->active)
                    add_dirty(object_rect(b));
            }
        }
        if (g->distance_fp / (4U * GAME_FP_ONE) != old->distance_fp / (4U * GAME_FP_ONE))
            add_dirty((rect_t){0, 204, DISPLAY_SERVICE_WIDTH, 4});
        if (g->score != old->score)
            add_dirty((rect_t){8, 14, 64, 18});
        if (g->best_score != old->best_score)
            add_dirty((rect_t){86, 14, 64, 18});
        if (g->level != old->level)
            add_dirty((rect_t){168, 14, 32, 18});
        if (g->lives != old->lives || g->shield != old->shield)
            add_dirty((rect_t){212, 10, 104, 24});
        if (g->coins != old->coins) {
            add_dirty((rect_t){44, 215, 28, 11});
            add_dirty((rect_t){268, 215, 30, 11});
        }
        if (g->combo != old->combo)
            add_dirty((rect_t){140, 215, 10, 11});
        if (g->speed != old->speed)
            add_dirty((rect_t){218, 215, 22, 11});
        if (g->toast != old->toast || (g->toast_ms == 0U) != (old->toast_ms == 0U))
            add_dirty((rect_t){80, 48, 160, 19});
    }
    for (uint32_t i = 0U; i < dirty_count; ++i)
        if (!render_rect(&r, dirty[i]))
            return false;
    return true;
}
