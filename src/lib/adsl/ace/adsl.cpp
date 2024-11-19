#include <stdio.h>

#include "adsl.hpp"
#include "ace/bitcount.hpp"

constexpr float POSITION_DECODE = 0.0001f / 60.f;
constexpr float POSITION_ENDECODE = 1.f / POSITION_DECODE;

OpenAce::PostConstruct ADSL::postConstruct()
{
    //    BaseModule::moduleByName(*this, Tuner::NAME);

    frameConsumerQueue = xQueueCreate(4, sizeof(OpenAce::RadioRxFrame));
    if (frameConsumerQueue == nullptr)
    {
        return OpenAce::PostConstruct::XQUEUE_ERROR;
    }

    return OpenAce::PostConstruct::OK;
}

void ADSL::start()
{
    xTaskCreate(adslReceiveTask, "adslReceiveTask", configMINIMAL_STACK_SIZE + 1024, this, tskIDLE_PRIORITY + 2, &taskHandle);
    // auto tuner = static_cast<Tuner*>(BaseModule::moduleByName(*this, Tuner::NAME));
    // tuner->startListen(OpenAce::DataSource::ADSL);
    getBus().subscribe(*this);
};

void ADSL::stop()
{
    getBus().unsubscribe(*this);
    // auto tuner = static_cast<Tuner*>(BaseModule::moduleByName(*this, Tuner::NAME));
    // tuner->stopListen(OpenAce::DataSource::ADSL);

    vTaskDelete(taskHandle);
    vQueueDelete(frameConsumerQueue);
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
    stream << ",\"queueFullErr\":" << statistics.queueFullErr;
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
void ADSL::on_receive(const OpenAce::RadioRxFrame &msg)
{
    if (msg.dataSource == OpenAce::DataSource::ADSL)
    {
        const OpenAce::RadioRxFrame cpy = msg;
        if (xQueueSendToBack(frameConsumerQueue, &cpy, TASK_DELAY_MS(5)) != pdPASS)
        {
            statistics.queueFullErr++;
        }
    }
}

void ADSL::on_receive(const OpenAce::OwnshipPositionMsg &msg)
{
    ownshipPosition = msg.position;
}

void ADSL::on_receive(const OpenAce::GpsStatsMsg &msg)
{
    gpsStats = msg;
}

void ADSL::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == "config")
    {
        openAceConfiguration = msg.config.openAceConfig();
    }
}

OpenAce::AddressType ADSL::addressMapToAddressType(uint8_t addressMap) const
{
    switch (addressMap)
    {
    case 0x00:
        return OpenAce::AddressType::RANDOM;
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
        return OpenAce::AddressType::RESERVED;
    case 0x05:
        return OpenAce::AddressType::ICAO;
    case 0x06:
        return OpenAce::AddressType::FLARM;
    case 0x07:
        return OpenAce::AddressType::OGN;
    case 0x08:
        return OpenAce::AddressType::FANET;
    // Not a bug, if we don't the the type we just say random
    default:
        return OpenAce::AddressType::UNKNOWN;
    }
}

uint8_t ADSL::addressTypeToAddressMap(OpenAce::AddressType addressType)
{
    switch (addressType)
    {
    case OpenAce::AddressType::RANDOM:
        return 0x00;
    case OpenAce::AddressType::RESERVED:
        return 0x01;
    case OpenAce::AddressType::ICAO:
        return 0x05;
    case OpenAce::AddressType::FLARM:
        return 0x06;
    case OpenAce::AddressType::OGN:
        return 0x07;
    case OpenAce::AddressType::FANET:
        return 0x08;
    default:
        return 0x00;
    }
}

