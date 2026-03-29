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
    stream << ",\"zone\":\"" << currentZone.value().c_str() << "\"";
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
            //         GATAS::RadioParameters{&CountryRegulations::PROTOCOL_NONE, CountryRegulations::Europe.baseFrequency, 0, 8},
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
                continue;
            }

            // Scan all assigned protocols starting from lastCheckedIndex+1 (round-robin fairness)
            // Pick the first one that is active in the current timeslot
            const CountryRegulations::ProtocolRxTimeSlot *bestSlot = nullptr;
            const CountryRegulations::ChannelTiming *bestTiming = nullptr;
            const size_t count = ref.protocolTimings.size();

            for (size_t i = 0; i < count; i++)
            {
                size_t idx = (ref.lastCheckedIndex + 1 + i) % count;
                const auto *ts = ref.protocolTimings[idx];
                auto *timing = CountryRegulations::findFittingTiming(currentSlot * CountryRegulations::SLOT_MS, ts->timeSlots);
                if (timing != nullptr)
                {
                    bestSlot = ts;
                    bestTiming = timing;
                    ref.lastCheckedIndex = static_cast<uint8_t>(idx);
                    break;
                }
            }

            if (bestSlot == nullptr)
            {
                continue;
            }

            auto frequency = CountryRegulations::getFrequency(bestSlot->rfConfig, bestTiming->channel);
            auto dataSource = bestSlot->radioConfig.dataSource();

            // Skip sending if the same datasource+frequency was already sent recently
            auto timeUs32Raw = CoreUtils::timeUs32Raw();
            bool sameConfig = (ref.lastDataSource == dataSource && ref.lastFrequency == frequency);
            bool withinTimeout = (timeUs32Raw - ref.lastSendUsRaw) < 2'500'000;
            if (sameConfig && withinTimeout)
            {
                continue;
            }

            radioTunerRx->getBus().receive(GATAS::RadioControlMsg{GATAS::RadioParameters(&bestSlot->radioConfig, &bestSlot->rfConfig, frequency, 0), ref.radioNo});
            ref.lastDataSource = dataSource;
            ref.lastFrequency = frequency;
            ref.lastSendUsRaw = timeUs32Raw;
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

    if (CoreUtils::isUsReachedRaw(lastTime) || currentZone.value() == CountryRegulations::Zone::ZONE0)
    {
        lastTime = CoreUtils::timeUs32Raw() + UPDATE_ZONE_REGULATION_EVERY;
        currentZone.set(CountryRegulations::zone(msg.position.lat, msg.position.lon));

        if (currentZone.isModified())
        {
            assignDataSources();
        }
    }
}

void RadioTunerRx::on_receive(const GATAS::IngressAircraftPositionMsg &msg)
{
    GATAS_ASSERT(msg.position.dataSource < GATAS::DataSource::_TRANSPROTOCOLS, "Invalid datasource");
    slotReceive[(uint8_t)msg.position.dataSource] += 1;
}

void RadioTunerRx::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::NAME)
    {
        configuredDatasources = msg.config.gaTasConfig().protocols;
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
    if (currentZone.value() == CountryRegulations::Zone::enum_type::ZONE0)
    {
        return;
    }

    if (!blockTasks())
    {
        return;
    }

    auto availableTimings = CountryRegulations::getProtocolRxTimingsForZone(currentZone.value(), configuredDatasources);

    for (auto &taskCtx : radioCtxList)
    {
        taskCtx.protocolTimings.clear();
        taskCtx.lastCheckedIndex = 0;
        taskCtx.lastDataSource = GATAS::DataSource::_ITEMS;
        taskCtx.lastFrequency = 0;
    }

    if (availableTimings.size() > 0)
    {
        // Calculate total active ms for each protocol
        auto totalActiveMs = [](const CountryRegulations::ProtocolRxTimeSlot *slot) -> uint16_t
        {
            uint16_t total = 0;
            for (const auto &t : slot->timeSlots)
            {
                total += (t.end - t.start);
            }
            return total;
        };

        // Track load per radio (total assigned active ms)
        etl::array<uint16_t, GATAS_MAX_RADIOS> radioLoad = {};

        // Sort indices by total active time descending (heaviest protocols assigned first)
        etl::array<uint8_t, GATAS_MAX_SOURCE_PER_RADIO * GATAS_MAX_RADIOS> sortedIndices = {};
        for (uint8_t i = 0; i < availableTimings.size(); i++)
        {
            sortedIndices[i] = i;
        }
        // Simple insertion sort (small N)
        for (size_t i = 1; i < availableTimings.size(); i++)
        {
            for (size_t j = i; j > 0 && totalActiveMs(availableTimings[sortedIndices[j]]) > totalActiveMs(availableTimings[sortedIndices[j - 1]]); j--)
            {
                auto tmp = sortedIndices[j];
                sortedIndices[j] = sortedIndices[j - 1];
                sortedIndices[j - 1] = tmp;
            }
        }

        // Greedy least-loaded assignment
        for (size_t i = 0; i < availableTimings.size(); i++)
        {
            const auto *protocol = availableTimings[sortedIndices[i]];

            // Find radio with minimum load
            size_t bestRadio = 0;
            for (size_t r = 1; r < radioCtxList.size(); r++)
            {
                if (radioLoad[r] < radioLoad[bestRadio])
                {
                    bestRadio = r;
                }
            }

            if (!radioCtxList[bestRadio].protocolTimings.full())
            {
                radioCtxList[bestRadio].protocolTimings.push_back(protocol);
                radioLoad[bestRadio] += totalActiveMs(protocol);
            }
        }

        // Gap filling: for each radio, find 200ms windows with no active protocol
        // and fill with protocols from other radios (up to MAX_EXTRA_SLOTS per radio)
        for (size_t r = 0; r < radioCtxList.size(); r++)
        {
            auto &ctx = radioCtxList[r];
            if (ctx.protocolTimings.full())
            {
                continue;
            }

            // Check each 200ms window in a second
            for (uint16_t slotStart = 0; slotStart < 1000; slotStart += CountryRegulations::SLOT_MS)
            {
                // Does this radio already have something active here?
                bool hasCoverage = false;
                for (const auto *ts : ctx.protocolTimings)
                {
                    if (CountryRegulations::findFittingTiming(slotStart, ts->timeSlots) != nullptr)
                    {
                        hasCoverage = true;
                        break;
                    }
                }
                if (hasCoverage)
                {
                    continue;
                }

                // Find a protocol from another radio that is active in this window
                for (size_t otherR = 0; otherR < radioCtxList.size(); otherR++)
                {
                    if (otherR == r)
                    {
                        continue;
                    }
                    for (const auto *ts : radioCtxList[otherR].protocolTimings)
                    {
                        if (CountryRegulations::findFittingTiming(slotStart, ts->timeSlots) != nullptr)
                        {
                            // Check not already assigned to this radio
                            bool alreadyAssigned = etl::find(ctx.protocolTimings.cbegin(), ctx.protocolTimings.cend(), ts) != ctx.protocolTimings.cend();
                            if (!alreadyAssigned && !ctx.protocolTimings.full())
                            {
                                ctx.protocolTimings.push_back(ts);
                                goto nextSlot;
                            }
                        }
                    }
                }
                nextSlot:;

                if (ctx.protocolTimings.full())
                {
                    break;
                }
            }
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
        GATAS_WARN("Failed to wait for event sync");
        return false;
    }
    return true;
}
void RadioTunerRx::releaseTasks()
{
    xTaskNotify(taskHandle, TaskState::UNBLOCK, eSetBits);
}