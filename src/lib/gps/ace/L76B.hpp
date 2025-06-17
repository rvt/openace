#pragma once

#include <stdint.h>
#include "AbstractGnss.hpp"
#include "etl/message_router.h"

class L76B : public AbstractGnss, public etl::message_router<L76B>
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
    //     return 290000; // Average value as observed
    // }

public:
    static constexpr const etl::string_view NAME = "L76B";
    L76B(etl::imessage_bus& bus, const GATAS::PinTypeMap& pins) : 
        AbstractGnss(bus, NAME, pins)
    {
    }

    L76B(etl::imessage_bus& bus, const Configuration &config)  : L76B(bus, config.pinMap(NAME))
    {
    }

    virtual ~L76B() = default;

};


