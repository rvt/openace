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
#include "ace/spinlockguard.hpp"

/**
 * GatasConnect protocol for EFB's that only support GatasConnect.
 * THis protocol is currently not recommende if you can also use GDL90, if you need to use NMEA and have BLE, use that
 * NOTE: This did not work well due to Android and possibleiOS and perhaps even telecom providers filtering to much UDP
 * However, it did work fine using a local WIFI network...
 */
class GatasConnect : public BaseModule, public etl::message_router<GatasConnect, GATAS::WifiConnectionStateMsg, GATAS::OwnshipPositionMsg, GATAS::ConfigUpdatedMsg, GATAS::GpsStatsMsg>
{
    friend class message_router;
    // THis is used to fix a android HotSpot issue where small packages are not routed.
    // It sends a large and a small package size to ensure that Android sees them as important
    static constexpr size_t ANDROIDHOTSPOT_FIX_HIGHMARK = 128*3;
    static constexpr size_t ANDROIDHOTSPOT_FIX_LOWMARK = 160;
    // After how many 'sends' the system sends larger packages again
    static constexpr uint8_t ANDROIDHOTSPOT_COUNT_UNTILL_HIGH = 5;
    // Seconds to wait accepting packages after a local confiugration change
    static constexpr uint8_t LOCALCONFIGURATIONCHANGE_HOLD_BACK = 5;

    struct
    {
        uint32_t bytesReceived = 0;
        uint32_t bytesSend = 0;
        uint32_t pkgReceived = 0;
        uint32_t pkgSend = 0;
        uint32_t bufferAllocErr = 0;
        uint32_t msgSendFailed = 0;
        bool hasConnection = false;
    } statistics;

    bool wifiConnected;
    int spinLock;
    bool androidHotspotFix;
    uint64_t gatasId;
    bool hasGpsFix;
    uint8_t lastSendCounter;
    TimerHandle_t requestTimer = nullptr;

    uint32_t icaoAddress;
    uint32_t gatasIp;
    uint32_t pinCode;
    // Stop accepting data from gatasConnect for LOCALCONFIGURATIONCHANGE_HOLD_BACK to ensure the server accepts any new configuration 
    // So we can disgard packages in transit. Can possibly be redesigned when a package counter can be send?
    uint8_t localConfigurationUpdateCnt; 
    udp_pcb *pcbSend;
    CobsStreamHandler cobsStreamHandler;

    etl::vector<uint32_t, GATAS::MAX_AIRCRAFT_CONFIG> allIcaoAddresses;
    GATAS::Config::IpPort gatasServer = {IPADDR_NONE, 0};
    GATAS::OwnshipPositionInfo ownshipPosition;
private:
    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;

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
    GatasConnect(etl::imessage_bus &bus,  Configuration &config) : BaseModule(bus, NAME),
    wifiConnected(false),
    spinLock(SpinlockGuard::claim()),
    androidHotspotFix(true),
    gatasId(0),
    hasGpsFix(false),
    lastSendCounter(0),
    requestTimer(nullptr),
    icaoAddress(0),
    gatasIp(0),
    pinCode(0),
    localConfigurationUpdateCnt(0),
    pcbSend(nullptr),
    cobsStreamHandler(CobsStreamHandler(bus, config)),
    ownshipPosition{}
    {
        (void)config;
        getConfig(config);
    }
    
    virtual ~GatasConnect() = default;
};
