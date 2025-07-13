#include <stdio.h>

#include "flarm2024.hpp"
#include "flarm/flarm2024packet.hpp"
#include "ace/manchester.hpp"

GATAS::PostConstruct Flarm2024::postConstruct()
{
    return GATAS::PostConstruct::OK;
}

void Flarm2024::start()
{
    getBus().subscribe(*this);
};

void Flarm2024::stop()
{
    getBus().unsubscribe(*this);
};

void Flarm2024::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    for (const auto &stat : dataSourceTimeStats)
    {
        stream << "\"f" << stat.frequency << "\":\"" << stat.timeTenthMs.to_string() << "\",";
    }
    stream << "\"receivedAircraftPositions\":" << statistics.receivedAircraftPositions;
    stream << ",\"transmittedAircraftPositions\":" << statistics.transmittedAircraftPositions;
    stream << ",\"crcErr\":" << statistics.crcErr;
    stream << ",\"outOfDistance\":" << statistics.outOfDistance;
    stream << ",\"messageTypeNot0x02\":" << statistics.messageTypeNot0x02;
    stream << "}\n";
}

void Flarm2024::addReceiveStat(uint32_t frequency)
{
    // TODO: Something strange happening with multiple frequencies we receive
    auto msInSec = 99 - CoreUtils::msInSecond() / 10;
    for (auto &stat : dataSourceTimeStats)
    {
        if (stat.frequency == frequency)
        {
            stat.timeTenthMs.set(msInSec);
            return;
        }
    }

    if (!dataSourceTimeStats.full())
    {
        // If frequency not found, add a new stat
        dataSourceTimeStats.push_back(DataSourceTimeStats{});
        dataSourceTimeStats.back().frequency = frequency;
        dataSourceTimeStats.back().timeTenthMs.set(msInSec);
    }
}

void Flarm2024::on_receive(const GATAS::RadioRxGfskMsg &msg)
{
    if (msg.dataSource == GATAS::DataSource::FLARM)
    {
        auto epochSeconds = CoreUtils::secondsSinceEpoch();

        Flarm2024Packet packet;
        auto ownship = ownshipPosition.load(etl::memory_order_acquire);
        auto result = packet.loadFromBuffer(epochSeconds, {msg.frame, Flarm2024Packet::TOTAL_LENGTH_WORDS});
        if (result == -1) {
            statistics.crcErr++;
            return;
        } else if (result != 0) {
            // LSB seconds error not matched, or any other
            return;
        }
        addReceiveStat(msg.frequency);

        if ( packet.messageType() != 0x02) {
            statistics.messageTypeNot0x02++;
            return;
        }

        if (packet.aircraftId() == gaTasConfiguration.address)
        {
            return;
        }

        auto otherPos = packet.getPosition(ownship.lat, ownship.lon);
        auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownship.lat, ownship.lon, otherPos.latitude, otherPos.longitude);

        if (fromOwn.distance > distanceIgnore)
        {
            statistics.outOfDistance++;
            return;
        }

        statistics.receivedAircraftPositions++;
        GATAS::AircraftPositionMsg aircraftPosition{
            GATAS::AircraftPositionInfo{
                CoreUtils::timeUs32(),
                "",
                packet.aircraftId(),
                addressTypeFromFlarm(packet.addressType()),
                GATAS::DataSource::FLARM,
                static_cast<GATAS::AircraftCategory>(packet.aircraftType()),
                packet.stealth(),
                packet.noTrack(),
                packet.groundSpeed() > GATAS::GROUNDSPEED_CONSIDERING_AIRBORN,
                otherPos.latitude,
                otherPos.longitude,
                packet.altitude(),
                packet.verticalSpeed(),
                packet.groundSpeed(),
                (int16_t)(packet.groundTrack() + 0.5f),
                packet.turnRate(),
                fromOwn.distance,
                fromOwn.relNorth,
                fromOwn.relEast,
                fromOwn.bearing},
            msg.rssidBm};

        statistics.transmittedAircraftPositions++;
        getBus().receive(aircraftPosition);
    }
}

void Flarm2024::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition.store(msg.position, etl::memory_order_release);
}

void Flarm2024::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::CONFIG)
    {
        gaTasConfiguration = msg.config.gaTasConfig();
    }
}

void Flarm2024::on_receive(const GATAS::RadioTxPositionRequestMsg &msg)
{
    if (msg.radioParameters.config->dataSource == GATAS::DataSource::FLARM)
    {
        Flarm2024Packet packet;
        auto epochSeconds = CoreUtils::secondsSinceEpoch();
        auto ownship = ownshipPosition.load(etl::memory_order_acquire);

        packet.aircraftId(gaTasConfiguration.address);
        packet.messageType(0x02);
        packet.addressType(static_cast<uint8_t>(gaTasConfiguration.addressType));
        packet.stealth(gaTasConfiguration.stealth);
        packet.noTrack(gaTasConfiguration.noTrack);
        packet.epochSeconds(epochSeconds);
        packet.aircraftType( static_cast<uint8_t>(gaTasConfiguration.category));
        packet.altitude(ownship.altitudeHAE);
        packet.setPosition(ownship.lat, ownship.lon);
        packet.turnRate(ownship.hTurnRate);
        packet.groundSpeed(ownship.groundSpeed);
        packet.verticalSpeed(ownship.verticalSpeed);
        packet.groundTrack(ownship.course);
        packet.movementStatus(ownship.groundSpeed > GATAS::GROUNDSPEED_CONSIDERING_AIRBORN ? 2 : 1);

        Radio::TxPacket txPacket;
        txPacket.radioParameters = msg.radioParameters;
        txPacket.length = Flarm2024Packet::TOTAL_LENGTH;

        packet.writeToBuffer(epochSeconds, txPacket.data32);
        getBus().receive(GATAS::RadioTxFrameMsg{
            txPacket,
            msg.radioNo});
    }
}

GATAS::AddressType Flarm2024::addressTypeFromFlarm(uint8_t addressType) const
{
    constexpr etl::array<GATAS::AddressType, 3> lookupTable =
        {
            GATAS::AddressType::RANDOM, // 0x00
            GATAS::AddressType::ICAO,   // 0x01
            GATAS::AddressType::FLARM,  // 0x02
        };
    if (addressType > 2)
        return GATAS::AddressType::RANDOM;
    return lookupTable[addressType];
}

uint8_t Flarm2024::addressTypeToFlarm(GATAS::AddressType addressType) const
{
    switch (addressType)
    {
    case GATAS::AddressType::ICAO:
        return 0x01;
    case GATAS::AddressType::FLARM:
        return 0x02;
    default:
        return 0;
    }
}
