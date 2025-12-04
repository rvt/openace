#pragma once

#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "timers.h"

/* ETLCPP */
#include "etl/message_bus.h"

/* GaTas */
#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/cobsstreamhandler.hpp"
#include "ace/tcpbufferedclient.hpp"
#include "ace/delimiterbitmap.hpp"

/**
 * GatasConnect protocol for EFB's that only support GatasConnect.
 * THis protocol is currently not recommende if you can also use GDL90, if you need to use NMEA and have BLE, use that
 */
class GatasConnect : public BaseModule, public etl::message_router<GatasConnect, GATAS::WifiConnectionStateMsg, GATAS::OwnshipPositionMsg, GATAS::ConfigUpdatedMsg, GATAS::GpsStatsMsg>
{
    friend class message_router;
    // THis values have been empirical tested on a Red Mi Note 10
    static constexpr size_t ANDROIDHOTSPOT_FIX_HIGHMARK = 128*3;
    static constexpr size_t ANDROIDHOTSPOT_FIX_LOWMARK = 160;

    struct
    {
        uint32_t bytesReceived = 0;
        uint32_t bytesSend = 0;
        uint32_t pkgReceived = 0;
        uint32_t pkgSend = 0;
        uint32_t msgSendFailed = 0;
        uint8_t startedCounter = 0;
        bool hasConnection = false;
    } statistics;

    bool wifiConnected;
    TimerHandle_t requestTimer = nullptr;
    GATAS::Config::IpPort gatasServer = {IPADDR_NONE, 0};
    uint64_t gatasId;
    uint32_t icaoAddress;
    uint32_t gatasIp;
    bool hasGpsFix;
    // Fixes an issue with the Android Hotspot that would not route any small packages
    // When this is set to true, larger packages are send
    bool androidHotspotFix;
    size_t androidHotspotCurrentMark;
    uint32_t spinLock;

    etl::vector<uint32_t, GATAS::MAX_AIRCRAFT_CONFIG> allIcaoAddresses;
    GATAS::OwnshipPositionInfo ownshipPosition;
    CobsStreamHandler cobsStreamHandler;
    TcpClient tcpClient;

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
    void tcpReceiveHandler(etl::span<uint8_t> data);
    void tcpSentHandler(uint16_t len);

    void getConfig(const Configuration &config);

public:
    static constexpr const char *NAME = "GatasConnect";
    GatasConnect(etl::imessage_bus &bus, Configuration &config) : BaseModule(bus, NAME),
                                                                  wifiConnected(false),
                                                                  requestTimer(nullptr),
                                                                  gatasServer{IPADDR_NONE, 0},
                                                                  gatasId(0),
                                                                  icaoAddress(0),
                                                                  gatasIp(0),
                                                                  hasGpsFix(false),
                                                                  androidHotspotFix(true),
                                                                  androidHotspotCurrentMark(ANDROIDHOTSPOT_FIX_HIGHMARK),
                                                                  spinLock(0),
                                                                   ownshipPosition{},
                                                                  cobsStreamHandler(CobsStreamHandler(bus, config)),
                                                                  tcpClient(
                                                                      GATAS::Config::IpPort{config.ipPortBypath(NAME, "gatasServer").ip,  GATAS_CONNECT_PORT},
                                                                      TcpClient::ReceiveHandler::create<GatasConnect, &GatasConnect::tcpReceiveHandler>(*this),
                                                                      TcpClient::SentHandler::create<GatasConnect, &GatasConnect::tcpSentHandler>(*this))
    {
        spinLock = SpinlockGuard::claim();
        getConfig(config);
        tcpClient.autoConnect(true);
    }

    virtual ~GatasConnect() = default;
};
