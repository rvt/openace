#include "aircrafttracker.hpp"
#include "ace/coreutils.hpp"
#include "ace/coreutils.hpp"
#include "ace/semaphoreguard.hpp"

#include "etl/algorithm.h"

OpenAce::PostConstruct AircraftTracker::postConstruct()
{
    transmitTimerHandle = xTimerCreate("aircraftTimerTask", TASK_DELAY_MS(1'000), pdFALSE /* Must not be autostart */, this, aircraftTimerTask);
    return OpenAce::PostConstruct::OK;
}

void AircraftTracker::start()
{
    xTaskCreate(aircraftTrackerTask, "AircraftTracker", TASK_STACK_SIZE, this, tskIDLE_PRIORITY + 2, &taskHandle);
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
    xTimerDelete(transmitTimerHandle, TASK_DELAY_MS(250));
};

void AircraftTracker::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == NAME)
    {
        // No configuraiton yet
    }
}

void AircraftTracker::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void AircraftTracker::on_receive(const OpenAce::IdleMsg &msg)
{
    (void)msg;
    xTaskNotify(taskHandle, TaskState::MAINTAIN, eSetBits);
}

void AircraftTracker::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"queueFullErr\":" << statistics.queueFullErr;
    stream << ",\"numberOfPlanesTracking\":" << trackedAircraft.size();
    stream << ",\"positionsProcessed\":" << statistics.positionsProcessed;
    stream << ",\"adaptiveRadius\":" << trackedAircraft.radius();
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
    // Always notify
    xTaskNotify(taskHandle, TaskState::NEW, eSetBits);
}

void AircraftTracker::aircraftTimerTask(TimerHandle_t timer)
{
    AircraftTracker *at = (AircraftTracker *)pvTimerGetTimerID(timer);
    xTaskNotify(at->taskHandle, TaskState::TIMER, eSetBits);
}

void AircraftTracker::aircraftTrackerTask(void *arg)
{
    AircraftTracker *at = static_cast<AircraftTracker *>(arg);
    xTimerChangePeriod(at->transmitTimerHandle, TASK_DELAY_MS(1000), TASK_DELAY_MS(25));
    while (true)
    {
        if (uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(1'000)))
        {
            if (notifyValue & TaskState::EXIT)
            {
                vTaskDelete(nullptr);
                return;
            }

            // Only do work at each begin which should be around ever HEARTBEAT_TIME

            // Handle timers
            if (notifyValue & TaskState::MAINTAIN)
            {
                at->maintenance();
            }

            // Handle timers
            if (notifyValue & TaskState::TIMER)
            {
                at->sendEligibleAircraft();
            }

            // Handle new aircraft
            if (notifyValue & TaskState::NEW)
            {
                at->handleNew();
            }
        }
    }
}

void AircraftTracker::handleNew()
{
    OpenAce::AircraftPositionInfo position;
    while (queue.pop(position))
    {
        trackedAircraft.insert(position);
        statistics.positionsProcessed++;
    }
}

void AircraftTracker::sendEligibleAircraft()
{
    //    puts("\033[2J\033[H");
    //    trackedAircraft.dump();
    auto delay = trackedAircraft.next([this](const OpenAce::AircraftPositionInfo &position)
                                      { getBus().receive(OpenAce::TrackedAircraftPositionMsg(position)); });
    if (delay == 0)
    {
        delay = 1;
    }
    xTimerChangePeriod(transmitTimerHandle, TASK_DELAY_MS(delay), TASK_DELAY_MS(25));
}

void AircraftTracker::maintenance()
{
    trackedAircraft.maintenance();
}