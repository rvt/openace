#include "aircrafttracker.hpp"
#include "ace/coreutils.hpp"
#include "ace/coreutils.hpp"
#include "ace/semaphoreguard.hpp"
#include "ace/moreutils.hpp"

#include "etl/algorithm.h"

OpenAce::PostConstruct AircraftTracker::postConstruct()
{
    // timerHandle = xTimerCreateStatic("aircraftTimerTask", TASK_DELAY_MS(1'000), pdFALSE /* Must not be autostart */, this, aircraftTimerTask, &timerBuffer);
    timerHandle = xTimerCreate("aircraftTimerTask", TASK_DELAY_MS(1'000), pdFALSE /* Must not be autostart */, this, aircraftTimerTask);
    maintenanceTimerHandle = xTimerCreate("maintenanceTimerTask", TASK_DELAY_MS(1'000), pdTRUE, this, maintenanceTimerTask);

    aircraftMutex = xSemaphoreCreateMutex();
    ownshipMutex = xSemaphoreCreateMutex();

    if (aircraftMutex == nullptr || ownshipMutex == nullptr)
    {
        return OpenAce::PostConstruct::MUTEX_ERROR;
    }
    return OpenAce::PostConstruct::OK;
}

void AircraftTracker::start()
{
    xTaskCreate(aircraftTrackerTask, "AircraftTracker", TASK_STACK_SIZE, this, tskIDLE_PRIORITY + 2, &taskHandle);
    xTimerStart(maintenanceTimerHandle, TASK_DELAY_MS(25));
    getBus().subscribe(*this);
};

void AircraftTracker::stop()
{
    getBus().unsubscribe(*this);
    xTaskNotify(taskHandle, TaskState::EXIT, eSetBits);
    while (eTaskGetState(taskHandle) != eDeleted)
    {
        vTaskDelay(TASK_DELAY_MS(50));
    }
    xTimerDelete(timerHandle, TASK_DELAY_MS(250));
    xTimerDelete(maintenanceTimerHandle, TASK_DELAY_MS(250));
    vSemaphoreDelete(aircraftMutex);
    vSemaphoreDelete(ownshipMutex);
};

void AircraftTracker::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == NAME)
    {
        puts("TODO: AircraftTracker handle config changes");
    }
}

void AircraftTracker::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void AircraftTracker::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"queueFullErr\":" << statistics.queueFullErr;
    stream << ",\"trackedFullErr\":" << statistics.trackedFullErr;
    stream << ",\"numberOfPlanesTracking\":" << statistics.numberOfPlanesTracking;
    stream << ",\"positionsProcessed\":" << statistics.positionsProcessed;
    stream << ",\"lastTrackPoint\":" << statistics.lastTrackPoint;
    stream << ",\"autoDistanceTrack\":" << autoDistanceTrack;
    stream << "}\n";
}

void AircraftTracker::on_receive(const OpenAce::AircraftPositionMsg &msg)
{
    if (!queue.full())
    {
        queue.push(msg.position);
    }
    else
    {
        statistics.queueFullErr++;
    }
    // Regardless if the queue was full, the tasks needs to get activated so it can start removing stale aircraft or other required processing.
    xTaskNotify(taskHandle, TaskState::NEW, eSetBits);
}

void AircraftTracker::on_receive(const OpenAce::OwnshipPositionMsg &msg)
{
    SemaphoreGuard<25> guard{ownshipMutex};
    if (guard)
    {
        ownshipPosition = msg.position;
    }
}

void AircraftTracker::aircraftTimerTask(TimerHandle_t timer)
{
    AircraftTracker *at = (AircraftTracker *)pvTimerGetTimerID(timer);
    xTaskNotify(at->taskHandle, TaskState::TIMER, eSetBits);
}

void AircraftTracker::maintenanceTimerTask(TimerHandle_t timer)
{
    AircraftTracker *at = (AircraftTracker *)pvTimerGetTimerID(timer);
    xTaskNotify(at->taskHandle, TaskState::MAINTENANCE, eSetBits);
}

void AircraftTracker::aircraftTrackerTask(void *arg)
{
    AircraftTracker *at = static_cast<AircraftTracker *>(arg);
    while (true)
    {
        if (uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(1'000)))
        {
            if (notifyValue & TaskState::EXIT)
            {
                vTaskDelete(nullptr);
                return;
            }

            // Handle timers
            if (notifyValue & TaskState::MAINTENANCE)
            {
                at->removeStaleEntries();
            }

            at->distanceEstimator();

            if (at->trackedAircraft.full())
            {
                at->statistics.trackedFullErr++;
            }

            // Handle timers
            if (notifyValue & TaskState::TIMER)
            {
                at->handleTimer();
                at->statistics.numberOfPlanesTracking = at->trackedAircraft.size();
            }

            // Send new aircraft imediatly
            if (notifyValue & TaskState::NEW)
            {
                at->handleNew();
            }
        }
    }
}

void AircraftTracker::handleNew()
{
    struct FindByIcao
    {
        FindByIcao(const OpenAce::AircraftPositionInfo &api) : address(api.address) {}
        bool operator()(const TrackInfo &i)
        {
            return i.position.address == address;
        }

    private:
        OpenAce::AircraftAddress address;
    };

    uint16_t timeSpread = 0; // Spread allows to put aircraft at slightly different timestamps in the queue for better processing.
    auto time = CoreUtils::timeUs32() + 1'000'000;
    OpenAce::AircraftPositionInfo position;
    while (queue.pop(position) && !trackedAircraft.full())
    {
        // Remove the Aircraft from the existing trackers
        const auto &it = etl::find_if(trackedAircraft.cbegin(), trackedAircraft.cend(), FindByIcao(position));
        if (it != trackedAircraft.cend())
        {
            // Copy the textual representation of the record in the tracked 
            // aircraft into the new aircraft so it doesn't require any more lookups
            position.icaoAddress = it->position.icaoAddress;
            trackedAircraft.erase(it);
        }

        // Put it at the back of the queue if within distance of aircraft that is beeing tracked
        if (position.distanceFromOwn < autoDistanceTrack)
        {
            // Add the textual representation of the address if this was really new
            if (position.icaoAddress.size() == 0) {
                position.icaoAddress = CoreUtils::makeIcaoAddress(position.address, position.addressType);
            }
            trackedAircraft.emplace_back(TrackInfo{time + timeSpread, 0, position});

            // Send immediately?
            // getBus().receive(
            //     OpenAce::TrackedAircraftPositionMsg(position));

            // Increase spread
            timeSpread += 10;
        }
    }

    // Inform via the task that there are possible new entries in trackedAircraft that needs to be processed
    xTaskNotify(taskHandle, TaskState::TIMER, eSetBits);
}

void AircraftTracker::handleTimer()
{
    auto next = trackedAircraft.begin();
    while (next != trackedAircraft.end() && next->nextSendTime < (CoreUtils::timeUs32() + 75'000)) // Subtract 75 ms to also send any other aircraft within 75ms, for efficienty
    {

        // Update stats for this entry
        next->nextSendTime += 1'000'000;
        next->numberOfTries++;

        // Move to end of the queue
        trackedAircraft.splice(trackedAircraft.end(), trackedAircraft, next);

        // Send
        getBus().receive(OpenAce::TrackedAircraftPositionMsg(next->position));

        statistics.positionsProcessed++;

        // Look for a other item that can be send
        next = trackedAircraft.begin();
    }

    // If the queue still holds entries set a timer so processing keeps happening
    if (next != trackedAircraft.end())
    {
        // +1 added to ensure to never get a 0 delay which is invalid
        uint16_t delay = (next->nextSendTime - CoreUtils::timeUs32()) / 1'000;
        if (delay == 0)
        {
            delay = 1;
        }
        xTimerChangePeriod(timerHandle, TASK_DELAY_MS(delay), TASK_DELAY_MS(25));
    }
}

void AircraftTracker::removeStaleEntries()
{

    // Remove entries, trying to guarantee that we make room for new aircraft by removing older entries
    // Removal based on distance from own and how many times the airplane was tracked
    uint8_t trackPoints = MAX_POSITION_INTERPOLATIONS;
    do
    {
        float trackDistance = autoDistanceTrack;
        trackedAircraft.erase(etl::remove_if(trackedAircraft.begin(), trackedAircraft.end(), [trackPoints, trackDistance](const auto &it)
                                             { return it.numberOfTries > trackPoints || it.position.distanceFromOwn > trackDistance; }),
                              trackedAircraft.end());
        trackPoints--;
        // Call the distance estimator to quickly autoDistanceTrack
        distanceEstimator();
    } while ((trackedAircraft.size() > CLEAR_UP_SIZE) && trackPoints > 2);

    statistics.lastTrackPoint = trackPoints;
}

void AircraftTracker::distanceEstimator()
{
    /**
     *  Simple distance estimator to ignore planes that are out of distance and might be irrelevant
     *  The goal is to keep the queue relative full, but with still remove for more inrush of aircraft
     */
    if ((trackedAircraft.size() > AUTO_DISTANCE_TRACK_UPPER))
    {
        autoDistanceTrack = etl::max(MIN_TRACKING_DISTANCE, etl::min(autoDistanceTrack - 5000, MAX_TRACKING_DISTANCE));
    }
    else if ((trackedAircraft.size() < AUTO_DISTANCE_TRACK_LOWER))
    {
        // Increase autoDistanceTrack each them with 1000 meters up untill MAX_TRACKING_DISTANCE when there is room again
        autoDistanceTrack = etl::max(MIN_TRACKING_DISTANCE, etl::min(autoDistanceTrack + 5000, MAX_TRACKING_DISTANCE));
    }
}