#ifndef CONSOLE_JOYSTICK_SERVICE_H
#define CONSOLE_JOYSTICK_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t raw_x; /* 0..4095 ADC counts */
    uint16_t raw_y; /* 0..4095 ADC counts */
    int16_t x;      /* -1000 left, +1000 right */
    int16_t y;      /* -1000 up, +1000 down */
    bool pressed;
    uint32_t press_sequence; /* Debounced press count, retained across TFT uploads. */
    uint32_t pressed_at_ms;  /* Time of the latest debounced press. */
    bool b_pressed;
    bool menu_pressed;
    uint32_t b_press_sequence;
    uint32_t b_pressed_at_ms;
    uint32_t menu_press_sequence;
    uint32_t menu_pressed_at_ms;
} joystick_input_t;

bool joystick_service_init(uint32_t timer_clock_hz);
bool joystick_service_read(joystick_input_t *input);

#endif /* CONSOLE_JOYSTICK_SERVICE_H */
