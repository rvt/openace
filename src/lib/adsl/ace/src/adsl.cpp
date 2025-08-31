#include <stdio.h>

#include "../adsl.hpp"
#include "ace/bitcount.hpp"

constexpr float POSITION_DECODE = 0.0001f / 60.f;

GATAS::PostConstruct ADSL::postConstruct()
{
    return GATAS::PostConstruct::OK;
}

void ADSL::start()
{
    getBus().subscribe(*this);
};

void ADSL::stop()
{
    getBus().unsubscribe(*this);
};

void ADSL::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    for (const auto &stat : dataSourceTimeStats)
    {
        stream << "\"f" << stat.frequency << "\":\"" << stat.timeTenthMs.to_string() << "\",\n";
    }
    stream << "\"receivedAircraftPositions\":" << statistics.receivedAircraftPositions;
    stream << ",\"transmittedAircraftPositions\":" << statistics.transmittedAircraftPositions;
    stream << ",\"fecErr\":" << statistics.fecErr;
    stream << ",\"outOfDistance\":" << statistics.outOfDistance;
    stream << ",\"encrypted\":" << statistics.encrypted;
    stream << ",\"relay\":" << statistics.relay;
    stream << "}\n";
}

void ADSL::addReceiveStat(uint32_t frequency)
{
    auto msInSec = 99 - CoreUtils::msInSecond() / 10;
    for (auto &stat : dataSourceTimeStats)
    {
        if (stat.frequency == frequency)
        {
            stat.timeTenthMs.set(msInSec);
            return;
        }
    }

    // If frequency not found, add a new stat
    if (!dataSourceTimeStats.full())
    {
        // If frequency not found, add a new stat
        dataSourceTimeStats.push_back(DataSourceTimeStats{});
        dataSourceTimeStats.back().frequency = frequency;
        dataSourceTimeStats.back().timeTenthMs.set(msInSec);
    }
}
void ADSL::on_receive(const GATAS::RadioRxGfskMsg &msg)
{
    ADSL_Packet packet;
    if (msg.dataSource == GATAS::DataSource::ADSL)
    {
        auto check = ADSL_Packet::Correct((uint8_t *)msg.frame, (uint8_t *)msg.err);
        if (check == -1)
        {
            statistics.fecErr += 1;
            return;
        }
        memcpy(packet.data(), msg.frame, ADSL_Packet::TotalTxBytes);
        packet.Descramble();

        if (packet.key != 0)
        {
            statistics.encrypted += 1;
            return;
        }

        // Ignore ownship address
        auto ownship = ownshipPosition.load(etl::memory_order_acquire);
        if (packet.address == ownship.conspicuity.icaoAddress)
        {
            return;
        }

        addReceiveStat(msg.frequency);
        parseFrame(packet, msg.rssidBm);
    }
}

void ADSL::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition.store(msg.position, etl::memory_order_release);
}

void ADSL::on_receive(const GATAS::GpsStatsMsg &msg)
{
    gpsStats = msg;
}

GATAS::AddressType ADSL::addressMapToAddressType(uint8_t addressMap) const
{
    switch (addressMap)
    {
    case 0x00:
        return GATAS::AddressType::RANDOM;
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
        return GATAS::AddressType::RESERVED;
    case 0x05:
        return GATAS::AddressType::ICAO;
    case 0x06:
        return GATAS::AddressType::FLARM;
    case 0x07:
        return GATAS::AddressType::OGN;
    case 0x08:
        return GATAS::AddressType::FANET;
    // Not a bug, if we don't the the type we just say random
    default:
        return GATAS::AddressType::UNKNOWN;
    }
}

uint8_t ADSL::addressTypeToAddressMap(GATAS::AddressType addressType)
{
    switch (addressType)
    {
    case GATAS::AddressType::RANDOM:
        return 0x00;
    case GATAS::AddressType::RESERVED:
        return 0x01;
    case GATAS::AddressType::ICAO:
        return 0x05;
    case GATAS::AddressType::FLARM:
        return 0x06;
    case GATAS::AddressType::OGN:
        return 0x07;
    case GATAS::AddressType::FANET:
        return 0x08;
    default:
        return 0x00;
    }
}

