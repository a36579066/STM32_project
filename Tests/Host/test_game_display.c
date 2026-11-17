#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "display_service.h"
#include "game.h"
#include "game_config.h"
#include "game_model.h"
#include "game_scene.h"

/* Only the host owns a full framebuffer. Firmware composes 1280-pixel tiles. */
static uint16_t framebuffer[DISPLAY_SERVICE_HEIGHT][DISPLAY_SERVICE_WIDTH];
static uint16_t reference[DISPLAY_SERVICE_HEIGHT][DISPLAY_SERVICE_WIDTH];
static uint32_t uploaded_pixels;
static bool fail_upload;

bool display_service_draw_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                 const uint8_t *pixels)
{
    assert(pixels != NULL && w > 0 && h > 0);
    assert((uint32_t)x + w <= DISPLAY_SERVICE_WIDTH);
    assert((uint32_t)y + h <= DISPLAY_SERVICE_HEIGHT);
    if (fail_upload)
        return false;
    uploaded_pixels += (uint32_t)w * h;
    for (uint16_t row = 0; row < h; ++row) {
        for (uint16_t column = 0; column < w; ++column) {
            const uint32_t i = ((uint32_t)row * w + column) * 2U;
            framebuffer[y + row][x + column] = (uint16_t)(pixels[i] << 8U | pixels[i + 1]);
        }
    }
    return true;
}

static void start(game_model_t *g, joystick_input_t *input, uint32_t now)
{
    memset(input, 0, sizeof(*input));
    game_model_init(g, now);
    input->pressed = true;
    input->press_sequence = 1;
    input->pressed_at_ms = now;
    assert(game_model_process(g, now, input));
    assert(g->mode == GAME_RUNNING && !g->jumping && g->lives == 3);
    input->pressed = false;
    assert(!game_model_process(g, now + 1U, input));
}

static void quiet(game_model_t *g)
{
    memset(g->objects, 0, sizeof(g->objects));
    g->spawn_ms = 1000000U;
}

static void press(joystick_input_t *input, uint32_t now)
{
    input->pressed = true;
    ++input->press_sequence;
    input->pressed_at_ms = now;
}

static void object_at_hero(game_model_t *g, game_object_kind_t kind)
{
    g->objects[0] = (game_object_t){
        .x_fp = g->hero_x_fp + 5 * GAME_FP_ONE,
        .y = kind == GAME_COIN ? 178 : 170,
        .width = kind == GAME_COIN ? 10 : 24,
        .height = kind == GAME_COIN ? 12 : 30,
        .kind = kind,
        .active = true,
    };
}

