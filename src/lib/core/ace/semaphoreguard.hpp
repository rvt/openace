#pragma once

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "semphr.h"

template <bool RECURSIVE = false>
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
    SemaphoreGuard(uint32_t waitMs, SemaphoreHandle_t &sem_) : sem(sem_), acquired(RECURSIVE ? xSemaphoreTakeRecursive(sem, TASK_DELAY_MS(waitMs)) == pdTRUE : xSemaphoreTake(sem, TASK_DELAY_MS(waitMs)) == pdTRUE) {}

    ~SemaphoreGuard()
    {
        if (acquired)
        {
            if (RECURSIVE)
            {
                xSemaphoreGiveRecursive(sem);
            }
            else
            {
                xSemaphoreGive(sem);
            }
        }
        else
        {
#if GATAS_DEBUG == 1
            printf("SemaphoreGuard not aquired");
#endif
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
        else
        {
            {
#if GATAS_DEBUG == 1
                printf("SpiGuard not aquired");
#endif
            }
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