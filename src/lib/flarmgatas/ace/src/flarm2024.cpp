#include <stdio.h>

#include "../flarm2024.hpp"
#include "flarm/flarm2024packet.hpp"
#include "ace/spinlockguard.hpp"

GATAS::PostConstruct Flarm2024::postConstruct()
{
    return GATAS::PostConstruct::OK;
}

void Flarm2024::start()
{
    getBus().subscribe(*this);
};

void Flarm2024::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    for (const auto &stat : datasourceTimeStats.span())
    {
        stream << "\"f" << stat.frequency << "\":\"" << stat.timeTenthMs.to_string() << "\",";
    }
    stream << "\"receivedAircraftPositions\":" << statistics.receivedAircraftPositions;
    stream << ",\"transmittedAircraftPositions\":" << statistics.transmittedAircraftPositions;
    stream << ",\"crcErr\":" << statistics.crcErr;
    stream << ",\"outOfDistance\":" << statistics.outOfDistance;
    stream << ",\"messageTypeNot0x02\":" << statistics.messageTypeNot0x02;
    stream << "}";
}

void Flarm2024::on_receive(const GATAS::RadioRxGfskMsg &msg)
{
    if (msg.dataSource == GATAS::DataSource::FLARM)
    {
        datasourceTimeStats.addReceiveStat(msg.frequency, CoreUtils::msInSecond());
        auto epochSeconds = CoreUtils::secondsSinceEpoch();

        Flarm2024Packet packet;

        auto result = packet.loadFromBuffer(epochSeconds, {msg.frame, Flarm2024Packet::TOTAL_LENGTH_WORDS});
        if (result == -1)
        {
            statistics.crcErr += 1;
            return;
        }
        else if (result != 0)
        {
            // LSB seconds error not matched, or any other
            return;
        }


        if (packet.messageType() != 0x02)
        {
            statistics.messageTypeNot0x02 += 1;
            return;
        }

        auto ownship = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), ownshipPosition);
        if (packet.aircraftId() == ownship.conspicuity.icaoAddress)
        {
            return;
        }

        auto otherPos = packet.getPosition(ownship.lat, ownship.lon);
        auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownship.lat, ownship.lon, otherPos.latitude, otherPos.longitude);

        if (fromOwn.distance > distanceIgnore)
        {
            statistics.outOfDistance += 1;
            return;
        }

        auto aircraftCat = toAircraftCategory(packet.aircraftType());

        GATAS::AircraftPositionMsg aircraftPosition{
            GATAS::AircraftPositionInfo{
                CoreUtils::timeUs32(),
                "",
                packet.aircraftId(),
                addressTypeFromFlarm(packet.addressType()),
                GATAS::DataSource::FLARM,
                aircraftCat,
                packet.stealth(),
                packet.noTrack(),
                CoreUtils::isAirborn(aircraftCat, packet.groundSpeed()),
                otherPos.latitude,
                otherPos.longitude,
                packet.altitude(),
                packet.verticalSpeed(),
                packet.groundSpeed(),
                (int16_t)(packet.groundTrack() + 0.5f),
                packet.turnRate(),
                fromOwn.distance,
                fromOwn.relNorth,
                fromOwn.relEast},
            msg.rssidBm};

        statistics.receivedAircraftPositions += 1;
        getBus().receive(aircraftPosition);
    }
}

void Flarm2024::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), msg.position);
}

