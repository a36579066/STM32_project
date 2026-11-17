#include "app_rtos_private.h"
#include "task.h"
#include "timebase.h"

static void publish(const console_model_t *state)
{
    console_status_t status;
    console_model_status(state, &status);
    (void)xQueueOverwrite(app_render_snapshot_queue, state);
    (void)xQueueOverwrite(app_oled_status_queue, &status);
}

void game_task_entry(void *argument)
{
    (void)argument;
    uint32_t ready = 0;
    while ((ready & APP_READY_ALL) != APP_READY_ALL) {
        uint32_t bits = 0;
        (void)xTaskNotifyWait(0, UINT32_MAX, &bits, portMAX_DELAY);
        ready |= bits;
    }
    static console_model_t state;
    console_model_init(&state, timebase_millis());
    publish(&state);
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
        joystick_input_t input = {0};
        if (xQueuePeek(app_input_snapshot_queue, &input, 0) == pdPASS &&
            console_model_process(&state, timebase_millis(), &input))
            publish(&state);
    }
}
