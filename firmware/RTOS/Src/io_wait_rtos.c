#include "io_wait.h"

#include <stddef.h>
#include "cpu.h"
#include "FreeRTOS.h"
#include "task.h"

/* Index 0 belongs to application startup notifications. */
#define IO_NOTIFICATION_INDEX 1U
static TaskHandle_t volatile waiters[IO_WAIT_COUNT];

_Static_assert(configTASK_NOTIFICATION_ARRAY_ENTRIES > IO_NOTIFICATION_INDEX,
               "I/O needs a separate task notification index");
_Static_assert(IO_IRQ_PRIORITY_ADC >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY &&
                   IO_IRQ_PRIORITY_SPI >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY &&
                   IO_IRQ_PRIORITY_UART >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
               "I/O IRQ priorities must permit FreeRTOS FromISR calls");
_Static_assert(IO_IRQ_PRIORITY_I2C >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
               "I2C must permit FreeRTOS FromISR calls");

static TickType_t wait_ticks(uint32_t milliseconds)
{
    const TickType_t ticks = pdMS_TO_TICKS(milliseconds);
    return ticks > 0U ? ticks : 1U;
}

void io_wait_locked(io_wait_source_t source, uint32_t saved_mask, uint32_t timeout_ms)
{
    configASSERT(source < IO_WAIT_COUNT);
    if ((xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) || (__get_IPSR() != 0U) ||
        (saved_mask != 0U)) {
        cpu_wait_for_interrupt();
        cpu_irq_restore(saved_mask);
        return;
    }

    const TaskHandle_t task = xTaskGetCurrentTaskHandle();
    configASSERT(waiters[source] == NULL || waiters[source] == task);
    waiters[source] = task;
    /* An IRQ arriving here is retained as a notification even before Take. */
    cpu_irq_restore(saved_mask);
    (void)ulTaskNotifyTakeIndexed(IO_NOTIFICATION_INDEX, pdTRUE, wait_ticks(timeout_ms));

    const uint32_t mask = cpu_irq_save();
    if (waiters[source] == task)
        waiters[source] = NULL;
    cpu_irq_restore(mask);
}

void io_wake_from_isr(io_wait_source_t source)
{
    const TaskHandle_t task = waiters[source];
    if (task == NULL || xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        return;
    waiters[source] = NULL;
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveIndexedFromISR(task, IO_NOTIFICATION_INDEX, &woken);
    portYIELD_FROM_ISR(woken);
}

bool io_delay_ms(uint32_t milliseconds)
{
    if ((xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) || (__get_IPSR() != 0U) ||
        (__get_PRIMASK() != 0U))
        return false;
    if (milliseconds > 0U)
        vTaskDelay(wait_ticks(milliseconds));
    return true;
}
