#include <stdio.h>

#include "gdloverudp.hpp"

/* GATAS */
#include "ace/coreutils.hpp"
#include "ace/semaphoreguard.hpp"
#include "ace/measure.hpp"

/* LwIP */
#include "lwip/ip_addr.h"
// #include "lwip/sockets.h"

#include "pico/cyw43_arch.h"

void GDLoverUDP::getConfiguration(const Configuration &config)
{
    getConfigurationNoMutex(config);
}

void GDLoverUDP::getConfigurationNoMutex(const Configuration &config)
{
    udpPorts.clear();
    customClients.clear();

    for (char i = '0'; i < GDL90OVERUDP_MAX_PORTS + '0'; i++)
    {
        char path[] = "defaultPorts/X";
        path[sizeof(path) - 2] = i;
        int32_t port = config.valueByPath(GDL90OVERUDP_DEFAULT_PORT, NAME, path);
        //            printf("GDLoverUDP %d\n",port);
        udpPorts.insert(port);
    }

    for (char i = '0'; i < GDL90OVERUDP_MAX_CUSTOM_CLIENTS + '0'; i++)
    {
        char path[] = "ips/X";
        path[sizeof(path) - 2] = i;
        auto ipPort = config.ipPortBypath(NAME, path);
        if (ipPort.ip != IPADDR_NONE && ipPort.port > 1023)
        {
            //                printf("GDLoverUDP %ld:%d\n",ipPort.ip, ipPort.port);
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

void GDLoverUDP::stop()
{
    getBus().unsubscribe(*this);
    xTaskNotify(taskHandle, TaskState::EXIT, eSetBits);
    while (eTaskGetState(taskHandle) != eDeleted)
    {
        vTaskDelay(TASK_DELAY_MS(50));
    }
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

    if (auto guard = SemaphoreGuard<10>(mutex))
    {
        gdlDataBuffer.push(reinterpret_cast<const char *>(msg.msg.cbegin()), msg.msg.size());

        // When the buffer is nearly full, request for immediate send
        if (gdlDataBuffer.length() >= (NUM_GDL_PACKETS - NUM_GDL_PACKETS / 4) * sizeof(GATAS::GDLData))
        {
            xTaskNotify(taskHandle, TaskState::TRANSMIT, eSetBits);
        }
    }
}

void GDLoverUDP::transmitBuffer()
{
    //    if (gdlDataBuffer.length() >= (NUM_GDL_PACKETS - 1) * sizeof(GATAS::GDLData))
    {
        auto m = Measure("GDLoverUDP::transmitBuffer ", 5000);

        const char *part = nullptr;
        size_t size = 0;
        if (auto guard = SemaphoreGuard<10>(mutex))
        {
            auto buffer = gdlDataBuffer.peek();
            part = buffer.part;
            size = buffer.size;
        }
        if (size == 0 || part == nullptr)
        {
            return;
        }

        cyw43_arch_lwip_begin();
        // Send to the connect clients and the defined ports
        for (auto ip : connectedClients)
        {
            // Connected clients are always on the accesspoint,
            // thus we don't need to test for the networkAddress
            for (auto port : udpPorts)
            {
                sendTo(part, size, ip, port);
            }
        }

        // Custom clients can have any netmask and thus need to be tested if they are on the local net
        for (const auto &client : customClients)
        {
            if ((client.ip & 0xFFFFFF) == networkAddress)
            {
                sendTo(part, size, client.ip, client.port);
            }
        }

        cyw43_arch_lwip_end();
        if (auto guard = SemaphoreGuard<10>(mutex))
        {
            // printf("GDLoverUDP: %zu bytes sent\n", size);
            gdlDataBuffer.accepted(size);
            gdlDataBuffer.compact();
        }
    }
}

void GDLoverUDP::sendTo(const char *part, size_t size, uint32_t ip, int16_t port)
{
    ip_addr_t addr;
    ip4_addr_set_u32(&addr, ip);

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);
    if (!p)
    {
        statistics.bufferAllocErr++;
        return;
    }

    // printf("IP:%d.%d.%d.%d:%d  %02d:%02d:%02d\n",  ip4_addr1(&addr), ip4_addr2(&addr), ip4_addr3(&addr), ip4_addr4(&addr), port, time.hour, time.minute, time.second);
    memcpy(p->payload, part, size);
    err_t err = udp_sendto(pcb, p, &addr, port);
    pbuf_free(p);

    if (err == ERR_OK)
    {
        statistics.heartbeatTx++;
    }
    else
    {
        statistics.sendFailureErr++;
    }
}

void GDLoverUDP::on_receive(const GATAS::WifiConnectionStateMsg &msg)
{
    networkAddress = msg.networkAddress;
}
