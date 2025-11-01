/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../wifiservice.hpp"
#include "lwip/apps/mdns.h"
#include "ace/coreutils.hpp"

#include "pico/lwip_freertos.h"
#include "pico/stdlib.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

GATAS::PostConstruct WifiService::postConstruct()
{
    if (cyw43_arch_init())
    {
        return GATAS::PostConstruct::HARDWARE_NOT_FOUND;
    }
    return GATAS::PostConstruct::OK;
}

void WifiService::start()
{
#if LWIP_MDNS_RESPONDER == 1
    mdns_resp_init();
#endif
    xTaskCreate(wifiTask, WifiService::NAME.cbegin(), configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY, &taskHandle);
    getBus().subscribe(*this);
};

void WifiService::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void WifiService::wifiTask(void *arg)
{
    uint32_t startScan = 0;
    uint8_t secondCounter = 0;
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
            // ----------------------------------------------------------
            // ClientMode handling
            switch (wifiService->connectionState)
            {

            case ConnectionState::START:
                wifiService->wifiMode = GATAS::WifiMode::NC;
                wifiService->connectionState = ConnectionState::ENABLESTA;
                wifiService->totalScanAttempt = NUMBER_OF_CONNECTION_ATTEMPTS;
                break;

            case ConnectionState::ENABLESTA:
                wifiService->enableSta();
                if (wifiService->dontScanJustConnectToClient)
                {
                    // When just connect, fill the scanResult with the configured SSD's
                    wifiService->scanResult.clear();
                    for (auto it : wifiService->wifiData.clients)
                    {
                        if (!wifiService->scanResult.full())
                        {
                            wifiService->scanResult.emplace_back(it.ssid);
                        }
                    }

                    wifiService->connectionState = ConnectionState::TRYCLIENTCONNECT;
                }
                else
                {
                    wifiService->connectionState = ConnectionState::WIFISCAN;
                }
                break;

            case ConnectionState::WIFISCAN:
                startScan = CoreUtils::timeUs32Raw() + (GATAS_WIFISERVICE_MAX_SCAN_TIME_MS * 1'000);
                wifiService->startWifiScan();
                wifiService->connectionState = ConnectionState::WIFISCANNING;
                break;

            case ConnectionState::WIFISCANNING:
                if (!wifiService->scanRunning())
                {
                    wifiService->connectionState = ConnectionState::TRYCLIENTCONNECT;
                }
                // If for whatever reason WIFI scan does not find any network, then stop scanning after GATAS_WIFISERVICE_MAX_SCAN_TIME_MS
                if (CoreUtils::isUsReachedRaw(startScan))
                {
                    cyw43_wifi_leave(&cyw43_state, 0);
                    wifiService->disableSta();
                    wifiService->connectionState = ConnectionState::APMODESTART;
                }
                break;

            case ConnectionState::TRYCLIENTCONNECT:
                // wifi_leave seems to be required for more reliable connections, I don't know why...
                cyw43_wifi_leave(&cyw43_state, 0);
                {

                    // *  CONNECTED Connection OK
                    // *  MORE No Connection, more work today
                    // *  EXHAUSTED No Connection, no more networks
                    auto cResult = wifiService->connectClient();
                    if (cResult == CONNECTED)
                    {
                        wifiService->mDnsInit(CYW43_ITF_STA);
                        wifiService->connectionState = ConnectionState::CLIENTMODESTARTED;
                        wifiService->successClientConnected = true;
                    }
                    else if (cResult == MORE)
                    {
                        //  *  1 No Connection, same network needs to be retried
                        //  *  2 No Connection, next network will be attempted if any
                    }
                    else /* EXHAUSTED */
                    {
                        wifiService->disableSta();

                        // When APP mode is disable, go back to scanning for networks
                        wifiService->totalScanAttempt -= 1;
                        if (wifiService->totalScanAttempt == 0)
                        {
                            wifiService->connectionState = ConnectionState::ENABLESTA;
                        }
                        else
                        {                            
                            if (wifiService->wifiData.apDisabled || wifiService->successClientConnected)
                            {
                                wifiService->connectionState = ConnectionState::START;
                            }
                            else
                            {
                                wifiService->connectionState = ConnectionState::APMODESTART;
                            }
                        }
                    }
                }
                break;

            case ConnectionState::CLIENTMODESTARTED:
                if (!wifiService->checkIfClientActive(CYW43_ITF_STA))
                {
                    wifiService->mDnsDeinit(CYW43_ITF_STA);
                    wifiService->connectionState = ConnectionState::CLIENTMODESTOPPED;
                }
                break;

            case ConnectionState::CLIENTMODESTOPPED:
                // TODO: If we got bad auth on the current connection try the next connection instead of a WIFI scan
                // So perhaps we can go to TRYCLIENTCONNECT??
                wifiService->connectionState = ConnectionState::WIFISCAN; // STA already enabled, so just scan for clients
                break;

            // ----------------------------------------------------------
            // AccessPoint handling
            case ConnectionState::APMODESTART:
                wifiService->startAccessPoint();
                wifiService->mDnsInit(CYW43_ITF_AP);
                wifiService->connectionState = ConnectionState::APSTARTED;

                break;

            case ConnectionState::APSTARTED:
                // every 5 seconds
                secondCounter += 1;
                if (secondCounter > 5)
                {
                    secondCounter = 0;
                    wifiService->handleAccesspointClients();
                }
                break;

            case ConnectionState::APSTOPPED:
                wifiService->mDnsDeinit(CYW43_ITF_AP);
                wifiService->connectionState = ConnectionState::WIFISCAN;
                wifiService->getBus().receive(GATAS::WifiConnectionStateMsg{GATAS::WifiMode::NC});
                break;

            default:
                // Handle unknown states if needed
                break;
            }
        }
    }
}

