/* Pure models and actual TFT/OLED renderers; fake only display transport. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "console_scene.h"
#include "display_service.h"
#include "game_config.h"
#include "oled.h"
static uint16_t framebuffer[240][320], reference[240][320];
static uint32_t uploads, pixels_written;
static uint32_t max_snake_dirty, max_breakout_dirty;
static bool fail_draw;

bool display_service_draw_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                 const uint8_t *data)
{
    assert(w && h && x + w <= 320 && y + h <= 240);
    if (fail_draw)
        return false;
    ++uploads;
    pixels_written += (uint32_t)w * h;
    for (unsigned row = 0; row < h; ++row)
        for (unsigned col = 0; col < w; ++col) {
            unsigned p = (row * w + col) * 2;
            framebuffer[y + row][x + col] = (uint16_t)(data[p] << 8 | data[p + 1]);
        }
    return true;
}

static uint8_t acknowledged = 0x3D, device_pages[1024], page, column;
static unsigned writes, probes, data_pages;
static bool fail_i2c, seen_charge, seen_dc, seen_page_mode;

i2c_status_t i2c1_write(uint8_t addr, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    assert((addr == 0x3C || addr == 0x3D) && timeout > 0);
    ++writes;
    if (!size) {
        ++probes;
        return addr == acknowledged ? I2C_STATUS_OK : I2C_STATUS_NACK;
    }
    assert(addr == acknowledged && data != NULL);
    if (fail_i2c)
        return I2C_STATUS_NACK;
    if (data[0] == 0x40) {
        assert(size == 129 && page < 8);
        memcpy(device_pages + page * 128, data + 1, 128);
        ++data_pages;
    } else {
        assert(data[0] == 0);
        if (size == 4 && (data[1] & 0xF0) == 0xB0) {
            page = data[1] & 7;
            column = data[2];
            assert(data[3] == 0x10);
        }
        for (unsigned i = 1; i + 1 < size; ++i) {
            if (data[i] == 0x8D && data[i + 1] == 0x14)
                seen_charge = true;
            if (data[i] == 0xAD && data[i + 1] == 0x8B)
                seen_dc = true;
            if (data[i] == 0x20 && data[i + 1] == 0x02)
                seen_page_mode = true;
        }
    }
    return I2C_STATUS_OK;
}

static void press(joystick_input_t *in, uint32_t now, int button)
{
    if (button == 0) {
        ++in->press_sequence;
        in->pressed_at_ms = now;
    } else if (button == 1) {
        ++in->b_press_sequence;
        in->b_pressed_at_ms = now;
    } else {
        ++in->menu_press_sequence;
        in->menu_pressed_at_ms = now;
    }
}

static void compare(const console_model_t *now, const console_model_t *old)
{
    uint32_t before = pixels_written;
    assert(console_scene_render(now, old));
    uint32_t dirty = pixels_written - before;
    if (old && now->mode == CONSOLE_PLAYING && old->mode == now->mode &&
        old->selected == now->selected) {
        if (now->selected == ARCADE_SNAKE && now->snake.phase == old->snake.phase &&
            now->snake.phase == MINI_RUNNING && dirty > max_snake_dirty)
            max_snake_dirty = dirty;
        if (now->selected == ARCADE_BREAKOUT && now->breakout.phase == old->breakout.phase &&
            now->breakout.phase == MINI_RUNNING && dirty > max_breakout_dirty)
            max_breakout_dirty = dirty;
    }
    memcpy(reference, framebuffer, sizeof(reference));
    assert(console_scene_render(now, NULL));
    assert(memcmp(framebuffer, reference, sizeof(reference)) == 0);
}

static void ppm(const char *dir, const char *name, const uint8_t *mono)
{
    if (!dir)
        return;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    assert(f);
    unsigned w = mono ? 128 : 320, h = mono ? 64 : 240;
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x) {
            uint8_t rgb[3];
            if (mono) {
                bool on = mono[x + (y / 8) * 128] & (1U << (y % 8));
                rgb[0] = on ? 210 : 4;
                rgb[1] = on ? 255 : 8;
                rgb[2] = on ? 255 : 12;
            } else {
                uint16_t p = framebuffer[y][x];
                rgb[0] = (p >> 11) * 255 / 31;
                rgb[1] = ((p >> 5) & 63) * 255 / 63;
                rgb[2] = (p & 31) * 255 / 31;
            }
            assert(fwrite(rgb, 1, 3, f) == 3);
        }
    fclose(f);
}

static void models(void)
{
    console_model_t c;
    joystick_input_t in = {0};
    console_model_init(&c, 0);
    assert(c.mode == CONSOLE_MENU);
    press(&in, 10, 0);
    assert(console_model_process(&c, 10, &in));
    assert(c.mode == CONSOLE_PLAYING && c.runner.mode == GAME_RUNNING && !c.runner.jumping);
    press(&in, 30, 0);
    assert(console_model_process(&c, 30, &in));
    assert(c.runner.jumping);
    console_model_process(&c, 170, &in);
    uint32_t run = c.runner.run_ms;
    int y = c.runner.hero_y;
    press(&in, 180, 2);
    assert(console_model_process(&c, 180, &in));
    assert(c.mode == CONSOLE_PAUSED);
    press(&in, 200, 0);
    in.pressed = true;                                     /* Resume consumes A, even when held. */
    assert(console_model_process(&c, 5200, &in) == false); /* Old retained press is stale. */
    assert(c.runner.run_ms == run && c.runner.hero_y == y);
    press(&in, 5210, 0);
    assert(console_model_process(&c, 5210, &in));
    assert(c.mode == CONSOLE_PLAYING && !c.runner.jump_pending);
    console_model_process(&c, 5230, &in);
    assert(c.runner.run_ms == run + 20);
    assert(!c.runner.jump_pending);
    /* Pause, choose GAME LIST, and preserve best scores across games. */
    c.runner.best_score = 321;
    console_model_process(&c, 5250, &in);
    press(&in, 5260, 2);
    console_model_process(&c, 5260, &in);
    c.pause_item = 2;
    press(&in, 5280, 0);
    console_model_process(&c, 5280, &in);
    assert(c.mode == CONSOLE_MENU && c.best[0] == 321);
    in.pressed = false;
    in.y = 1000;
    assert(console_model_process(&c, 5300, &in) && c.selected == ARCADE_SNAKE);
    assert(!console_model_process(&c, 5400, &in)); /* Repeat delay. */
    assert(console_model_process(&c, 5650, &in) && c.selected == ARCADE_BREAKOUT);
    in.y = 0;
    console_model_process(&c, 5660, &in);
    press(&in, 5680, 0);
    console_model_process(&c, 5680, &in);
    assert(c.breakout.phase == MINI_SERVE && c.mode == CONSOLE_PLAYING);
    press(&in, 5700, 0);
    console_model_process(&c, 5700, &in);
    assert(c.breakout.phase == MINI_RUNNING);
    /* Timer wrap does not break menu repeat or input-age checks. */
    console_model_init(&c, UINT32_MAX - 20U);
    in = (joystick_input_t){.y = 1000};
    assert(console_model_process(&c, UINT32_MAX - 10U, &in));
    assert(console_model_process(&c, 340U, &in));
    assert(c.selected == ARCADE_BREAKOUT);

    snake_model_t snake;
    snake_init(&snake, 0, 0);
    in = (joystick_input_t){0};
    snake.food = snake.body[0] + 1;
    for (unsigned t = 20; t <= 180; t += 20)
        snake_process(&snake, t, &in, false);
    assert(snake.length == 4 && snake.score == 10 && snake.best == 10);
    in.x = -1000;
    snake_process(&snake, 200, &in, false);
    assert(snake.next_direction == 1); /* Reject reverse. */
    snake.body[0] = 19;
    snake.direction = 1;
    snake.next_direction = 1;
    snake.accumulated_ms = 200;
    snake_process(&snake, 220, &in, false);
    assert(snake.phase == MINI_OVER);
    snake_process(&snake, 240, &in, true);
    assert(snake.phase == MINI_RUNNING && snake.best == 10);
    /* Entering the old tail is legal if no food is eaten. */
    snake.length = 4;
    snake.body[0] = 21;
    snake.body[1] = 22;
    snake.body[2] = 42;
    snake.body[3] = 41;
    snake.direction = 2;
    snake.next_direction = 2;
    snake.food = 100;
    snake.accumulated_ms = 200;
    in = (joystick_input_t){0};
    snake_process(&snake, 260, &in, false);
    assert(snake.phase == MINI_RUNNING && snake.body[0] == 41);
    /* Fill the last free cell, including the food scan's bounded termination. */
    snake_init(&snake, 0, 0);
    snake.length = SNAKE_CELLS - 1;
    snake.body[0] = 0;
    for (unsigned i = 1; i < SNAKE_CELLS - 1; ++i)
        snake.body[i] = i + 1;
    snake.food = 1;
    snake.accumulated_ms = 200;
    snake_process(&snake, 20, &in, false);
    assert(snake.length == SNAKE_CELLS && snake.phase == MINI_WON);

    breakout_model_t b;
    breakout_init(&b, 0, 0);
    breakout_process(&b, 20, &in, true);
    assert(b.phase == MINI_RUNNING);
    b.ball_x_fp = 4 * 256;
    b.ball_y_fp = 150 * 256;
    b.vx = -200;
    b.vy = 0;
    breakout_process(&b, 40, &in, false);
    assert(b.vx > 0 && b.ball_x_fp >= 3 * 256);
    memset(b.bricks, 0, sizeof(b.bricks));
    b.bricks[0] = 1;
    b.bricks[39] = 1;
    b.ball_x_fp = (6 + 14) * 256;
    b.ball_y_fp = 66 * 256;
    b.vx = 0;
    b.vy = -200;
    breakout_process(&b, 60, &in, false);
    assert(!b.bricks[0] && b.score == 10 && b.vy > 0);
    b.ball_x_fp = (b.paddle_fp / 256 + 24) * 256;
    b.ball_y_fp = 209 * 256;
    b.vy = 200;
    breakout_process(&b, 80, &in, false);
    assert(b.vy < 0);
    b.ball_y_fp = 231 * 256;
    b.ball_x_fp = 2 * 256;
    b.vy = 200;
    breakout_process(&b, 100, &in, false);
    assert(b.lives == 2 && b.phase == MINI_SERVE);
    in.x = 1000;
    breakout_process(&b, 160, &in, false);
    assert(b.paddle_fp > 136 * 256);
    for (unsigned i = 0; i < 40; ++i)
        b.bricks[i] = 0;
    b.phase = MINI_RUNNING;
    breakout_process(&b, 180, &in, false);
    assert(b.level == 2 && b.phase == MINI_SERVE && b.lives == 2);
}

