#ifndef HOST_QUEUE_H
#define HOST_QUEUE_H
#include "FreeRTOS.h"
QueueHandle_t xQueueCreateStatic(UBaseType_t length, UBaseType_t size, uint8_t *storage,
                                 StaticQueue_t *control);
void vQueueAddToRegistry(QueueHandle_t queue, const char *name);
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t timeout);
BaseType_t xQueueSendFromISR(QueueHandle_t queue, const void *item, BaseType_t *woken);
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t timeout);
BaseType_t xQueuePeek(QueueHandle_t queue, void *item, TickType_t timeout);
BaseType_t xQueueOverwrite(QueueHandle_t queue, const void *item);
#endif