static void test_input_and_jump(void)
{
    game_model_t g;
    joystick_input_t input = {.x = 1000, .y = 1000};
    game_model_init(&g, 0);
    assert(!game_model_process(&g, 1000, &input));
    assert(g.mode == GAME_READY); /* Axes never start or restart the game. */
    input.press_sequence = 3;
    input.pressed_at_ms = 0;
    assert(!game_model_process(&g, 1000, &input)); /* Stale startup press. */

    for (uint32_t phase = 1; phase < GAME_FRAME_INTERVAL_MS; ++phase) {
        start(&g, &input, 0);
        quiet(&g);
        press(&input, phase);
        assert(!game_model_process(&g, phase, &input));
        input.pressed = false;
        assert(!game_model_process(&g, phase, &input));
        assert(game_model_process(&g, 20, &input));
        assert(g.jumping);
        assert(game_model_process(&g, 40, &input));
        assert(g.hero_y < (int16_t)(GAME_GROUND_Y - GAME_HERO_HEIGHT));
    }

    /* A completed pulse is retained even if no foreground call saw it held. */
    start(&g, &input, 0);
    quiet(&g);
    press(&input, 5);
    input.pressed = false;
    assert(game_model_process(&g, 20, &input) && g.jumping);
    input.y = 1000;
    for (uint32_t now = 40; now <= 1000; now += 20) {
        assert(game_model_process(&g, now, &input));
        if (now == 300)
            assert(g.hero_y == (int16_t)(GAME_GROUND_Y - GAME_HERO_HEIGHT - GAME_JUMP_HEIGHT));
    }
    assert(!g.jumping); /* Y-axis cannot generate phantom jumps. */

    start(&g, &input, 0);
    quiet(&g);
    press(&input, 20);
    for (uint32_t now = 20; now <= 1000; now += 20)
        assert(game_model_process(&g, now, &input));
    assert(!g.jumping); /* Holding a button does not repeatedly jump. */

    start(&g, &input, 0);
    quiet(&g);
    for (uint32_t now = 20; now <= 600; now += 20) {
        input.pressed = false;
        if (now == 20 || now == 480)
            press(&input, now);
        assert(game_model_process(&g, now, &input));
        if (now == 580)
            assert(g.jumping && g.jump_started_ms == 580);
    }
    start(&g, &input, 0);
    quiet(&g);
    for (uint32_t now = 20; now <= 600; now += 20) {
        input.pressed = false;
        if (now == 20 || now == 300)
            press(&input, now);
        assert(game_model_process(&g, now, &input));
    }
    assert(!g.jumping && !g.jump_pending); /* Expired airborne press is ignored. */

    const uint32_t origin = UINT32_MAX - 50U;
    start(&g, &input, origin);
    quiet(&g);
    press(&input, origin + 3U);
    assert(!game_model_process(&g, origin + 3U, &input));
    input.pressed = false;
    for (uint32_t elapsed = 20; elapsed <= 600; elapsed += 20) {
        assert(game_model_process(&g, origin + elapsed, &input));
    }
    assert(g.run_ms == 600 && !g.jumping); /* Millisecond counter wrap. */
}

static void test_movement(void)
{
    game_model_t g;
    joystick_input_t input;
    start(&g, &input, 0);
    quiet(&g);
    const int32_t initial_x = g.hero_x_fp;
    input.x = 10;
    for (uint32_t now = 20; now <= 1000; now += 20)
        assert(game_model_process(&g, now, &input));
    assert(g.hero_x_fp > initial_x + GAME_FP_ONE);
    input.x = -1000;
    for (uint32_t now = 1020; now <= 3000; now += 20)
        assert(game_model_process(&g, now, &input));
    assert(g.hero_x_fp == (int32_t)(GAME_HERO_MIN_X * GAME_FP_ONE));
    input.x = 1000;
    for (uint32_t now = 3020; now <= 5000; now += 20)
        assert(game_model_process(&g, now, &input));
    assert(g.hero_x_fp == (int32_t)((DISPLAY_SERVICE_WIDTH - GAME_HERO_WIDTH) * GAME_FP_ONE));
    assert(game_model_process(&g, 10000, &input));
    assert(g.run_ms == 5060); /* A slow upload cannot teleport the world. */
}