int main(int argc, char **argv)
{
    models();
    const char *dir = argc > 1 ? argv[1] : NULL;
    console_model_t c, old;
    joystick_input_t in = {0};
    uint8_t mono[1024];
    console_status_t status;
    console_model_init(&c, 0);
    compare(&c, NULL);
    ppm(dir, "arcade_menu", NULL);
    unsigned initial_uploads = uploads;
    assert(console_scene_render(&c, &c) && uploads == initial_uploads);
    old = c;
    c.selected = ARCADE_SNAKE;
    uint32_t initial_pixels = pixels_written;
    assert(console_scene_render(&c, &old));
    assert(pixels_written - initial_pixels < 320U * 120U); /* Selection is a partial repaint. */
    c = old;
    console_model_status(&c, &status);
    console_oled_render(&status, mono);
    ppm(dir, "oled_menu", mono);
    assert(oled_init(OLED_SSD1306) == I2C_STATUS_OK && oled_address() == 0x3D);
    assert(probes == 2 && seen_charge && seen_page_mode && !seen_dc && column == 0 &&
           data_pages == 8);
    assert(oled_present(mono) == I2C_STATUS_OK && memcmp(mono, device_pages, 1024) == 0);
    unsigned before = writes;
    assert(oled_present(mono) == I2C_STATUS_OK && writes == before);
    mono[130] ^= 1;
    assert(oled_present(mono) == I2C_STATUS_OK && writes == before + 2);
    fail_i2c = true;
    mono[0] ^= 1;
    assert(oled_present(mono) == I2C_STATUS_NACK);
    fail_i2c = false;
    before = data_pages;
    assert(oled_present(mono) == I2C_STATUS_OK && data_pages == before + 8);
    seen_charge = false;
    seen_page_mode = false;
    assert(oled_init(OLED_SH1106) == I2C_STATUS_OK && seen_dc && !seen_charge && !seen_page_mode &&
           column == 2);
    acknowledged = 0;
    assert(oled_init(OLED_SSD1306) == I2C_STATUS_NACK);
    acknowledged = 0x3C;

    for (unsigned game = 0; game < ARCADE_GAME_COUNT; ++game) {
        console_model_init(&c, 0);
        c.selected = (arcade_game_t)game;
        in = (joystick_input_t){0};
        press(&in, 10, 0);
        console_model_process(&c, 10, &in);
        if (game == ARCADE_BREAKOUT) {
            press(&in, 30, 0);
            console_model_process(&c, 30, &in);
        }
        compare(&c, NULL);
        ppm(dir,
            game == 0   ? "arcade_coin_dash"
            : game == 1 ? "arcade_snake"
                        : "arcade_breakout",
            NULL);
        for (unsigned frame = 0; frame < 100; ++frame) {
            old = c;
            for (unsigned skip = 0; skip < 3; ++skip) {
                uint32_t t = 50 + frame * 60 + skip * 20;
                in.x = (frame % 2) ? 700 : -700;
                in.y = (frame % 4 < 2) ? 600 : -600;
                if (frame % 13 == 0)
                    press(&in, t, 0);
                console_model_process(&c, t, &in);
            }
            compare(&c, &old);
        }
        old = c;
        press(&in, 7000, 2);
        console_model_process(&c, 7000, &in);
        compare(&c, &old);
        if (game == 0) {
            ppm(dir, "arcade_paused", NULL);
            console_model_status(&c, &status);
            console_oled_render(&status, mono);
            ppm(dir, "oled_paused", mono);
        }
        old = c;
        in.y = 1000;
        console_model_process(&c, 7010, &in);
        compare(&c, &old);
    }
    old = c;
    c.pause_item = (c.pause_item + 1) % 3;
    fail_draw = true;
    assert(!console_scene_render(&c, &old));
    assert(max_snake_dirty < 20000 && max_breakout_dirty < 30000);
    printf(
        "PASS: arcade catalog/pause/resume/input age/wrap, original runner integration, snake growth/reverse/tail/win, "
        "breakout walls/bricks/paddle/lives/levels, 300 skipped-frame pixel comparisons, "
        "SSD1306/SH1106 ACK/page-cache/error recovery; %u uploads %u pixels\n",
        uploads, pixels_written);
    printf("Arcade dirty-frame maxima: snake %u / breakout %u pixels (full screen 76800)\n",
           max_snake_dirty, max_breakout_dirty);
    return 0;
}
