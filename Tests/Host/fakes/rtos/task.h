#ifndef HOST_TASK_H
#define HOST_TASK_H
#include "FreeRTOS.h"
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING     1
#define taskSCHEDULER_SUSPENDED   2

typedef enum {
    eNoAction,
    eSetBits,
    eIncrement
} eNotifyAction;

BaseType_t xTaskGetSchedulerState(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
TickType_t xTaskGetTickCount(void);
TaskHandle_t xTaskCreateStatic(TaskFunction_t entry, const char *name, uint32_t depth,
                               void *argument, UBaseType_t priority, StackType_t *stack,
                               StaticTask_t *control);
void vTaskStartScheduler(void);
void vTaskSuspend(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
void vTaskDelayUntil(TickType_t *last_wake, TickType_t period);
uint32_t ulTaskNotifyTakeIndexed(UBaseType_t index, BaseType_t clear, TickType_t timeout);
void vTaskNotifyGiveIndexedFromISR(TaskHandle_t task, UBaseType_t index, BaseType_t *woken);
BaseType_t xTaskNotify(TaskHandle_t task, uint32_t bits, eNotifyAction action);
BaseType_t xTaskNotifyWait(uint32_t clear_entry, uint32_t clear_exit, uint32_t *bits,
                           TickType_t timeout);
#endif