void WifiService::handleAccesspointClients()
{
    auto cyw43TickMs = cyw43_hal_ticks_ms();
    // Generate a lit of connected clients and send it
    etl::set<uint32_t, GATAS_MAXIMUM_TCP_CLIENTS> clients;
    for (uint8_t i = 0; i < DHCPS_MAX_IP; i++)
    {
        uint32_t expiry = dhcp_server.lease[i].expiry << 16 | 0xffff;
        if ((int32_t)(expiry - cyw43TickMs) > 0 && dhcp_server.lease[i].ip.addr != 0)
        {
            clients.insert(dhcp_server.lease[i].ip.addr);
        }
    }
    getBus().receive(GATAS::AccessPointClientsMsg{clients});
}

void WifiService::startAccessPoint()
{
    // GATAS_LOG("Starting access point");
    cyw43_arch_enable_ap_mode(wifiData.ap.ssid.c_str(), wifiData.ap.password.c_str(), CYW43_AUTH_WPA2_AES_PSK);
    // cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));
    //  https://github.com/raspberrypi/pico-sdk/issues/1661#issuecomment-3238252048
    //  TODO: Still testing, this might solve a STALL issue we have seen very rarely
    cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);

    ip4_addr_t mask;
    ip_addr_t gw;
    IP4_ADDR(ip_2_ip4(&gw), 192, 168, 1, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    dhcp_server_init(&dhcp_server, &gw, &mask);
    dns_server_init(&dns_server, &gw);

    wifiMode = GATAS::WifiMode::AP;
    showSsidPwdIp(wifiData.ap.ssid, wifiData.ap.password);
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
        // GATAS_LOG("Scan Result received");
        WifiService *service = (WifiService *)env;
        GATAS::SsidOrPasswdStr name = (char *)result->ssid;

        auto it = etl::find_if(service->wifiData.clients.begin(), service->wifiData.clients.end(),
                               [&name](const GATAS::Config::WifiNamePassword &client)
                               {
                                   return client.ssid == name;
                               });

        if (it != service->wifiData.clients.end() && !service->scanResult.full())
        {
            // printf("Added: %s\n", name.c_str());
            service->scanResult.emplace_back(name);
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
    scan_options.scan_type = 1;
    memset(&scan_options, 0, sizeof(cyw43_wifi_scan_options_t));
    cyw43_wifi_scan(&cyw43_state, &scan_options, (void *)this, scanResultCb);
}

/**
 * Try to connect to a client
 * returns
 *  0 Connection OK
 *  1 No Connection, more work today here
 *  3 No Connection, no more networks
 */
WifiService::ConnectClientResult WifiService::connectClient()
{
    // Keeps track of the number of connections to the same network
    if (scanResult.empty())
    {
        return EXHAUSTED;
    }

    auto nameIt = scanResult.begin();
    auto it = etl::find_if(wifiData.clients.begin(), wifiData.clients.end(),
                           [&nameIt](const GATAS::Config::WifiNamePassword &client)
                           {
                               return client.ssid == nameIt->ssid;
                           });

    // When the client's data not found, remove it from the scan result list, this should normally not happen
    if (it != wifiData.clients.end())
    {

        // for (auto it : wifiData.clients) {
        //     printf("Configured: %s\n", it.ssid.c_str());
        // }
        // for (auto it : scanResult) {
        //     printf("found: %s\n", it.c_str());
        // }
        printf("WifiService: Client Connecting %s %s\n", it->ssid.c_str(), "<hidden>");
        auto result = cyw43_arch_wifi_connect_timeout_ms(it->ssid.c_str(), it->password.c_str(), CYW43_AUTH_WPA2_AES_PSK, 10000);
        //    printf("WifiService: Result: %d\n", result);

        if (result == PICO_OK)
        {
            // After connection, reset to NUMBER_OF_CONNECTION_ATTEMPTS so if the connections get's lost
            // a possible fast reconnect will happen.
            nameIt->connectNo = NUMBER_OF_CONNECTION_ATTEMPTS;
            wifiMode = GATAS::WifiMode::CLIENT;
            showSsidPwdIp(it->ssid, "<hidden>");
            return CONNECTED;
        }
        else
        {
            // case PICO_ERROR_BADAUTH:        // Sometimes seen that auth just did not work on my own network
            // case PICO_ERROR_TIMEOUT:        // TImeout if the network was not responding somehow
            // case PICO_ERROR_CONNECT_FAILED: // Seems to indicate that the network is available, but somehow could not connect
            // Test if it can be re-scanned
            nameIt->connectNo--;
            if (nameIt->connectNo)
            {
                return MORE;
            }
        }
        scanResult.erase(nameIt);
    }
    return scanResult.empty() ? EXHAUSTED : MORE;
}

bool WifiService::checkIfClientActive(int itf)
{
    (void)itf;
    return netif_default && netif_is_up(netif_default) && netif_is_link_up(netif_default);
}

void WifiService::enableSta()
{
    cyw43_arch_enable_sta_mode();
    // cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));
    // https://github.com/raspberrypi/pico-sdk/issues/1661#issuecomment-3238252048
    cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);
}