GATAS::AircraftCategory ADSL::mapAircraftCategory(ADSL_Packet::AircraftCategory category)
{
    // map ADSL_Packet::AircraftCategory to GATAS::AircraftCategory
    switch (category)
    {
    case ADSL_Packet::AircraftCategory::AC_NoEmitterCategory:
        return GATAS::AircraftCategory::UNKNOWN;
    case ADSL_Packet::AircraftCategory::AC_LightFixedWing:
        return GATAS::AircraftCategory::LIGHT;
    case ADSL_Packet::AircraftCategory::AC_SmallToHeavyFixedWing:
        return GATAS::AircraftCategory::SMALL;
    case ADSL_Packet::AircraftCategory::AC_Rotorcraft:
        return GATAS::AircraftCategory::ROTORCRAFT;
    case ADSL_Packet::AircraftCategory::AC_GliderSailplane:
        return GATAS::AircraftCategory::GLIDER;
    case ADSL_Packet::AircraftCategory::AC_LighterThanAir:
        return GATAS::AircraftCategory::LIGHT_THAN_AIR;
    case ADSL_Packet::AircraftCategory::AC_Ultralight:
        return GATAS::AircraftCategory::ULTRA_LIGHT_FIXED_WING;
    case ADSL_Packet::AircraftCategory::AC_HangGliderParaglider:
        return GATAS::AircraftCategory::HANG_GLIDER;
    case ADSL_Packet::AircraftCategory::AC_ParachutistSkydiverWingsuit:
        return GATAS::AircraftCategory::SKY_DIVER;
    case ADSL_Packet::AircraftCategory::AC_Gyrocopter:
        return GATAS::AircraftCategory::ROTORCRAFT;
    case ADSL_Packet::AircraftCategory::AC_UASOpenCategory:
        return GATAS::AircraftCategory::UN_MANNED;
    case ADSL_Packet::AircraftCategory::AC_AircraftCategoryReserved:
        return GATAS::AircraftCategory::UNKNOWN;
    default:
        return GATAS::AircraftCategory::UNKNOWN;
    }
}

ADSL_Packet::AircraftCategory ADSL::mapAircraftCategory(GATAS::AircraftCategory category)
{
    switch (category)
    {
    case GATAS::AircraftCategory::UNKNOWN:
        return ADSL_Packet::AircraftCategory::AC_NoEmitterCategory;
    case GATAS::AircraftCategory::LIGHT:
        return ADSL_Packet::AircraftCategory::AC_LightFixedWing;
    case GATAS::AircraftCategory::SMALL:
        return ADSL_Packet::AircraftCategory::AC_SmallToHeavyFixedWing;
    case GATAS::AircraftCategory::LARGE:
        return ADSL_Packet::AircraftCategory::AC_SmallToHeavyFixedWing;
    case GATAS::AircraftCategory::HIGH_VORTEX:
        return ADSL_Packet::AircraftCategory::AC_SmallToHeavyFixedWing;
    case GATAS::AircraftCategory::HEAVY_ICAO:
        return ADSL_Packet::AircraftCategory::AC_SmallToHeavyFixedWing;
    case GATAS::AircraftCategory::AEROBATIC:
        return ADSL_Packet::AircraftCategory::AC_LightFixedWing; // Or could be AC_SmallToHeavyFixedWing
    case GATAS::AircraftCategory::ROTORCRAFT:
        return ADSL_Packet::AircraftCategory::AC_Rotorcraft;
    case GATAS::AircraftCategory::GLIDER:
        return ADSL_Packet::AircraftCategory::AC_GliderSailplane;
    case GATAS::AircraftCategory::LIGHT_THAN_AIR:
        return ADSL_Packet::AircraftCategory::AC_LighterThanAir;
    case GATAS::AircraftCategory::SKY_DIVER:
        return ADSL_Packet::AircraftCategory::AC_ParachutistSkydiverWingsuit;
    case GATAS::AircraftCategory::ULTRA_LIGHT_FIXED_WING:
        return ADSL_Packet::AircraftCategory::AC_Ultralight;
    case GATAS::AircraftCategory::SPACE_VEHICLE:
        return ADSL_Packet::AircraftCategory::AC_UASOpenCategory; 
    case GATAS::AircraftCategory::UN_MANNED:
        return ADSL_Packet::AircraftCategory::AC_EVTOL_UAM; // Closest match
    case GATAS::AircraftCategory::SURFACE_EMERGENCY_VEHICLE:
    case GATAS::AircraftCategory::SURFACE_VEHICLE:
        return ADSL_Packet::AircraftCategory::AC_NoEmitterCategory; // No surface category in target
    case GATAS::AircraftCategory::POINT_OBSTACLE:
    case GATAS::AircraftCategory::CLUSTER_OBSTACLE:
    case GATAS::AircraftCategory::LINE_OBSTACLE:
        return ADSL_Packet::AircraftCategory::AC_NoEmitterCategory; // No obstacle category in target

    // Handle custom values
    case GATAS::AircraftCategory::GYROCOPTER:
        return ADSL_Packet::AircraftCategory::AC_Gyrocopter;
    case GATAS::AircraftCategory::HANG_GLIDER:
        return ADSL_Packet::AircraftCategory::AC_HangGliderParaglider;
    case GATAS::AircraftCategory::PARA_GLIDER:
        return ADSL_Packet::AircraftCategory::AC_HangGliderParaglider;
    case GATAS::AircraftCategory::DROP_PLANE:
        return ADSL_Packet::AircraftCategory::AC_LightFixedWing;
    case GATAS::AircraftCategory::MILITARY:
        return ADSL_Packet::AircraftCategory::AC_SmallToHeavyFixedWing;

    default:
        return ADSL_Packet::AircraftCategory::AC_NoEmitterCategory;
    }
}

