#include <stdio.h>

#include "pico/stdlib.h"

// #include "../fanet/fanet_common.hpp"
// #include "../fanet/fanet_packet.hpp"
#include "fanet/fanet.hpp"
#include "fanet/packetParser.hpp"

#include "fanetace.hpp"

#include "ace/messagerouter.hpp"
#include "ace/coreutils.hpp"
#include "ace/semaphoreguard.hpp"

void FanetAce::start()
{
    xTaskCreate(FanetAceTask, "FanetAceTask", configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY + 2, &taskHandle);
    getBus().subscribe(*this);
};

void FanetAce::stop()
{
    getBus().unsubscribe(*this);
    xTaskNotify(taskHandle, TaskState::EXIT, eSetBits);
};

OpenAce::PostConstruct FanetAce::postConstruct()
{
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr)
    {
        return OpenAce::PostConstruct::MUTEX_ERROR;
    }

    return OpenAce::PostConstruct::OK;
}

void FanetAce::FanetAceTask(void *arg)
{
    (void)arg;
    FanetAce *fanetAce = static_cast<FanetAce *>(arg);
    uint32_t waitUntil = 100;
    while (true)
    {

        auto delay = (waitUntil - CoreUtils::timeMs32()) + 1;
        //printf("Delay %ld\n", delay);
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

void FanetAce::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == "config")
    {
        openAceConfiguration = msg.config.openAceConfig();
        protocol.ownAddress(FANET::Address{openAceConfiguration.address});
    }
}

