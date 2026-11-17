#ifndef HOST_FREERTOS_H
#define HOST_FREERTOS_H
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t StackType_t;
typedef void (*TaskFunction_t)(void *);

typedef struct {
    TaskFunction_t entry;
    UBaseType_t priority;
    uint32_t stack_words;
    uint32_t notification[2];
} StaticTask_t;

typedef StaticTask_t *TaskHandle_t;

typedef struct {
    uint8_t *storage;
    UBaseType_t length, item_size, count, head, tail;
} StaticQueue_t;

typedef StaticQueue_t *QueueHandle_t;
#include "FreeRTOSConfig.h"
#undef configASSERT
#define configASSERT(x)   assert(x)
#define pdTRUE            1
#define pdFALSE           0
#define pdPASS            1
#define pdFAIL            0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY     UINT32_MAX
void host_task_critical_enter(void);
void host_task_critical_exit(void);
void host_port_yield(BaseType_t woken);
#define taskENTER_CRITICAL()      host_task_critical_enter()
#define taskEXIT_CRITICAL()       host_task_critical_exit()
#define portYIELD_FROM_ISR(woken) host_port_yield(woken)
#endif
