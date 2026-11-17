#include "gpio.h"

void gpio_write(GPIO_TypeDef *port, uint8_t pin, bool high)
{
    const uint32_t bit = 1UL << pin;

    /* BSRR updates the output atomically without modifying other pins. */
    port->BSRR = high ? bit : (bit << 16U);
}

void gpio_config_output(GPIO_TypeDef *port, uint8_t pin, bool initial_high)
{
    const uint32_t shift = 2U * pin;
    const uint32_t bit = 1UL << pin;

    /* Load the output level before switching the pin into output mode. */
    gpio_write(port, pin, initial_high);
    port->OTYPER &= ~bit;
    port->OSPEEDR &= ~(3UL << shift);
    port->PUPDR &= ~(3UL << shift);
    port->MODER = (port->MODER & ~(3UL << shift)) | (1UL << shift);
}

void gpio_config_input(GPIO_TypeDef *port, uint8_t pin)
{

    const uint32_t shift = 2U * pin;
    port->MODER &= ~(3UL << shift); // Set mode to input (00)
    port->PUPDR &= ~(3UL << shift); // No pull-up, no pull-down (00)
}

bool gpio_read(GPIO_TypeDef *port, uint8_t pin)
{

    const uint32_t bit = 1UL << pin;
    return (port->IDR & bit) != 0U;
}