static void test_coins_damage_retry(void)
{
    game_model_t g;
    joystick_input_t input;
    start(&g, &input, 0);
    quiet(&g);
    for (uint32_t now = 20; now <= 200; now += 20) {
        object_at_hero(&g, GAME_COIN);
        assert(game_model_process(&g, now, &input));
    }
    assert(g.coins == 10 && g.combo == 5 && g.shield && g.score >= 400);
    object_at_hero(&g, GAME_CRATE);
    assert(game_model_process(&g, 220, &input));
    assert(g.lives == 3 && !g.shield && g.invulnerable_ms > 0 && g.combo == 0);
    assert(g.toast == GAME_TOAST_BLOCKED);
    for (uint32_t now = 240; now <= 1000; now += 20) {
        object_at_hero(&g, GAME_CRATE);
        assert(game_model_process(&g, now, &input));
    }
    assert(g.lives == 3); /* Grace period prevents repeated contact damage. */
    for (uint32_t hit = 0; hit < 3; ++hit) {
        g.invulnerable_ms = 0;
        object_at_hero(&g, GAME_CRATE);
        assert(game_model_process(&g, 1020 + hit * 20, &input));
        assert(g.lives == 2 - hit);
    }
    assert(g.mode == GAME_OVER && g.best_score == g.score);
    const uint32_t best = g.best_score;
    input.x = 1000;
    assert(!game_model_process(&g, 1100, &input));
    press(&input, 1101);
    assert(game_model_process(&g, 1101, &input));
    assert(g.mode == GAME_RUNNING && g.lives == 3 && g.score == 0);
    assert(g.best_score == best && !g.shield && !g.jumping);

    quiet(&g);
    input.pressed = false;
    input.x = 0;
    object_at_hero(&g, GAME_COIN);
    assert(game_model_process(&g, 1121, &input));
    assert(g.combo == 1);
    for (uint32_t now = 1141; now <= 3001; now += 20)
        assert(game_model_process(&g, now, &input));
    assert(g.combo == 0);
    object_at_hero(&g, GAME_CRATE);
    g.objects[0].kind = GAME_DRONE;
    g.objects[0].y = 148;
    g.objects[0].height = 16;
    assert(game_model_process(&g, 3021, &input));
    assert(g.lives == 3); /* Walk underneath a drone without jumping. */
    g.hero_y = 130;
    g.jumping = true;
    g.jump_started_ms = g.run_ms - 80;
    object_at_hero(&g, GAME_CRATE);
    g.objects[0].kind = GAME_DRONE;
    g.objects[0].y = 148;
    g.objects[0].height = 16;
    assert(game_model_process(&g, 3041, &input));
    assert(g.lives == 2); /* Jumping blindly into a drone is dangerous. */
}

static void same_as_full_render(const game_model_t *g)
{
    memcpy(reference, framebuffer, sizeof(reference));
    assert(game_scene_render(g, NULL));
    if (memcmp(reference, framebuffer, sizeof(reference)) != 0) {
        for (uint32_t y = 0; y < DISPLAY_SERVICE_HEIGHT; ++y) {
            for (uint32_t x = 0; x < DISPLAY_SERVICE_WIDTH; ++x) {
                if (reference[y][x] != framebuffer[y][x]) {
                    fprintf(stderr, "Dirty render mismatch at %u,%u, run=%u: %04x != %04x\n", x, y,
                            g->run_ms, reference[y][x], framebuffer[y][x]);
                    assert(false);
                }
            }
        }
    }
}

static void test_shield_and_hit_render(void)
{
    game_model_t g, old;
    joystick_input_t input;
    start(&g, &input, 0);
    quiet(&g);
    g.coins = GAME_COINS_PER_SHIELD - 1U;
    object_at_hero(&g, GAME_COIN);
    assert(game_model_process(&g, 20, &input));
    assert(g.shield && g.coins == GAME_COINS_PER_SHIELD);

    /* A charged shield changes the HUD, not the artwork around the hero. */
    old = g;
    old.shield = false;
    assert(game_scene_render(&old, NULL));
    memcpy(reference, framebuffer, sizeof(reference));
    assert(game_scene_render(&g, &old));
    const size_t hud_bytes = GAME_HUD_HEIGHT * sizeof(framebuffer[0]);
    assert(memcmp(reference, framebuffer, hud_bytes) != 0);
    assert(memcmp(reference[GAME_HUD_HEIGHT], framebuffer[GAME_HUD_HEIGHT],
                  sizeof(framebuffer) - hud_bytes) == 0);
    same_as_full_render(&g);

    old = g;
    object_at_hero(&g, GAME_CRATE);
    assert(game_model_process(&g, 40, &input));
    assert(!g.shield && g.lives == GAME_START_LIVES);
    assert(g.invulnerable_ms == GAME_HIT_GRACE_MS);
    assert(game_scene_render(&g, &old));
    same_as_full_render(&g);

    /* Exercise real damage, all blink phases, and their eventual cleanup. */
    for (uint32_t hit = 0; hit < 2; ++hit) {
        quiet(&g);
        for (uint32_t elapsed = 0; elapsed < GAME_HIT_GRACE_MS; elapsed += 20) {
            old = g;
            assert(game_model_process(&g, g.last_update_ms + 20U, &input));
            assert(game_scene_render(&g, &old));
            if (elapsed % 80U == 0 || g.invulnerable_ms == 0)
                same_as_full_render(&g);
        }
        assert(g.invulnerable_ms == 0 && !g.shield);
        if (hit == 0) {
            old = g;
            object_at_hero(&g, GAME_CRATE);
            assert(game_model_process(&g, g.last_update_ms + 20U, &input));
            assert(g.lives == GAME_START_LIVES - 1U && !g.shield);
            assert(g.invulnerable_ms == GAME_HIT_GRACE_MS);
            assert(game_scene_render(&g, &old));
            same_as_full_render(&g);
        }
    }
}