void ADSL::on_receive(const GATAS::RadioTxPositionRequestMsg &msg)
{

    if (msg.radioParameters.config->dataSource == GATAS::DataSource::ADSL)
    {
        auto ownship = ownshipPosition.load(etl::memory_order_acquire);

        ADSL_Packet packet;
        packet.payloadIdent = 0x02; // ADS-L.4.SRD860.F.2.1 :: iConspicuity
        packet.addressMapping = addressTypeToAddressMap(ownship.conspicuity.addressType);
        packet.address = ownship.conspicuity.icaoAddress;
        packet.reserved1 = 0;
        packet.relay = 0;
        packet.timeStamp = (CoreUtils::msSinceEpoch() / 250) % 60;
        packet.flightState = ownship.airborne ? ADSL_Packet::FlightState::FS_Airborne : ADSL_Packet::FlightState::FS_OnGround;
        packet.aircraftCategory = mapAircraftCategory(ownship.conspicuity.category);
        packet.emergencyStatus = ADSL_Packet::ES_NoEmergency;

        packet.setLatitude(ownship.lat);
        packet.setLongitude(ownship.lon);
        packet.setGroundSpeed(ownship.groundSpeed);
        packet.setHeightHAE(static_cast<int32_t>(ownship.ellipseHeight));
        packet.setVerticalRate(ownship.verticalSpeed);
        packet.setTrack(ownship.course);

        packet.designAssurance = ADSL_Packet::DesignAsurance::DA_None;
        packet.navigationIntegrity = ADSL_Packet::NavigationIntegrity::NI_LessThan25m;
        packet.setHorAccur((gpsStats.hDop * 2 + 5) / 10);
        packet.setVerAccur((gpsStats.pDop * 3 + 5) / 10);
        packet.reserved2 = 0;

        packet.Scramble();
        packet.setCRC();

        // Takes about 4ms from the request to end up here
        getBus().receive(GATAS::RadioTxFrameMsg{
            Radio::TxPacket{
                msg.radioParameters,
                ADSL_Packet::TotalTxBytes,
                packet.data()},
            msg.radioNo});
        statistics.transmittedAircraftPositions += 1;
    }
}

int8_t ADSL::parseFrame(const ADSL_Packet &packet, int16_t rssiDbm)
{
    uint32_t positionTs = CoreUtils::timeUs32();
    auto ownship = ownshipPosition.load(etl::memory_order_acquire);

    float fLatitude = packet.getLatitude();
    float fLongitude = packet.getLongitude();
    auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownship.lat, ownship.lon, fLatitude, fLongitude);

    if (fromOwn.distance > distanceIgnore)
    {
        statistics.outOfDistance += 1;
        return -1;
    }

    //    printf("ADSL: address:%06X latitude:%0.6f longitude:%0.6f altitude:%ld climbRate:%0.2f speed:%0.2f heading:%0.2f \n",
    //      packet.address, fLatitude, fLongitude, packet.getaltitudeGeoid(), packet.getVerticalRate(), packet.getGroundSpeed(), packet.getTrack());

    GATAS::AircraftPositionMsg aircraftPosition{
        GATAS::AircraftPositionInfo{
            positionTs,
            "",
            packet.address,
            addressMapToAddressType(packet.addressMapping),
            GATAS::DataSource::ADSL,
            mapAircraftCategory(packet.aircraftCategory),
            packet.addressMapping == 0x00,
            false,
            packet.flightState == ADSL_Packet::FlightState::FS_Airborne, // airBorn
            fLatitude,
            fLongitude,
            packet.getHeightHAE(),
            packet.getVerticalRate(),
            packet.getGroundSpeed(),
            static_cast<int16_t>(packet.getTrack()),
            0.0,
            fromOwn.distance,
            fromOwn.relNorth,
            fromOwn.relEast
        },
        rssiDbm};
    statistics.receivedAircraftPositions += 1;
    getBus().receive(aircraftPosition);
    return 0;
}
