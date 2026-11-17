#include "app_rtos_private.h"
#include <stdbool.h>
#include "board.h"
#include "console_scene.h"
#include "spi.h"
#include "task.h"
#include "timebase.h"

static void stop_display(rtos_log_event_type_t type)
{
    rtos_log_event_t event = {.type = type};
    const display_service_error_t *error = display_service_last_error();
    if (error != NULL)
        event.display_error = *error;
    app_rtos_post_event(&event);
    vTaskSuspend(app_game_task_handle);
    for (;;)
        vTaskSuspend(NULL);
}

void display_task_entry(void *argument)
{
    (void)argument;
    board_tft_pins_init();
    spi1_init(2U, false, false);
    spi1_dma_init();
    if (!display_service_init(timebase_delay_ms, timebase_millis))
        stop_display(RTOS_LOG_TFT_INIT_FAILURE);
    display_benchmark_result_t benchmark;
    if (!display_service_run_startup_benchmark(&benchmark))
        stop_display(RTOS_LOG_TFT_BENCHMARK_FAILURE);
    app_rtos_post_log(RTOS_LOG_TFT_BENCHMARK, benchmark.average_upload_tenths_ms,
                      benchmark.upload_rate_tenths_fps, benchmark.elapsed_ms);
    (void)xTaskNotify(app_game_task_handle, APP_READY_DISPLAY, eSetBits);

    /* The erasure reference is always the last frame actually drawn, even
       when a newer queue snapshot replaces one that was never displayed. */
    static console_model_t previous, next;
    bool have_previous = false;
    for (;;) {
        if (xQueueReceive(app_render_snapshot_queue, &next, portMAX_DELAY) != pdPASS)
            continue;
        if (!console_scene_render(&next, have_previous ? &previous : NULL))
            stop_display(have_previous ? RTOS_LOG_TFT_RENDER_FAILURE : RTOS_LOG_GAME_INIT_FAILURE);
        previous = next;
        if (!have_previous) {
            app_rtos_post_log(RTOS_LOG_GAME_READY, 0U, 0U, 0U);
            app_rtos_post_log(RTOS_LOG_BOOT, 0U, 0U, 0U);
        }
        have_previous = true;
    }
}
