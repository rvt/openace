#pragma once

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"

#include "pico/stdlib.h"

#include "etl/message_bus.h"
#include "etl/list.h"
#include "etl/forward_list.h"
#include "etl/queue_spsc_atomic.h"

#include "ace/constants.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"

#include "trackerdata.hpp"
#include "ace/antennaRadiationPattern.hpp"

/**
 * Client that can connect to a host and a port and expect to receive line terminated NMEA Messages
 * Part of this code taken from the example from Raspbery
 */
class AircraftTracker : public BaseModule, public etl::message_router<AircraftTracker, GATAS::ConfigUpdatedMsg, GATAS::AircraftPositionMsg, GATAS::AircraftPositionsMsg, GATAS::Every5SecMsg>
{
private:
    friend class message_router;

    static constexpr uint8_t MAX_TRACKING_PLANES = 32;
    static constexpr uint32_t CLEAR_UP_SIZE = (MAX_TRACKING_PLANES * 95) / 100;
    static constexpr uint32_t AUTO_DISTANCE_TRACK_UPPER = (MAX_TRACKING_PLANES * 90) / 100;
    static constexpr uint32_t AUTO_DISTANCE_TRACK_LOWER = (MAX_TRACKING_PLANES * 80) / 100;
    static constexpr uint8_t TIMESLICES = 10;
    struct
    {
        uint32_t queueFullErr = 0; // Might mean ther eis to much pressure on this system and the queue needs to be increased in size. But this will never work if trackedFullErr is also increasing
        uint32_t positionsProcessed = 0;
    } statistics;

private:
    struct TrackInfo
    {
        uint32_t nextSendTime;
        uint8_t numberOfTries;
        GATAS::AircraftPositionInfo position;
        bool operator<(const TrackInfo &other) const
        {
            return nextSendTime < other.nextSendTime;
        }
    };

    TaskHandle_t taskHandle;
    TrackerData<MAX_TRACKING_PLANES, TIMESLICES> trackedAircraft;
    GATAS::AircraftAddress ownshipAddress;

    // Producer Consumer queue to handle data between this task and the send task
    etl::queue_spsc_atomic<GATAS::AircraftPositionInfo, 16, etl::memory_model::MEMORY_MODEL_SMALL> queue;
    using ProtocolRadPattern = GATAS::AntennaRadiationPattern<GATAS_STATSCOLLECTOR_NUM_RADIALS>;
    etl::array<ProtocolRadPattern, static_cast<uint8_t>(GATAS::DataSource::_TRANSPROTOCOLS)> antennaRadiationPattern;

    enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        TIMER = 1 << 1,
        NEW = 1 << 2,
        MAINTAIN = 1 << 3
    };

    void on_receive_unknown(const etl::imessage &msg);
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);
    void on_receive(const GATAS::AircraftPositionMsg &msg);
    void on_receive(const GATAS::AircraftPositionsMsg &msg);
    void on_receive(const GATAS::Every5SecMsg &msg);
    static void aircraftTrackerTask(void *arg);
    void handleNew();
    void sendEligibleAircraft();
    void maintenance();

    void handleTrackedAircraft(const GATAS::AircraftPositionInfo &position)
    {
        getBus().receive(GATAS::TrackedAircraftPositionMsg(position));
    }

public:
    static constexpr const etl::string_view NAME = "AircraftTracker";
    AircraftTracker(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
                                                                           taskHandle(nullptr)
    {
        on_receive(GATAS::ConfigUpdatedMsg{config, Configuration::CONFIG});
    }

    virtual ~AircraftTracker() = default;

    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
};
