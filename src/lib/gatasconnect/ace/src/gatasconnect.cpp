#include <stdio.h>

#include "../gatasconnect.hpp"
#include "ace/coreutils.hpp"
#include "ace/cobs.hpp"
#include "ace/debug.hpp"

#include "etl/array.h"
#include "etl/algorithm.h"

GATAS::PostConstruct GatasConnect::postConstruct()
{
    requestTimer = xTimerCreate(GatasConnect::NAME, TASK_DELAY_MS(1000), pdTRUE, this, requestTimerCallbackTrampoline);

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
}

void GatasConnect::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"hasGpsFix:b\":" << hasGpsFix;
    stream << ",\"output\":\"" << output.c_str() << "\"";
    stream << ",\"localConfigurationUpdateCnt\":" << localConfigurationUpdateCnt;
    stream << ",\"lastRadioTrafficUs\":" << lastRadioTrafficUs;
    stream << "}";
}

void GatasConnect::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void GatasConnect::on_receive(const GATAS::WifiConnectionStateMsg &wcs)
{
    gatasIp = wcs.gatasIp;
}

void GatasConnect::on_receive(const GATAS::GpsStatsMsg &msg)
{
    hasGpsFix = msg.gpsStats.gpsFix.hasFix;
}

void GatasConnect::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), msg.position);
}

void GatasConnect::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == GatasConnect::NAME || msg.moduleName == Configuration::NAME)
    {
        getConfig(msg.config);
    }
}

void GatasConnect::on_receive(const GATAS::IngressAircraftPositionMsg &msg)
{
    if (msg.position.dataSource < GATAS::DataSource::_RADIO)
    {
        lastRadioTrafficUs = CoreUtils::timeUs64();
    }
}

void GatasConnect::on_receive(const GATAS::GatasConnectRx &msg)
{
    if (localConfigurationUpdateCnt)
    {
        localConfigurationUpdateCnt -= 1;
        return;
    }

    if (!msg.cobsMessage || msg.length == 0)
    {
        return;
    }

    auto ownship = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), ownshipPosition);
    cobsStreamHandler.handle(ownship.lat, ownship.lon, etl::span<uint8_t>(msg.cobsMessage.get(), msg.length));
}

void GatasConnect::getConfig(const Configuration &config)
{
    pinCode = static_cast<uint32_t>(config.valueByPath(0, NAME, "pinCode"));
    pinCode = (pinCode == 0) ? 0 : etl::clamp(pinCode, static_cast<uint32_t>(1000), static_cast<uint32_t>(999999));

    auto outputValue = config.strValueByPath("udp", NAME, "output");
    if (outputValue == "bluetooth")
    {
        output = GATAS::GatasConnectOutput::Bluetooth;
    }
    else
    {
        output = GATAS::GatasConnectOutput::UDP;
    }

    auto gatasConfig = config.gaTasConfig();
    auto guard = SpinlockGuard{CoreUtils::sharedSpinLock()};
    localConfigurationUpdateCnt = LOCALCONFIGURATIONCHANGE_HOLD_BACK;
    icaoAddress = gatasConfig.conspicuity.icaoAddress;
    allIcaoAddresses = gatasConfig.allIcaoAddresses;
    groundStation = gatasConfig.conspicuity.groundStation;
    gatasId = config.internalStore()->gatasId;
}

void GatasConnect::publishTx(GATAS::GatasConnectOutput output_, const uint8_t *data, size_t length)
{
    if (length == 0)
    {
        return;
    }

    auto &pool = BaseModule::getGlobalPool();
    auto *copy = static_cast<uint8_t *>(pool.alloc(length));
    if (copy == nullptr)
    {
        GATAS_WARN("GatasConnect: failed to allocate %u bytes for tx payload", static_cast<unsigned>(length));
        return;
    }

    etl::copy(data, data + length, copy);
    getBus().receive(GATAS::GatasConnectTx(pool, output_, copy, length));
}

