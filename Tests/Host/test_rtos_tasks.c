/* Execute the actual task bodies with deterministic queue/task API doubles.
   This verifies data flow and ownership; it is not a Cortex-M scheduler emulator. */
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "app_rtos.h"
#include "app_rtos_private.h"
#include "board.h"
#include "console_scene.h"
#include "oled_config.h"
#include "spi.h"
#include "timebase.h"
#include "uart.h"

uint32_t SystemCoreClock = 180000000U;
static StaticTask_t *tasks[5], *current_task;
static unsigned task_count, registered_queues, critical_depth, idle_calls;
static uint32_t now_ms, steps, step_limit, min_hero_y, jump_count;
static bool inject_display_ready, sample_ok = true, fail_draw, last_frame_valid;
static joystick_input_t sample;
static game_model_t last_game;
static console_model_t last_console, last_delivered, extra_frames[3];
static bool oled_available = true;
static unsigned oled_inits, oled_frames;
static unsigned frame_index, extra_count, frame_checks;
static uint16_t framebuffer[DISPLAY_SERVICE_HEIGHT][DISPLAY_SERVICE_WIDTH];
static uint16_t reference[DISPLAY_SERVICE_HEIGHT][DISPLAY_SERVICE_WIDTH];
static char uart_output[12000];
static size_t uart_length;
static jmp_buf task_exit;

enum {
    DISPLAY,
    GAME,
    INPUT,
    LOGGER,
    OLED
};

enum {
    WAITING = 1,
    FINISHED,
    SUSPENDED
};

static TaskHandle_t explicitly_suspended;

uint32_t __get_PRIMASK(void)
{
    return 0U;
}

uint32_t __get_IPSR(void)
{
    return 0U;
}

void __disable_irq(void)
{
}

void cpu_wait_for_interrupt(void)
{
    ++idle_calls;
}

void host_task_critical_enter(void)
{
    ++critical_depth;
}

void host_task_critical_exit(void)
{
    assert(critical_depth > 0);
    --critical_depth;
}

void host_port_yield(BaseType_t woken)
{
    (void)woken;
}

BaseType_t xTaskGetSchedulerState(void)
{
    return taskSCHEDULER_RUNNING;
}

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    return current_task;
}

TickType_t xTaskGetTickCount(void)
{
    return now_ms;
}

uint32_t timebase_millis(void)
{
    return now_ms;
}

void timebase_delay_ms(uint32_t ms)
{
    now_ms += ms;
}

void vTaskDelay(TickType_t ticks)
{
    assert(current_task == tasks[OLED]);
    assert(ticks == (oled_available ? 200U : 2000U));
    if (steps++ == step_limit)
        longjmp(task_exit, FINISHED);
    now_ms += ticks;
}

void vTaskStartScheduler(void)
{
    assert(false);
}

TaskHandle_t xTaskCreateStatic(TaskFunction_t entry, const char *name, uint32_t depth,
                               void *argument, UBaseType_t priority, StackType_t *stack,
                               StaticTask_t *control)
{
    assert(task_count < 5 && name != NULL && argument == NULL && stack != NULL);
    *control = (StaticTask_t){.entry = entry, .priority = priority, .stack_words = depth};
    tasks[task_count++] = control;
    return control;
}

void vTaskSuspend(TaskHandle_t task)
{
    if (task != NULL)
        explicitly_suspended = task;
    else
        longjmp(task_exit, SUSPENDED);
}

BaseType_t xTaskNotify(TaskHandle_t task, uint32_t bits, eNotifyAction action)
{
    assert(task != NULL && action == eSetBits);
    task->notification[0] |= bits;
    return pdPASS;
}

BaseType_t xTaskNotifyWait(uint32_t clear_entry, uint32_t clear_exit, uint32_t *bits,
                           TickType_t timeout)
{
    assert(current_task == tasks[GAME] && clear_entry == 0 && timeout == portMAX_DELAY);
    if (current_task->notification[0] == 0U) {
        assert(app_render_snapshot_queue->count == 0); /* No title before both inputs are ready. */
        if (!inject_display_ready)
            longjmp(task_exit, WAITING);
        inject_display_ready = false;
        (void)xTaskNotify(current_task, APP_READY_DISPLAY, eSetBits);
    }
    *bits = current_task->notification[0];
    current_task->notification[0] &= ~clear_exit;
    return pdPASS;
}

QueueHandle_t xQueueCreateStatic(UBaseType_t length, UBaseType_t size, uint8_t *storage,
                                 StaticQueue_t *control)
{
    assert(length > 0 && size > 0 && storage != NULL);
    *control = (StaticQueue_t){.storage = storage, .length = length, .item_size = size};
    return control;
}

