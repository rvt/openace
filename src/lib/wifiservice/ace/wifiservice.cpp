/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "wifiservice.hpp"
#include "lwip/apps/mdns.h"

OpenAce::PostConstruct WifiService::postConstruct()
{
    if (cyw43_arch_init())
    {
        return OpenAce::PostConstruct::HARDWARE_NOT_FOUND;
    }
    return OpenAce::PostConstruct::OK;
}

void WifiService::start()
{
    xTaskCreate(wifiTask, "wifiTask", configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY + 2, &taskHandle);
    getBus().subscribe(*this);
};

void WifiService::stop()
{
    getBus().unsubscribe(*this);
}

void WifiService::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void WifiService::wifiTask(void *arg)
{
    uint32_t startScan = 0;

    WifiService *wifiService = (WifiService *)arg;
    while (true)
    {
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(1'000));
        if (notifyValue != 0)
        {
            if (notifyValue & TaskState::SHUTDOWN)
            {
                vTaskDelete(nullptr);
                return;
            }
        }
        else
        {
            // printf("State: %d\n", wifiService->connectionState);
            // This switch acts like a state machine:
            switch (wifiService->connectionState)
            {

            case ConnectionState::START:
                wifiService->connectionState = ConnectionState::ENABLESTA;
                break;

            case ConnectionState::ENABLESTA:
                wifiService->enableSta();
                wifiService->connectionState = ConnectionState::WIFISCAN;
                break;

            case ConnectionState::WIFISCAN:
                startScan = CoreUtils::timeUs32() + (OPENACE_WIFISERVICE_MAX_SCAN_TIME_MS * 1'000);
                wifiService->startWifiScan();
                wifiService->connectionState = ConnectionState::WIFISCANNING;
                break;

            case ConnectionState::WIFISCANNING:
                if (!wifiService->scanRunning())
                {
                    wifiService->connectionState = ConnectionState::TRYCLIENTCONNECT;
                }
                // If for whatever reason WIFI scan does not find any network, then stop scanning after OPENACE_WIFISERVICE_MAX_SCAN_TIME_MS
                if (CoreUtils::isUsReached(startScan))
                {
                    wifiService->connectionState = ConnectionState::APMODESTART;
                    cyw43_wifi_leave(&cyw43_state, 0);
                    wifiService->disableSta();
                    wifiService->connectionState = ConnectionState::APMODESTART;
                }
                break;

            case ConnectionState::TRYCLIENTCONNECT:
                // wifi_leave seems to be required for more reliable connections, I don't know why...
                cyw43_wifi_leave(&cyw43_state, 0);
                {
                    auto cResult = wifiService->connectClient();
                    if (cResult == 0)
                    {
                        wifiService->mDnsInit();
                        wifiService->connectionState = ConnectionState::CLIENTMODESTARTED;
                    }
                    else
                    {
                        if (wifiService->scanResult.empty())
                        {
                            wifiService->disableSta();
                            wifiService->connectionState = ConnectionState::APMODESTART;
                        }
                    }
                }
                break;

            case ConnectionState::CLIENTMODESTARTED:
                if (!wifiService->checkIfClientActive(CYW43_ITF_STA))
                {
                    wifiService->mDnsDeinit();
                    wifiService->connectionState = ConnectionState::CLIENTMODESTOPPED;
                }
                break;

            case ConnectionState::CLIENTMODESTOPPED:
                wifiService->connectionState = ConnectionState::WIFISCAN; // STA already enabled, so just scan for clients
                break;

            case ConnectionState::APMODESTART:
                wifiService->startAccessPoint();
                wifiService->mDnsInit();
                wifiService->connectionState = ConnectionState::APSTARTED;
                break;

            case ConnectionState::APSTARTED:
                static uint8_t count = 0;

                // every 5 seconds
                if (count++ > 5)
                {
                    count = 0;
                    wifiService->accessPointConnectionScanner();
                }
                break;

            case ConnectionState::APSTOPPED:
                wifiService->connectionState = ConnectionState::WIFISCAN;
                wifiService->getBus().receive(OpenAce::WifiConnectionStateMsg{false});
                break;

            default:
                // Handle unknown states if needed
                break;
            }
        }
    }
}

void WifiService::accessPointConnectionScanner()
{
    auto cyw43TickMs = cyw43_hal_ticks_ms();
    // Generate a lit of connected clients and send it
    etl::set<uint32_t, OPENACE_MAXIMUM_TCP_CLIENTS> clients;
    for (uint8_t i = 0; i < DHCPS_MAX_IP; i++)
    {
        uint32_t expiry = dhcp_server.lease[i].expiry << 16 | 0xffff;
        if ((int32_t)(expiry - cyw43TickMs) > 0 && dhcp_server.lease[i].ip.addr != 0)
        {
            clients.insert(dhcp_server.lease[i].ip.addr);
        }
    }
    getBus().receive(OpenAce::AccessPointClientsMsg{clients});
}

void WifiService::startAccessPoint()
{
    // puts("Starting access point");
    cyw43_arch_enable_ap_mode(wifiData.ap.ssid.c_str(), wifiData.ap.password.c_str(), CYW43_AUTH_WPA2_AES_PSK);
    cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));

    ip4_addr_t mask;
    ip_addr_t gw;
    IP4_ADDR(ip_2_ip4(&gw), 192, 168, 1, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    dhcp_server_init(&dhcp_server, &gw, &mask);
    dns_server_init(&dns_server, &gw);

    showSsidPwdIp(wifiData.ap.ssid, wifiData.ap.password, true);
}

void WifiService::stopAccessPoint()
{
    dns_server_deinit(&dns_server);
    dhcp_server_deinit(&dhcp_server);
    cyw43_arch_disable_ap_mode();
}

int WifiService::scanResultCb(void *env, const cyw43_ev_scan_result_t *result)
{
    if (result)
    {
        // puts("Scan Result received");
        WifiService *service = (WifiService *)env;
        OpenAce::SsidOrPasswdStr name = (char *)result->ssid;

        auto it = etl::find_if(service->wifiData.clients.begin(), service->wifiData.clients.end(),
                               [&name](const OpenAce::Config::WifiNamePassword &client)
                               {
                                   return client.ssid == name;
                               });

        if (it != service->wifiData.clients.end() && !service->scanResult.full())
        {
            // printf("Added: %s\n", name.c_str());
            service->scanResult.insert(name);
        }

        // printf("ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
        //        result->ssid, result->rssi, result->channel,
        //        result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
        //        result->auth_mode);
    }
    return 0;
}

bool WifiService::scanRunning()
{
    return cyw43_wifi_scan_active(&cyw43_state);
}

void WifiService::startWifiScan()
{
    scanResult.empty();
    cyw43_wifi_scan_options_t scan_options;
    memset(&scan_options, 0, sizeof(cyw43_wifi_scan_options_t));
    cyw43_wifi_scan(&cyw43_state, &scan_options, (void *)this, scanResultCb);
}

/**
 * Try to connect to a client
 * returns
 *  0 Connection OK
 *  1 No Connection
 */
uint8_t WifiService::connectClient()
{
    constexpr uint8_t NUMBER_OF_CONNECTION_ATTEMPTS = 3;
    // Keeps track of the number of connections to the same network
    if (scanResult.empty())
    {
        return 1;
    }

    auto nameIt = scanResult.begin();
    auto it = etl::find_if(wifiData.clients.begin(), wifiData.clients.end(),
                           [&nameIt](const OpenAce::Config::WifiNamePassword &client)
                           {
                               return client.ssid == *nameIt;
                           });

    // Was this one found?
    if (it == wifiData.clients.end())
    {
        scanResult.erase(nameIt);
        return 1;
    }

    printf("WifiService: Client Connecting %s %s\n", it->ssid.c_str(), "<hidden>");
    auto result = cyw43_arch_wifi_connect_timeout_ms(it->ssid.c_str(), it->password.c_str(), CYW43_AUTH_WPA2_AES_PSK, 10000);
    //    printf("Result: %d\n", result);
    switch (result)
    {

    case PICO_OK:
        connectionAttempt = 0;
        showSsidPwdIp(it->ssid, "<hidden>", false);
        return 0;

    //    case PICO_ERROR_TIMEOUT:      // Timeout might mean that the network is just lost
    case PICO_ERROR_CONNECT_FAILED: // Seems to indicate that the network is available, but somehow could not connect
        if (connectionAttempt < NUMBER_OF_CONNECTION_ATTEMPTS)
        {
            connectionAttempt++;
        }
        return 1;
    default:
        connectionAttempt = 0;
        scanResult.erase(nameIt);
        return 1;
    }
}

bool WifiService::checkIfClientActive(int itf)
{
    // Periodically check the Wi-Fi connection status
    int status = cyw43_tcpip_link_status(&cyw43_state, itf);

    switch (status)
    {
    case CYW43_LINK_UP:
        return true;
    default:
        return false;
    }
}

void WifiService::enableSta()
{
    cyw43_arch_enable_sta_mode();
    cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));
}

