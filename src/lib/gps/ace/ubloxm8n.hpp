#pragma once

#include <stdint.h>

#include "AbstractGnss.hpp"

#include "etl/message_router.h"

class UbloxM8N : public AbstractGnss, public etl::message_router<UbloxM8N>
{
private:
    static constexpr uint32_t REQUIRED_GPS_BAUDRATE = 115200; // If you change this, you need to change the baudrate in the ublox config as well

    friend class message_router;

    void on_receive_unknown(const etl::imessage& msg)
    {
        (void)msg;
    }

    virtual bool configureGnss() override;

    /**
     * Future: When PPS is not receive, this value can be used as an average delay to at least get some time right
     */
    // virtual uint32_t delayUs() override {
    //     return 35000;
    // }

public:
    static constexpr const etl::string_view NAME = "UbloxM8N";
    UbloxM8N(etl::imessage_bus& bus, const GATAS::PinTypeMap& pins) : 
        AbstractGnss(bus, NAME, pins)
    {
    }

    UbloxM8N(etl::imessage_bus& bus, const Configuration &config)  : UbloxM8N(bus, config.pinMap(NAME))
    {
    }

    virtual ~UbloxM8N() = default;

};
