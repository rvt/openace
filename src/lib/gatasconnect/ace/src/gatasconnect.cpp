#include <stdio.h>

#include "../gatasconnect.hpp"
#include "ace/coreutils.hpp"
#include "ace/debug.hpp"

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
    stream << ",\"gdl90BridgeEnabled:b\":" << gdl90BridgeEnabled;
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
    wifiMode = wcs.wifiMode;
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

void GatasConnect::on_receive(const GATAS::GdlMsg &msg)
{
    if (output != GATAS::GatasConnectOutput::Bluetooth || !gdl90BridgeEnabled || msg.msg.empty())
    {
        return;
    }

    const size_t cobsSize = BinaryMessages::serializeGdl90FramedSizeV1(msg.msg.size());
    auto &pool = BaseModule::getGlobalPool();
    auto *cobsPayload = static_cast<uint8_t *>(pool.alloc(cobsSize));
    if (cobsPayload == nullptr)
    {
        GATAS_WARN("GatasConnect: failed to allocate %u bytes for GDL90 bridge payload", static_cast<unsigned>(cobsSize));
        return;
    }

    const size_t encodedSize = BinaryMessages::serializeGdl90V1(cobsPayload, cobsSize, etl::span<const uint8_t>(msg.msg.data(), msg.msg.size()));
    if (encodedSize == 0)
    {
        GATAS_WARN("GatasConnect: failed to encode GDL90 bridge payload");
        pool.release(cobsPayload);
        return;
    }

    getBus().receive(GATAS::GatasConnectTx{pool, GATAS::GatasConnectOutput::Bluetooth, cobsPayload, encodedSize});
}

void GatasConnect::getConfig(const Configuration &config)
{
    pinCode = static_cast<uint32_t>(config.valueByPath(0, NAME, "pinCode"));
    pinCode = (pinCode == 0) ? 0 : etl::clamp(pinCode, static_cast<uint32_t>(1000), static_cast<uint32_t>(999999));
    gdl90BridgeEnabled = config.valueByPath(false, NAME, "enableGdl90Bridge");

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

/**
 * Prepare a position request to the connected transport. The transport will
 * forward the COBS payload to the appropriate endpoint.
 */
void GatasConnect::requestTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    sendAircraftConfiguration();
    sendOwnshipPosition();
}

void GatasConnect::sendOwnshipPosition()
{
    bool groundStationSnap = false;
    bool hasGpsFixSnap = false;
    uint64_t lastRadioTrafficUsSnap = 0;
    GATAS::OwnshipPositionInfo ownshipSnap{};
    GATAS::GatasConnectOutput outputSnap = GATAS::GatasConnectOutput::UDP;

    if (auto guard = SpinlockGuard{CoreUtils::sharedSpinLock()})
    {
        groundStationSnap = groundStation;
        hasGpsFixSnap = hasGpsFix;
        lastRadioTrafficUsSnap = lastRadioTrafficUs;
        ownshipSnap = ownshipPosition;
        outputSnap = output;
    }

    // Receive traffic for at least 60 seconds more
    // This is here to reduce pressure on the GatasServer when there is no known traffic
    // that can benefit from GATAS
    constexpr uint64_t RADIO_TRAFFIC_WINDOW_US = 60'000'000ULL;
    const bool hasRecentRadioTraffic = (CoreUtils::timeUs64() - lastRadioTrafficUsSnap) < RADIO_TRAFFIC_WINDOW_US;

    // In groundstation mode we require that we only fetch traffic when we see actual traffic to reduce load on the gatasServer
    // Otherwhise the system would keep fetching traffic for no reason.
    const bool sendOwnship = (hasRecentRadioTraffic || !groundStationSnap) && hasGpsFixSnap;
    if (!sendOwnship)
    {
        return;
    }

    const size_t ownshipFrameSize = BinaryMessages::serializeOwnshipPositionFramedSizeV1();
    auto &pool = BaseModule::getGlobalPool();
    auto *cobsPayload = static_cast<uint8_t *>(pool.alloc(ownshipFrameSize));
    if (cobsPayload == nullptr)
    {
        GATAS_WARN("GatasConnect: failed to allocate %u bytes for ownship request payload", ownshipFrameSize);
        return;
    }

    const size_t written = BinaryMessages::serializeOwnshipPositionV1(cobsPayload, ownshipFrameSize, ownshipSnap);
    if (written == 0)
    {
        GATAS_WARN("GatasConnect: failed to encode ownship request payload");
        pool.release(cobsPayload);
        return;
    }

    getBus().receive(GATAS::GatasConnectTx{pool, outputSnap, cobsPayload, written});
}

void GatasConnect::sendAircraftConfiguration()
{
    uint64_t gatasIdSnap = 0;
    uint32_t icaoAddressSnap = 0;
    uint32_t gatasIpSnap = 0;
    uint32_t pinCodeSnap = 0;
    GATAS::WifiMode wifiModeSnap = GATAS::WifiMode::NC;
    etl::vector<uint32_t, GATAS::MAX_AIRCRAFT_CONFIG> allIcaoAddressesSnap;

    if (auto guard = SpinlockGuard{CoreUtils::sharedSpinLock()})
    {
        gatasIdSnap = gatasId;
        icaoAddressSnap = icaoAddress;
        gatasIpSnap = gatasIp;
        pinCodeSnap = pinCode;
        wifiModeSnap = wifiMode;
        allIcaoAddressesSnap = allIcaoAddresses;
    }

    const size_t configFrameSize = BinaryMessages::serializeAircraftConfigurationFramedSizeV2(allIcaoAddressesSnap.size());
    auto &pool = BaseModule::getGlobalPool();
    auto *cobsPayload = static_cast<uint8_t *>(pool.alloc(configFrameSize));
    if (cobsPayload == nullptr)
    {
        GATAS_WARN("GatasConnect: failed to allocate %u bytes for configuration payload", configFrameSize);
        return;
    }

    const size_t written = BinaryMessages::serializeAircraftConfigurationV2(cobsPayload, configFrameSize, gatasIdSnap, icaoAddressSnap, allIcaoAddressesSnap, gatasIpSnap, pinCodeSnap, wifiModeSnap);
    if (written == 0)
    {
        GATAS_WARN("GatasConnect: failed to encode configuration payload");
        pool.release(cobsPayload);
        return;
    }

    getBus().receive(GATAS::GatasConnectTx{pool, GATAS::GatasConnectOutput::Broadcast, cobsPayload, written});
}

void GatasConnect::requestTimerCallbackTrampoline(TimerHandle_t xTimer)
{
    GatasConnect *taskCtx = (GatasConnect *)pvTimerGetTimerID(xTimer);
    taskCtx->requestTimerCallback(xTimer);
}