void vQueueAddToRegistry(QueueHandle_t queue, const char *name)
{
    assert(queue != NULL && name != NULL);
    ++registered_queues;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t timeout)
{
    assert(timeout == 0);
    if (queue->count == queue->length)
        return pdFAIL;
    memcpy(queue->storage + queue->head * queue->item_size, item, queue->item_size);
    queue->head = (queue->head + 1U) % queue->length;
    ++queue->count;
    return pdPASS;
}

BaseType_t xQueueSendFromISR(QueueHandle_t queue, const void *item, BaseType_t *woken)
{
    *woken = pdTRUE;
    return xQueueSend(queue, item, 0U);
}

BaseType_t xQueuePeek(QueueHandle_t queue, void *item, TickType_t timeout)
{
    assert(timeout == 0);
    if (queue->count == 0)
        return pdFAIL;
    memcpy(item, queue->storage + queue->tail * queue->item_size, queue->item_size);
    return pdPASS;
}

BaseType_t xQueueOverwrite(QueueHandle_t queue, const void *item)
{
    assert(queue->length == 1U);
    memcpy(queue->storage, item, queue->item_size);
    queue->count = 1U;
    if (queue == app_render_snapshot_queue && current_task == tasks[GAME]) {
        last_console = *(const console_model_t *)item;
        const game_model_t *g = &last_console.runner;
        if (g->jumping && !last_game.jumping)
            ++jump_count;
        if (last_console.mode == CONSOLE_PLAYING && g->hero_y < (int16_t)min_hero_y)
            min_hero_y = (uint32_t)g->hero_y;
        last_game = *g;
    }
    return pdPASS;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t timeout)
{
    if (queue == app_render_snapshot_queue && current_task == tasks[DISPLAY]) {
        if (last_frame_valid) {
            memcpy(reference, framebuffer, sizeof(reference));
            assert(console_scene_render(&last_delivered, NULL));
            assert(memcmp(reference, framebuffer, sizeof(reference)) == 0);
            ++frame_checks;
            last_frame_valid = false;
        }
        if (queue->count == 0U && frame_index < extra_count)
            (void)xQueueOverwrite(queue, &extra_frames[frame_index++]);
    }
    if (queue->count == 0) {
        if (timeout > 0)
            longjmp(task_exit, FINISHED);
        return pdFAIL;
    }
    memcpy(item, queue->storage + queue->tail * queue->item_size, queue->item_size);
    queue->tail = (queue->tail + 1U) % queue->length;
    --queue->count;
    if (queue == app_render_snapshot_queue && current_task == tasks[DISPLAY]) {
        last_delivered = *(const console_model_t *)item;
        last_frame_valid = true;
    }
    return pdPASS;
}

void vTaskDelayUntil(TickType_t *last_wake, TickType_t period)
{
    assert(period == 10U);
    if (steps == step_limit)
        longjmp(task_exit, FINISHED);
    ++steps;
    *last_wake += period;
    now_ms = *last_wake;
    if (current_task == tasks[GAME]) {
        sample = (joystick_input_t){.x = 1000, .y = steps == 1 ? 0 : 1000, .press_sequence = 1};
        if (steps == 1) {
            sample.pressed = true;
            sample.pressed_at_ms = now_ms;
        }
        if (steps >= 171) {
            sample.press_sequence = 2;
            sample.pressed_at_ms = 1710U;
        }
        if (steps == 171)
            sample.pressed = true;
        (void)xQueueOverwrite(app_input_snapshot_queue, &sample);
    } else {
        assert(current_task == tasks[INPUT]);
        sample = (joystick_input_t){
            .raw_x = 2048, .raw_y = 2050, .x = 500, .press_sequence = 1, .pressed_at_ms = 10};
        sample_ok = steps != 3 && steps != 4;
    }
}

void board_oled_pins_init(void)
{
    assert(current_task == tasks[OLED]);
}

i2c_status_t i2c1_init(uint32_t pclk, uint32_t bus)
{
    assert(current_task == tasks[OLED] && pclk == 45000000 && bus == 100000);
    return I2C_STATUS_OK;
}

i2c_status_t oled_init(oled_controller_t controller)
{
    assert(current_task == tasks[OLED] && controller == APP_OLED_CONTROLLER);
    ++oled_inits;
    return oled_available ? I2C_STATUS_OK : I2C_STATUS_NACK;
}

