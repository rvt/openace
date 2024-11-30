#include <stdio.h>

#include "gdloverudp.hpp"

/* OpenACE */
#include "ace/coreutils.hpp"

/* LwIP */
#include "lwip/ip_addr.h"
// #include "lwip/sockets.h"

#include "pico/cyw43_arch.h"

void GDLoverUDP::getConfiguration(const Configuration &config)
{
    if (xSemaphoreTake(configMutex, (TickType_t)10) == pdTRUE)
    {
        getConfigurationNoMutex(config);
        xSemaphoreGive(configMutex);
    }
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

OpenAce::PostConstruct GDLoverUDP::postConstruct()
{
    pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb == nullptr)
    {
        return OpenAce::PostConstruct::NETWORK_ERROR;
    }

    configMutex = xSemaphoreCreateMutex();
    if (configMutex == nullptr)
    {
        return OpenAce::PostConstruct::MUTEX_ERROR;
    }

    return OpenAce::PostConstruct::OK;
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

void GDLoverUDP::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void GDLoverUDP::on_receive(const OpenAce::AccessPointClientsMsg &msg)
{
    connectedClients = msg.msg;
}

void GDLoverUDP::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    getConfiguration(msg.config);
}

void GDLoverUDP::on_receive(const OpenAce::GDLMsg &msg)
{
    // Send to the connect clients and the defined ports
    if (xSemaphoreTake(configMutex, (TickType_t)100) == pdTRUE)
    {
        for (auto ip : connectedClients)
        {
            for (auto port : udpPorts)
            {
                sendTo(msg, ip, port);
            }
        }

        for (const auto &client : customClients)
        {
            sendTo(msg, client.ip, client.port);
        }
        xSemaphoreGive(configMutex);
    } else {
        // puts("GDLoverUDP: Failed ");
    }
}

void GDLoverUDP::sendTo(const OpenAce::GDLMsg &msg, uint32_t ip, int16_t port)
{
    ip_addr_t addr;
    ip4_addr_set_u32(&addr, ip);

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, msg.msg.size(), PBUF_RAM);
    if (p != NULL)
    {
        memcpy(p->payload, msg.msg.data(), msg.msg.size());
        // printf("IP:%d.%d.%d.%d:%d  %02d:%02d:%02d\n",  ip4_addr1(&addr), ip4_addr2(&addr), ip4_addr3(&addr), ip4_addr4(&addr), port, time.hour, time.minute, time.second);
        cyw43_arch_lwip_begin();
        err_t err = udp_sendto(pcb, p, &addr, port);
        pbuf_free(p);
        cyw43_arch_lwip_end();
        if (err == ERR_OK)
        {
            statistics.heartbeatTx++;
        }
        else
        {
            statistics.sendFailureErr++;
        }
    }
    else
    {
        statistics.bufferAllocErr++;
    }
}
