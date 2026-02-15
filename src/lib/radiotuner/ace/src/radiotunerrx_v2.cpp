#include "../radiotunerrx_v2.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "../countryregulations_v2.hpp"

#include "etl/algorithm.h"

#include "pico/rand.h"

GATAS::PostConstruct RadioTunerRx::postConstruct()
{
    if (moduleByName(*this, Radio::NAMES[0]))
    {
        radioCtxList.emplace_back(this, 0);
    }
    else
    {
        return GATAS::PostConstruct::DEP_NOT_FOUND;
    }
    if (moduleByName(*this, Radio::NAMES[1]))
    {
        radioCtxList.emplace_back(this, 1);
    }

    if (xTaskCreate(radioTuneTask, RadioTunerRx::NAME.cbegin(), configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY + 2, &taskHandle) != pdPASS)
    {
        return GATAS::PostConstruct::TASK_ERROR;
    }

    const auto result = CountryRegulations::validateProtocolTxTimings();
    static_assert(result == 0, "ProtocolTxTimeSlot table is INVALID");

    return GATAS::PostConstruct::OK;
}

void RadioTunerRx::start()
{
    getBus().subscribe(*this);
};

void RadioTunerRx::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"_dummy\": 0";
    for (const auto &taskCtx : radioCtxList)
    {
        taskCtx.getData(stream);
    }
    stream << ",\"zone\":\"" << CountryRegulations::zoneToString(currentZone) << "\"";
    stream << "}";
}

//*********************** Tuner tasks ***********************

void RadioTunerRx::radioTuneTask(void *arg)
{
    (void)arg;
    uint16_t nextDelay = 200;
    RadioTunerRx *radioTunerRx = static_cast<RadioTunerRx *>(arg);
    bool taskBlocked = false;
    while (true)
    {
        // radioTunerRx->assignDataSources();
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(nextDelay));
        if (notifyValue & TaskState::UNBLOCK)
        {
            taskBlocked = false;
            radioTunerRx->eventSync.set(BIT_EVENT_DONE);
            continue;
        }
        if (notifyValue & TaskState::BLOCK)
        {
            taskBlocked = true;
            // TODO: Create a new way or message to disaple receivers?
            // // Disable the transceivers during reconfiguration
            // for (auto &ref : radioTunerRx->radioCtxList)
            // {
            //     radioTunerRx->getBus().receive(GATAS::RadioControlMsg{
            //         Radio::RadioParameters{&CountryRegulations::PROTOCOL_NONE, CountryRegulations::Europe.baseFrequency, 0, 8},
            //         ref.radioNo});
            // }
            radioTunerRx->eventSync.set(BIT_EVENT_DONE);
            continue;
        }

        if (taskBlocked)
        {
            continue;
        }

        auto timeMs = CoreUtils::msInSecond();
        auto currentSlot = (timeMs / CountryRegulations::SLOT_MS);

        // On the RP2350 we have a little more math speed compared to RP2040, we can get timing slightly more tight
#if defined(PICO_RP2350)
        int32_t expectedSlot = (timeMs + 75) / CountryRegulations::SLOT_MS;
        int16_t diff = CountryRegulations::SLOT_MS * expectedSlot - timeMs;
        if (diff > 0)
        {
            // GATAS_INFO("Fixing time differences of %d %dms", timeMs, diff);
            vTaskDelay(TASK_DELAY_MS(diff));
        }
#endif
        for (auto &&ref : radioTunerRx->radioCtxList)
        {
            if (ref.protocolTimings.empty())
            {
                // No protocol timings available, skip everything
                continue;
            }

            // Take next protocol config
            etl::advance(ref.protocolIterator, 1);
            const auto &timeSlot = *ref.protocolIterator;

            // Get the timigs for this slot and validate if that is valid
            auto timingPtr = CountryRegulations::findFittingTiming(currentSlot * CountryRegulations::SLOT_MS, timeSlot->timeSlots);
            if (timingPtr == nullptr)
            {
                continue;
            }

            auto frequency = CountryRegulations::getFrequency(timeSlot->frequency, timingPtr->channel);

            // Note to self, the tranceiver itself will decide if to reconfigure or not
            radioTunerRx->getBus().receive(GATAS::RadioControlMsg{Radio::RadioParameters(&timeSlot->radioConfig, frequency, timeSlot->frequency.powerdBm), ref.radioNo});
            ref.statistics.taskActivity += 1;
        }

        // Calculate delay to next slot
        // +1 is needed to fix half a milisecond fixes to ensure we go to the correct slot
        // Unfortunatly it meens that sometimes we are 1ms late... Checing us timer might be overkill to fix a 1ms disperency.
        nextDelay = CoreUtils::msDelayToReference((currentSlot + 1) * CountryRegulations::SLOT_MS + 1, CoreUtils::msInSecond());
    }
}

