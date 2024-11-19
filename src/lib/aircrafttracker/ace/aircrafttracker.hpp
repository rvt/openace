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

/**
 * Client that can connect to a host and a port and expect to receive line terminated NMEA Messages
 * Part of this code taken from the example from Raspbery
 */
class AircraftTracker : public BaseModule, public etl::message_router<AircraftTracker, OpenAce::ConfigUpdatedMsg, OpenAce::OwnshipPositionMsg, OpenAce::AircraftPositionMsg>
{
private:
    friend class message_router;

    static constexpr uint8_t MAX_TRACKING_PLANES = 32;
    static constexpr uint8_t MAX_POSITION_INTERPOLATIONS = 10; // Maximum number of times we interpolate position of a plate when it was not received, after that we removed it from the tracker
    static constexpr uint16_t TASK_STACK_SIZE = configMINIMAL_STACK_SIZE + 256;

    static constexpr uint32_t CLEAR_UP_SIZE = (MAX_TRACKING_PLANES * 95) / 100;

    static constexpr uint32_t AUTO_DISTANCE_TRACK_UPPER = (MAX_TRACKING_PLANES * 90) / 100;
    static constexpr uint32_t AUTO_DISTANCE_TRACK_LOWER = (MAX_TRACKING_PLANES * 80) / 100;
    static constexpr float MAX_TRACKING_DISTANCE = 75000; // In meters
    static constexpr float MIN_TRACKING_DISTANCE = 10000; // In meters


    struct
    {
        uint32_t queueFullErr = 0;
        uint32_t trackedFullErr = 0;
        uint32_t positionsProcessed = 0;
        uint8_t numberOfPlanesTracking = 0;
        uint8_t lastTrackPoint = 0; // Need a better name for this.
    } statistics;

private:
    struct TrackInfo
    {
        uint32_t nextSendTime;
        uint8_t numberOfTries;
        OpenAce::AircraftPositionInfo position;
        bool operator<(const TrackInfo &other) const
        {
            return nextSendTime < other.nextSendTime;
        }
    };

    TaskHandle_t taskHandle;
    TimerHandle_t timerHandle;
    TimerHandle_t maintenanceTimerHandle;

    SemaphoreHandle_t aircraftMutex;
    SemaphoreHandle_t ownshipMutex;

    using TrackedAircrafts = etl::list<TrackInfo, MAX_TRACKING_PLANES>;
    TrackedAircrafts trackedAircraft;

    // Producer Consumer queue to handle data between this task and the send task
    etl::queue_spsc_atomic<OpenAce::AircraftPositionInfo, 2> queue;

    OpenAce::OwnshipPositionInfo ownshipPosition;

    float autoDistanceTrack;

    enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        TIMER = 1 << 1,
        NEW = 1 << 2,
        MAINTENANCE = 1 << 3
    };

    struct FindNewlyArrived
    {
        bool operator()(const TrackInfo &i)
        {
            return i.nextSendTime == 0;
        }
    };

    struct StaleAircraft
    {
        uint64_t msSinceEpoch;
        StaleAircraft(uint64_t msSinceEpoch_) : msSinceEpoch(msSinceEpoch_) {}
        bool operator()(const TrackInfo &i)
        {
            return i.position.timestamp < msSinceEpoch;
        }
    };

    void on_receive_unknown(const etl::imessage &msg);

    void on_receive(const OpenAce::ConfigUpdatedMsg &msg);
    void on_receive(const OpenAce::AircraftPositionMsg &msg);
    void on_receive(const OpenAce::OwnshipPositionMsg &msg);
    static void aircraftTrackerTask(void *arg);
    static void aircraftTimerTask(TimerHandle_t timer);
    static void maintenanceTimerTask(TimerHandle_t timer);
    void handleNew();
    void handleTimer();
    void removeStaleEntries();
    void distanceEstimator();

public:
    static constexpr const etl::string_view NAME = "AircraftTracker";
    AircraftTracker(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
        taskHandle(nullptr), timerHandle(nullptr), aircraftMutex(nullptr), ownshipMutex(nullptr), autoDistanceTrack(MAX_TRACKING_DISTANCE)
    {
        (void)config;
    }

    virtual ~AircraftTracker() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
};
