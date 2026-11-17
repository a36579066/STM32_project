#include "uart.h"

#include "cpu.h"
#include "io_wait.h"
#include "timebase.h"

#define UART_TX_CAPACITY   1024U
#define UART_TX_TIMEOUT_MS 100U

static uint8_t tx_buffer[UART_TX_CAPACITY];
static volatile uint32_t tx_head;
static volatile uint32_t tx_tail;
static volatile bool tx_idle;
static uint32_t dropped_bytes;
static bool initialized;

void uart_init(uint32_t pclk1_hz, uint32_t baud_rate)
{
    if (baud_rate == 0U)
        return;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~((0xFU << 8U) | (0xFU << 12U))) | (7U << 8U) | (7U << 12U);
    GPIOA->MODER = (GPIOA->MODER & ~((3U << 4U) | (3U << 6U))) | (2U << 4U) | (2U << 6U);
    GPIOA->OTYPER &= ~((1U << 2U) | (1U << 3U));
    GPIOA->PUPDR &= ~((3U << 4U) | (3U << 6U));
    tx_head = 0U;
    tx_tail = 0U;
    tx_idle = true;
    dropped_bytes = 0U;
    USART2->CR1 = 0U;
    USART2->BRR = (pclk1_hz + baud_rate / 2U) / baud_rate;
    NVIC_ClearPendingIRQ(USART2_IRQn);
    NVIC_SetPriority(USART2_IRQn, IO_IRQ_PRIORITY_UART);
    NVIC_EnableIRQ(USART2_IRQn);
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    initialized = true;
}

void uart_putc(char c)
{
    /* Single foreground producer; ISR consumes bytes. Logging from an ISR or
       with interrupts masked is unsupported and must never deadlock. */
    if (!initialized || (__get_IPSR() != 0U) || (__get_PRIMASK() != 0U)) {
        ++dropped_bytes;
        return;
    }
    const uint32_t start = timebase_millis();
    for (;;) {
        const uint32_t mask = cpu_irq_save();
        const uint32_t next = (tx_head + 1U) % UART_TX_CAPACITY;
        if (next != tx_tail) {
            tx_buffer[tx_head] = (uint8_t)c;
            tx_head = next;
            tx_idle = false;
            USART2->CR1 = (USART2->CR1 & ~USART_CR1_TCIE) | USART_CR1_TXEIE;
            cpu_irq_restore(mask);
            return;
        }
        if ((uint32_t)(timebase_millis() - start) >= UART_TX_TIMEOUT_MS) {
            cpu_irq_restore(mask);
            ++dropped_bytes;
            return;
        }
        io_wait_locked(IO_WAIT_UART, mask,
                       UART_TX_TIMEOUT_MS - (uint32_t)(timebase_millis() - start));
    }
}

void uart_write(const char *s)
{
    while (*s != '\0') {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

bool uart_flush(uint32_t timeout_ms)
{
    if (!initialized)
        return true;
    if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
        return false;
    const uint32_t start = timebase_millis();
    for (;;) {
        const uint32_t mask = cpu_irq_save();
        const bool idle = tx_idle;
        const bool expired = (uint32_t)(timebase_millis() - start) >= timeout_ms;
        if (!idle && !expired) {
            io_wait_locked(IO_WAIT_UART, mask, timeout_ms - (uint32_t)(timebase_millis() - start));
        } else
            cpu_irq_restore(mask);
        if (idle || expired)
            return idle;
    }
}

uint32_t uart_dropped_bytes(void)
{
    return dropped_bytes;
}

void uart_irq_handler(void)
{
    /* Capture the UART status flags once for this interrupt handler pass. */
    const uint32_t status = USART2->SR;

    /* TXE means the transmit data register can accept another byte.
       Handle it only while the TXE interrupt source is enabled. */
    if (((status & USART_SR_TXE) != 0U) && ((USART2->CR1 & USART_CR1_TXEIE) != 0U)) {
        /* Different head and tail indices mean the software TX buffer has data. */
        if (tx_tail != tx_head) {
            /* Load the next queued byte into the UART transmit data register. */
            USART2->DR = tx_buffer[tx_tail];

            /* Consume this buffer entry and wrap the read index at the buffer end. */
            tx_tail = (tx_tail + 1U) % UART_TX_CAPACITY;
        } else {
            /* The software buffer is empty. Disable TXE interrupts to prevent
               repeated interrupts from an empty transmit data register.
               Enable TC interrupts to wait for the final byte to finish on the wire. */
            USART2->CR1 = (USART2->CR1 & ~USART_CR1_TXEIE) | USART_CR1_TCIE;
        }
    }

    /* TC means the entire UART frame, including its stop bit, has been sent.
       Confirm that TC interrupts are enabled and no new bytes are queued. */
    if (((status & USART_SR_TC) != 0U) && ((USART2->CR1 & USART_CR1_TCIE) != 0U) &&
        (tx_tail == tx_head)) {
        /* Stop TC interrupts now that this transmission has completed. */
        USART2->CR1 &= ~USART_CR1_TCIE;

        /* Report that both the software buffer and UART transmitter are idle.
           uart_flush() checks this flag before returning successfully. */
        tx_idle = true;
    }

    /* RXNE indicates unread received data; ORE indicates a receive overrun.
       Received data is discarded because this driver has no RX consumer yet. */
    if ((status & (USART_SR_RXNE | USART_SR_ORE)) != 0U) {
        /* SR was read above. Reading DR consumes received data and completes
           the SR-then-DR sequence required to clear ORE. Ignore the byte value. */
        (void)USART2->DR;
    }

    /* Notify a registered waiter, if any, that TX buffer space or transmission
       status may have changed. The waiter rechecks its condition after waking. */
    io_wake_from_isr(IO_WAIT_UART);
}
