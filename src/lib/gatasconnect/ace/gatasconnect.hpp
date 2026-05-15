#pragma once

#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "timers.h"

/* ETLCPP */
#include "etl/message_bus.h"

/* GaTas */
#include "ace/constants.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/binarymessages.hpp"
#include "cobsstreamhandler.hpp"

#include "ace/spinlockguard.hpp"

/**
 * Core GatasConnect protocol handling. This class only owns the COBS framing and
 * serializes/deserializes the binary protocol. Transport specific modules
 * subscribe to GatasConnectTx and publish GatasConnectRx.
 */
class GatasConnect : public BaseModule, public etl::message_router<GatasConnect, GATAS::WifiConnectionStateMsg, GATAS::OwnshipPositionMsg, GATAS::ConfigUpdatedMsg, GATAS::GpsStatsMsg, GATAS::IngressAircraftPositionMsg, GATAS::GatasConnectRx, GATAS::GdlMsg>
{
    friend class message_router;

    static constexpr uint8_t LOCALCONFIGURATIONCHANGE_HOLD_BACK = 5;

    bool hasGpsFix = false;
    uint64_t lastRadioTrafficUs = 0;
    uint8_t localConfigurationUpdateCnt = 0;
    TimerHandle_t requestTimer = nullptr;
    GATAS::GatasConnectOutput output = GATAS::GatasConnectOutput::UDP;
    bool gdl90BridgeEnabled = false;

    uint32_t icaoAddress = 0;
    uint32_t gatasIp = 0;
    uint32_t pinCode = 0;
    uint64_t gatasId = 0;
    bool groundStation = false;

    etl::vector<uint32_t, GATAS::MAX_AIRCRAFT_CONFIG> allIcaoAddresses;
    GATAS::OwnshipPositionInfo ownshipPosition = {};
    CobsStreamHandler cobsStreamHandler;

private:
    virtual GATAS::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    void on_receive_unknown(const etl::imessage &msg);
    void on_receive(const GATAS::WifiConnectionStateMsg &wcs);
    void on_receive(const GATAS::GpsStatsMsg &msg);
    void on_receive(const GATAS::OwnshipPositionMsg &msg);
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);
    void on_receive(const GATAS::IngressAircraftPositionMsg &msg);
    void on_receive(const GATAS::GatasConnectRx &msg);
    void on_receive(const GATAS::GdlMsg &msg);

    static void requestTimerCallbackTrampoline(TimerHandle_t xTimer);
    void requestTimerCallback(TimerHandle_t xTimer);

    void getConfig(const Configuration &config);
    void publishTx(GATAS::GatasConnectOutput output, const uint8_t *data, size_t length);

public:
    static constexpr const char *NAME = "GatasConnect";
    GatasConnect(etl::imessage_bus &bus, Configuration &config) : BaseModule(bus, NAME), cobsStreamHandler(CobsStreamHandler(bus, config))
    {
        getConfig(config);
    }

    virtual ~GatasConnect() = default;
};
