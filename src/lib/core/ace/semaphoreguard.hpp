
/* FreeRTOS. */
#include "FreeRTOS.h"
#include "semphr.h"

template <uint32_t TIMEMS>
struct SemaphoreGuard
{
    SemaphoreHandle_t &sem;
    bool acquired;
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

    operator bool() const
    {
        return acquired;
    }
};