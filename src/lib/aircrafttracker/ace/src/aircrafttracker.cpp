#include "../aircrafttracker.hpp"
#include "ace/coreutils.hpp"
#include "ace/models.hpp"

#include "etl/algorithm.h"

GATAS::PostConstruct AircraftTracker::postConstruct()
{
    return GATAS::PostConstruct::OK;
}

void AircraftTracker::start()
{
    xTaskCreate(aircraftTrackerTask, AircraftTracker::NAME.cbegin(), configMINIMAL_STACK_SIZE + 512, this, tskIDLE_PRIORITY + 6, &taskHandle);
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
};

void AircraftTracker::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::CONFIG)
    {
        ownshipAddress = msg.config.gaTasConfig().conspicuity.icaoAddress;
    }
}

void AircraftTracker::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void AircraftTracker::on_receive(const GATAS::Every5SecMsg &msg)
{
    (void)msg;
    xTaskNotify(taskHandle, TaskState::MAINTAIN, eSetBits);
}

void AircraftTracker::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    for (uint8_t i = 0; i < static_cast<uint8_t>(GATAS::DataSource::_TRANSPROTOCOLS); i++)
    {
        stream << "\"" << GATAS::dataSourceIntToString(i) << "\":";
        antennaRadiationPattern[i].serialize(stream);
        stream << ",";
    }
    stream << "\"queueFullErr\":" << statistics.queueFullErr;
    stream << ",\"numberOfObjectsTracking\":" << trackedAircraft.size();
    stream << ",\"positionsProcessed\":" << statistics.positionsProcessed;
    stream << ",\"adaptiveRadius\":" << trackedAircraft.radius();
    stream << "}\n";
}

void AircraftTracker::on_receive(const GATAS::AircraftPositionsMsg &msg)
{
    bool full = false;
    for (const auto &aircraft : msg.positions)
    {
        if (!queue.full())
        {
            if (ownshipAddress == aircraft.address)
            {
                // Ignore ownship so we don't get our own plane on EFB's
                return;
            }
            queue.push(aircraft);
        }
        else
        {
            xTaskNotify(taskHandle, TaskState::NEW, eSetBits);
            statistics.queueFullErr += 1;
            full = true;
        }
    }
    xTaskNotify(taskHandle, TaskState::NEW, eSetBits);
    if (full)
    {
        vTaskDelay(TASK_DELAY_MS(100));
    }
}

void AircraftTracker::on_receive(const GATAS::AircraftPositionMsg &msg)
{
    if (ownshipAddress == msg.position.address)
    {
        // Ignore ownship so we don't get our own plane on EFB's
        return;
    }

    uint8_t dataSource = static_cast<uint8_t>(msg.position.dataSource);
    if (dataSource < antennaRadiationPattern.size())
    {
        antennaRadiationPattern[dataSource].put(msg);
    }

    if (!queue.full())
    {
        queue.push(msg.position);
    }
    else
    {
        statistics.queueFullErr += 1;
    }

    // Always notify
    xTaskNotify(taskHandle, TaskState::NEW, eSetBits);
}

void AircraftTracker::aircraftTrackerTask(void *arg)
{
    AircraftTracker *at = static_cast<AircraftTracker *>(arg);
    while (true)
    {
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(1000 / TIMESLICES));
        if (notifyValue & TaskState::EXIT)
        {
            vTaskDelete(nullptr);
            return;
        }
        // Handle timers
        if (notifyValue & TaskState::MAINTAIN)
        {
            at->maintenance();
            at->getBus().receive(GATAS::AdapativeRadiusMsg(at->trackedAircraft.radius()));
        }

        // Handle timers
        if (notifyValue == 0 || notifyValue & TaskState::TIMER)
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

void AircraftTracker::handleNew()
{
    GATAS::AircraftPositionInfo position;
    while (queue.pop(position))
    {
        trackedAircraft.insert(position);
        statistics.positionsProcessed += 1;
    }
}

void AircraftTracker::sendEligibleAircraft()
{
    trackedAircraft.next(
        etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<AircraftTracker, &AircraftTracker::handleTrackedAircraft>(*this));
}

void AircraftTracker::maintenance()
{
    trackedAircraft.maintenance();
}