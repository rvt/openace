#include <stdio.h>

#include "pico/stdlib.h"

#include "fanet/fanet.hpp"
#include "fanet/packetParser.hpp"

#include "fanetace.hpp"

#include "ace/messagerouter.hpp"
#include "ace/coreutils.hpp"
#include "ace/semaphoreguard.hpp"
#include "ace/measure.hpp"

void FanetAce::start()
{
    xTaskCreate(FanetAceTask, FanetAce::NAME.cbegin(), configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY + 2, &taskHandle);
    getBus().subscribe(*this);
};

void FanetAce::stop()
{
    getBus().unsubscribe(*this);
    xTaskNotify(taskHandle, TaskState::EXIT, eSetBits);
};

GATAS::PostConstruct FanetAce::postConstruct()
{
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr)
    {
        return GATAS::PostConstruct::MUTEX_ERROR;
    }

    return GATAS::PostConstruct::OK;
}

void FanetAce::FanetAceTask(void *arg)
{
    (void)arg;
    FanetAce *fanetAce = static_cast<FanetAce *>(arg);
    uint32_t waitUntil = 100;
    while (true)
    {

        auto delay = (waitUntil - CoreUtils::timeMs32()) + 1;
        // printf("Delay %ld\n", delay);
        if (uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(delay)))
        {
            (void)notifyValue;

            if (notifyValue & TaskState::EXIT)
            {
                vTaskDelete(nullptr);
                return;
            }
        }
        if (auto guard = SemaphoreGuard<100>(fanetAce->mutex))
        {
            waitUntil = fanetAce->protocol.handleTx();
        }
    }
}

void FanetAce::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::CONFIG)
    {
        gaTasConfiguration = msg.config.gaTasConfig();
        protocol.ownAddress(FANET::Address{gaTasConfiguration.address});
    }
}

void FanetAce::on_receive(const GATAS::RadioTxPositionRequestMsg &msg)
{
    if (msg.radioParameters.config->dataSource == GATAS::DataSource::FANET)
    {
        auto ownship = ownshipPosition.load(etl::memory_order_acquire);

        FANET::TrackingPayload payload;
        payload.latitude(ownship.lat)
            .longitude(ownship.lon)
            .altitude(ownship.heightMsl())
            .speed(ownship.groundSpeed * MS_TO_KPH)
            .groundTrack(ownship.course)
            .climbRate(ownship.verticalSpeed)
            .tracking(!gaTasConfiguration.noTrack)
            .turnRate(ownship.hTurnRate)
            .aircraftType(mapAircraftCategory(gaTasConfiguration.category));

        auto packet = FANET::Packet<1>()
                          .payload(payload)
                          .forward(true);

        if (auto guard = SemaphoreGuard<10>(mutex))
        {
            radioParameters = msg.radioParameters;
            radioNo = msg.radioNo;
            protocol.sendPacket(packet, 0);
        }
        xTaskNotify(taskHandle, TaskState::HANDLETX, eSetBits);
    }
}

uint32_t FanetAce::fanet_getTick() const
{
    return CoreUtils::timeMs32();
}

bool FanetAce::fanet_sendFrame(uint8_t codingRate, etl::span<const uint8_t> data)
{
    (void)data;
    statistics.send++;
    radioParameters.codingRate = codingRate;
    getBus().receive(GATAS::RadioTxFrameMsg{
        Radio::TxPacket{radioParameters, data},
        radioNo});

    return true;
}

void FanetAce::fanet_ackReceived(uint16_t id)
{
    (void)id;
}

void FanetAce::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition.store(msg.position, etl::memory_order_release);
}

