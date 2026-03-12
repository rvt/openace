#include <stdio.h>

#include "../gatasconnectudp.hpp"
#include "ace/coreutils.hpp"
#include "ace/binarymessages.hpp"
#include "ace/cobs.hpp"
#include "ace/lwiplock.hpp"
#include "ace/scopedpbuf.hpp"
#include "ace/moreutils.hpp"
#include "ace/debug.hpp"

#include "etl/algorithm.h"

/* LwIP */
#include "lwip/ip_addr.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/dns.h"

GATAS::PostConstruct GatasConnect::postConstruct()
{
    requestTimer = xTimerCreate(GatasConnect::NAME,
                                TASK_DELAY_MS(1000),
                                pdTRUE,
                                this,
                                requestTimerCallbackTrampoline);

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
    stream << "\"bytesReceived:kb\":" << statistics.bytesReceived;
    stream << ",\"bytesSend:kb\":" << statistics.bytesSend;
    stream << ",\"pkgReceived:k\":" << statistics.pkgReceived;
    stream << ",\"pkgSend:k\":" << statistics.pkgSend;
    stream << ",\"bufferAlloc:err\":" << statistics.bufferAllocErr;
    stream << ",\"msgSendFailed:err\":" << statistics.msgSendFailed;
    stream << ",\"hasConnection:b\":" << statistics.hasConnection;
    stream << "}";
}

void GatasConnect::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void GatasConnect::on_receive(const GATAS::GpsStatsMsg &msg)
{
    hasGpsFix = msg.gpsFix.hasFix;
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
    ownshipPosition = SpinlockGuard::copyWithLock(spinLock, msg.position);
}

void GatasConnect::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == GatasConnect::NAME || msg.moduleName == Configuration::NAME)
    {
        getConfig(msg.config);
    }
}

void GatasConnect::getConfig(const Configuration &config)
{
    pinCode = static_cast<uint32_t>(config.valueByPath(0, NAME, "pinCode"));
    // 0 for no pincode
    pinCode = (pinCode == 0) ? 0 : etl::clamp(pinCode, static_cast<uint32_t>(1000), static_cast<uint32_t>(999999));

    auto gatasConfig = config.gaTasConfig();
    if (SPINLOCK_GUARD(spinLock))
    {
        gatasServerStr = config.strValueByPath("gatas.vantwisk.nl", NAME, "gatasServer/ip");
        icaoAddress = gatasConfig.conspicuity.icaoAddress;
        allIcaoAddresses = gatasConfig.allIcaoAddresses;
        gatasId = config.internalStore()->gatasId;
        localConfigurationUpdateCnt = LOCALCONFIGURATIONCHANGE_HOLD_BACK;
        gatasServerIPAddress=IPADDR4_INIT(IPADDR_NONE);
    }
}

void GatasConnect::resolveGatasServerCallback(const char *name, const ip_addr_t *ipaddr, void *arg)
{
    (void)name;
    GatasConnect *ctx = static_cast<GatasConnect *>(arg);
    if (ipaddr != nullptr && ipaddr->addr != IPADDR_NONE)
    {
        ctx->gatasServerIPAddress = *ipaddr;
        GATAS_INFO("Resolved GATAS server %s -> %u.%u.%u.%u", name, ip4_addr1(ipaddr), ip4_addr2(ipaddr), ip4_addr3(ipaddr), ip4_addr4(ipaddr));
    }
    else
    {
        GATAS_WARN("Failed to resolve GATAS server %s, will retry", name);
    }
}

bool GatasConnect::resolveIP()
{
    if (gatasServerIPAddress.addr == IPADDR_NONE)
    {
        // dns_gethostbyname will parse the IP if the hostname is an IP string, so we can use it for both hostname and IP address resolution
        err_t err = dns_gethostbyname(gatasServerStr.c_str(), &gatasServerIPAddress, resolveGatasServerCallback, this);
        if (err == ERR_INPROGRESS || err == ERR_OK)
        {
            // Async lookup in progress or IP is known
        }
        else
        {
            // Immediate failure, will retry next tick
            GATAS_WARN("dns_gethostbyname failed for %s, retrying", gatasServerStr.c_str());
        }
    }
    return gatasServerIPAddress.addr != IPADDR_NONE;
}

void GatasConnect::receiveUdpMessage(void *arg, struct udp_pcb *pcb,
                                     struct pbuf *pbuf, const ip_addr_t *addr, u16_t port)
{
    (void)pcb;
    (void)addr;
    (void)port;
    GatasConnect *taskCtx = (GatasConnect *)(arg);

    if (pbuf == nullptr)
    {
        return;
    }
    ScopedPbuf scopedPbuf(pbuf);

