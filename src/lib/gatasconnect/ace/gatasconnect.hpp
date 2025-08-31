#pragma once

#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "timers.h"

/* LwIP */
#include "lwip/udp.h"

/* ETLCPP */
#include "etl/message_bus.h"

/* GaTas */
#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/cobsstreamhandler.hpp"

/**
 * GatasConnect protocol for EFB's that only support GatasConnect.
 * THis protocol is currently not recommende if you can also use GDL90, if you need to use NMEA and have BLE, use that
 */
class GatasConnect : public BaseModule, public etl::message_router<GatasConnect, GATAS::WifiConnectionStateMsg, GATAS::OwnshipPositionMsg, GATAS::ConfigUpdatedMsg, GATAS::GpsStatsMsg>
{
    friend class message_router;
    struct
    {
        uint32_t bytesReceived = 0;
        uint32_t bytesSend = 0;
        uint32_t bufferAllocErr = 0;
        uint32_t msgSendFailed = 0;
    } statistics;

    bool wifiConnected;

    udp_pcb *pcbSend;
    TimerHandle_t requestTimer = nullptr;
    etl::atomic<GATAS::OwnshipPositionInfo> ownshipPosition;
    GATAS::Config::IpPort gatasServer = {IPADDR_NONE, 0};
    CobsStreamHandler cobsStreamHandler;

    uint64_t gatasId;
    uint32_t currentAddress;
    etl::vector<uint32_t, GATAS::MAX_AIRCRAFT_CONFIGURATIONS> allIcaoAddresses;
    bool hasGpsFix;
private:
    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    void on_receive(const GATAS::DataPortMsg &msg);

    void on_receive(const GATAS::WifiConnectionStateMsg &wcs);

    void on_receive(const GATAS::GpsStatsMsg &msg);

    void on_receive_unknown(const etl::imessage &msg);

    void on_receive(const GATAS::OwnshipPositionMsg &msg);

    void on_receive(const GATAS::ConfigUpdatedMsg &msg);

    static void requestTimerCallback(TimerHandle_t xTimer);
    static void receiveUdpMessage(void *arg, struct udp_pcb *pcb,
                           struct pbuf *p, const ip_addr_t *addr, u16_t port);

    void getConfig(const Configuration &config);
public:
    static constexpr const char *NAME = "GatasConnect";
    GatasConnect(etl::imessage_bus &bus,  Configuration &config) : BaseModule(bus, NAME), wifiConnected(false), pcbSend(nullptr), cobsStreamHandler(CobsStreamHandler(bus, config)), hasGpsFix(false)
    {
        (void)config;
        getConfig(config);
    }
    
    virtual ~GatasConnect() = default;
};
