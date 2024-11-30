#pragma once

#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"

// typedef QueueHandle_t SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex()
{
    printf("xSemaphoreCreateRecursiveMutex\n");
    return NULL;
}

#define vSemaphoreDelete(xSemaphore) vQueueDelete((QueueHandle_t)(xSemaphore))

#define xSemaphoreTakeRecursive(xMutex, xBlockTime) xQueueTakeMutexRecursive((QueueHandle_t)(xMutex), (xBlockTime))

#define xSemaphoreGiveRecursive(xMutex) xQueueGiveMutexRecursive((QueueHandle_t)(xMutex))

inline uint32_t xSemaphoreTakeReturn = pdFALSE;
inline uint32_t xSemaphoreTake(SemaphoreHandle_t, uint32_t)
{
    return xSemaphoreTakeReturn;
}

inline bool xSemaphoreGiveCalled = pdFALSE;
inline void xSemaphoreGive(SemaphoreHandle_t)
{
    xSemaphoreGiveCalled = pdTRUE;
}

inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *t)
{
    printf("xSemaphoreCreateMutexStatic\n");
    return NULL;
}

inline SemaphoreHandle_t xSemaphoreCreateMutex()
{
    printf("xSemaphoreCreateMutex\n");
    return NULL;
}
