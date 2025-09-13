#include <stdio.h>

#include "../gatasconnect.hpp"
#include "ace/coreutils.hpp"
#include "ace/binarymessages.hpp"
#include "ace/cobs.hpp"
#include "ace/assert.hpp"
#include "ace/lwiplock.hpp"

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
    stream << "\"bytesReceived\":" << statistics.bytesReceived;
    stream << ",\"bytesSend\":" << statistics.bytesSend;
    stream << ",\"pkgReceived\":" << statistics.pkgReceived;
    stream << ",\"pkgSend\":" << statistics.pkgSend;
    stream << ",\"bufferAllocErr\":" << statistics.bufferAllocErr;
    stream << ",\"msgSendFailed\":" << statistics.msgSendFailed;
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
    wifiConnected = wcs.wifiMode != GATAS::WifiMode::NC;
    gatasIp = wcs.gatasIp;
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
    icaoAddress = config.gaTasConfig().conspicuity.icaoAddress;
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
    (void)pcb;
    (void)addr;
    (void)port;
    if (p == nullptr)
    {
        return;
    }
    GatasConnect *taskCtx = (GatasConnect *)(arg);
    auto ownship = taskCtx->ownshipPosition.load(etl::memory_order_acquire);
    // Reset the pkgCount to get a honest pkgSend vs pkg Received
    if (!taskCtx->statistics.hasConnection) {
        taskCtx->statistics.pkgReceived = 0;
        taskCtx->statistics.hasConnection = true;
    }
    taskCtx->statistics.bytesReceived += p->tot_len;
    taskCtx->statistics.pkgReceived += 1;
    taskCtx->cobsStreamHandler.handle(ownship.lat, ownship.lon, etl::span<uint8_t>(reinterpret_cast<uint8_t *>(p->payload), p->tot_len));
    taskCtx->cobsStreamHandler.clear();
    pbuf_free(p);
}

/**
 * Prepare a position request to the server, the serveer will then send aircraft information back
 */
void GatasConnect::requestTimerCallback(TimerHandle_t xTimer)
{
    GatasConnect *taskCtx = (GatasConnect *)pvTimerGetTimerID(xTimer);
    if (!taskCtx->wifiConnected)
    {
        return;
    }
    constexpr size_t COBS_EXTRA_BYTES = 3;

    constexpr size_t OWN_MAX =
        BinaryMessages::serializeOwnshipPositionSizeV1().items(1);
    constexpr size_t CFG_MAX =
        BinaryMessages::serializeAircraftConfigurationSizeV1().items(GATAS::MAX_AIRCRAFT_CONFIGURATIONS);
    constexpr size_t MAX_MSG = (OWN_MAX > CFG_MAX ? OWN_MAX : CFG_MAX);

    const size_t ownshipSize = BinaryMessages::serializeOwnshipPositionSizeV1().items(1);
    const size_t configSize = BinaryMessages::serializeAircraftConfigurationSizeV1().items(taskCtx->allIcaoAddresses.size());
    GATAS_ASSERT((etl::max(ownshipSize, configSize) + COBS_EXTRA_BYTES) < 255, "COBS max length exceeded");

    LwipLock lock;
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, (ownshipSize + configSize) + 2 * COBS_EXTRA_BYTES, PBUF_RAM);
    if (!p)
    {
        taskCtx->statistics.bufferAllocErr++;
        return;
    }

    etl::array<uint8_t, MAX_MSG + COBS_EXTRA_BYTES> storage;
    etl::bit_stream_writer writer(storage.data(), storage.size(), etl::endian::big);

    size_t position = 0;

    // --- Ownship (optional)
    if (taskCtx->hasGpsFix)
    {
        writer.restart();
        BinaryMessages::serializeOwnshipPositionV1(writer, taskCtx->ownshipPosition.load(etl::memory_order_acquire));
        auto size = encodeCOBS(storage.data(), ownshipSize, (uint8_t *)p->payload + position, p->len - position, true);
        position += size;
    }

    // --- Aircraft configuration (always send)
    writer.restart();
    BinaryMessages::serializeAircraftConfigurationV1(writer, taskCtx->gatasId, taskCtx->icaoAddress, taskCtx->allIcaoAddresses, taskCtx->gatasIp);
    auto size = encodeCOBS(storage.data(), configSize, (uint8_t *)p->payload + position, p->len - position, true);
    position += size;

    // --- Send
    ip_addr_t addr;
    ip4_addr_set_u32(&addr, taskCtx->gatasServer.ip);
    if (position > 0)
    {
        p->len = p->tot_len = position; // trim
        auto err = udp_sendto(taskCtx->pcbSend, p, &addr, taskCtx->gatasServer.port);
        if (err != ERR_OK)
        {
            taskCtx->statistics.msgSendFailed++;
        }
        else
        {
            taskCtx->statistics.bytesSend += position;
            taskCtx->statistics.pkgSend += 1;
        }
    }

    pbuf_free(p);
    xTimerChangePeriod(taskCtx->requestTimer, TASK_DELAY_MS(1000), portMAX_DELAY);
}
