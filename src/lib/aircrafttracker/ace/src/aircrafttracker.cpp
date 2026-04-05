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
    xTaskCreate(aircraftTrackerTrampoline, AircraftTracker::NAME.cbegin(), configMINIMAL_STACK_SIZE + 768, this, tskIDLE_PRIORITY + 6, &taskHandle);
    getBus().subscribe(*this);
};

void AircraftTracker::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::NAME || msg.moduleName == AircraftTracker::NAME)
    {
        ownshipAddress = msg.config.gaTasConfig().conspicuity.icaoAddress;
        groundStation_ = msg.config.gaTasConfig().conspicuity.groundStation;
        trackedAircraft.ddbEnabled(msg.config.valueByPath(false, NAME, "ddbEnabled"));
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
        stream << "\"" << GATAS::dataSourceIntToString(i) << ":AntPolar\":";
        antennaRadiationPattern[i].serialize(stream);
        stream << ",";
    }
    stream << "\"queueFull:err\":" << statistics.queueFullErr;
    stream << ",\"numberOfObjectsTracking\":" << trackedAircraft.size();
    stream << ",\"positionsProcessed:k\":" << statistics.positionsProcessed;
    stream << ",\"adaptiveRadius:m\":" << trackedAircraft.radius();
    stream << "}";
}

void AircraftTracker::on_receive(const GATAS::IngressAircraftPositionsMsg &msg)
{
    for (const auto &aircraft : msg.positions)
    {
        if (ownshipAddress == aircraft.address)
        {
            continue;
        }
        if (!queue.full())
        {
            queue.push(aircraft);
        }
        else
        {
            GATAS_WARN("Queue Full");
            statistics.queueFullErr += 1;
            break;
        }
    }
    xTaskNotify(taskHandle, TaskState::NEW, eSetBits);
}

void AircraftTracker::on_receive(const GATAS::RadioTxPositionRequestMsg &msg)
{
    // radioParameters.id == 1 means O-Band Uplink
    // We do that here, because we neeed to send 10 aircraft instead of just ownship
    // Only function as ADSL uplink in ground station mode
    if (groundStation_ && msg.radioParameters.config->dataSource() == GATAS::DataSource::ADSLO_HDR && msg.radioParameters.id == 1)
    {
        if (!tXqueue.full())
        {
            tXqueue.push(Tx_Struct{msg.radioParameters, msg.radioNo});
        }
        xTaskNotify(taskHandle, TaskState::CLOSEST_10, eSetBits);
    }
}

void AircraftTracker::on_receive(const GATAS::IngressAircraftPositionMsg &msg)
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
    xTaskNotify(taskHandle, TaskState::NEW, eSetBits);
}

void AircraftTracker::aircraftTrackerTrampoline(void *arg)
{
    static_cast<AircraftTracker *>(arg)->aircraftTrackerTask(arg);
}

void AircraftTracker::aircraftTrackerTask(void *arg)
{
    (void)arg;
    while (true)
    {
        uint32_t notifyValue = 0;
        xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifyValue, TASK_DELAY_MS(1000 / TIMESLICES));

        // Handle timers
        if (notifyValue & TaskState::MAINTAIN)
        {
            maintenance();
            getBus().receive(GATAS::AdapativeRadiusMsg(trackedAircraft.radius()));
        }

        // Handle timers
        if (notifyValue == 0 || notifyValue & TaskState::TIMER)
        {
            sendEligibleAircraft();
        }

        // Handle timers
        if (notifyValue & TaskState::CLOSEST_10)
        {
            closest10();
        }

        // Handle new aircraft
        if (notifyValue & TaskState::NEW)
        {
            handleNew();
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

void AircraftTracker::handleTrackedAircraft(const GATAS::AircraftPositionInfo &position)
{
    getBus().receive(GATAS::EgressAircraftPositionMsg(position));
}

void AircraftTracker::sendEligibleAircraft()
{
    trackedAircraft.sendScheduled(
        etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<AircraftTracker, &AircraftTracker::handleTrackedAircraft>(*this));
}

void AircraftTracker::closest10()
{
    if (Tx_Struct msg; tXqueue.pop(msg))
    {
        auto aircraft = trackedAircraft.forClosest();
        getBus().receive(GATAS::EgressAircraftPositionsMsg(aircraft, msg.radioParameters, msg.radioNo));
        tXqueue.clear();
    }
}

void AircraftTracker::maintenance()
{
    trackedAircraft.maintenance();
}