OpenAce::AircraftCategory ADSL::mapAircraftCategory(ADSL_Packet::AircraftCategory category)
{
    // map ADSL_Packet::AircraftCategory to OpenAce::AircraftCategory
    switch (category)
    {
    case ADSL_Packet::AircraftCategory::AC_NoEmitterCategory:
        return OpenAce::AircraftCategory::Unknown;
    case ADSL_Packet::AircraftCategory::AC_LightFixedWing:
        return OpenAce::AircraftCategory::ReciprocatingEngine;
    case ADSL_Packet::AircraftCategory::AC_SmallToHeavyFixedWing:
        return OpenAce::AircraftCategory::JetTurbopropEngine;
    case ADSL_Packet::AircraftCategory::AC_Rotorcraft:
        return OpenAce::AircraftCategory::Helicopter;
    case ADSL_Packet::AircraftCategory::AC_GliderSailplane:
        return OpenAce::AircraftCategory::GliderMotorGlider;
    case ADSL_Packet::AircraftCategory::AC_LighterThanAir:
        return OpenAce::AircraftCategory::Balloon;
    case ADSL_Packet::AircraftCategory::AC_Ultralight:
        return OpenAce::AircraftCategory::ReciprocatingEngine;
    case ADSL_Packet::AircraftCategory::AC_HangGliderParaglider:
        return OpenAce::AircraftCategory::Paraglider;
    case ADSL_Packet::AircraftCategory::AC_ParachutistSkydiverWingsuit:
        return OpenAce::AircraftCategory::Skydiver;
    case ADSL_Packet::AircraftCategory::AC_Gyrocopter:
        return OpenAce::AircraftCategory::Helicopter;
    case ADSL_Packet::AircraftCategory::AC_UASOpenCategory:
        return OpenAce::AircraftCategory::Uav;
    case ADSL_Packet::AircraftCategory::AC_AircraftCategoryReserved:
        return OpenAce::AircraftCategory::ReservedE;
    default:
        return OpenAce::AircraftCategory::Unknown;
    }
}

ADSL_Packet::AircraftCategory ADSL::mapAircraftCategory(OpenAce::AircraftCategory category)
{
    switch (category)
    {
    case OpenAce::AircraftCategory::Unknown:
        return ADSL_Packet::AircraftCategory::AC_NoEmitterCategory;
    case OpenAce::AircraftCategory::ReciprocatingEngine:
    case OpenAce::AircraftCategory::TowPlane:
        return ADSL_Packet::AircraftCategory::AC_LightFixedWing;
    case OpenAce::AircraftCategory::JetTurbopropEngine:
    case OpenAce::AircraftCategory::DropPlane:
        return ADSL_Packet::AircraftCategory::AC_SmallToHeavyFixedWing;
    case OpenAce::AircraftCategory::Helicopter:
        return ADSL_Packet::AircraftCategory::AC_Rotorcraft;
    case OpenAce::AircraftCategory::GliderMotorGlider:
        return ADSL_Packet::AircraftCategory::AC_GliderSailplane;
    case OpenAce::AircraftCategory::Balloon:
    case OpenAce::AircraftCategory::Airship:
        return ADSL_Packet::AircraftCategory::AC_LighterThanAir;
    case OpenAce::AircraftCategory::Paraglider:
    case OpenAce::AircraftCategory::HangGlider:
        return ADSL_Packet::AircraftCategory::AC_HangGliderParaglider;
    case OpenAce::AircraftCategory::Skydiver:
        return ADSL_Packet::AircraftCategory::AC_ParachutistSkydiverWingsuit;
    case OpenAce::AircraftCategory::Uav:
        return ADSL_Packet::AircraftCategory::AC_UASOpenCategory;
    case OpenAce::AircraftCategory::ReservedE:
        return ADSL_Packet::AircraftCategory::AC_AircraftCategoryReserved;
    case OpenAce::AircraftCategory::StaticObstacle:
    default:
        return ADSL_Packet::AircraftCategory::AC_NoEmitterCategory;
    }
}

