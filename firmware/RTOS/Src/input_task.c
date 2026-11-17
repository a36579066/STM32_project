#include "app_rtos_private.h"
#include "board.h"
#include "joystick_service.h"
#include "queue.h"
#include "task.h"
#include "timebase.h"

static void post_input(rtos_log_event_type_t type, const joystick_input_t *input)
{
    const rtos_log_event_t event = {
        .type = type,
        .input = *input,
        .value0 = board_joystick_button_is_pressed() ? 0U : 1U,
    };
    app_rtos_post_event(&event);
}

void input_task_entry(void *argument)
{
    (void)argument;
    board_button_init();
    app_rtos_post_log(RTOS_LOG_CALIBRATING, 0U, 0U, 0U);
    if (!joystick_service_init(SystemCoreClock / 2U)) {
        app_rtos_post_log(RTOS_LOG_JOYSTICK_INIT_FAILURE, 0U, 0U, 0U);
        for (;;)
            vTaskSuspend(NULL);
    }

    app_rtos_post_log(RTOS_LOG_JOYSTICK_READY, 0U, 0U, 0U);
    joystick_input_t current = {0}, previous = {0};
    (void)joystick_service_read(&current);
    (void)xQueueOverwrite(app_input_snapshot_queue, &current);
    (void)xTaskNotify(app_game_task_handle, APP_READY_INPUT, eSetBits);
    uint32_t last_report = timebase_millis();
    bool fault = false;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        joystick_input_t sampled;
        if (joystick_service_read(&sampled)) {
            current = sampled;
            fault = false;
            if (current.press_sequence != previous.press_sequence ||
                current.pressed != previous.pressed ||
                current.b_press_sequence != previous.b_press_sequence ||
                current.b_pressed != previous.b_pressed ||
                current.menu_press_sequence != previous.menu_press_sequence ||
                current.menu_pressed != previous.menu_pressed)
                post_input(RTOS_LOG_JOYSTICK_BUTTON, &current);
            previous = current;
        } else {
            if (!fault)
                app_rtos_post_log(RTOS_LOG_JOYSTICK_FAULT, 0U, 0U, 0U);
            fault = true;
            current.x = 0;
            current.y = 0;
            current.pressed = false;
            current.b_pressed = false;
            current.menu_pressed = false;
        }

        (void)xQueueOverwrite(app_input_snapshot_queue, &current);
        uint32_t edge;

        while (xQueueReceive(app_button_event_queue, &edge, 0U) == pdPASS)
            app_rtos_post_log(RTOS_LOG_BUTTON_EDGE, 0U, 0U, 0U);

        const uint32_t dropped = app_rtos_take_dropped_button_count();
        if (dropped > 0U)
            app_rtos_post_log(RTOS_LOG_BUTTON_DROPPED, dropped, 0U, 0U);
        const uint32_t now = timebase_millis();

        if ((uint32_t)(now - last_report) >= 500U) {
            last_report = now;
            post_input(RTOS_LOG_JOYSTICK_SAMPLE, &current);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10U));
    }
}
