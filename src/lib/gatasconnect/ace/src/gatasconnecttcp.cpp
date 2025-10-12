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

void GatasConnect::tcpReceiveHandler(etl::span<uint8_t> data)
{

    // Reset the pkgCount to get a honest pkgSend vs pkg Received
    if (!statistics.hasConnection)
    {
        statistics.pkgReceived = 0;
        statistics.hasConnection = true;
    }
    statistics.bytesReceived += data.size();
    statistics.pkgReceived += 1;

    printf("COBS Size %d", data.size());

    auto ownship = ownshipPosition.load(etl::memory_order_acquire);
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
    GatasConnect *taskCtx = (GatasConnect *)pvTimerGetTimerID(xTimer);
    if (!taskCtx->wifiConnected) {
        return;
    }
    if (taskCtx->tcpClient.isDisconnected())
    {
        taskCtx->stoppedCounter++;
        taskCtx->tcpClient.start();
    }
    if (taskCtx->tcpClient.isConnected())
    {
        xTimerChangePeriod(taskCtx->requestTimer, TASK_DELAY_MS(1000), portMAX_DELAY);
        return;
    }

    constexpr size_t COBS_EXTRA_BYTES = 3;
    constexpr size_t OWN_MAX = BinaryMessages::serializeOwnshipPositionSizeV1().items();
    constexpr size_t CFG_MAX = BinaryMessages::serializeAircraftConfigurationSizeV1().items(GATAS::MAX_AIRCRAFT_CONFIGURATIONS);
    constexpr size_t MAX_MSG = etl::max(OWN_MAX, CFG_MAX);

    const size_t ownshipSize = BinaryMessages::serializeOwnshipPositionSizeV1().items(1);
    const size_t configSize = BinaryMessages::serializeAircraftConfigurationSizeV1().items(taskCtx->allIcaoAddresses.size());
    GATAS_ASSERT((etl::max(ownshipSize, configSize) + COBS_EXTRA_BYTES) < 255, "COBS max length exceeded");

    etl::array<uint8_t, MAX_MSG + COBS_EXTRA_BYTES> cobsPayload;
    size_t position = 0;
    etl::array<uint8_t, OWN_MAX + MAX_MSG + COBS_EXTRA_BYTES * 2> perCobsBuffer;
    etl::bit_stream_writer writer(perCobsBuffer.data(), perCobsBuffer.size(), etl::endian::big);

    // --- Ownship (optional)
    if (taskCtx->hasGpsFix)
    {
        writer.restart();
        BinaryMessages::serializeOwnshipPositionV1(writer, taskCtx->ownshipPosition.load(etl::memory_order_acquire));
        auto size = encodeCOBS(perCobsBuffer.data(), ownshipSize, cobsPayload.data() + position, cobsPayload.size() - position, true);
        position += size;
    }

    // --- Aircraft configuration (always send)
    writer.restart();
    BinaryMessages::serializeAircraftConfigurationV1(writer, taskCtx->gatasId, taskCtx->icaoAddress, taskCtx->allIcaoAddresses, taskCtx->gatasIp);
    auto size = encodeCOBS(perCobsBuffer.data(), configSize, cobsPayload.data() + position, cobsPayload.size() - position, true);
    position += size;

    // --- Send
    taskCtx->tcpClient.write(etl::span<uint8_t>(cobsPayload.begin(), position));

    xTimerChangePeriod(taskCtx->requestTimer, TASK_DELAY_MS(1000), portMAX_DELAY);
}
