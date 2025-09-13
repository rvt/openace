#include "../basemodule.hpp"
#include "../semaphoreguard.hpp"
/* FreeRTOS. */
#include "FreeRTOS.h"
#include "task.h"

/* PICO. */
#include "hardware/gpio.h"

/* GaTas */
#include "../coreutils.hpp"

const char *postConstructToString(GATAS::PostConstruct value)
{
    switch (value)
    {
    case GATAS::PostConstruct::OK:
        return "Ok";
    case GATAS::PostConstruct::FAILED:
        return "GATAS::PostConstruct failed";
    case GATAS::PostConstruct::MEMORY:
        return "Memory error";
    case GATAS::PostConstruct::DEP_NOT_FOUND:
        return "A dependency was not found";
    case GATAS::PostConstruct::XQUEUE_ERROR:
        return "xQueue error";
    case GATAS::PostConstruct::TASK_ERROR:
        return "Task Error";
    case GATAS::PostConstruct::HARDWARE_NOT_FOUND:
        return "Hardware not found";
    case GATAS::PostConstruct::HARDWARE_ERROR:
        return "Hardware error";
    case GATAS::PostConstruct::NETWORK_ERROR:
        return "network error";
    case GATAS::PostConstruct::CONFIG_ERROR:
        return "Configuration error";
    case GATAS::PostConstruct::MUTEX_ERROR:
        return "Failed to create mutex";
    case GATAS::PostConstruct::TIMER_ERROR:
        return "Timer error";
    case GATAS::PostConstruct::HARDWARE_NOT_CONFIGURED:
        return "Hardware configuration not found";
    default:
        return "unknown error";
    }
}

BaseModule::BaseModule(etl::imessage_bus &bus_, const etl::string_view name_) : bus(bus_), moduleName(name_)
{
    // Tech Depth: For now disabled. However, Modules are only created single core during startup
    // This is for now because the Config module is initialised before everything else
    // This propely requires some (smallish) refactoring of the COnfig module
    //    if (xSemaphoreTakeRecursive(BaseModule::xMutex, portMAX_DELAY) == pdTRUE)
    {
        if (BaseModule::moduleLoaderMap.full())
        {
            printf("WARNING: More than %d modules registered\n", MAX_MODULES);
            panic("Too many modules");
        }
        else
        {
            // printf("Registering: %p %s ", this, name);
            BaseModule::moduleLoaderMap[name_].module = this;
        }
        //        xSemaphoreGiveRecursive(BaseModule::xMutex);
    }
}

BaseModule *BaseModule::moduleByName(const BaseModule &that, const etl::string_view requesting)
{
    // printf("Looking %s depends on %s\n", that.name(), requesting);
    // Look for it's direct name ex:AceSpi
    if (auto guard = SemaphoreGuard<portMAX_DELAY>(BaseModule::xMutex))
    {

        if (BaseModule::moduleLoaderMap.contains(requesting))
        {
            auto module = BaseModule::moduleLoaderMap[requesting];
            if (module.result == GATAS::PostConstruct::OK)
            {
                return module.module;
            }
        }
        // Look for it's provider name eg: _SPI
        for (auto it = BaseModule::moduleLoaderMap.cbegin(); it != BaseModule::moduleLoaderMap.cend(); it++)
        {
            if (strcmp(it->second.module->name().cbegin(), requesting.cbegin()) == 0)
            {
                return it->second.module;
            }
        }
        printf("Module %s requests dependecy on %s but it is not registered\n", that.name().cbegin(), requesting.cbegin());
    }

    return nullptr;
}

void __isr __time_critical_func(BaseModule::gpioInterrupt)(uint pin, uint32_t event)
{
    // Handle the interrupt and call back over callback or task notification
    // printf("Pin %d event %d\n", pin, event);
    // Cannot wrap this in a mutex since when there is an interrupt we get an assert on suspend
    // This is in reality only an issue when modules are added/removed which is not expected during normal operation
    // BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (pinInterruptHandlers.contains(pin) && pinInterruptHandlers[pin].enabled)
    {
        pinInterruptHandler &iHandler = pinInterruptHandlers[pin];
        if ((iHandler.event & event) == iHandler.event)
        {
            if (iHandler.callback != nullptr)
            {
                iHandler.callback(event);
            }
            else
            {
                xTaskNotifyFromISR(iHandler.handler, iHandler.notificationValue, eSetBits, nullptr /* &xHigherPriorityTaskWoken*/);
            }
        }
    }
    // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * Register a pin interrupt handler with task notification
 */
void BaseModule::registerPinInterrupt(uint8_t pin, uint32_t events, TaskHandle_t handler, uint32_t notificationValue)
{
    if (auto guard = SemaphoreGuard<portMAX_DELAY>(BaseModule::xMutex))
    {
        if (pinInterruptHandlers.full())
        {
            panic("pinInterruptHandlers is full");
        }
        // TODO: https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#group_hardware_gpio_1ga6165f07f4b619dd08ea6dc97d069e78a
        // Might need to change this to gpio_add_raw_irq_handler
        gpio_set_irq_enabled_with_callback(pin, events, true, gpioInterrupt);
        pinInterruptHandlers[pin] = {events, handler, notificationValue};
    }
}

/**
 * Register a pin interrupt handler with callback function
 */
void BaseModule::registerPinInterrupt(uint8_t pin, uint32_t events, pinIntrCallback_t callback)
{
    if (auto guard = SemaphoreGuard<portMAX_DELAY>(BaseModule::xMutex))
    {
        if (pinInterruptHandlers.full())
        {
            panic("pinInterruptHandlers is full");
        }
        printf("Registering pin %d interrupt", pin);
        gpio_set_irq_enabled_with_callback(pin, events, true, gpioInterrupt);
        pinInterruptHandlers[pin] = {events, callback};
    }
}

/**
 * Disable the interrupt callback. The interrupt itself will not be disabled
 */
void BaseModule::disablePinInterrupt(uint8_t pin)
{
    pinInterruptHandlers[pin].enabled = false;
}

/**
 * Re-Enable the interrupt that was previously disabled with \sa disablePinInterrupt()
 */
void BaseModule::enablePinInterrupt(uint8_t pin, uint32_t notificationValue)
{
    pinInterruptHandlers[pin].enabled = true;
    pinInterruptHandlers[pin].notificationValue = notificationValue;
}


/**
 * Unregister a pin interrupt handler.
 * THis will remove the interrupt from the callbacks
 */
void BaseModule::unregisterPinInterrupt(uint8_t pin)
{
    if (auto guard = SemaphoreGuard<portMAX_DELAY>(BaseModule::xMutex))
    {
        gpio_set_irq_enabled(pin, 0x00, false);
        pinInterruptHandlers.erase(pin);
    }
}
