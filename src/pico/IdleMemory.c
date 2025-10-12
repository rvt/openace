#include "FreeRTOS.h"
#include <stdio.h>
#include <task.h>
#include "pico/time.h"

// /* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
// implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
// used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize, BaseType_t xCoreID)
{
    (void)xCoreID;
    /* If the buffers to be provided to the Idle task are declared inside this
    function then they must be declared static – otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE + 256];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task’s
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task’s stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE + 256;
}

// /* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
// application must provide an implementation of vApplicationGetTimerTaskMemory()
// to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    /* If the buffers to be provided to the Timer task are declared inside this
    function then they must be declared static – otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task’s state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task’s stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationMallocFailedHook(void)
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
        free memory available in the FreeRTOS heap.  pvPortMalloc() is called
        internally by FreeRTOS API functions that create tasks, queues, software
        timers, and semaphores.  The size of the FreeRTOS heap is set by the
        configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

    /* Force an assert. */
    puts("----------------------------------------------");
    puts("Malloc Failed");
    puts("----------------------------------------------");

    configASSERT((volatile void *)NULL);
}
// /*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    (void)pcTaskName;
    (void)pxTask;

    /* Run time stack overflow checking is performed if
        configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
        function is called if a stack overflow is detected. */

    puts("----------------------------------------------");
    printf("STACK OVERFLOW on %s\n", pcTaskName);
    puts("----------------------------------------------");
    taskDISABLE_INTERRUPTS();
    for (;;)
        ;

    /* Force an assert. */
    configASSERT((volatile void *)NULL);
}
// /*-----------------------------------------------------------*/

void vAssertCalled(const char *pcFile, uint32_t ulLine)
{
    puts("----------------------------------------------");
    printf("ASSERT FAILED %s line: %ld\n", pcFile, ulLine);
    puts("----------------------------------------------");
    taskDISABLE_INTERRUPTS();
    for (;;)
        ;
}

void vApplicationIdleHook(void)
{
}

void vApplicationTickHook(void)
{
}

static uint32_t start_time_us = 0;
void configureRuntimeStatsTimer(void) { start_time_us = time_us_32(); }
uint32_t getRuntimeCounterValue(void) { return time_us_32() - start_time_us; }
