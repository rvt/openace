#include <stdio.h>

#include "../gdloverudp.hpp"

/* GATAS */
#include "ace/coreutils.hpp"
#include "ace/semaphoreguard.hpp"
#include "ace/measure.hpp"
#include "ace/lwiplock.hpp"

/* LwIP */
#include "lwip/ip_addr.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"

void GDLoverUDP::getConfiguration(const Configuration &config)
{
    getConfigurationNoMutex(config);
}

void GDLoverUDP::getConfigurationNoMutex(const Configuration &config)
{
    udpPorts.clear();
    customClients.clear();

    etl::string<16> path;
    for (uint8_t i = 0; i < GATAS_GDL90OVERUDP_MAX_PORTS; i++)
    {
        if (!udpPorts.full())
        {
            path.clear();
            etl::string_stream stream(path);
            stream << etl::make_string("defaultPorts/") << i;
            int32_t p = config.valueByPath(GDL90OVERUDP_DEFAULT_PORT, NAME, path);
            udpPorts.insert(p);
        }
    }

    for (char i = 0; i < GATAS_GDL90OVERUDP_MAX_CUSTOM_CLIENTS; i++)
    {
        path.clear();
        etl::string_stream stream(path);
        stream << etl::make_string("ips/") << i;
        auto ipPort = config.ipPortBypath(NAME, path);
        if (ipPort.ip != IPADDR_NONE && !customClients.full())
        {
            // printf("GDLoverUDP %ld:%d\n",ipPort.ip, ipPort.port);
            customClients.emplace_back(ipPort.ip, ipPort.port);
        }
    }
}

GATAS::PostConstruct GDLoverUDP::postConstruct()
{
    pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb == nullptr)
    {
        return GATAS::PostConstruct::NETWORK_ERROR;
    }
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr)
    {
        return GATAS::PostConstruct::MUTEX_ERROR;
    }
    xTaskCreate(gdlOverUDPTask, GDLoverUDP::NAME.cbegin(), configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY + 3, &taskHandle);

    return GATAS::PostConstruct::OK;
}

void GDLoverUDP::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"heartbeatTx\":" << statistics.heartbeatTx;
    stream << ",\"bufferAllocErr\":" << statistics.bufferAllocErr;
    stream << ",\"sendFailureErr\":" << statistics.sendFailureErr;
    stream << "}\n";
}

void GDLoverUDP::start()
{
    getBus().subscribe(*this);
};

void GDLoverUDP::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void GDLoverUDP::on_receive(const GATAS::AccessPointClientsMsg &msg)
{
    connectedClients = msg.msg;
}

void GDLoverUDP::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == GDLoverUDP::NAME)
    {
        getConfiguration(msg.config);
    }
}

void GDLoverUDP::gdlOverUDPTask(void *arg)
{
    GDLoverUDP *at = static_cast<GDLoverUDP *>(arg);
    while (true)
    {
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(1000));
        if (notifyValue & TaskState::EXIT)
        {
            vTaskDelete(nullptr);
            return;
        }

        // Handle TRANSMIT
        if (notifyValue == 0 || notifyValue & TaskState::TRANSMIT)
        {
            at->transmitBuffer();
        }
    }
}

void GDLoverUDP::on_receive(const GATAS::GdlMsg &msg)
{

    if (auto guard = SemaphoreGuard(10, mutex))
    {
        gdlDataBuffer.set(msg.msg);

        // When the buffer is nearly full 3/4, request for immediate send
        if (gdlDataBuffer.packets() >= (NUM_GDL_PACKETS - NUM_GDL_PACKETS / 4))
        {
            xTaskNotify(taskHandle, TaskState::TRANSMIT, eSetBits);
        }
    }
}

void GDLoverUDP::transmitBuffer()
{
    // When no network, don't send any UDP packages
    if (!wifiConnected)
    {
        return;
    }

    // Calculate how many pbufs we needna d reference them
    uint8_t totalpBufs = connectedClients.size() * udpPorts.size() + gateWayClient ? udpPorts.size() : 0;
    for (const auto &client : customClients)
    {
        if ((client.ip & 0xFFFFFF) == networkAddress)
        {
            totalpBufs += 1;
        }
    }
    if (totalpBufs == 0)
    {
        return;
    }

    etl::optional<etl::span<uint8_t>> part;
    if (auto guard = SemaphoreGuard(1000, mutex))
    {
        gdlDataBuffer.compact();
        part = gdlDataBuffer.read();
    }
    if (!part)
    {
        return;
    }

    auto data = part.value();
    LwipLock lock;
    // Send to the connect clients and the defined ports
    for (auto ip : connectedClients)
    {
        // Connected clients are always on the accesspoint,
        // thus we don't need to test for the networkAddress
        for (auto port : udpPorts)
        {
            sendTo(ip, port, data);
        }
    }

    // Send to the gateway client, which is most lickly running a EFB
    if (gateWayClient)
    {
        for (auto port : udpPorts)
        {
            sendTo(gateWayClient, port, data);
        }
    }

    // Custom clients can have any netmask and thus need to be tested if they are on the local net
    for (const auto &client : customClients)
    {
        if ((client.ip & 0xFFFFFF) == networkAddress)
        {
            sendTo(client.ip, client.port == 0 ? GDL90OVERUDP_DEFAULT_PORT : client.port, data);
        }
    }

    // if (auto guard = SemaphoreGuard(1000, mutex))
    // {
    //     // printf("GDLoverUDP: %zu bytes sent\n", size);
    //     gdlDataBuffer.compact();
    // }
}

void GDLoverUDP::sendTo(uint32_t ip, int16_t port, etl::span<uint8_t> data)
{
    ip_addr_t addr;
    ip4_addr_set_u32(&addr, ip);

    struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, data.size(), PBUF_POOL);

    if (!pbuf)
    {
        return;
    }
    if (pbuf_take(pbuf, data.begin(), data.size()) != ERR_OK)
    {
        pbuf_free(pbuf);
        return;
    }
    err_t err = udp_sendto(pcb, pbuf, &addr, port);
    pbuf_free(pbuf);

    if (err == ERR_OK)
    {
        statistics.heartbeatTx += 1;
    }
    else
    {
        statistics.sendFailureErr += 1;
    }
}

void GDLoverUDP::on_receive(const GATAS::WifiConnectionStateMsg &msg)
{
    wifiConnected = msg.wifiMode != GATAS::WifiMode::NC;
    networkAddress = msg.gatasIp & 0xFFFFFF;
    if (msg.wifiMode == GATAS::WifiMode::CLIENT)
    {
        gateWayClient = msg.gateWay;
    }
    else
    {
        gateWayClient = 0;
    }
}
