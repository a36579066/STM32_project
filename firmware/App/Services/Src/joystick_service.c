#include "joystick_service.h"
#include "joystick_config.h"

#include <stddef.h>

#include "adc.h"
#include "board.h"
#include "cpu.h"
#include "io_wait.h"
#include "timebase.h"

#define JOYSTICK_CALIBRATION_SAMPLES 32U
#define JOYSTICK_DEADZONE            180U

static uint16_t center_x;
static uint16_t center_y;
static bool initialized;
static bool raw_button_pressed;
static bool stable_button_pressed;
static uint32_t button_changed_ms;
static bool button_sampled;
static uint32_t calibration_count;
static uint32_t sum_x;
static uint32_t sum_y;
static volatile bool calibrated;
static volatile uint32_t sampled_at_ms;
static volatile joystick_input_t latest_input;

typedef struct {
    bool sampled, raw, stable;
    uint32_t changed_ms;
} extra_button_t;

static extra_button_t button_b, button_menu;

static bool debounce_extra(extra_button_t *button, bool pressed, uint32_t now,
                           volatile uint32_t *sequence, volatile uint32_t *pressed_at)
{
    if (!button->sampled) {
        *button =
            (extra_button_t){.sampled = true, .raw = pressed, .stable = pressed, .changed_ms = now};
    }
    if (pressed != button->raw) {
        button->raw = pressed;
        button->changed_ms = now;
    } else if (pressed != button->stable && (uint32_t)(now - button->changed_ms) >= 25U) {
        button->stable = pressed;
        if (pressed) {
            ++*sequence;
            *pressed_at = now;
        }
    }
    return button->stable;
}

static int16_t normalize_axis(uint16_t raw, uint16_t center, bool invert)
{
    const int32_t delta = (int32_t)raw - (int32_t)center;
    const uint32_t magnitude = (uint32_t)((delta < 0) ? -delta : delta);
    if (magnitude <= JOYSTICK_DEADZONE) {
        return 0;
    }

    const int32_t span = (delta < 0) ? center : (4095 - (int32_t)center);
    if (span <= (int32_t)JOYSTICK_DEADZONE) {
        return 0;
    }

    int32_t scaled =
        (int32_t)(magnitude - JOYSTICK_DEADZONE) * 1000 / (span - (int32_t)JOYSTICK_DEADZONE);
    if (scaled > 1000) {
        scaled = 1000;
    }
    if (delta < 0) {
        scaled = -scaled;
    }
    if (invert) {
        scaled = -scaled;
    }
    return (int16_t)scaled;
}

/* Runs once per complete X/Y pair in DMA IRQ context, including while the
   foreground is sleeping through a TFT transfer. No UART or display calls. */
static void on_adc_sample(uint16_t raw_x, uint16_t raw_y, uint32_t now)
{
    if (!calibrated) {
        sum_x += raw_x;
        sum_y += raw_y;
        if (++calibration_count == JOYSTICK_CALIBRATION_SAMPLES) {
            center_x = (uint16_t)(sum_x / JOYSTICK_CALIBRATION_SAMPLES);
            center_y = (uint16_t)(sum_y / JOYSTICK_CALIBRATION_SAMPLES);
            calibrated = true;
        }
    }
    latest_input.raw_x = raw_x;
    latest_input.raw_y = raw_y;
    latest_input.x = calibrated ? normalize_axis(raw_x, center_x, JOYSTICK_INVERT_X) : 0;
    latest_input.y = calibrated ? normalize_axis(raw_y, center_y, JOYSTICK_INVERT_Y) : 0;

    const bool sampled_pressed = board_joystick_button_is_pressed();
    if (!button_sampled) {
        raw_button_pressed = sampled_pressed;
        stable_button_pressed = sampled_pressed;
        button_sampled = true;
        button_changed_ms = now;
    }
    if (sampled_pressed != raw_button_pressed) {
        raw_button_pressed = sampled_pressed;
        button_changed_ms = now;
    } else if ((sampled_pressed != stable_button_pressed) &&
               ((uint32_t)(now - button_changed_ms) >= 25U)) {
        stable_button_pressed = sampled_pressed;
        if (stable_button_pressed) {
            ++latest_input.press_sequence;
            latest_input.pressed_at_ms = now;
        }
    }
    latest_input.pressed = stable_button_pressed;
    latest_input.b_pressed =
        debounce_extra(&button_b, board_game_b_is_pressed(), now, &latest_input.b_press_sequence,
                       &latest_input.b_pressed_at_ms);
    latest_input.menu_pressed =
        debounce_extra(&button_menu, board_game_menu_is_pressed(), now,
                       &latest_input.menu_press_sequence, &latest_input.menu_pressed_at_ms);
    sampled_at_ms = now;
}

bool joystick_service_init(uint32_t timer_clock_hz)
{
    initialized = false;
    calibrated = false;
    button_sampled = false;
    button_b = (extra_button_t){0};
    button_menu = (extra_button_t){0};
    calibration_count = 0U;
    sum_x = 0U;
    sum_y = 0U;
    latest_input = (joystick_input_t){0};
    board_joystick_init();
    if (adc1_start(timer_clock_hz, on_adc_sample) != ADC_STATUS_OK) {
        return false;
    }
    const uint32_t start = timebase_millis();
    /* 32 hardware samples at 100 Hz take about 320 ms; sleep between IRQs. */
    while (!calibrated) {
        if ((adc1_status() != ADC_STATUS_OK) || ((uint32_t)(timebase_millis() - start) >= 500U)) {
            adc1_stop();
            return false;
        }
        const uint32_t mask = cpu_irq_save();
        if (!calibrated) {
            io_wait_locked(IO_WAIT_ADC, mask, 10U);
        } else
            cpu_irq_restore(mask);
    }
    initialized = true;
    return true;
}

bool joystick_service_read(joystick_input_t *input)
{
    if (!initialized || (input == NULL) || (adc1_status() != ADC_STATUS_OK)) {
        return false;
    }
    const uint32_t mask = cpu_irq_save();
    *input = latest_input; /* One coherent snapshot of ISR-owned data. */
    const uint32_t sampled_ms = sampled_at_ms;
    cpu_irq_restore(mask);
    if ((uint32_t)(timebase_millis() - sampled_ms) > 50U) {
        return false;
    }
    return true;
}
