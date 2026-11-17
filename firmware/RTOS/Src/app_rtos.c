#include "app_rtos.h"
#include <stdint.h>
#include "FreeRTOS.h"
#include "app_rtos_private.h"
#include "cpu.h"
#include "queue.h"
#include "task.h"
#include "uart.h"

#define BUTTON_QUEUE_LENGTH      8U
#define LOGGER_QUEUE_LENGTH      24U
#define DISPLAY_TASK_STACK_WORDS 768U
#define GAME_TASK_STACK_WORDS    512U
#define INPUT_TASK_STACK_WORDS   384U
#define LOGGER_TASK_STACK_WORDS  512U
#define IDLE_TASK_STACK_WORDS    192U
#define OLED_TASK_STACK_WORDS    768U
#define TASK_PRIORITY_LOGGER     1U
#define TASK_PRIORITY_GAME       3U
#define TASK_PRIORITY_DISPLAY    2U
#define TASK_PRIORITY_INPUT      4U

QueueHandle_t app_button_event_queue, app_input_snapshot_queue;
QueueHandle_t app_render_snapshot_queue, app_log_event_queue;
QueueHandle_t app_oled_status_queue;
TaskHandle_t app_game_task_handle;
static StaticQueue_t button_queue_control, input_queue_control;
static StaticQueue_t render_queue_control, logger_queue_control;
static StaticQueue_t oled_queue_control;
static uint8_t button_queue_storage[BUTTON_QUEUE_LENGTH * sizeof(uint32_t)];
static joystick_input_t input_queue_storage[1];
static console_model_t render_queue_storage[1];
static console_status_t oled_queue_storage[1];
static rtos_log_event_t logger_queue_storage[LOGGER_QUEUE_LENGTH];
static StaticTask_t display_task_control, game_task_control, input_task_control;
static StaticTask_t logger_task_control, idle_task_control;
static StaticTask_t oled_task_control;
static StackType_t display_task_stack[DISPLAY_TASK_STACK_WORDS];
static StackType_t game_task_stack[GAME_TASK_STACK_WORDS];
static StackType_t input_task_stack[INPUT_TASK_STACK_WORDS];
static StackType_t logger_task_stack[LOGGER_TASK_STACK_WORDS];
static StackType_t idle_task_stack[IDLE_TASK_STACK_WORDS];
static StackType_t oled_task_stack[OLED_TASK_STACK_WORDS];
static volatile uint32_t dropped_log_records, dropped_button_events;

void app_rtos_post_event(const rtos_log_event_t *event)
{
    if (app_log_event_queue != NULL && xQueueSend(app_log_event_queue, event, 0U) != pdPASS) {
        taskENTER_CRITICAL();
        ++dropped_log_records;
        taskEXIT_CRITICAL();
    }
}

void app_rtos_post_log(rtos_log_event_type_t type, uint32_t a, uint32_t b, uint32_t c)
{
    const rtos_log_event_t event = {.type = type, .value0 = a, .value1 = b, .value2 = c};
    app_rtos_post_event(&event);
}

uint32_t app_rtos_take_dropped_log_count(void)
{
    taskENTER_CRITICAL();
    const uint32_t count = dropped_log_records;
    dropped_log_records = 0U;
    taskEXIT_CRITICAL();
    return count;
}

uint32_t app_rtos_take_dropped_button_count(void)
{
    taskENTER_CRITICAL();
    const uint32_t count = dropped_button_events;
    dropped_button_events = 0U;
    taskEXIT_CRITICAL();
    return count;
}

bool app_rtos_init(void)
{
    app_button_event_queue = xQueueCreateStatic(BUTTON_QUEUE_LENGTH, sizeof(uint32_t),
                                                button_queue_storage, &button_queue_control);
    app_input_snapshot_queue = xQueueCreateStatic(
        1U, sizeof(joystick_input_t), (uint8_t *)input_queue_storage, &input_queue_control);
    app_render_snapshot_queue = xQueueCreateStatic(
        1U, sizeof(console_model_t), (uint8_t *)render_queue_storage, &render_queue_control);
    app_oled_status_queue = xQueueCreateStatic(1U, sizeof(console_status_t),
                                               (uint8_t *)oled_queue_storage, &oled_queue_control);
    app_log_event_queue =
        xQueueCreateStatic(LOGGER_QUEUE_LENGTH, sizeof(rtos_log_event_t),
                           (uint8_t *)logger_queue_storage, &logger_queue_control);
    if (app_button_event_queue == NULL || app_input_snapshot_queue == NULL ||
        app_render_snapshot_queue == NULL || app_log_event_queue == NULL ||
        app_oled_status_queue == NULL)
        return false;
    vQueueAddToRegistry(app_button_event_queue, "B1 events");
    vQueueAddToRegistry(app_input_snapshot_queue, "Input");
    vQueueAddToRegistry(app_render_snapshot_queue, "Frames");
    vQueueAddToRegistry(app_log_event_queue, "Logs");
    vQueueAddToRegistry(app_oled_status_queue, "OLED status");

    const TaskHandle_t display =
        xTaskCreateStatic(display_task_entry, "Display", DISPLAY_TASK_STACK_WORDS, NULL,
                          TASK_PRIORITY_DISPLAY, display_task_stack, &display_task_control);
    app_game_task_handle =
        xTaskCreateStatic(game_task_entry, "Game", GAME_TASK_STACK_WORDS, NULL, TASK_PRIORITY_GAME,
                          game_task_stack, &game_task_control);
    const TaskHandle_t input =
        xTaskCreateStatic(input_task_entry, "Input", INPUT_TASK_STACK_WORDS, NULL,
                          TASK_PRIORITY_INPUT, input_task_stack, &input_task_control);
    const TaskHandle_t logger =
        xTaskCreateStatic(logger_task_entry, "Logger", LOGGER_TASK_STACK_WORDS, NULL,
                          TASK_PRIORITY_LOGGER, logger_task_stack, &logger_task_control);
    const TaskHandle_t oled =
        xTaskCreateStatic(oled_task_entry, "OLED", OLED_TASK_STACK_WORDS, NULL,
                          TASK_PRIORITY_LOGGER, oled_task_stack, &oled_task_control);
    return display != NULL && app_game_task_handle != NULL && input != NULL && logger != NULL &&
           oled != NULL;
}

void app_rtos_start(void)
{
    vTaskStartScheduler();
    uart_write("ERROR: FreeRTOS scheduler failed to start\n");
    (void)uart_flush(200U);
    __disable_irq();
    for (;;)
        cpu_wait_for_interrupt();
}

void app_rtos_button_event_from_isr(void)
{
    BaseType_t woken = pdFALSE;
    const uint32_t event = 1U;
    if (app_button_event_queue != NULL) {
        if (xQueueSendFromISR(app_button_event_queue, &event, &woken) != pdPASS)
            ++dropped_button_events;
        portYIELD_FROM_ISR(woken);
    }
}

void vApplicationGetIdleTaskMemory(StaticTask_t **control, StackType_t **stack, uint32_t *size)
{
    *control = &idle_task_control;
    *stack = idle_task_stack;
    *size = IDLE_TASK_STACK_WORDS;
}

void vApplicationIdleHook(void)
{
    cpu_wait_for_interrupt();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    __disable_irq();
    for (;;)
        cpu_wait_for_interrupt();
}
