#include <stdio.h>

#include "adsl.hpp"
#include "ace/bitcount.hpp"

constexpr float POSITION_DECODE = 0.0001f / 60.f;

GATAS::PostConstruct ADSL::postConstruct()
{
    //    BaseModule::moduleByName(*this, Tuner::NAME);

    frameConsumerQueue = xQueueCreate(4, sizeof(GATAS::RadioRxGfskMsg));
    if (frameConsumerQueue == nullptr)
    {
        return GATAS::PostConstruct::XQUEUE_ERROR;
    }

    return GATAS::PostConstruct::OK;
}

void ADSL::start()
{
    xTaskCreate(adslReceiveTask, ADSL::NAME.cbegin(), configMINIMAL_STACK_SIZE + 128, this, tskIDLE_PRIORITY + 2, &taskHandle);
    // auto tuner = static_cast<Tuner*>(BaseModule::moduleByName(*this, Tuner::NAME));
    // tuner->startListen(GATAS::DataSource::ADSL);
    getBus().subscribe(*this);
};

void ADSL::stop()
{
    getBus().unsubscribe(*this);
    // auto tuner = static_cast<Tuner*>(BaseModule::moduleByName(*this, Tuner::NAME));
    // tuner->stopListen(GATAS::DataSource::ADSL);

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
void ADSL::on_receive(const GATAS::RadioRxGfskMsg &msg)
{
    if (msg.dataSource == GATAS::DataSource::ADSL)
    {
        const GATAS::RadioRxGfskMsg cpy = msg;
        if (xQueueSendToBack(frameConsumerQueue, &cpy, TASK_DELAY_MS(5)) != pdPASS)
        {
            statistics.queueFullErr++;
        }
    }
}

void ADSL::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition = msg.position;
}

void ADSL::on_receive(const GATAS::GpsStatsMsg &msg)
{
    gpsStats = msg;
}

void ADSL::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::CONFIG)
    {
        gaTasConfiguration = msg.config.gaTasConfig();
    }
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
        return GATAS::AircraftCategory::Unknown;
    case ADSL_Packet::AircraftCategory::AC_LightFixedWing:
        return GATAS::AircraftCategory::ReciprocatingEngine;
    case ADSL_Packet::AircraftCategory::AC_SmallToHeavyFixedWing:
        return GATAS::AircraftCategory::JetTurbopropEngine;
    case ADSL_Packet::AircraftCategory::AC_Rotorcraft:
        return GATAS::AircraftCategory::Helicopter;
    case ADSL_Packet::AircraftCategory::AC_GliderSailplane:
        return GATAS::AircraftCategory::GliderMotorGlider;
    case ADSL_Packet::AircraftCategory::AC_LighterThanAir:
        return GATAS::AircraftCategory::Balloon;
    case ADSL_Packet::AircraftCategory::AC_Ultralight:
        return GATAS::AircraftCategory::ReciprocatingEngine;
    case ADSL_Packet::AircraftCategory::AC_HangGliderParaglider:
        return GATAS::AircraftCategory::Paraglider;
    case ADSL_Packet::AircraftCategory::AC_ParachutistSkydiverWingsuit:
        return GATAS::AircraftCategory::Skydiver;
    case ADSL_Packet::AircraftCategory::AC_Gyrocopter:
        return GATAS::AircraftCategory::Helicopter;
    case ADSL_Packet::AircraftCategory::AC_UASOpenCategory:
        return GATAS::AircraftCategory::Uav;
    case ADSL_Packet::AircraftCategory::AC_AircraftCategoryReserved:
        return GATAS::AircraftCategory::ReservedE;
    default:
        return GATAS::AircraftCategory::Unknown;
    }
}

ADSL_Packet::AircraftCategory ADSL::mapAircraftCategory(GATAS::AircraftCategory category)
{
    switch (category)
    {
    case GATAS::AircraftCategory::Unknown:
        return ADSL_Packet::AircraftCategory::AC_NoEmitterCategory;
    case GATAS::AircraftCategory::ReciprocatingEngine:
    case GATAS::AircraftCategory::TowPlane:
        return ADSL_Packet::AircraftCategory::AC_LightFixedWing;
    case GATAS::AircraftCategory::JetTurbopropEngine:
    case GATAS::AircraftCategory::DropPlane:
        return ADSL_Packet::AircraftCategory::AC_SmallToHeavyFixedWing;
    case GATAS::AircraftCategory::Helicopter:
        return ADSL_Packet::AircraftCategory::AC_Rotorcraft;
    case GATAS::AircraftCategory::GliderMotorGlider:
        return ADSL_Packet::AircraftCategory::AC_GliderSailplane;
    case GATAS::AircraftCategory::Balloon:
    case GATAS::AircraftCategory::Airship:
        return ADSL_Packet::AircraftCategory::AC_LighterThanAir;
    case GATAS::AircraftCategory::Paraglider:
    case GATAS::AircraftCategory::HangGlider:
        return ADSL_Packet::AircraftCategory::AC_HangGliderParaglider;
    case GATAS::AircraftCategory::Skydiver:
        return ADSL_Packet::AircraftCategory::AC_ParachutistSkydiverWingsuit;
    case GATAS::AircraftCategory::Uav:
        return ADSL_Packet::AircraftCategory::AC_UASOpenCategory;
    case GATAS::AircraftCategory::ReservedE:
        return ADSL_Packet::AircraftCategory::AC_AircraftCategoryReserved;
    case GATAS::AircraftCategory::StaticObstacle:
    default:
        return ADSL_Packet::AircraftCategory::AC_NoEmitterCategory;
    }
}


void ADSL::on_receive(const GATAS::RadioTxPositionRequestMsg &msg)
{

    if (msg.radioParameters.config.dataSource == GATAS::DataSource::ADSL)
    {
        ADSL_Packet packet;
        packet.payloadIdent = 0x02; // ADS-L.4.SRD860.F.2.1 :: iConspicuity
        packet.addressMapping = addressTypeToAddressMap(gaTasConfiguration.addressType);
        packet.address = gaTasConfiguration.address;
        packet.reserved1 = 0;
        packet.relay = 0;
        packet.timeStamp = (CoreUtils::msSinceEpoch() / 250) % 60;
        packet.flightState = ownshipPosition.airborne ? ADSL_Packet::FlightState::FS_Airborne : ADSL_Packet::FlightState::FS_OnGround;
        packet.aircraftCategory = mapAircraftCategory(gaTasConfiguration.category);
        packet.emergencyStatus = ADSL_Packet::ES_NoEmergency;

        packet.setLatitude(ownshipPosition.lat);
        packet.setLongitude(ownshipPosition.lon);
        packet.setGroundSpeed(ownshipPosition.groundSpeed);
        packet.setHeightHAE(static_cast<int32_t>(ownshipPosition.altitudeHAE));
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
        getBus().receive(GATAS::RadioTxFrameMsg{
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
    uint32_t positionTs = CoreUtils::timeUs32();

    float fLatitude = packet.getLatitude();
    float fLongitude = packet.getLongitude();
    auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownshipPosition.lat, ownshipPosition.lon, fLatitude, fLongitude);

    if (fromOwn.distance > distanceIgnore)
    {
        statistics.outOfDistance++;
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
    GATAS::RadioRxGfskMsg msg;
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
            if (packet.address == adsl->gaTasConfiguration.address) {
                continue;
            }

            adsl->addReceiveStat(msg.frequency);
            adsl->parseFrame(packet, msg.rssidBm);
        }
    }
}
