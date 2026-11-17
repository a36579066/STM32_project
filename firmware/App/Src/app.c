#include "app.h"
#include "app_rtos.h"
#include "board.h"

bool app_init(void)
{
    board_init();
    return app_rtos_init();
}

void app_start(void)
{
    app_rtos_start();
}

void app_button_irq_notify(void)
{
    app_rtos_button_event_from_isr();
}
