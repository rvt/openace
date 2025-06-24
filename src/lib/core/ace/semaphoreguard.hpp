#pragma once

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "semphr.h"

template <uint32_t TIMEMS>
/**
 * @brief Clasic semaphore Guard based on a FreeRTOS mutex
 * 
 */
class SemaphoreGuard
{
private:
    SemaphoreHandle_t &sem;
    bool acquired;

public:
    SemaphoreGuard(SemaphoreHandle_t &sem_) : sem(sem_), acquired(xSemaphoreTake(sem, TASK_DELAY_MS(TIMEMS)) == pdTRUE) {}
    ~SemaphoreGuard()
    {
        if (acquired)
        {
            xSemaphoreGive(sem);
        }
    }

    SemaphoreGuard(const SemaphoreGuard &) = delete;
    SemaphoreGuard &operator=(const SemaphoreGuard &) = delete;
    SemaphoreGuard(SemaphoreGuard &&) = delete;
    SemaphoreGuard &operator=(SemaphoreGuard &&) = delete;

    operator bool() const
    {
        return acquired;
    }
};

class SpiGuard
{
private:
    SemaphoreHandle_t &sem;
    bool &acquired;

public:
    SpiGuard(SemaphoreHandle_t &sem_, bool &acquired_)
        : sem(sem_), acquired(acquired_)
    {
        acquired = xSemaphoreTake(sem, TASK_DELAY_MS(50)) == pdTRUE;
    }
    ~SpiGuard()
    {
        if (acquired)
        {
            xSemaphoreGive(sem);
        }
    }

    SpiGuard(const SpiGuard &) = delete;
    SpiGuard &operator=(const SpiGuard &) = delete;
    SpiGuard(SpiGuard &&) = delete;
    SpiGuard &operator=(SpiGuard &&) = delete;

    operator bool() const
    {
        return acquired;
    }
};