void Flarm2024::on_receive(const GATAS::RadioTxPositionRequestMsg &msg)
{
    if (msg.radioParameters.config->isTxDataSource(GATAS::DataSource::FLARM))
    {
        Flarm2024Packet packet;
        auto epochSeconds = CoreUtils::secondsSinceEpoch();

        auto ownship = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), ownshipPosition);

        packet.aircraftId(ownship.conspicuity.icaoAddress);
        packet.messageType(0x02);
        packet.addressType(static_cast<uint8_t>(ownship.conspicuity.addressType));
        packet.stealth(ownship.conspicuity.stealth);
        packet.noTrack(ownship.conspicuity.noTrack);
        packet.epochSeconds(epochSeconds);
        packet.aircraftType(fromAircraftCategory(ownship.conspicuity.category));
        packet.altitude(ownship.ellipseHeight);
        packet.setPosition(ownship.lat, ownship.lon);
        packet.turnRate(ownship.hTurnRate);
        packet.groundSpeed(ownship.groundSpeed);
        packet.verticalSpeed(ownship.verticalSpeed);
        packet.groundTrack(ownship.track);
        // Abuse GROUNDSPEED_CONSIDERING_AIRBORN for movement status, which is not AirBorn status, but more what the aircraft is doing
        packet.movementStatus(ownship.groundSpeed > GATAS::GROUNDSPEED_CONSIDERING_AIRBORN ? 2 : 1);

        Radio::TxPacket txPacket;
        txPacket.radioParameters = msg.radioParameters;
        txPacket.length = Flarm2024Packet::TOTAL_LENGTH;

        packet.writeToBuffer(epochSeconds, txPacket.data32);

        statistics.transmittedAircraftPositions += 1;
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

GATAS::AircraftCategory Flarm2024::toAircraftCategory(uint8_t flarmCode) const
{

    // clang-format off
    switch (flarmCode) {
        case 1:    return GATAS::AircraftCategory::GLIDER;
        case 2:    
        case 8:    return GATAS::AircraftCategory::LIGHT;
        case 3:    return GATAS::AircraftCategory::ROTORCRAFT;
        case 4:    return GATAS::AircraftCategory::SKY_DIVER;
        case 5:    return GATAS::AircraftCategory::DROP_PLANE;
        case 6:    return GATAS::AircraftCategory::HANG_GLIDER;
        case 7:    return GATAS::AircraftCategory::PARA_GLIDER;
        case 9:    return GATAS::AircraftCategory::SMALL;
        case 0x0b:              
        case 0x0c: return GATAS::AircraftCategory::LIGHT_THAN_AIR;
        case 0x0d: return GATAS::AircraftCategory::UN_MANNED;
        case 0x0f: return GATAS::AircraftCategory::POINT_OBSTACLE;
        default:   return GATAS::AircraftCategory::UNKNOWN;
    }
    // clang-format on
}

uint8_t Flarm2024::fromAircraftCategory(GATAS::AircraftCategory category) const
{
    // clang-format off
    switch ( category) {
        case GATAS::AircraftCategory::GLIDER:                  return 0x01;
        case GATAS::AircraftCategory::GYROCOPTER:
        case GATAS::AircraftCategory::ROTORCRAFT:              return 0x03;
        case GATAS::AircraftCategory::SKY_DIVER:               return 0x04;
        case GATAS::AircraftCategory::DROP_PLANE:              return 0x05;
        case GATAS::AircraftCategory::HANG_GLIDER:             return 0x06;
        case GATAS::AircraftCategory::PARA_GLIDER:             return 0x07;
        case GATAS::AircraftCategory::ULTRA_LIGHT_FIXED_WING:
        case GATAS::AircraftCategory::LIGHT:                   return 0x08;
        case GATAS::AircraftCategory::SMALL:
        case GATAS::AircraftCategory::HIGH_VORTEX:
        case GATAS::AircraftCategory::HEAVY_ICAO:
        case GATAS::AircraftCategory::AEROBATIC:
        case GATAS::AircraftCategory::LARGE:                   return 0x09;
        case GATAS::AircraftCategory::LIGHT_THAN_AIR:          return 0x0b;
        case GATAS::AircraftCategory::UN_MANNED:               return 0x0d;
        case GATAS::AircraftCategory::POINT_OBSTACLE:          return 0x0f;
        
        // Cases with no direct equivalent
        case GATAS::AircraftCategory::SPACE_VEHICLE:
        case GATAS::AircraftCategory::SURFACE_EMERGENCY_VEHICLE:
        case GATAS::AircraftCategory::SURFACE_VEHICLE:
        case GATAS::AircraftCategory::CLUSTER_OBSTACLE:
        case GATAS::AircraftCategory::LINE_OBSTACLE:
        case GATAS::AircraftCategory::MILITARY:
        case GATAS::AircraftCategory::UNKNOWN:
        default: return 0;
    }
    // clang-format on
}