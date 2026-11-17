#ifndef CONSOLE_GPIO_H
#define CONSOLE_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx.h"

/* Enable the GPIO port clock before calling these functions. Pin is 0..15. */
void gpio_config_output(GPIO_TypeDef *port, uint8_t pin, bool initial_high);
void gpio_write(GPIO_TypeDef *port, uint8_t pin, bool high);

void gpio_config_input(GPIO_TypeDef *port, uint8_t pin);
bool gpio_read(GPIO_TypeDef *port, uint8_t pin);

#endif /* CONSOLE_GPIO_H */
