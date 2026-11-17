#ifndef CONSOLE_APP_RTOS_PRIVATE_H
#define CONSOLE_APP_RTOS_PRIVATE_H
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "display_service.h"
#include "console_model.h"

#define APP_READY_INPUT   (1UL << 0U)
#define APP_READY_DISPLAY (1UL << 1U)
#define APP_READY_ALL     (APP_READY_INPUT | APP_READY_DISPLAY)

typedef enum {
    RTOS_LOG_BOOT,
    RTOS_LOG_CALIBRATING,
    RTOS_LOG_JOYSTICK_READY,
    RTOS_LOG_JOYSTICK_INIT_FAILURE,
    RTOS_LOG_JOYSTICK_FAULT,
    RTOS_LOG_JOYSTICK_SAMPLE,
    RTOS_LOG_JOYSTICK_BUTTON,
    RTOS_LOG_TFT_BENCHMARK,
    RTOS_LOG_GAME_READY,
    RTOS_LOG_BUTTON_EDGE,
    RTOS_LOG_BUTTON_DROPPED,
    RTOS_LOG_TFT_INIT_FAILURE,
    RTOS_LOG_TFT_BENCHMARK_FAILURE,
    RTOS_LOG_GAME_INIT_FAILURE,
    RTOS_LOG_TFT_RENDER_FAILURE,
    RTOS_LOG_OLED_READY,
    RTOS_LOG_OLED_FAILURE,
} rtos_log_event_type_t;

typedef struct {
    rtos_log_event_type_t type;
    uint32_t value0, value1, value2;
    joystick_input_t input;
    display_service_error_t display_error;
} rtos_log_event_t;

extern QueueHandle_t app_button_event_queue;
extern QueueHandle_t app_input_snapshot_queue;
extern QueueHandle_t app_render_snapshot_queue;
extern QueueHandle_t app_log_event_queue;
extern QueueHandle_t app_oled_status_queue;
extern TaskHandle_t app_game_task_handle;

void app_rtos_post_event(const rtos_log_event_t *event);
void app_rtos_post_log(rtos_log_event_type_t type, uint32_t value0, uint32_t value1,
                       uint32_t value2);
uint32_t app_rtos_take_dropped_log_count(void);
uint32_t app_rtos_take_dropped_button_count(void);
void display_task_entry(void *argument);
void game_task_entry(void *argument);
void input_task_entry(void *argument);
void logger_task_entry(void *argument);
void oled_task_entry(void *argument);
#endif