i2c_status_t oled_present(const uint8_t pixels[1024])
{
    assert(current_task == tasks[OLED] && pixels);
    ++oled_frames;
    return I2C_STATUS_OK;
}

uint8_t oled_address(void)
{
    return 0x3C;
}

const char *i2c_status_name(i2c_status_t status)
{
    (void)status;
    return "NACK";
}

void board_button_init(void)
{
    assert(current_task == tasks[INPUT]);
}

void board_tft_pins_init(void)
{
    assert(current_task == tasks[DISPLAY]);
}

bool board_joystick_button_is_pressed(void)
{
    return false;
}

bool joystick_service_init(uint32_t timer_hz)
{
    assert(timer_hz == 90000000U);
    return true;
}

bool joystick_service_read(joystick_input_t *out)
{
    *out = sample;
    return sample_ok;
}

void spi1_init(uint32_t divider, bool cpol, bool cpha)
{
    assert(current_task == tasks[DISPLAY] && divider == 2U && !cpol && !cpha);
}

void spi1_dma_init(void)
{
    assert(current_task == tasks[DISPLAY]);
}

bool display_service_init(display_service_delay_fn delay, display_service_now_ms_fn now)
{
    assert(delay == timebase_delay_ms && now == timebase_millis);
    return true;
}

bool display_service_run_startup_benchmark(display_benchmark_result_t *out)
{
    *out = (display_benchmark_result_t){
        .average_upload_tenths_ms = 1100, .upload_rate_tenths_fps = 91, .elapsed_ms = 1100};
    return true;
}

const display_service_error_t *display_service_last_error(void)
{
    static const display_service_error_t error = {
        .operation = "draw_rgb565",
        .reason = "simulated error",
        .x = 317,
        .y = 168,
        .width = 3,
        .height = 32,
    };
    return fail_draw ? &error : NULL;
}

bool display_service_draw_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                 const uint8_t *pixels)
{
    assert(current_task == tasks[DISPLAY]);
    assert(w > 0 && h > 0 && (uint32_t)x + w <= DISPLAY_SERVICE_WIDTH &&
           (uint32_t)y + h <= DISPLAY_SERVICE_HEIGHT);
    if (fail_draw)
        return false;
    for (uint16_t row = 0; row < h; ++row)
        for (uint16_t col = 0; col < w; ++col) {
            const uint32_t p = ((uint32_t)row * w + col) * 2U;
            framebuffer[y + row][x + col] = (uint16_t)(pixels[p] << 8U | pixels[p + 1]);
        }
    return true;
}

void uart_putc(char c)
{
    assert(current_task == tasks[LOGGER] && uart_length + 1 < sizeof(uart_output));
    uart_output[uart_length++] = c;
    uart_output[uart_length] = '\0';
}

