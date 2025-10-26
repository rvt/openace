#include <stdio.h>

#include "../gatasconnectudp.hpp"
#include "ace/coreutils.hpp"
#include "ace/binarymessages.hpp"
#include "ace/cobs.hpp"
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
    stream << ",\"hasConnection\":" << statistics.hasConnection;
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
    ownshipPosition = SpinlockGuard::withLock(spinLock, msg.position);
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
    if (auto guard = SpinlockGuard(spinLock))
    {
        icaoAddress = gatasConfig.conspicuity.icaoAddress;
        allIcaoAddresses = gatasConfig.allIcaoAddresses;
        gatasId = config.internalStore()->gatasId;
    }
}

void GatasConnect::on_receive(const GATAS::GpsStatsMsg &msg)
{
    hasGpsFix = msg.fixType == 3;
}

void GatasConnect::receiveUdpMessage(void *arg, struct udp_pcb *pcb,
                                     struct pbuf *pbuf, const ip_addr_t *addr, u16_t port)
{
    (void)pcb;
    (void)addr;
    (void)port;
    if (pbuf == nullptr)
    {
        return;
    }

    GatasConnect *taskCtx = (GatasConnect *)(arg);
    taskCtx->lastSendCounter = 0;
    auto ownship = SpinlockGuard::withLock(taskCtx->spinLock, taskCtx->ownshipPosition);
    // Reset the pkgCount to get a honest pkgSend vs pkg Received
    if (!taskCtx->statistics.hasConnection)
    {
        taskCtx->statistics.pkgReceived = 0;
        taskCtx->statistics.hasConnection = true;
    }
    taskCtx->statistics.bytesReceived += pbuf->tot_len;
    taskCtx->statistics.pkgReceived += 1;

    for (struct pbuf *q = pbuf; q != NULL; q = q->next)
    {
        taskCtx->cobsStreamHandler.handle(ownship.lat, ownship.lon, etl::span<uint8_t>((uint8_t *)q->payload, q->len));
    }

    pbuf_free(pbuf);
}

/**
 * Prepare a position request to the server, the serveer will then send aircraft information back
 */
void GatasConnect::requestTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;

    GatasConnect *taskCtx = (GatasConnect *)pvTimerGetTimerID(xTimer);

    constexpr size_t COBS_EXTRA_BYTES = 3;
    constexpr size_t OWN_MAX = BinaryMessages::serializeOwnshipPositionSizeV1().items();
    constexpr size_t CFG_MAX = BinaryMessages::serializeAircraftConfigurationSizeV1().items(GATAS::MAX_AIRCRAFT_CONFIGURATIONS);
    constexpr size_t MAX_MSG = etl::max(OWN_MAX, CFG_MAX);

    const size_t ownshipSize = BinaryMessages::serializeOwnshipPositionSizeV1().items(1);
    const size_t configSize = BinaryMessages::serializeAircraftConfigurationSizeV1().items(taskCtx->allIcaoAddresses.size());
    GATAS_ASSERT((etl::max(ownshipSize, configSize) + COBS_EXTRA_BYTES) < 255, "COBS max length exceeded");
    //    printf("Max Size: %u, %u,%u, %u, %u\n", OWN_MAX, CFG_MAX, ownshipSize, configSize, etl::max(ownshipSize, configSize));

    if (!taskCtx->wifiConnected)
    {
        return;
    }

    static_assert((MAX_MSG + COBS_EXTRA_BYTES) < ANDROIDHOTSPOT_FIX_HIGHMARK, "Data object can be bigger than ANDROIDHOTSPOT_FIX_HIGHMARK");
    etl::array<uint8_t, ANDROIDHOTSPOT_FIX_HIGHMARK> cobsPayload;
    size_t position = 0;
    etl::array<uint8_t, OWN_MAX + MAX_MSG + COBS_EXTRA_BYTES * 2> perCobsBuffer;
    etl::bit_stream_writer writer(perCobsBuffer.data(), perCobsBuffer.size(), etl::endian::big);

    if (auto guard = SpinlockGuard(taskCtx->spinLock))
    {
        // --- Ownship (optional)
        if (taskCtx->hasGpsFix)
        {
            writer.restart();
            // 28 Byte
            BinaryMessages::serializeOwnshipPositionV1(writer, taskCtx->ownshipPosition);
            auto size = encodeCOBS(perCobsBuffer.data(), ownshipSize, cobsPayload.data() + position, cobsPayload.size() - position, true);
            position += size;
        }

        // --- Aircraft configuration (always send)
        writer.restart();
        // 22 Byte
        BinaryMessages::serializeAircraftConfigurationV1(writer, taskCtx->gatasId, taskCtx->icaoAddress, taskCtx->allIcaoAddresses, taskCtx->gatasIp);
        auto size = encodeCOBS(perCobsBuffer.data(), configSize, cobsPayload.data() + position, cobsPayload.size() - position, true);
        position += size;
    }

    // Possible add android hotspot fix
    if (taskCtx->androidHotspotFix)
    {
        taskCtx->lastSendCounter += 1;
        size_t fillSize = ANDROIDHOTSPOT_FIX_LOWMARK;
        if (taskCtx->lastSendCounter > ANDROIDHOTSPOT_COUNT_UNTILL_HIGH)
        {
            taskCtx->lastSendCounter = ANDROIDHOTSPOT_COUNT_UNTILL_HIGH;
            fillSize = ANDROIDHOTSPOT_FIX_HIGHMARK;
        }

        size_t toFill = fillSize - position;
        if (toFill > 0)
        {
            memset(cobsPayload.begin() + position, 0x00, toFill);
            position = fillSize;
        }
    }

    struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, position, PBUF_POOL);
    if (!pbuf)
    {
        taskCtx->statistics.bufferAllocErr++;
        return;
    }
    if (pbuf_take(pbuf, cobsPayload.begin(), position) != ERR_OK)
    {
        pbuf_free(pbuf);
        return;
    }

    // --- Send
    ip_addr_t addr;
    ip4_addr_set_u32(&addr, taskCtx->gatasServer.ip);
    auto err = udp_sendto(taskCtx->pcbSend, pbuf, &addr, taskCtx->gatasServer.port);
    pbuf_free(pbuf);
    if (err == ERR_OK)
    {
        taskCtx->statistics.hasConnection = true;
        taskCtx->statistics.pkgSend += 1;
        taskCtx->statistics.bytesSend += position;
    }
    else
    {
        taskCtx->statistics.msgSendFailed += 1;
        taskCtx->statistics.hasConnection = false;
    }
}