void WifiService::disableSta()
{
    cyw43_arch_disable_sta_mode();
}

void WifiService::showSsidPwdIp(const etl::string_view &ssid, const etl::string_view &password) const
{
    const char *mode;
    char ipStr[IP4ADDR_STRLEN_MAX];
    if (wifiMode == GATAS::WifiMode::AP)
    {
        mode = "Access Point";
    }
    else if (wifiMode == GATAS::WifiMode::CLIENT)
    {
        mode = "Client";
    }
    else
    {
        mode = "AP";
    }

    ip4_addr_t ip = getInterfaceInfo().ip;

    ip4addr_ntoa_r(&ip, ipStr, IP4ADDR_STRLEN_MAX);
    puts("###################################");
    printf("## Mode: %s\n## SSID: %s Password: %s IP: %s\n", mode, ssid.begin(), password.begin(), ipStr);
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

void WifiService::mDnsInit(int itf)
{
#if LWIP_MDNS_RESPONDER == 1
    mdns_resp_add_netif(&cyw43_state.netif[itf], GATAS_MDNS_NAME);
    mdnsSlot = mdns_resp_add_service(&cyw43_state.netif[itf], GATAS_MDNS_NAME, "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
    mdns_resp_announce(&cyw43_state.netif[itf]);
#endif
}

void WifiService::mDnsDeinit(int itf)
{
#if LWIP_MDNS_RESPONDER == 1
    mdns_resp_del_service(&cyw43_state.netif[itf], mdnsSlot);
    mdns_resp_remove_netif(&cyw43_state.netif[itf]);
#endif
}

WifiService::IpGw WifiService::getInterfaceInfo()
{
    struct netif *netif = netif_list;
    if (netif != NULL)
    {
        return {netif->ip_addr, netif->gw};
    }
    return {0, 0};
}

void WifiService::on_receive(const GATAS::Every5SecMsg &msg)
{
    (void)msg;
    bool active = checkIfClientActive(CYW43_ITF_STA) || checkIfClientActive(CYW43_ITF_AP);

    if (active == currentWifiActiveStatus)
    {
        return;
    }

    const auto interface = getInterfaceInfo();
    if (!active)
    {
        getBus().receive(GATAS::WifiConnectionStateMsg{GATAS::WifiMode::NC});
        currentWifiActiveStatus = false;
        return;
    }

    if (interface.ip.addr != 0)
    {
        getBus().receive(GATAS::WifiConnectionStateMsg{wifiMode, /* ip & 0xFFFFFF */ interface.ip.addr, interface.gateWay.addr});
        return;
    }
}