void uart_write(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

bool uart_flush(uint32_t ms)
{
    (void)ms;
    return true;
}

static int run_task(unsigned task, uint32_t limit)
{
    current_task = tasks[task];
    steps = 0;
    step_limit = limit;
    const int reason = setjmp(task_exit);
    if (reason == 0) {
        current_task->entry(NULL);
        assert(false);
    }
    assert(critical_depth == 0U);
    return reason;
}

extern void vApplicationIdleHook(void);

int main(void)
{
    assert(app_rtos_init());
    assert(task_count == 5 && registered_queues == 5);
    assert(tasks[INPUT]->priority > tasks[GAME]->priority &&
           tasks[GAME]->priority > tasks[DISPLAY]->priority &&
           tasks[DISPLAY]->priority > tasks[LOGGER]->priority);
    assert(app_input_snapshot_queue->item_size == sizeof(joystick_input_t));
    assert(app_render_snapshot_queue->item_size == sizeof(console_model_t));
    assert(configSUPPORT_DYNAMIC_ALLOCATION == 0 && configUSE_IDLE_HOOK == 1);
    vApplicationIdleHook();
    assert(idle_calls == 1);

    assert(run_task(GAME, 0) == WAITING);
    assert(app_render_snapshot_queue->count == 0);
    tasks[GAME]->notification[0] = APP_READY_INPUT;
    inject_display_ready = true;
    min_hero_y = GAME_GROUND_Y;
    assert(run_task(GAME, 0) == FINISHED);
    assert(last_console.mode == CONSOLE_MENU);
    app_render_snapshot_queue->count = 0;

    /* Input retains a complete short press/release and reports ADC faults. */
    for (unsigned i = 0; i < 11; ++i)
        app_rtos_button_event_from_isr();
    now_ms = 0;
    sample = (joystick_input_t){0};
    assert(run_task(INPUT, 60) == FINISHED);
    assert(uart_length == 0); /* Only Logger writes UART. */
    assert(run_task(LOGGER, 0) == FINISHED);
    assert(strstr(uart_output, "BTN PB0 PRESSED count=1 at=10 ms") != NULL);
    assert(strstr(uart_output, "BTN PB0 RELEASED (SW=0)") != NULL);
    assert(strstr(uart_output, "Button events dropped (queue full): 3") != NULL);
    assert(strstr(uart_output, "JOY X=500 Y=0 ADC=2048/2050 SW=0 PB0=1 B=0 MENU=0") != NULL);
    assert(strstr(uart_output, "sample unavailable") != NULL);

    /* Exercise the real Game task through input and latest-frame queues. */
    now_ms = 0;
    jump_count = 0;
    min_hero_y = GAME_GROUND_Y;
    tasks[GAME]->notification[0] = APP_READY_ALL;
    assert(run_task(GAME, 200) == FINISHED);
    assert(last_game.mode == GAME_RUNNING && jump_count == 1);
    assert(last_game.hero_x_fp ==
           (int32_t)((DISPLAY_SERVICE_WIDTH - GAME_HERO_WIDTH) * GAME_FP_ONE));
    assert(min_hero_y == GAME_GROUND_Y - GAME_HERO_HEIGHT - GAME_JUMP_HEIGHT);

    /* Display can skip intermediate frames and still erase the drawn frame. */
    extra_frames[0] = last_console;
    extra_frames[0].runner.hero_x_fp = 64 * GAME_FP_ONE;
    extra_frames[0].runner.hero_y = 168;
    extra_frames[1] = extra_frames[0];
    extra_frames[1].runner.mode = GAME_OVER;
    extra_frames[1].runner.lives = 0;
    extra_frames[2] = extra_frames[0];
    extra_frames[2].runner.hero_x_fp = 296 * GAME_FP_ONE;
    extra_frames[2].runner.shield = true;
    extra_count = 3;
    frame_index = 0;
    assert(run_task(DISPLAY, 0) == FINISHED && frame_checks == 4);
    assert(run_task(LOGGER, 0) == FINISHED);
    assert(strstr(uart_output, "COIN DASH ready: button to start/jump/retry; stick to move") !=
           NULL);
    assert(strstr(uart_output, "Boot OK (HAL clock, FreeRTOS)") != NULL);

    /* OLED owns only I2C and its independent status snapshot. Failure/retry
       neither consumes the TFT queue nor suspends Game. */
    unsigned pending_frames = app_render_snapshot_queue->count;
    assert(run_task(OLED, 2) == FINISHED && oled_frames == 3 && oled_inits == 1);
    assert(app_render_snapshot_queue->count == pending_frames);
    assert(run_task(LOGGER, 0) == FINISHED);
    assert(strstr(uart_output, "OLED ready: I2C1 address 0x3C SSD1306 bus=100000 Hz"));
    oled_available = false;
    unsigned inits = oled_inits;
    assert(run_task(OLED, 2) == FINISHED && oled_inits == inits + 3);
    assert(explicitly_suspended == NULL);
    assert(run_task(LOGGER, 0) == FINISHED);
    assert(strstr(uart_output, "OLED unavailable: NACK; TFT menu remains available"));
    oled_available = true;
    assert(run_task(OLED, 0) == FINISHED);
    assert(run_task(LOGGER, 0) == FINISHED);

    fail_draw = true;
    extra_count = 0;
    last_frame_valid = false;
    (void)xQueueOverwrite(app_render_snapshot_queue, &last_console);
    assert(run_task(DISPLAY, 0) == SUSPENDED);
    assert(explicitly_suspended == app_game_task_handle);
    assert(run_task(LOGGER, 0) == FINISHED);
    assert(strstr(uart_output, "Game initialization failed") != NULL);
    assert(strstr(uart_output, "TFT error: draw_rgb565 / simulated error x=317 y=168 w=3 h=32") !=
           NULL);
    for (unsigned i = 0; i < app_log_event_queue->length + 6U; ++i)
        app_rtos_post_log(RTOS_LOG_BUTTON_EDGE, 0U, 0U, 0U);
    assert(run_task(LOGGER, 0) == FINISHED);
    assert(strstr(uart_output, "Logger dropped records: 6") != NULL);
    puts(
        "PASS: RTOS static tasks/queues, startup barrier, retained PB0 logs, fault diagnostics, "
        "game input/jump/right edge, skipped-frame rendering, display error propagation, idle WFI");
    return 0;
}
