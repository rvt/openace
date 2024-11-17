#pragma once

/* System. */
#include <stdint.h>

/* FreeRTOS. */
#include "FreeRTOS.h"

/* PICO. */
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

/* Vendor. */
#include "etl/message_bus.h"

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
    static constexpr uint8_t READ_BIT = 0x80;
    const uint8_t clk;
    const uint8_t mosi;
    const uint8_t miso;
    const uint8_t rst;
    uint8_t lastBusFrequency;
    SemaphoreHandle_t mutex;
public:
    static constexpr const etl::string_view NAME = "AceSpi";
    AceSpi(etl::imessage_bus &bus, const OpenAce::PinTypeMap &pins) : SpiModule(bus),
                                                                      clk(pins.at(OpenAce::PinType::CLK)),
                                                                      mosi(pins.at(OpenAce::PinType::MOSI)),
                                                                      miso(pins.at(OpenAce::PinType::MISO)),
                                                                      rst(pins.at(OpenAce::PinType::RST)),
                                                                      lastBusFrequency(OPENOPENACE_SPI_DEFAULT_BUS_FREQUENCY)
    {
    }

    AceSpi(etl::imessage_bus &bus, const Configuration &config) : AceSpi(bus, config.pinMap(NAME))
    {
    }

    virtual ~AceSpi() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void read_registers(uint8_t cs, uint8_t reg, uint8_t *buf, uint16_t len, uint8_t delayMs) const override;

    virtual void read_registers_select(uint8_t cs, uint8_t reg) const override;

    virtual void read_registers_read(uint8_t cs, uint8_t *buf, uint16_t len) const override;

    virtual void cs_select(uint8_t cs) const override;

    virtual void cs_deselect(uint8_t cs) const override;

    virtual void write_array(uint8_t cs, uint8_t *data, uint8_t length, uint8_t delayMs) const override;

    virtual void write_byte(uint8_t cs, uint8_t data, uint8_t delayMs) const override;

    virtual bool acquireSlotSync(uint8_t busFrequencyMhz) override;
    virtual bool acquireSlotSyncCb(uint8_t busFrequencyMhz, const etl::delegate<void()>& delegate) override;

    virtual void releaseSlotSync() override;

    /**
     * Reset all attached devices that is using the rst pin
     */
    void resetDevices() const;

    void on_receive_unknown(const etl::imessage &msg);
};