void FanetAce::on_receive(const OpenAce::RadioTxPositionRequestMsg &msg)
{
    if (msg.radioParameters.config.dataSource == OpenAce::DataSource::FANET)
    {
        FANET::TrackingPayload payload;
        payload.latitude(ownshipPosition.lat)
            .longitude(ownshipPosition.lon)
            .altitude(ownshipPosition.heightEgm96)
            .speed(ownshipPosition.groundSpeed * MS_TO_KPH)
            .groundTrack(ownshipPosition.course)
            .climbRate(ownshipPosition.verticalSpeed)
            .tracking(true)
            .turnRate(ownshipPosition.hTurnRate)
            .aircraftType(mapAircraftCategory(openAceConfiguration.category));

        auto packet = FANET::Packet<1>()
                          .payload(payload)
                          .forward(true);

        if (auto guard = SemaphoreGuard<1>(mutex))
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
    radioParameters.config.codingRate = codingRate;
    // getBus().receive(OpenAce::RadioTxFrameMsg{
    //     Radio::TxPacket{radioParameters, data},
    //     radioNo});

    return true;
}

void FanetAce::fanet_ackReceived(uint16_t id)
{
    (void)id;
}

void FanetAce::on_receive(const OpenAce::OwnshipPositionMsg &msg)
{
    if (auto guard = SemaphoreGuard<1>(mutex))
    {
        ownshipPosition = msg.position;
    }
}

void FanetAce::on_receive(const OpenAce::RadioRxLoraMsg &msg)
{
    (void)msg;
    statistics.received++;

    FANET::Header::MessageType messageType;
    if (auto guard = SemaphoreGuard<1>(mutex))
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

    switch (messageType)
    {
    case FANET::Header::MessageType::TRACKING:
    {
        FANET::TrackingPayload tp = etl::get<FANET::TrackingPayload>(packet.payload().value());
        auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownshipPosition.lat, ownshipPosition.lon, tp.latitude(), tp.longitude());

        if (fromOwn.distance > 50000)
        {
            statistics.outOfDistance++;
            return;
        }

        OpenAce::AircraftPositionMsg aircraftPosition{
            OpenAce::AircraftPositionInfo{
                CoreUtils::timeUs32(),
                "",
                packet.source().asUint(),
                OpenAce::AddressType::FANET,
                OpenAce::DataSource::FANET,
                mapAircraftCategory(tp.aircraftType()),
                false,
                !tp.tracking(), // FANET uses 'tracking' to indicate it want's to be tracked
                tp.speed() > (OpenAce::GROUNDSPEED_CONSIDERING_AIRBORN * MS_TO_KPH),
                tp.latitude(),
                tp.longitude(),
                tp.altitude(),
                tp.climbRate(),
                tp.speed() * KPH_TO_MS,
                static_cast<int16_t>(tp.groundTrack()),
                tp.turnRate(),
                static_cast<uint16_t>(fromOwn.distance),
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
        auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownshipPosition.lat, ownshipPosition.lon, tp.latitude(), tp.longitude());

        if (fromOwn.distance > 50000)
        {
            statistics.outOfDistance++;
            return;
        }

        OpenAce::AircraftPositionMsg aircraftPosition{
            OpenAce::AircraftPositionInfo{
                CoreUtils::timeUs32(),
                "",
                packet.source().asUint(),
                OpenAce::AddressType::FANET,
                OpenAce::DataSource::FANET,
                OpenAce::AircraftCategory::StaticObstacle,
                false,
                !tp.tracking(), // FANET uses 'tracking' to indicate it want's to be tracked
                false,
                tp.latitude(),
                tp.longitude(),
                ownshipPosition.heightEgm96,
                0,
                0,
                0,
                0,
                static_cast<uint16_t>(fromOwn.distance),
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

OpenAce::AircraftCategory FanetAce::mapAircraftCategory(FANET::TrackingPayload::AircraftType category) const
{
    // map ADSL_Packet::AircraftCategory to OpenAce::AircraftCategory
    switch (category)
    {
    case FANET::TrackingPayload::AircraftType::PARAGLIDER:
        return OpenAce::AircraftCategory::Paraglider;
    case FANET::TrackingPayload::AircraftType::HANGLIDER:
        return OpenAce::AircraftCategory::HangGlider;
    case FANET::TrackingPayload::AircraftType::BALLOON:
        return OpenAce::AircraftCategory::Balloon;
    case FANET::TrackingPayload::AircraftType::GLIDER:
        return OpenAce::AircraftCategory::GliderMotorGlider;
    case FANET::TrackingPayload::AircraftType::POWERED_AIRCRAFT:
        return OpenAce::AircraftCategory::ReciprocatingEngine;
    case FANET::TrackingPayload::AircraftType::HELICOPTER:
        return OpenAce::AircraftCategory::Helicopter;
    case FANET::TrackingPayload::AircraftType::UAV:
        return OpenAce::AircraftCategory::Uav;
    default:
        return OpenAce::AircraftCategory::Unknown;
    }
}

FANET::TrackingPayload::AircraftType FanetAce::mapAircraftCategory(OpenAce::AircraftCategory category) const
{
    switch (category)
    {
    case OpenAce::AircraftCategory::Paraglider:
        return FANET::TrackingPayload::AircraftType::PARAGLIDER;
    case OpenAce::AircraftCategory::HangGlider:
        return FANET::TrackingPayload::AircraftType::HANGLIDER;
    case OpenAce::AircraftCategory::Balloon:
        return FANET::TrackingPayload::AircraftType::BALLOON;
    case OpenAce::AircraftCategory::GliderMotorGlider:
        return FANET::TrackingPayload::AircraftType::GLIDER;
    case OpenAce::AircraftCategory::TowPlane:
    case OpenAce::AircraftCategory::DropPlane:
    case OpenAce::AircraftCategory::ReciprocatingEngine:
    case OpenAce::AircraftCategory::JetTurbopropEngine:
        return FANET::TrackingPayload::AircraftType::POWERED_AIRCRAFT;
    case OpenAce::AircraftCategory::Helicopter:
        return FANET::TrackingPayload::AircraftType::HELICOPTER;
    case OpenAce::AircraftCategory::Uav:
        return FANET::TrackingPayload::AircraftType::UAV;
    default:
        return FANET::TrackingPayload::AircraftType::OTHER;
    }
}
