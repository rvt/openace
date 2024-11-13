#pragma once

/* System. */
#include <stdint.h>

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* PICO. */
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

/* Vendor. */
#include "etl/message_bus.h"
#include "etl/function.h"
#include "etl/delegate.h"

/* OpenACE. */
#include "ace/constants.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"




/**
* Class that is responsible for managing the Single SPI bus between various devices
*/
class AceSpi : public SpiModule, public etl::message_router<AceSpi>
{
private:
    // Maximum time a module can hold the spi bus
    static constexpr uint16_t INIT_SPI_MAX_WAIT = 100;
    struct ConsumerRequest
    {
        uint8_t busFrequency;
        uint32_t notificationValue;
        TaskHandle_t taskHandle;
        //Constructor
        ConsumerRequest(uint8_t busFrequency_, TaskHandle_t taskHandle_, uint32_t bits_) : busFrequency(busFrequency_), notificationValue(SpiModule::SPI_BUS_READY | bits_), taskHandle(taskHandle_) {}
        ConsumerRequest() : busFrequency(0), notificationValue(SpiModule::SPI_BUS_READY), taskHandle(nullptr) {}
    };
    static constexpr uint8_t READ_BIT = 0x80;
    const uint8_t clk;
    const uint8_t mosi;
    const uint8_t miso;
    const uint8_t rst;
    QueueHandle_t spiConsumerQueue;
    TaskHandle_t taskHandle;
public:
    static constexpr const etl::string_view NAME = "AceSpi";
    AceSpi(etl::imessage_bus& bus, const OpenAce::PinTypeMap &pins) : SpiModule(bus),
        clk(pins.at(OpenAce::PinType::CLK)),
        mosi(pins.at(OpenAce::PinType::MOSI)),
        miso(pins.at(OpenAce::PinType::MISO)),
        rst(pins.at(OpenAce::PinType::RST)),
        spiConsumerQueue(nullptr),
        taskHandle(nullptr)
    {
    }
    AceSpi(etl::imessage_bus& bus, const Configuration &config) : AceSpi(bus, config.pinMap(NAME))
    {
    }

    virtual ~AceSpi() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override
    {
        xTaskCreate(aceSpiTask, "AceSpiTask", configMINIMAL_STACK_SIZE+128, this, tskIDLE_PRIORITY + 1, &taskHandle);
        getBus().subscribe(*this);
    };

    virtual void stop() override
    {
        getBus().unsubscribe(*this);
        vTaskDelete(taskHandle);
    };

    virtual void read_registers(uint8_t cs, uint8_t reg, uint8_t *buf, uint16_t len, uint8_t delayMs) const override
    {
        reg |= READ_BIT;
        cs_select(cs);
        spi_write_blocking(OPENACE_SPI_DEFAULT, &reg, 1);
        vTaskDelay(TASK_DELAY_MS(delayMs));
        spi_read_blocking(OPENACE_SPI_DEFAULT, 0, buf, len);
        cs_deselect(cs);
        vTaskDelay(TASK_DELAY_MS(delayMs));
    }

    virtual void read_registers_select(uint8_t cs, uint8_t reg) const override
    {
        reg |= READ_BIT;
        cs_select(cs);
        spi_write_blocking(OPENACE_SPI_DEFAULT, &reg, 1);
    }

    virtual void read_registers_read(uint8_t cs, uint8_t *buf, uint16_t len) const override
    {
        spi_read_blocking(OPENACE_SPI_DEFAULT, 0, buf, len);
        cs_deselect(cs);
    }

    virtual void cs_select(uint8_t cs) const override
    {
        asm volatile("nop \n nop \n nop");
        gpio_put(cs, 0);  // Active low
        asm volatile("nop \n nop \n nop");
    }

    virtual void cs_deselect(uint8_t cs) const override
    {
        asm volatile("nop \n nop \n nop");
        gpio_put(cs, 1);
        asm volatile("nop \n nop \n nop");
    }

    virtual void write_array(uint8_t cs, uint8_t *data, uint8_t length, uint8_t delayMs) const override
    {
        cs_select(cs);
        spi_write_blocking(OPENACE_SPI_DEFAULT, data, length);
        cs_deselect(cs);
        vTaskDelay(TASK_DELAY_MS(delayMs));
    }

    virtual void write_byte(uint8_t cs, uint8_t data, uint8_t delayMs) const override
    {
        write_array(cs, &data, 1, delayMs);
    }

    /**
     * Client registering to be called every xxms
    */
    virtual bool aquireSlot(uint8_t busFrequencyMhz, TaskHandle_t consumerHandle, uint32_t bits=0) const override;

    virtual void releaseSlot() const override
    {
        xTaskNotifyGive(taskHandle);
    }

    /**
     * Reset all attached devices that is using the rst pin
    */
    void resetDevices() const
    {
        gpio_put(rst, 0);
        vTaskDelay(TASK_DELAY_MS(100));
        gpio_put(rst, 1);
    }


    static void aceSpiTask(void *arg);

    void on_receive_unknown(const etl::imessage& msg);
};