// ******************** Message bus receive handlers ********************

void RadioTunerRx::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    // Set to current time else bluetooth connections will fail
    static auto lastTime = CoreUtils::timeUs32Raw();

    if (CoreUtils::isUsReachedRaw(lastTime) || static_cast<uint8_t>(currentZone) == static_cast<uint8_t>(CountryRegulations::Zone::ZONE0))
    {
        lastTime = CoreUtils::timeUs32Raw() + UPDATE_ZONE_REGULATION_EVERY;
        currentZone.set(CountryRegulations::zone(msg.position.lat, msg.position.lon));

        if (currentZone.isModified())
        {
            assignDataSources();
        }
    }
}

void RadioTunerRx::on_receive(const GATAS::AircraftPositionMsg &msg)
{
    GATAS_ASSERT(msg.position.dataSource < GATAS::DataSource::_TRANSPROTOCOLS, "Invalid datasource");
    slotReceive[(uint8_t)msg.position.dataSource] += 1;
}

void RadioTunerRx::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::NAME)
    {
        dataSources = msg.config.gaTasConfig().protocols;
        assignDataSources();
    }
}

void RadioTunerRx::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

/**
 * Return true igf for this protocol data was received;
 */
bool RadioTunerRx::hasReceived(GATAS::DataSource ds)
{
    GATAS_ASSERT(ds < GATAS::DataSource::_TRANSPROTOCOLS, "Invalid datasource");
    bool hasReception = slotReceive[static_cast<uint8_t>(ds)] != 0;
    slotReceive[static_cast<uint8_t>(ds)] = 0;
    return hasReception;
}

void RadioTunerRx::assignDataSources()
{
    if (currentZone == CountryRegulations::ZONE0)
    {
        return;
    }

    if (!blockTasks())
    {
        return;
    }

    auto availableTimings = CountryRegulations::getProtocolRxTimingsForZone(currentZone.value(), dataSources);

    if (availableTimings.size() > 0)
    {
        const size_t itemsPerRadio = availableTimings.size() / radioCtxList.size();
        auto avTimigsIt = availableTimings.cbegin();

        for (auto &&taskCtx = radioCtxList.begin(); taskCtx != radioCtxList.end(); ++taskCtx)
        {
            taskCtx->protocolTimings.clear();
            if (etl::next(taskCtx) == radioCtxList.end())
            {
                taskCtx->protocolTimings.insert(taskCtx->protocolTimings.end(), avTimigsIt, availableTimings.cend());
            }
            else
            {
                taskCtx->protocolTimings.insert(taskCtx->protocolTimings.end(), avTimigsIt, avTimigsIt + itemsPerRadio);
                avTimigsIt += itemsPerRadio;
            }
            taskCtx->protocolIterator = etl::circular_iterator<TimeSlotVector::const_iterator>(taskCtx->protocolTimings.cbegin(), taskCtx->protocolTimings.cend());
        }
    }

    releaseTasks();
}

bool RadioTunerRx::blockTasks()
{
    eventSync.clear(BIT_EVENT_DONE);
    xTaskNotify(taskHandle, TaskState::BLOCK, eSetBits);

    // Expected is 200ms per tick, we wait 10 times as long, much much longer
    // We 'should' never end up here??
    if (!eventSync.wait(BIT_EVENT_DONE, pdMS_TO_TICKS(CountryRegulations::SLOT_MS * 10)))
    {
        GATAS_INFO("RadioTunerRx: Failed to wait for event sync");
        return false;
    }
    return true;
}
void RadioTunerRx::releaseTasks()
{
    xTaskNotify(taskHandle, TaskState::UNBLOCK, eSetBits);
}