void FanetAce::on_receive(const GATAS::RadioRxLoraMsg &msg)
{
    (void)msg;
    statistics.received++;

    FANET::Header::MessageType messageType;
    if (auto guard = SemaphoreGuard<10>(mutex))
    {
        messageType = protocol.handleRx(msg.rssidBm, msg.frame);
    }
    else
    {
        return;
    }

    auto packet = FANET::PacketParser<12>::parse(msg.frame);
    if (packet.source() == protocol.ownAddress())
    {
        return;
    }
    xTaskNotify(taskHandle, TaskState::HANDLETX, eSetBits);
    auto ownship = ownshipPosition.load(etl::memory_order_acquire);

    switch (messageType)
    {
    case FANET::Header::MessageType::TRACKING:
    {
        FANET::TrackingPayload tp = etl::get<FANET::TrackingPayload>(packet.payload().value());
        auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownship.lat, ownship.lon, tp.latitude(), tp.longitude());

        if (fromOwn.distance > distanceIgnore)
        {
            statistics.outOfDistance++;
            return;
        }

        GATAS::AircraftPositionMsg aircraftPosition{
            GATAS::AircraftPositionInfo{
                CoreUtils::timeUs32(),
                "",
                packet.source().asUint(),
                GATAS::AddressType::FANET,
                GATAS::DataSource::FANET,
                mapAircraftCategory(tp.aircraftType()),
                false,
                !tp.tracking(), // FANET uses 'tracking' to indicate it want's to be tracked
                tp.speed() > (GATAS::GROUNDSPEED_CONSIDERING_AIRBORN * MS_TO_KPH),
                tp.latitude(),
                tp.longitude(),
                tp.altitude() + CoreUtils::egmGeoidOffset(tp.latitude(), tp.longitude()),
                tp.climbRate(),
                tp.speed() * KPH_TO_MS,
                static_cast<int16_t>(tp.groundTrack()),
                tp.turnRate(),
                fromOwn.distance,
                fromOwn.relNorth,
                fromOwn.relEast,
                fromOwn.bearing},
            msg.rssidBm};
        getBus().receive(aircraftPosition);
    }
    break;
    case FANET::Header::MessageType::GROUND_TRACKING:
    {
        FANET::GroundTrackingPayload tp = etl::get<FANET::GroundTrackingPayload>(packet.payload().value());
        auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownship.lat, ownship.lon, tp.latitude(), tp.longitude());

        if (fromOwn.distance > distanceIgnore)
        {
            statistics.outOfDistance++;
            return;
        }

        GATAS::AircraftPositionMsg aircraftPosition{
            GATAS::AircraftPositionInfo{
                CoreUtils::timeUs32(),
                "",
                packet.source().asUint(),
                GATAS::AddressType::FANET,
                GATAS::DataSource::FANET,
                GATAS::AircraftCategory::StaticObstacle,
                false,
                !tp.tracking(), // FANET uses 'tracking' to indicate it want's to be tracked
                false,
                tp.latitude(),
                tp.longitude(),
                ownship.heightMsl(),
                0,
                0,
                0,
                0,
                fromOwn.distance,
                fromOwn.relNorth,
                fromOwn.relEast,
                fromOwn.bearing},
            msg.rssidBm};
        getBus().receive(aircraftPosition);
    }
    break;
    case FANET::Header::MessageType::ACK:
    case FANET::Header::MessageType::NAME:
    case FANET::Header::MessageType::MESSAGE:
    case FANET::Header::MessageType::SERVICE:
    case FANET::Header::MessageType::LANDMARKS:
    case FANET::Header::MessageType::REMOTE_CONFIG:
    {
    }
    break;
    }
}

void FanetAce::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"received\":" << statistics.received;
    stream << ",\"send\":" << statistics.send;
    stream << ",\"outOfDistance\":" << statistics.outOfDistance;
    stream << ",\"neighbors\":" << protocol.neighborTable().size();
    stream << ",\"Pool Size\":" << protocol.pool().getAllocatedBlocks().size();
    stream << ",\"Airtime\":" << protocol.airTime();
    stream << "}\n";
}

GATAS::AircraftCategory FanetAce::mapAircraftCategory(FANET::TrackingPayload::AircraftType category) const
{
    // map ADSL_Packet::AircraftCategory to GATAS::AircraftCategory
    switch (category)
    {
    case FANET::TrackingPayload::AircraftType::PARAGLIDER:
        return GATAS::AircraftCategory::Paraglider;
    case FANET::TrackingPayload::AircraftType::HANGLIDER:
        return GATAS::AircraftCategory::HangGlider;
    case FANET::TrackingPayload::AircraftType::BALLOON:
        return GATAS::AircraftCategory::Balloon;
    case FANET::TrackingPayload::AircraftType::GLIDER:
        return GATAS::AircraftCategory::GliderMotorGlider;
    case FANET::TrackingPayload::AircraftType::POWERED_AIRCRAFT:
        return GATAS::AircraftCategory::ReciprocatingEngine;
    case FANET::TrackingPayload::AircraftType::HELICOPTER:
        return GATAS::AircraftCategory::Helicopter;
    case FANET::TrackingPayload::AircraftType::UAV:
        return GATAS::AircraftCategory::Uav;
    default:
        return GATAS::AircraftCategory::Unknown;
    }
}

FANET::TrackingPayload::AircraftType FanetAce::mapAircraftCategory(GATAS::AircraftCategory category) const
{
    switch (category)
    {
    case GATAS::AircraftCategory::Paraglider:
        return FANET::TrackingPayload::AircraftType::PARAGLIDER;
    case GATAS::AircraftCategory::HangGlider:
        return FANET::TrackingPayload::AircraftType::HANGLIDER;
    case GATAS::AircraftCategory::Balloon:
        return FANET::TrackingPayload::AircraftType::BALLOON;
    case GATAS::AircraftCategory::GliderMotorGlider:
        return FANET::TrackingPayload::AircraftType::GLIDER;
    case GATAS::AircraftCategory::TowPlane:
    case GATAS::AircraftCategory::DropPlane:
    case GATAS::AircraftCategory::ReciprocatingEngine:
    case GATAS::AircraftCategory::JetTurbopropEngine:
        return FANET::TrackingPayload::AircraftType::POWERED_AIRCRAFT;
    case GATAS::AircraftCategory::Helicopter:
        return FANET::TrackingPayload::AircraftType::HELICOPTER;
    case GATAS::AircraftCategory::Uav:
        return FANET::TrackingPayload::AircraftType::UAV;
    default:
        return FANET::TrackingPayload::AircraftType::OTHER;
    }
}