static void save_ppm(const char *dir, const char *name)
{
    if (dir == NULL)
        return;
    char path[1024];
    assert(snprintf(path, sizeof(path), "%s/%s.ppm", dir, name) < (int)sizeof(path));
    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    fprintf(f, "P6\n%u %u\n255\n", DISPLAY_SERVICE_WIDTH, DISPLAY_SERVICE_HEIGHT);
    for (uint32_t y = 0; y < DISPLAY_SERVICE_HEIGHT; ++y) {
        for (uint32_t x = 0; x < DISPLAY_SERVICE_WIDTH; ++x) {
            const uint16_t c = framebuffer[y][x];
            const unsigned char rgb[3] = {
                (unsigned char)(((c >> 11U) & 31U) * 255U / 31U),
                (unsigned char)(((c >> 5U) & 63U) * 255U / 63U),
                (unsigned char)((c & 31U) * 255U / 31U),
            };
            assert(fwrite(rgb, 1, 3, f) == 3);
        }
    }
    assert(fclose(f) == 0);
}

static void runner_input(const game_model_t *g, joystick_input_t *input, uint32_t now)
{
    input->pressed = false;
    if (g->jumping)
        return;
    const int32_t hero_front = g->hero_x_fp / GAME_FP_ONE + GAME_HERO_WIDTH - 5;
    for (uint32_t i = 0; i < GAME_OBJECT_CAPACITY; ++i) {
        const game_object_t *o = &g->objects[i];
        const int32_t gap = o->x_fp / GAME_FP_ONE + 3 - hero_front;
        if (o->active && !o->passed && (o->kind == GAME_CRATE || o->kind == GAME_SPIKES) &&
            gap <= (int32_t)g->speed / 8 && gap >= -8) {
            press(input, now);
            break;
        }
    }
}

static void test_uneven_frame_times(void)
{
    for (uint32_t seed = 1; seed <= 3; ++seed) {
        game_model_t g;
        joystick_input_t input;
        start(&g, &input, seed * 17);
        uint32_t now = seed * 17;
        for (uint32_t frame = 0; g.run_ms < 120000; ++frame) {
            now += 20 + (frame * 13 + seed) % 41; /* 20..60 ms transfer intervals. */
            runner_input(&g, &input, now);
            assert(game_model_process(&g, now, &input));
            assert(g.mode == GAME_RUNNING && g.lives == 3);
        }
        assert(g.level == GAME_MAX_LEVEL && g.coins > 10);
    }
}

