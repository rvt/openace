#pragma once

#include <stdint.h>

#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "etl/message_bus.h"

#include "ace/constants.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"

/**
 * Client that can connect to a host and a port and expect to receive line terminated NMEA Messages
 * Part of this code taken from the example from Raspbery
 * Blink pattern:
 * if the rithm is every 3 seconds, the GPS has not yet a lock
 * Steady blink WIFI not connected
 * Two fast blinks, GAT/AS acts as a Access Point
 * One single fast blink, GA/TAS is in client mode and connected to a network
 */
class Idle : public BaseModule, public etl::message_router<Idle, GATAS::ConfigUpdatedMsg, GATAS::WifiConnectionStateMsg, GATAS::GpsStatsMsg>
{
    static constexpr uint8_t PATTERN_STEPS = 24;
    // clang-format off
    static constexpr uint8_t WIFI_NC_PATTERN =     0b00010101;
    static constexpr uint8_t WIFI_AP_PATTERN =     0b00001001;
    static constexpr uint8_t WIFI_CLIENT_PATTERN = 0b00000001;
    // clang-format on

    friend class message_router;
    TaskHandle_t taskHandle;
    int8_t ledStatusIndicatorPin;
    bool hasGpsFix;
    GATAS::WifiMode wifiMode;
    uint32_t blinkPattern;                     // Calculate blink Pattern created by /sa calculatePattern()
    uint8_t patternStep = Idle::PATTERN_STEPS; // Current pattern step
    uint32_t runningPattern;                   // Current patterns that is rotating
    repeating_timer_t timer;                   // callback timer struct
private:
    void on_receive(const GATAS::GpsStatsMsg &msg);
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);
    void on_receive(const GATAS::WifiConnectionStateMsg &wcs);
    void on_receive_unknown(const etl::imessage &msg);
    void calculatePattern();
    static void idleTask(void *arg);
    static bool blinkCb(repeating_timer_t *t);

public:
    static constexpr const etl::string_view NAME = "Idle";
    Idle(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
                                                                taskHandle(nullptr),
                                                                ledStatusIndicatorPin(-1),
                                                                hasGpsFix(false),
                                                                wifiMode(GATAS::WifiMode::NC),
                                                                blinkPattern(0b0000000111000111),
                                                                patternStep(Idle::PATTERN_STEPS),
                                                                runningPattern(0x11111111)
    {
        ledStatusIndicatorPin = config.valueByPath(26, "port5", "O0");
        (void)config;
    }

    virtual ~Idle() = default;

    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
};
