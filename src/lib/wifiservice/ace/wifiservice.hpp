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
 * AP Mode: You connect to GA/TAS
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
class WifiService : public BaseModule, public etl::message_router<WifiService, GATAS::IdleMsg>
{
private:
static constexpr uint8_t NUMBER_OF_CONNECTION_ATTEMPTS = 2; // Number of times connection to the same network is attempted before trying an other network
static constexpr uint8_t NUMBER_OF_SCAN_ATTEMPTS = 2; // Number is scans done and if no known network is found, create the AP

    friend class message_router;
    GATAS::Config::WifiServiceData wifiData;

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

    dhcp_server_t dhcp_server;
    dns_server_t dns_server;

    TaskHandle_t taskHandle;
    TimerHandle_t timerHandle;

    uint8_t networkConnectionAttempt; 
    uint8_t totalScanAttempt; 

    // set of all known networks found during scan
    etl::set<GATAS::SsidOrPasswdStr, 4> scanResult;

    static void wifiTask(void *arg);

    void startAccessPoint();
    void stopAccessPoint();
    /**
     * When Access Point is active, GaTas will keep track of all clients connecting to it
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
    bool checkIfClientActive(int itf);

    void mDnsInit();
    void mDnsDeinit();
    void showSsidPwdIp(const etl::string_view &ssid, const etl::string_view &password, bool ap) const;
    void on_receive_unknown(const etl::imessage &msg);
    void on_receive(const GATAS::IdleMsg &msg);

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
        networkConnectionAttempt(0),
        totalScanAttempt(0)
    {
    }

    virtual ~WifiService() = default;

    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

   /**
     * Get's the current local IP address
     */
    static ip4_addr_t getIpAddr();

};
