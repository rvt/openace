#include "FreeRTOS.h"
#include "timers.h"

#include "etl/map.h"
#include "etl/message_bus.h"
#include "etl/string.h"
#include "etl/set.h"

#include "pico/cyw43_arch.h"
#include "pico/lwip_freertos.h"
#include "pico/stdlib.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"

#include "../dhcpserver/dhcpserver.h"
#include "../dnsserver/dnsserver.h"

/**
 * WifiService handles both Access Point and Client Mode
 * AP Mode: You connect to OpenAce
 * Client Mode: Open Ace connects to you
 *
 * It handles up to 4 network clients, process of connecting
 * 1) Scan for networks and match them with configured networks (this means hidden SSID's are currenty not supported)
 * 2) For each network:
 * 3) If the connection fails with bad auth, try next network
 * 4) If connection fails, try again up to 3 times
 * 5) When no client can be connected, setup an Access Point
 *
 * - Future ideas:
 * 1) Allow hidden SSID's
 * 2) Allow to disable an client SSID
 * 3) Allow for client mode only, never set up an AP, just keep trying
 * 4) Open Portal in AP mode
 * 5) Scan the network for any devices (ping?? openPort??) and send GDL90 to these devices found?
 * 6) When in AP mode, restart client mode with a button
 */
class WifiService : public BaseModule, public etl::message_router<WifiService>
{
private:
    friend class message_router;
    OpenAce::Config::WifiServiceData wifiData;

    enum ConnectionState
    {
        START,
        ENABLESTA,

        WIFISCAN,
        WIFISCANNING,

        TRYCLIENTCONNECT,
        CLIENTMODESTARTED,
        CLIENTMODESTOPPED,
        DISABLESTA,

        APMODESTART,
        APSTARTED,
        APSTOPPED,
    };

    ConnectionState connectionState;
    // using ConnectionOrder = etl::array<const ConnectionState, 3>;
    // static constexpr ConnectionOrder connectionState = {ConnectionState::CLIENTMODE, ConnectionState::APMODE};
    // ConnectionOrder::iterator connectionStateIt;

    dhcp_server_t dhcp_server;
    dns_server_t dns_server;

    TaskHandle_t taskHandle;
    TimerHandle_t timerHandle;

    uint8_t connectionAttempt; 

    // Producer Consumer queue to handle data between this task and the send task
    etl::set<OpenAce::SsidOrPasswdStr, 4> scanResult;

    static void wifiTask(void *arg);

    void startAccessPoint();
    void stopAccessPoint();
    /**
     * When Access Point is active, OpenAce will keep track of all clients connecting to it
     * It will send regular AccessPointClientsMsg not notify any modules like GDL90
     */
    void accessPointConnectionScanner();

    static int scanResultCb(void *env, const cyw43_ev_scan_result_t *result);
    bool scanRunning();
    void enableSta();
    void disableSta();
    void startWifiScan();
    uint8_t connectClient();
    void stopClient();
    bool checkIfClientActive();

    void mDnsInit();
    void mDnsDeinit();
    void showSsidPwdIp(const etl::string_view &ssid, const etl::string_view &password, bool ap) const;

public:
    static constexpr const etl::string_view NAME = "WifiService";
    enum TaskState : uint8_t
    {
        SHUTDOWN = 1 << 0,
        TIMER = 1 << 1
    };

    WifiService(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
        wifiData(config.wifiService()),
        connectionState(ConnectionState::START),
        taskHandle(nullptr),
        timerHandle(nullptr),
        connectionAttempt(1)
    {
    }

    virtual ~WifiService() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    void on_receive_unknown(const etl::imessage &msg);
};
