#ifndef CONSOLE_APP_RTOS_H
#define CONSOLE_APP_RTOS_H

#include <stdbool.h>

bool app_rtos_init(void);
void app_rtos_start(void);
void app_rtos_button_event_from_isr(void);

#endif /* CONSOLE_APP_RTOS_H */
