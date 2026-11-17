#include "app_rtos_private.h"
#include "queue.h"
#include "task.h"
#include "uart.h"
#include "i2c.h"

static bool reported_pressed;
static uint32_t reported_sequence;
static bool extra_held[2];
static uint32_t extra_sequence[2];

static void write_uint(uint32_t value)
{
    char digits[10];
    uint8_t count = 0U;
    do {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value > 0U);
    while (count > 0U)
        uart_putc(digits[--count]);
}

static void write_signed(int16_t value)
{
    int32_t magnitude = value;
    if (magnitude < 0) {
        uart_putc('-');
        magnitude = -magnitude;
    }
    write_uint((uint32_t)magnitude);
}

static void write_tenths(uint32_t value)
{
    write_uint(value / 10U);
    uart_putc('.');
    uart_putc((char)('0' + value % 10U));
}

static void report_display_error(const display_service_error_t *error)
{
    if (error->operation == NULL)
        return;
    uart_write("TFT error: ");
    uart_write(error->operation);
    uart_write(" / ");
    uart_write(error->reason);
    uart_write(" x=");
    write_uint(error->x);
    uart_write(" y=");
    write_uint(error->y);
    uart_write(" w=");
    write_uint(error->width);
    uart_write(" h=");
    write_uint(error->height);
    uart_write("\n");
}

static void report_button(const joystick_input_t *input)
{
    const bool retained = input->press_sequence != reported_sequence;
    if (retained || (input->pressed && !reported_pressed)) {
        uart_write("BTN PB0 PRESSED count=");
        write_uint(input->press_sequence);
        if (retained) {
            uart_write(" at=");
            write_uint(input->pressed_at_ms);
            uart_write(" ms");
        }
        uart_write("\n");
    }
    if (!input->pressed && (reported_pressed || retained))
        uart_write("BTN PB0 RELEASED (SW=0)\n");
    reported_pressed = input->pressed;
    reported_sequence = input->press_sequence;
    const bool held[] = {input->b_pressed, input->menu_pressed};
    const uint32_t sequence[] = {input->b_press_sequence, input->menu_press_sequence};
    const uint32_t at[] = {input->b_pressed_at_ms, input->menu_pressed_at_ms};
    static const char *const names[] = {"BTN B PB5", "BTN MENU PA8"};
    for (unsigned i = 0; i < 2; ++i) {
        bool retained_extra = sequence[i] != extra_sequence[i];
        if (retained_extra || (held[i] && !extra_held[i])) {
            uart_write(names[i]);
            uart_write(" PRESSED count=");
            write_uint(sequence[i]);
            uart_write(" at=");
            write_uint(at[i]);
            uart_write(" ms\n");
        }
        if (!held[i] && (extra_held[i] || retained_extra)) {
            uart_write(names[i]);
            uart_write(" RELEASED\n");
        }
        extra_held[i] = held[i];
        extra_sequence[i] = sequence[i];
    }
}

static void report_event(const rtos_log_event_t *event)
{
    switch (event->type) {
    case RTOS_LOG_BOOT:
        uart_write("STM32 Game Console Boot OK (HAL clock, FreeRTOS)\n");
        break;
    case RTOS_LOG_CALIBRATING:
        uart_write("KY-023 calibrating; keep centered for about 320 ms\n");
        break;
    case RTOS_LOG_JOYSTICK_READY:
        uart_write("KY-023 centered; TIM3 100 Hz -> ADC1 IN0/IN1 -> DMA2 S4; SW on PB0\n");
        break;
    case RTOS_LOG_JOYSTICK_INIT_FAILURE:
        uart_write("KY-023 initialization failed\n");
        break;
    case RTOS_LOG_JOYSTICK_FAULT:
        uart_write("KY-023 sample unavailable (DMA/overrun/stale)\n");
        break;
    case RTOS_LOG_JOYSTICK_SAMPLE:
        uart_write("JOY X=");
        write_signed(event->input.x);
        uart_write(" Y=");
        write_signed(event->input.y);
        uart_write(" ADC=");
        write_uint(event->input.raw_x);
        uart_write("/");
        write_uint(event->input.raw_y);
        uart_write(" SW=");
        uart_putc(event->input.pressed ? '1' : '0');
        uart_write(" PB0=");
        write_uint(event->value0);
        uart_write(" B=");
        write_uint(event->input.b_pressed);
        uart_write(" MENU=");
        write_uint(event->input.menu_pressed);
        uart_write("\n");
        break;
    case RTOS_LOG_JOYSTICK_BUTTON:
        report_button(&event->input);
        break;
    case RTOS_LOG_TFT_BENCHMARK:
        uart_write("TFT upload avg: ");
        write_tenths(event->value0);
        uart_write(" ms/frame; rate ");
        write_tenths(event->value1);
        uart_write(" FPS (SPI upload, not panel scan rate)\n");
        break;
    case RTOS_LOG_GAME_READY:
        uart_write("TFT game ready: ");
        write_uint(DISPLAY_SERVICE_WIDTH);
        uart_putc('x');
        write_uint(DISPLAY_SERVICE_HEIGHT);
        uart_write(" landscape\n");
        uart_write("ARCADE HUB ready: stick up/down, A select, MENU pause, B back\n");
        uart_write("COIN DASH ready: button to start/jump/retry; stick to move\n");
        uart_write("Buttons to GND: A PB0/A3, B PB5/D4, MENU PA8/D7\n");
        break;
    case RTOS_LOG_OLED_READY:
        uart_write("OLED ready: I2C1 address 0x3");
        uart_putc(event->value0 == 0x3DU ? 'D' : 'C');
        uart_write(event->value1 == 0U ? " SSD1306" : " SH1106");
        uart_write(" bus=");
        write_uint(event->value2);
        uart_write(" Hz\n");
        break;
    case RTOS_LOG_OLED_FAILURE:
        uart_write("OLED unavailable: ");
        uart_write(i2c_status_name((i2c_status_t)event->value0));
        uart_write("; TFT menu remains available; retry in 2 s\n");
        break;
    case RTOS_LOG_BUTTON_EDGE:
        uart_write("Button Press Edge (no debounce)\n");
        break;
    case RTOS_LOG_BUTTON_DROPPED:
        uart_write("Button events dropped (queue full): ");
        write_uint(event->value0);
        uart_write("\n");
        break;
    case RTOS_LOG_TFT_INIT_FAILURE:
        uart_write("TFT initialization failed\n");
        report_display_error(&event->display_error);
        break;
    case RTOS_LOG_TFT_BENCHMARK_FAILURE:
        uart_write("TFT benchmark failed\n");
        report_display_error(&event->display_error);
        break;
    case RTOS_LOG_GAME_INIT_FAILURE:
        uart_write("Game initialization failed\n");
        report_display_error(&event->display_error);
        break;
    case RTOS_LOG_TFT_RENDER_FAILURE:
        uart_write("TFT animation update failed\n");
        report_display_error(&event->display_error);
        break;
    default:
        break;
    }
}

void logger_task_entry(void *argument)
{
    (void)argument;
    for (;;) {
        rtos_log_event_t event;
        if (xQueueReceive(app_log_event_queue, &event, pdMS_TO_TICKS(1000U)) == pdPASS)
            report_event(&event);
        const uint32_t dropped = app_rtos_take_dropped_log_count();
        if (dropped > 0U) {
            uart_write("Logger dropped records: ");
            write_uint(dropped);
            uart_write("\n");
        }
    }
}
