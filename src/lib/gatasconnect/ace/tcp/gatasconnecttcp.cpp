#include <stdio.h>

#include "../gatasconnecttcp.hpp"
#include "ace/coreutils.hpp"
#include "ace/binarymessages.hpp"
#include "ace/cobs.hpp"
#include "ace/lwiplock.hpp"
#include "ace/spinlockguard.hpp"

/* LwIP */
#include "lwip/ip_addr.h"
#include "pico/cyw43_arch.h"

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
    stream << ",\"msgSendFailed:err\":" << statistics.msgSendFailed;
    stream << ",\"hasConnection:b\":" << statistics.hasConnection;
    stream << "}";
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
    ownshipPosition = SpinlockGuard::copyWithLock(spinLock, msg.position);
}

void GatasConnect::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{

    if (msg.moduleName == Configuration::NAME)
    {
        getConfig(msg.config);
    }
}

void GatasConnect::getConfig(const Configuration &config)
{
    gatasServer = config.ipPortBypath(NAME, "gatasServer");
    gatasServer.port = GATAS_CONNECT_PORT;

    auto gatasConfig = config.gaTasConfig();
    if (SPINLOCK_GUARD(spinLock))
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

void GatasConnect::tcpReceiveHandler(etl::span<uint8_t> data)
{
    androidHotspotCurrentMark = ANDROIDHOTSPOT_FIX_LOWMARK;
    statistics.bytesReceived += data.size();
    statistics.pkgReceived += 1;

    auto ownship = SpinlockGuard::copyWithLock(spinLock, ownshipPosition);
    cobsStreamHandler.handle(ownship.lat, ownship.lon, data);
}

void GatasConnect::tcpSentHandler(uint16_t data)
{
    (void)data;
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
    constexpr size_t CFG_MAX = BinaryMessages::serializeAircraftConfigurationSizeV1().items(GATAS::MAX_AIRCRAFT_CONFIG);
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

    if (SPINLOCK_GUARD(taskCtx->spinLock))
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

    if (taskCtx->androidHotspotFix)
    {
        size_t toFill = taskCtx->androidHotspotCurrentMark - position;
        if (toFill>0) {
            memset(cobsPayload.begin() + position, 0x00, toFill);
            position = taskCtx->androidHotspotCurrentMark;
        }
    }

    // --- Send
    if (taskCtx->tcpClient.write(etl::span<uint8_t>(cobsPayload.begin(), position), true))
    {
        taskCtx->statistics.hasConnection = true;
        taskCtx->statistics.pkgSend += 1;
        taskCtx->statistics.bytesSend += position;
    }
    else
    {
        taskCtx->statistics.msgSendFailed += 1;
        taskCtx->statistics.hasConnection = false;
        taskCtx->androidHotspotCurrentMark = ANDROIDHOTSPOT_FIX_HIGHMARK;
    }
}
