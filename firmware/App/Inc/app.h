#ifndef CONSOLE_APP_H
#define CONSOLE_APP_H

#include <stdbool.h>

bool app_init(void);
void app_start(void);

/* Called by the EXTI ISR; records an edge for deferred processing. */
void app_button_irq_notify(void);

#endif /* CONSOLE_APP_H */
