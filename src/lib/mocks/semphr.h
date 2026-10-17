#pragma once

#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"
#include <mutex>

// typedef QueueHandle_t SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex()
{
    printf("xSemaphoreCreateRecursiveMutex\n");
    return reinterpret_cast<SemaphoreHandle_t>(new std::recursive_mutex);
}

#define vSemaphoreDelete(xSemaphore) vQueueDelete((QueueHandle_t)(xSemaphore))

inline BaseType_t mockTakeRecursive(SemaphoreHandle_t mutex, uint32_t)
{
    reinterpret_cast<std::recursive_mutex *>(mutex)->lock();
    return pdTRUE;
}
inline BaseType_t mockGiveRecursive(SemaphoreHandle_t mutex)
{
    reinterpret_cast<std::recursive_mutex *>(mutex)->unlock();
    return pdTRUE;
}
#define xSemaphoreTakeRecursive(xMutex, xBlockTime) mockTakeRecursive((xMutex), (xBlockTime))

#define xSemaphoreGiveRecursive(xMutex) mockGiveRecursive((xMutex))

inline uint32_t xSemaphoreTakeValue = pdFALSE;
inline uint32_t xSemaphoreTake(SemaphoreHandle_t, uint32_t)
{
    return xSemaphoreTakeValue;
}

inline bool xSemaphoreGiveCalled = pdFALSE;
inline void xSemaphoreGive(SemaphoreHandle_t)
{
    xSemaphoreGiveCalled = pdTRUE;
}

inline SemaphoreHandle_t xSemaphoreCreateMutexStaticValue=(SemaphoreHandle_t)(0x1);
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *t)
{
    printf("xSemaphoreCreateMutexStatic\n");
    return xSemaphoreCreateMutexStaticValue;
}

inline SemaphoreHandle_t xSemaphoreCreateMutexValue=(SemaphoreHandle_t)(0x1);
inline SemaphoreHandle_t xSemaphoreCreateMutex()
{
    printf("xSemaphoreCreateMutex\n");
    return xSemaphoreCreateMutexValue;
}
