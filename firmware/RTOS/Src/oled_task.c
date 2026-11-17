#include "app_rtos_private.h"
#include "board.h"
#include "console_scene.h"
#include "oled_config.h"
#include "timebase.h"

void oled_task_entry(void *argument)
{
    (void)argument;
    board_oled_pins_init();
    i2c_status_t status = i2c1_init(SystemCoreClock / 4U, APP_OLED_BUS_HZ);
    bool online = false, reported_failure = false;
    static uint8_t pixels[1024];
    console_status_t view = {0};
    for (;;) {
        if (!online) {
            if (status != I2C_STATUS_OK)
                status = i2c1_init(SystemCoreClock / 4U, APP_OLED_BUS_HZ);
            if (status == I2C_STATUS_OK)
                status = oled_init(APP_OLED_CONTROLLER);
            if (status == I2C_STATUS_OK) {
                online = true;
                reported_failure = false;
                app_rtos_post_log(RTOS_LOG_OLED_READY, oled_address(), APP_OLED_CONTROLLER,
                                  APP_OLED_BUS_HZ);
            }
        }
        if (online) {
            (void)xQueuePeek(app_oled_status_queue, &view, 0);
            console_oled_render(&view, pixels);
            status = oled_present(pixels);
            if (status != I2C_STATUS_OK)
                online = false;
        }
        if (!online && !reported_failure) {
            app_rtos_post_log(RTOS_LOG_OLED_FAILURE, status, 0, 0);
            reported_failure = true;
        }
        vTaskDelay(pdMS_TO_TICKS(online ? 200U : 2000U));
    }
}
