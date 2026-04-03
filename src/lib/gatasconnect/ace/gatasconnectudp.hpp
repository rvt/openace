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
class GatasConnect : public BaseModule, public etl::message_router<GatasConnect, GATAS::WifiConnectionStateMsg, GATAS::OwnshipPositionMsg, GATAS::ConfigUpdatedMsg, GATAS::GpsStatsMsg, GATAS::IngressAircraftPositionMsg>
{
    friend class message_router;
    // THis is used to fix a android HotSpot issue where small packages are not routed.
    // It sends a large and a small package size to ensure that Android sees them as important
    static constexpr size_t ANDROIDHOTSPOT_FIX_HIGHMARK = 128*3;
    static constexpr size_t ANDROIDHOTSPOT_FIX_LOWMARK = 160;
    static constexpr bool ANDROIDHOTSPOT_FIX = true;
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

    bool wifiConnected = false;
    spin_lock_t * spinLock = SpinlockGuard::claim();
    uint64_t gatasId = 0;
    bool hasGpsFix = false;
    uint64_t lastRadioTrafficUs = 0;
    bool groundStation = false;
    uint8_t lastSendCounter = 0;
    TimerHandle_t requestTimer = nullptr;

    uint32_t icaoAddress = 0;
    uint32_t gatasIp = 0;
    uint32_t pinCode = 0;
    // Stop accepting data from gatasConnect for LOCALCONFIGURATIONCHANGE_HOLD_BACK to ensure the server accepts any new configuration
    // So we can disgard packages in transit. Can possibly be redesigned when a package counter can be send?
    uint8_t localConfigurationUpdateCnt = 0;
    udp_pcb *pcbSend = nullptr;
    CobsStreamHandler cobsStreamHandler;

    etl::vector<uint32_t, GATAS::MAX_AIRCRAFT_CONFIG> allIcaoAddresses;
    ip_addr_t gatasServerIPAddress = IPADDR4_INIT(IPADDR_NONE);
    GATAS::ConfigString gatasServerStr;
    GATAS::OwnshipPositionInfo ownshipPosition = {};

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

    void on_receive(const GATAS::IngressAircraftPositionMsg &msg);

    static void requestTimerCallbackTrampoline(TimerHandle_t xTimer);
    void requestTimerCallback(TimerHandle_t xTimer);
    static void receiveUdpMessage(void *arg, struct udp_pcb *pcb,
                           struct pbuf *p, const ip_addr_t *addr, u16_t port);

    void getConfig(const Configuration &config);
    bool resolveIP();
    static void resolveGatasServerCallback(const char *name, const ip_addr_t *ipaddr, void *arg);
public:
    static constexpr const char *NAME = "GatasConnect";
    GatasConnect(etl::imessage_bus &bus, Configuration &config) : BaseModule(bus, NAME),
    cobsStreamHandler(CobsStreamHandler(bus, config))
    {
        getConfig(config);
    }

    virtual ~GatasConnect() = default;
};