/**
 * Prepare a position request to the connected transport. The transport will
 * forward the COBS payload to the appropriate endpoint.
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

    size_t position = 0;
    etl::array<uint8_t, OWN_MAX + MAX_MSG + COBS_EXTRA_BYTES * 2> perCobsBuffer;
    etl::bit_stream_writer writer(perCobsBuffer.data(), perCobsBuffer.size(), etl::endian::big);

    // Snapshot shared state under lock, then do encoding outside the critical section.
    bool groundStationSnap = false;
    bool hasGpsFixSnap = false;
    uint64_t lastRadioTrafficUsSnap = 0;
    uint64_t gatasIdSnap = 0;
    uint32_t icaoAddressSnap = 0;
    uint32_t gatasIpSnap = 0;
    uint32_t pinCodeSnap = 0;
    GATAS::OwnshipPositionInfo ownshipSnap{};
    etl::vector<uint32_t, GATAS::MAX_AIRCRAFT_CONFIG> allIcaoAddressesSnap;

    if (auto guard = SpinlockGuard{CoreUtils::sharedSpinLock()})
    {
        groundStationSnap = groundStation;
        hasGpsFixSnap = hasGpsFix;
        lastRadioTrafficUsSnap = lastRadioTrafficUs;
        gatasIdSnap = gatasId;
        icaoAddressSnap = icaoAddress;
        gatasIpSnap = gatasIp;
        pinCodeSnap = pinCode;
        ownshipSnap = ownshipPosition;
        allIcaoAddressesSnap = allIcaoAddresses;
    }

    // Receive traffic for at least 60 seconds more
    // This is here to reduce pressure on the GatasServer when there is no known traffic
    // that can benefit from GATAS
    constexpr uint64_t RADIO_TRAFFIC_WINDOW_US = 60'000'000ULL;
    const bool hasRecentRadioTraffic = (CoreUtils::timeUs64() - lastRadioTrafficUsSnap) < RADIO_TRAFFIC_WINDOW_US;

    // In groundstation mode we require that we only fetch traffic when we see actual traffic to reduce load on the gatasServer
    // Otherwhise the system would keep fetching traffic for no reason.
    const bool sendOwnship = (hasRecentRadioTraffic || !groundStationSnap) && hasGpsFixSnap;

    const size_t ownshipCobsSize = sendOwnship ? getCOBSBufferSize(ownshipSize, true) : 0;
    const size_t configCobsSize = getCOBSBufferSize(configSize, true);
    const size_t totalCobsSize = ownshipCobsSize + configCobsSize;
    auto &pool = BaseModule::getGlobalPool();
    auto *cobsPayload = static_cast<uint8_t *>(pool.alloc(totalCobsSize));
    if (cobsPayload == nullptr)
    {
        GATAS_WARN("GatasConnect: failed to allocate %u bytes for request payload", static_cast<unsigned>(totalCobsSize));
        return;
    }

    if (sendOwnship)
    {
        // --- Ownship position: requests surrounding traffic data from server
        writer.restart();
        BinaryMessages::serializeOwnshipPositionV1(writer, ownshipSnap);
        auto size = encodeCOBS(perCobsBuffer.data(), ownshipSize, cobsPayload + position, totalCobsSize - position, true);
        position += size;
    }

    // --- Aircraft configuration (always send)
    writer.restart();
    // > 25 Byte
    BinaryMessages::serializeAircraftConfigurationV2(writer, gatasIdSnap, icaoAddressSnap, allIcaoAddressesSnap, gatasIpSnap, pinCodeSnap);
    auto size = encodeCOBS(perCobsBuffer.data(), configSize, cobsPayload + position, totalCobsSize - position, true);
    position += size;

    getBus().receive(GATAS::GatasConnectTx{pool, output, cobsPayload, position});
}

void GatasConnect::requestTimerCallbackTrampoline(TimerHandle_t xTimer)
{
    GatasConnect *taskCtx = (GatasConnect *)pvTimerGetTimerID(xTimer);
    taskCtx->requestTimerCallback(xTimer);
}
