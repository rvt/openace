#include <stdio.h>

#include "../gatasconnect.hpp"
#include "ace/coreutils.hpp"
#include "ace/binarymessages.hpp"
#include "ace/cobs.hpp"
#include "ace/assert.hpp"

/* LwIP */
#include "lwip/ip_addr.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"

GATAS::PostConstruct GatasConnect::postConstruct()
{
    requestTimer = xTimerCreate(GatasConnect::NAME,
                                TASK_DELAY_MS(1000),
                                pdTRUE,
                                this,
                                requestTimerCallback);

    if (requestTimer == nullptr)
    {
        return GATAS::PostConstruct::TIMER_ERROR;
    }
    pcbSend = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcbSend == nullptr)
    {
        xTimerDelete(requestTimer, portMAX_DELAY);
        return GATAS::PostConstruct::NETWORK_ERROR;
    }
    udp_recv(pcbSend, receiveUdpMessage, this);

    return GATAS::PostConstruct::OK;
}

void GatasConnect::start()
{
    xTimerChangePeriod(requestTimer, TASK_DELAY_MS(1000), portMAX_DELAY);
    getBus().subscribe(*this);
};

void GatasConnect::stop()
{
    xTimerStop(requestTimer, portMAX_DELAY);
    getBus().unsubscribe(*this);
};

void GatasConnect::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"bytesSend\":" << statistics.bytesSend;
    stream << ",\"bytesReceived\":" << statistics.bytesReceived;
    stream << ",\"bufferAllocErr\":" << statistics.bufferAllocErr;
    stream << "}\n";
}

void GatasConnect::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

/**
 * Track wifi connects and disconnect
 */
void GatasConnect::on_receive(const GATAS::WifiConnectionStateMsg &wcs)
{
    wifiConnected = wcs.connected;
}

void GatasConnect::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition.store(msg.position, etl::memory_order_release);
}

void GatasConnect::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{

    if (msg.moduleName == Configuration::CONFIG)
    {
        getConfig(msg.config);
    }
}

void GatasConnect::getConfig(const Configuration &config)
{
    gatasServer = config.ipPortBypath(NAME, "gatasServer");
    gatasServer.port = GATAS_CONNECT_PORT;

    auto gatasConfig = config.gaTasConfig();
    currentAddress = config.gaTasConfig().conspicuity.icaoAddress;
    allIcaoAddresses = config.gaTasConfig().allIcaoAddresses;
    gatasId = config.internalStore()->gatasId;
}

void GatasConnect::on_receive(const GATAS::GpsStatsMsg &msg)
{
    hasGpsFix = msg.fixType == 3;
}

void GatasConnect::receiveUdpMessage(void *arg, struct udp_pcb *pcb,
                                     struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    (void)arg;
    (void)pcb;
    (void)addr;
    (void)port;
    if (p == nullptr)
    {
        return;
    }
    GatasConnect *taskCtx = (GatasConnect *)(arg);
    auto ownship = taskCtx->ownshipPosition.load(etl::memory_order_acquire);
    taskCtx->statistics.bytesReceived += p->tot_len;
    taskCtx->cobsStreamHandler.handle(ownship.lat, ownship.lon, etl::span<uint8_t>(reinterpret_cast<uint8_t *>(p->payload), p->tot_len));
    taskCtx->cobsStreamHandler.clear();
    pbuf_free(p);
}

/**
 * Prepare a position request to the server, the serveer will then send aircraft information back
 */
void GatasConnect::requestTimerCallback(TimerHandle_t xTimer)
{
    // Don't send anything if there is no gpsFix
    GatasConnect *taskCtx = (GatasConnect *)pvTimerGetTimerID(xTimer);
    if (!taskCtx->wifiConnected)
    {
        return;
    }
    constexpr size_t COBS_EXTRA_BYTES = 3; // Size Byte + 0 handling byte + 0 end byte
    constexpr size_t maxSize = BinaryMessages::serializeOwnshipPositionSizeV1().items(1) + BinaryMessages::serializeAircraftConfigurationV1Size().items(GATAS::MAX_AIRCRAFT_CONFIGURATIONS);
    GATAS_ASSERT(maxSize < 255, "Must be smaller than 255 due to cobs encoding");

    size_t currentSize = BinaryMessages::serializeOwnshipPositionSizeV1().items(1) + BinaryMessages::serializeAircraftConfigurationV1Size().items(taskCtx->allIcaoAddresses.size());

    std::array<uint8_t, maxSize> storage;
    etl::bit_stream_writer writer(storage.data(), storage.size(), etl::endian::big);

    // Write the ownship
    if (taskCtx->hasGpsFix)
    {
        BinaryMessages::serializeOwnshipPositionV1(writer, taskCtx->ownshipPosition.load(etl::memory_order_acquire));
    }

    // Inform current basic confiuration
    BinaryMessages::serializeAircraftConfigurationV1(writer, taskCtx->gatasId, taskCtx->currentAddress, taskCtx->allIcaoAddresses);

    // Send to the server
    auto server = taskCtx->gatasServer;
    ip_addr_t addr;
    ip4_addr_set_u32(&addr, server.ip);
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, currentSize + COBS_EXTRA_BYTES, PBUF_RAM); 
    if (p == nullptr)
    {
        taskCtx->statistics.bufferAllocErr += 1;
        cyw43_arch_lwip_end();
        return;
    }

    // For now, just use cobs on all the messages as this is easer on the controller.
    // If we change that one day, we could simply create a new version... Then use a different port and on gatasServer
    // we enable that on the port
    auto size = encodeCOBS(storage.data(), currentSize, (uint8_t *)p->payload, currentSize + COBS_EXTRA_BYTES, true);
    if (size)
    {
        // Trim to right size so the cobs decoder on the receiving end does not get confused
        p->len = size;
        p->tot_len = size;
        auto err = udp_sendto(taskCtx->pcbSend, p, &addr, server.port);
        if (err != ERR_OK)
        {
            taskCtx->statistics.msgSendFailed += 1;
        }
        else
        {
            taskCtx->statistics.bytesSend += size;
        }
    }
    pbuf_free(p);
    cyw43_arch_lwip_end();
    xTimerChangePeriod(taskCtx->requestTimer, TASK_DELAY_MS(1000), portMAX_DELAY);
}