    if (taskCtx->localConfigurationUpdateCnt)
    {
        taskCtx->localConfigurationUpdateCnt -= 1;
    }
    else
    {
        taskCtx->lastSendCounter = 0;
        auto ownship = SpinlockGuard::copyWithLock(taskCtx->spinLock, taskCtx->ownshipPosition);
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
    }
}

void GatasConnect::requestTimerCallbackTrampoline(TimerHandle_t xTimer)
{
    GatasConnect *taskCtx = (GatasConnect *)pvTimerGetTimerID(xTimer);
    taskCtx->requestTimerCallback(xTimer);
}

/**
 * Prepare a position request to the server, the serveer will then send aircraft information back
 */
void GatasConnect::requestTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;

    constexpr size_t COBS_EXTRA_BYTES = 3;
    constexpr size_t OWN_MAX = BinaryMessages::serializeOwnshipPositionSizeV1().items();
    constexpr size_t CFG_MAX = BinaryMessages::serializeAircraftConfigurationSizeV2().items(GATAS::MAX_AIRCRAFT_CONFIG);
    constexpr size_t MAX_MSG = etl::max(OWN_MAX, CFG_MAX);

    const size_t ownshipSize = BinaryMessages::serializeOwnshipPositionSizeV1().items(1);
    const size_t configSize = BinaryMessages::serializeAircraftConfigurationSizeV2().items(allIcaoAddresses.size());
    GATAS_ASSERT((etl::max(ownshipSize, configSize) + COBS_EXTRA_BYTES) < 255, "COBS max length exceeded");
    //    printf("Max Size: %u, %u,%u, %u, %u\n", OWN_MAX, CFG_MAX, ownshipSize, configSize, etl::max(ownshipSize, configSize));

    if (!wifiConnected || !resolveIP())
    {
        return;
    }

    static_assert((MAX_MSG + COBS_EXTRA_BYTES) < ANDROIDHOTSPOT_FIX_HIGHMARK, "Data object cannot be bigger than ANDROIDHOTSPOT_FIX_HIGHMARK");
    etl::array<uint8_t, ANDROIDHOTSPOT_FIX_HIGHMARK> cobsPayload;
    size_t position = 0;
    etl::array<uint8_t, OWN_MAX + MAX_MSG + COBS_EXTRA_BYTES * 2> perCobsBuffer;
    etl::bit_stream_writer writer(perCobsBuffer.data(), perCobsBuffer.size(), etl::endian::big);

    if (SPINLOCK_GUARD(spinLock))
    {
        // --- Ownship (optional)
        if (hasGpsFix)
        {
            writer.restart();
            // 28 Byte
            BinaryMessages::serializeOwnshipPositionV1(writer, ownshipPosition);
            auto size = encodeCOBS(perCobsBuffer.data(), ownshipSize, cobsPayload.data() + position, cobsPayload.size() - position, true);
            position += size;
        }

        // --- Aircraft configuration (always send)
        writer.restart();
        // > 25 Byte
        BinaryMessages::serializeAircraftConfigurationV2(writer, gatasId, icaoAddress, allIcaoAddresses, gatasIp, pinCode);
        auto size = encodeCOBS(perCobsBuffer.data(), configSize, cobsPayload.data() + position, cobsPayload.size() - position, true);
        position += size;
    }

    // Add android hotspot fix when Android would not route packages because they where to small
    if (androidHotspotFix)
    {
        lastSendCounter += 1;
        size_t fillSize = ANDROIDHOTSPOT_FIX_LOWMARK;
        if (lastSendCounter > ANDROIDHOTSPOT_COUNT_UNTILL_HIGH)
        {
            lastSendCounter = ANDROIDHOTSPOT_COUNT_UNTILL_HIGH;
            fillSize = ANDROIDHOTSPOT_FIX_HIGHMARK;
        }

        size_t toFill = fillSize - position;
        if (toFill > 0)
        {
            etl::fill_n(cobsPayload.begin() + position, toFill, 0x00);
            position = fillSize;
        }
    }

    struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, position, PBUF_POOL);
    if (!pbuf)
    {
        statistics.bufferAllocErr++;
        return;
    }
    if (pbuf_take(pbuf, cobsPayload.begin(), position) != ERR_OK)
    {
        pbuf_free(pbuf);
        return;
    }

    // --- Send
    //    ip_addr_t addr;
    //    ip4_addr_set_u32(&addr, gatasServer.ip);
    auto err = udp_sendto(pcbSend, pbuf, &gatasServerIPAddress, GATAS_CONNECT_PORT);
    pbuf_free(pbuf);
    if (err == ERR_OK)
    {
        statistics.hasConnection = true;
        statistics.pkgSend += 1;
        statistics.bytesSend += position;
    }
    else
    {
        statistics.msgSendFailed += 1;
        statistics.hasConnection = false;
    }
}