void ADSL::on_receive(const OpenAce::RadioTxPositionRequest &msg)
{

    if (msg.radioParameters.config.dataSource == OpenAce::DataSource::ADSL)
    {
        ADSL_Packet packet;
        packet.payloadIdent = 0x02; // ADS-L.4.SRD860.F.2.1 :: iConspicuity
        packet.addressMapping = addressTypeToAddressMap(openAceConfiguration.addressType);
        packet.address = openAceConfiguration.address;
        packet.reserved1 = 0;
        packet.relay = 0;
        packet.timeStamp = (CoreUtils::msSinceEpoch() / 250) % 60;
        packet.flightState = ownshipPosition.airborne ? ADSL_Packet::FlightState::FS_Airborne : ADSL_Packet::FlightState::FS_OnGround;
        packet.aircraftCategory = mapAircraftCategory(openAceConfiguration.category);
        packet.emergencyStatus = ADSL_Packet::ES_NoEmergency;

        packet.setLatitude(ownshipPosition.lat);
        packet.setLongitude(ownshipPosition.lon);
        packet.setGroundSpeed(ownshipPosition.groundSpeed);
        packet.setAltitudeWGS84(static_cast<int32_t>(ownshipPosition.altitudeWgs84));
        packet.setVerticalRate(ownshipPosition.verticalSpeed);
        packet.setTrack(ownshipPosition.course);

        packet.designAssurance = ADSL_Packet::DesignAsurance::DA_None;
        packet.navigationIntegrity = ADSL_Packet::NavigationIntegrity::NI_LessThan25m;
        packet.setHorAccur((gpsStats.hDop * 2 + 5) / 10);
        packet.setVerAccur((gpsStats.pDop * 3 + 5) / 10);
        packet.reserved2 = 0;

        packet.Scramble();
        packet.setCRC();

        // Takes about 4ms from the request to end up here
        getBus().receive(OpenAce::RadioTxFrame{
            Radio::TxPacket{
                msg.radioParameters,
                ADSL_Packet::TotalTxBytes,
                packet.data()},
            msg.radioNo});
        statistics.transmittedAircraftPositions++;
    }
}

int8_t ADSL::parseFrame(const ADSL_Packet &packet, int16_t rssiDbm)
{
    OpenAce::positionTs positionTs = CoreUtils::getPositionTs();

    float fLatitude = packet.getLatitude();
    float fLongitude = packet.getLongitude();
    auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownshipPosition.lat, ownshipPosition.lon, fLatitude, fLongitude);

    if (fromOwn.distance > distanceIgnore)
    {
        statistics.outOfDistance++;
        return -1;
    }

//    printf("ADSL: address:%06X latitude:%0.6f longitude:%0.6f altitude:%ld climbRate:%0.2f speed:%0.2f heading:%0.2f \n",
//      packet.address, fLatitude, fLongitude, packet.getAltitudeWGS84(), packet.getVerticalRate(), packet.getGroundSpeed(), packet.getTrack());

    OpenAce::AircraftPositionMsg aircraftPosition{
        OpenAce::AircraftPositionInfo{
            positionTs,
            "",
            packet.address,
            addressMapToAddressType(packet.addressMapping),
            OpenAce::DataSource::ADSL,
            static_cast<OpenAce::AircraftCategory>(packet.aircraftCategory),
            packet.addressMapping == 0x00,
            false,
            packet.flightState == ADSL_Packet::FlightState::FS_Airborne, // airBorn
            fLatitude,
            fLongitude,
            packet.getAltitudeWGS84(), // relative to WGS84 ellipsoid
            packet.getVerticalRate(),
            packet.getGroundSpeed(),
            static_cast<int16_t>(packet.getTrack()),
            0.0,
            static_cast<uint16_t>(fromOwn.distance),
            fromOwn.relNorth,
            fromOwn.relEast,
            fromOwn.bearing},
        rssiDbm};
    statistics.receivedAircraftPositions++;
    getBus().receive(aircraftPosition);
    return 0;
}

void ADSL::adslReceiveTask(void *arg)
{
    ADSL *adsl = static_cast<ADSL *>(arg);
    ADSL_Packet packet;
    OpenAce::RadioRxFrame msg;
    while (true)
    {
        // msg length expected to be 0x1b == 25byte
        if (xQueueReceive(adsl->frameConsumerQueue, &msg, portMAX_DELAY) == pdPASS)
        {
            auto check = ADSL_Packet::Correct((uint8_t *)msg.frame, (uint8_t *)msg.err);
            if (check == -1)
            {
                adsl->statistics.fecErr++;
                continue;
            }
            memcpy(packet.data(), msg.frame, ADSL_Packet::TotalTxBytes);
            packet.Descramble();

            if (packet.key != 0)
            {
                adsl->statistics.encrypted++;
                continue;
            }

            // Ignore ownship address
            if (packet.address == adsl->openAceConfiguration.address) {
                continue;
            }

            adsl->addReceiveStat(msg.frequency);
            adsl->parseFrame(packet, msg.rssidBm);
        }
    }
}
