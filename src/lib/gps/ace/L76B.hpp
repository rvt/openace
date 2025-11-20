#pragma once

#include <stdint.h>
#include "AbstractGnss.hpp"
#include "etl/message_router.h"

class L76B : public AbstractGnss, public etl::message_router<L76B>
{
private:
    static constexpr uint32_t REQUIRED_GPS_BAUDRATE = 115200; // If you change this, you need to change the baudrate in the ublox config as well
    static constexpr uint32_t softPPSlagUs = 250'000;         // PMTK255,1 // When SoftPPS is used, use this as a lag compensation in microseconds

    friend class message_router;

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    virtual bool configureGnss() override;

    /** 
     * Pop messages And wait for a specific sentence
     */
    void popGPSMessages(etl::string_view waitFor="");

public:
    static constexpr const etl::string_view NAME = "L76B";
    L76B(etl::imessage_bus &bus, const GATAS::PinTypeMap &pins, bool softPPS) : AbstractGnss(bus, NAME, pins, softPPS, softPPSlagUs)
    {
    }

    L76B(etl::imessage_bus &bus, const Configuration &config) : L76B(bus, config.pinMap(NAME), config.valueByPath(1, NAME, "softPPS"))
    {
    }

    virtual ~L76B() = default;
};
