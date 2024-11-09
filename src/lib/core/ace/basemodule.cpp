#include "basemodule.hpp"

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "task.h"

/* PICO. */
#include "hardware/gpio.h"

/* OpenAce */
#include "coreutils.hpp"

const char *postConstructToString(OpenAce::PostConstruct value)
{
    switch (value)
    {
    case OpenAce::PostConstruct::OK:
        return "Ok";
    case OpenAce::PostConstruct::FAILED:
        return "OpenAce::PostConstruct failed";
    case OpenAce::PostConstruct::MEMORY:
        return "PMemory error";
    case OpenAce::PostConstruct::DEP_NOT_FOUND:
        return "PA dependency was not found";
    case OpenAce::PostConstruct::XQUEUE_ERROR:
        return "xQueue error";
    case OpenAce::PostConstruct::TASK_ERROR:
        return "Task Error";
    case OpenAce::PostConstruct::HARDWARE_NOT_FOUND:
        return "Hardware not found";
    case OpenAce::PostConstruct::HARDWARE_ERROR:
        return "Hardware error";
    case OpenAce::PostConstruct::NETWORK_ERROR:
        return "network error";
    case OpenAce::PostConstruct::CONFIG_ERROR:
        return "Configuration error";
    case OpenAce::PostConstruct::MUTEX_ERROR:
        return "Failed to create mutex";
    case OpenAce::PostConstruct::TIMER_ERROR:
        return "Timer error";
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

BaseModule *BaseModule::moduleByName(const BaseModule &that, const etl::string_view requesting, bool panicIfNotFound)
{
    // printf("Looking %s depends on %s\n", that.name(), requesting);
    // Look for it's direct name ex:AceSpi
    if (xSemaphoreTakeRecursive(BaseModule::xMutex, portMAX_DELAY) == pdTRUE)
    {

        if (BaseModule::moduleLoaderMap.contains(requesting))
        {
            auto module = BaseModule::moduleLoaderMap[requesting];
            if (module.result == OpenAce::PostConstruct::OK)
            {
                xSemaphoreGiveRecursive(BaseModule::xMutex);
                return module.module;
            }
        }
        // Look for it's provider name eg: _SPI
        for (auto it = BaseModule::moduleLoaderMap.cbegin(); it != BaseModule::moduleLoaderMap.cend(); it++)
        {
            if (strcmp(it->second.module->name().cbegin(), requesting.cbegin()) == 0)
            {

                xSemaphoreGiveRecursive(BaseModule::xMutex);
                return it->second.module;
                // if (it->second.result == OpenAce::PostConstruct::OK) {
                //     return it->second.module;
                // }
            }
        }
        if (panicIfNotFound)
        {
            printf("Module %s depends on %s but it is not registered\n", that.name().cbegin(), requesting.cbegin());
            panic("Module not found but required");
        }
        xSemaphoreGiveRecursive(BaseModule::xMutex);
    }

    return nullptr;
}

void __time_critical_func(BaseModule::gpioInterrupt)(uint pin, uint32_t event)
{
    // Handle the interrupt and call back over callback or task notification
    // printf("Pin %d event %d\n", pin, event);
    // Cannot wrap this in a mutex since when tehre is an imterrupt we get an asset on suspend
    // THis is in reality only an isue when modules are added/removed which is not expected during normal operation
    if (pinInteruptHandlers.contains(pin) && pinInteruptHandlers[pin].enabled)
    {
        pinInterruptHandler &iHandler = pinInteruptHandlers[pin];
        if (iHandler.callback && ((iHandler.event & event) == iHandler.event))
        {
            iHandler.callback(event);
        }
        else if ((iHandler.event & event) == iHandler.event)
        {
            // Takes about 3ms to reach the RX function in SX1262
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xTaskNotifyFromISR(iHandler.handler, iHandler.notificationValue, eSetBits, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

/**
 * Register a pin interupt handler with task notification
 */
void BaseModule::registerPinInterupt(uint8_t pin, uint32_t events, TaskHandle_t handler, uint32_t notificationValue)
{
    if (xSemaphoreTakeRecursive(BaseModule::xMutex, portMAX_DELAY) == pdTRUE)
    {
        if (pinInteruptHandlers.full())
        {
            panic("pinInteruptHandlers is full");
        }
        gpio_set_irq_enabled_with_callback(pin, events, true, gpioInterrupt);
        pinInteruptHandlers[pin] = {events, handler, notificationValue};
        xSemaphoreGiveRecursive(BaseModule::xMutex);
    }
}

/**
 * Register a pin interupt handler with callback function
 */
void BaseModule::registerPinInterupt(uint8_t pin, uint32_t events, pinIntrCallback_t callback)
{
    if (xSemaphoreTakeRecursive(BaseModule::xMutex, portMAX_DELAY) == pdTRUE)
    {
        if (pinInteruptHandlers.full())
        {
            panic("pinInteruptHandlers is full");
        }
        gpio_set_irq_enabled_with_callback(pin, events, true, gpioInterrupt);
        pinInteruptHandlers[pin] = {events, callback};
        xSemaphoreGiveRecursive(BaseModule::xMutex);
    }
}


void BaseModule::disablePinInterrupt(uint8_t pin) {
    pinInteruptHandlers[pin].enabled = false;
}

void BaseModule::enablePinInterrupt(uint8_t pin) {
    pinInteruptHandlers[pin].enabled = true;    
}

/**
 * Unregister a pin interupt handler
 */
void BaseModule::unregisterPinInterupt(uint8_t pin)
{
    if (xSemaphoreTakeRecursive(BaseModule::xMutex, portMAX_DELAY) == pdTRUE)
    {
        gpio_set_irq_enabled(pin, 0x00, false);
        pinInteruptHandlers.erase(pin);
        xSemaphoreGiveRecursive(BaseModule::xMutex);
    }
}