static void test_long_run_and_render(const char *preview_dir)
{
    game_model_t g, old;
    joystick_input_t input;
    game_model_init(&g, 0);
    assert(game_scene_render(&g, NULL));
    save_ppm(preview_dir, "coin_dash_title");
    old = g;
    start(&g, &input, 0);
    assert(game_scene_render(&g, &old));
    bool saved_run = false;
    uint32_t max_pixels = 0;
    uint64_t total_pixels = 0;
    for (uint32_t now = 20; now <= 120000; now += 20) {
        old = g;
        runner_input(&g, &input, now);
        assert(game_model_process(&g, now, &input));
        if (g.mode != GAME_RUNNING || g.lives != 3) {
            fprintf(stderr, "Autoplay hit at %u ms; y=%d jump=%u lives=%u speed=%u\n", now,
                    g.hero_y, (unsigned)g.jumping, g.lives, g.speed);
            for (uint32_t i = 0; i < GAME_OBJECT_CAPACITY; ++i) {
                const game_object_t *o = &g.objects[i];
                if (o->active)
                    fprintf(stderr, " object %u kind=%u x=%ld y=%d passed=%u\n", i, o->kind,
                            (long)(o->x_fp / GAME_FP_ONE), o->y, (unsigned)o->passed);
            }
        }
        assert(g.mode == GAME_RUNNING && g.lives == 3);
        /* Full renderer comparison checks clipping, erasure, overlaps and UI. */
        uploaded_pixels = 0;
        assert(game_scene_render(&g, &old));
        if (uploaded_pixels > max_pixels)
            max_pixels = uploaded_pixels;
        total_pixels += uploaded_pixels;
        assert(uploaded_pixels < DISPLAY_SERVICE_WIDTH * DISPLAY_SERVICE_HEIGHT / 2U);
        if (now <= 1000 || now % 740 == 0)
            same_as_full_render(&g);
        if (!saved_run && g.coins >= 10 && g.jumping && g.hero_y < 130) {
            save_ppm(preview_dir, "coin_dash_run");
            saved_run = true;
        }
    }
    assert(g.coins > 10 && g.level == GAME_MAX_LEVEL && g.waves > 60 && saved_run);
    printf("Game: 120 s autoplay, %u waves, %u coins, level %u, all 3 lives retained\n", g.waves,
           g.coins, g.level);
    printf("Renderer: max %u pixels per sampled frame; first-pass total %llu pixels\n", max_pixels,
           (unsigned long long)total_pixels);

    const game_model_t completed_run = g;
    /* Deliberately cross boundaries and change every visible HUD/overlay field. */
    for (uint32_t frame = 0; frame < 300; ++frame) {
        old = g;
        g.run_ms += 20;
        g.hero_x_fp = (int32_t)(GAME_HERO_MIN_X + frame % (DISPLAY_SERVICE_WIDTH - GAME_HERO_WIDTH -
                                                           GAME_HERO_MIN_X + 1U)) *
                      GAME_FP_ONE;
        g.hero_y = (int16_t)(96 + frame % 73);
        g.score += 37;
        g.best_score = g.score + 17;
        g.coins += frame % 3;
        g.combo = (uint8_t)(frame % 6);
        g.shield = frame % 2 == 0;
        g.lives = (uint8_t)(frame % 4);
        g.invulnerable_ms = frame % 3 == 0 ? 300 : 0;
        g.toast = (game_toast_t)(frame % 7);
        g.toast_ms = g.toast == GAME_TOAST_NONE ? 0 : 500;
        for (uint32_t i = 0; i < GAME_OBJECT_CAPACITY; ++i) {
            g.objects[i] = (game_object_t){
                .x_fp = ((int32_t)((frame * 7 + i * 37) % 390) - 40) * GAME_FP_ONE,
                .y = (int16_t)(120 + (i % 3) * 24),
                .width = 28,
                .height = 16,
                .kind = (game_object_kind_t)(i % 4),
                .active = (frame + i) % 3 != 0,
            };
        }
        assert(game_scene_render(&g, &old));
        same_as_full_render(&g);
    }
    old = g;
    g = completed_run;
    g.mode = GAME_OVER;
    g.lives = 0;
    g.shield = false;
    g.best_score = g.score;
    assert(game_scene_render(&g, &old));
    same_as_full_render(&g);
    save_ppm(preview_dir, "coin_dash_over");
}

static void test_upload_failure(void)
{
    fail_upload = true;
    assert(!game_init(0));
    fail_upload = false;
    assert(game_init(0));
    joystick_input_t input = {.pressed = true, .press_sequence = 1, .pressed_at_ms = 20};
    fail_upload = true;
    assert(!game_process(20, &input));
    fail_upload = false;
    assert(game_init(40));
    assert(game_process(40, NULL));
}

int main(int argc, char **argv)
{
    test_input_and_jump();
    test_movement();
    test_coins_damage_retry();
    test_shield_and_hit_render();
    test_uneven_frame_times();
    test_long_run_and_render(argc > 1 ? argv[1] : NULL);
    test_upload_failure();
    puts("Game/display host regressions passed");
    return 0;
}
