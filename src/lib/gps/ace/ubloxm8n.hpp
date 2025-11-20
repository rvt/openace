#pragma once

#include <stdint.h>

#include "AbstractGnss.hpp"
#include "etl/message_router.h"

class UbloxM8N : public AbstractGnss, public etl::message_router<UbloxM8N>
{
private:
    static constexpr uint32_t REQUIRED_GPS_BAUDRATE = 115200; // If you change this, you need to change the baudrate in the ublox config as well
    static constexpr uint32_t softPPSlagUs = 27'000;          // When SoftPPS is used, use this as a lag compensation in microseconds

    friend class message_router;

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    virtual bool configureGnss() override;

public:
    static constexpr const etl::string_view NAME = "UbloxM8N";
    UbloxM8N(etl::imessage_bus &bus, const GATAS::PinTypeMap &pins, bool softPPS) : AbstractGnss(bus, NAME, pins, softPPS, softPPSlagUs)
    {
    }

    UbloxM8N(etl::imessage_bus &bus, const Configuration &config) : UbloxM8N(bus, config.pinMap(NAME), config.valueByPath(1, NAME, "softPPS"))
    {
    }

    virtual ~UbloxM8N() = default;
};