void WifiService::disableSta()
{
    cyw43_arch_disable_sta_mode();
}

void WifiService::showSsidPwdIp(const etl::string_view &ssid, const etl::string_view &password, bool ap) const
{
    extern cyw43_t cyw43_state;
    uint32_t ip_addr;
    const char *mode;
    if (ap)
    {
        ip_addr = cyw43_state.netif[CYW43_ITF_AP].ip_addr.addr;
        mode = "Access Point";
    }
    else
    {
        ip_addr = cyw43_state.netif[CYW43_ITF_STA].ip_addr.addr;
        mode = "Client";
    }

    puts("###################################");
    printf("## Mode: %s\n## SSID: %s    Password: %s    IP: %lu.%lu.%lu.%lu\n", mode, ssid.begin(), password.begin(), ip_addr & 0xFF, (ip_addr >> 8) & 0xFF, (ip_addr >> 16) & 0xFF, ip_addr >> 24);
    puts("###################################");
}

// static void
// mdns_example_report(struct netif *netif, u8_t result, s8_t service)
// {
//     LWIP_PLATFORM_DIAG(("mdns status[netif %d][service %d]: %d\n", netif->num, service, result));
// }

#if LWIP_MDNS_RESPONDER == 1

static void
srv_txt(struct mdns_service *service, void *txt_userdata)
{
    err_t res;
    LWIP_UNUSED_ARG(txt_userdata);

    res = mdns_resp_add_service_txtitem(service, "path=/", 6);
    LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
}
#endif

void WifiService::mDnsInit()
{
#if LWIP_MDNS_RESPONDER == 1
    mdns_resp_init();

    mdns_resp_add_netif(netif_default, "openace");
    mdns_resp_add_service(netif_default, "myweb", "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
    mdns_resp_announce(netif_default);
#endif
}

void WifiService::mDnsDeinit()
{
#if LWIP_MDNS_RESPONDER == 1
    mdns_resp_remove_netif(&cyw43_state.netif[CYW43_ITF_STA]);
#endif
}

void WifiService::on_receive(const OpenAce::IdleMsg &msg)
{
    static bool previous = false;
    (void)msg;
    bool active = checkIfClientActive(CYW43_ITF_STA) || checkIfClientActive(CYW43_ITF_AP);

    if (active != previous) {
        getBus().receive(OpenAce::WifiConnectionStateMsg{active});
        previous = active;
    }
}
