#include "../radiotunerrx_v2.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "../countryregulations_v2.hpp"

#include "etl/algorithm.h"

#define GATAS_TUNE_LOG(fmt, ...) GATAS_LOG_IF(false, fmt, ##__VA_ARGS__)

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
    RadioTunerRx *radioTunerRx = static_cast<RadioTunerRx *>(arg);
    bool taskBlocked = false;
    uint16_t nextDelayMs = 200;

    while (true)
    {
        uint32_t notifyValue = 0;
        xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifyValue, TASK_DELAY_MS(nextDelayMs));

        if (notifyValue & TaskState::BLOCK)
        {
            taskBlocked = true;
            nextDelayMs = 100;
            radioTunerRx->eventSync.set(BIT_EVENT_DONE);
        }
        if (notifyValue & TaskState::UNBLOCK)
        {
            taskBlocked = false;
            radioTunerRx->eventSync.set(BIT_EVENT_DONE);
        }

        if (taskBlocked)
        {
            continue;
        }

        const uint32_t loopStartMs = CoreUtils::msSinceMidnight();
        const uint32_t nowMs = loopStartMs;
        uint32_t nextBoundaryInMs = std::numeric_limits<uint32_t>::max();

        for (auto &&ref : radioTunerRx->radioCtxList)
        {
            if (ref.protocolTimings.empty() || ref.maxTimeMs == 0)
            {
                continue;
            }

            // Position within the repeating cycle
            const int32_t cycleMs = nowMs % ref.maxTimeMs;

            // Find which entry covers this time
            size_t foundIdx = 0;
            for (size_t i = 0; i < ref.protocolTimings.size(); i++)
            {
                auto lenientCycleMs = cycleMs + 20;
                if (lenientCycleMs >= ref.protocolTimings[i].start && lenientCycleMs < ref.protocolTimings[i].end)
                {
                    foundIdx = i;
                    break;
                }
            }

//            GATAS_INFO("Radio %d cycleMs: %ld, ppsMS: %d  current entry: %u, start: %d, end: %d, ds: %s\n", ref.radioNo, cycleMs,  CoreUtils::msInSecond(), foundIdx, ref.protocolTimings[foundIdx].start, ref.protocolTimings[foundIdx].end, (ref.protocolTimings[foundIdx].protocolTimeConfig != nullptr) ? GATAS::toString(ref.protocolTimings[foundIdx].protocolTimeConfig->radioConfig.dataSource()) : "NOOP");
            const auto &entry = ref.protocolTimings[foundIdx];

            // Only reconfigure radio if protocol changed
            if (foundIdx != ref.currentProtocolIdx)
            {
                ref.currentProtocolIdx = foundIdx;
                if (entry.protocolTimeConfig != nullptr)
                {
                    // GATAS_TUNE_LOG("Radio %d switching to DS %s on channel %d", ref.radioNo, GATAS::toString(entry.protocolTimeConfig->radioConfig.dataSource()), static_cast<uint8_t>(entry.channel));
                    auto frequency = CountryRegulations::getFrequency(entry.protocolTimeConfig->rfConfig, entry.channel);
                    radioTunerRx->getBus().receive(GATAS::RadioControlMsg{
                        GATAS::RadioParameters(&entry.protocolTimeConfig->radioConfig, &entry.protocolTimeConfig->rfConfig, frequency, 0),
                        ref.radioNo});
                    ref.statistics.taskActivity += 1;
                }
            }

            // Time until this radio hits its next timing boundary.
            // We take the minimum across all radios so the task wakes on the earliest boundary.
            uint32_t timeToBoundary = static_cast<uint32_t>(entry.end - cycleMs);
            if (timeToBoundary < nextBoundaryInMs)
            {
                nextBoundaryInMs = timeToBoundary;
            }
        }

        if (nextBoundaryInMs == std::numeric_limits<uint32_t>::max())
        {
            nextBoundaryInMs = 1000;
        }

        const uint32_t loopEndMs = CoreUtils::msSinceMidnight();
        const uint32_t loopElapsedMs = (loopEndMs >= loopStartMs)
                                           ? (loopEndMs - loopStartMs)
                                           : (86'400'000U - loopStartMs + loopEndMs);
        const int32_t correctedDelayMs = static_cast<int32_t>(nextBoundaryInMs) - static_cast<int32_t>(loopElapsedMs);
        nextDelayMs = static_cast<uint16_t>(etl::max(correctedDelayMs, static_cast<int32_t>(1)));
    }
}

// ******************** Message bus receive handlers ********************

void RadioTunerRx::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
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
    if (static_cast<uint8_t>(msg.position.dataSource) < slotReceive.size())
    {
        slotReceive[static_cast<uint8_t>(msg.position.dataSource)] += 1;
    }
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

bool RadioTunerRx::hasReceived(GATAS::DataSource ds)
{
    GATAS_ASSERT(ds < GATAS::DataSource::_TRANSPROTOCOLS, "Invalid datasource");
    bool hasReception = slotReceive[static_cast<uint8_t>(ds)] != 0;
    slotReceive[static_cast<uint8_t>(ds)] = 0;
    return hasReception;
}

void RadioTunerRx::assignDataSources()
{
    auto guard = BaseModule::lockSharedMutex();

    if (currentZone.value() == CountryRegulations::Zone::enum_type::ZONE0)
    {
        return;
    }

    if (!blockTasks())
    {
        return;
    }

    auto availableTimings = CountryRegulations::getProtocolRxTimingsForZone(currentZone.value(), configuredDatasources);

    // Assign availableTimings evenly over the radios
    size_t radioCount = radioCtxList.size();
    size_t idx = 0;
    for (auto &ctx : radioCtxList)
    {
        ctx.clear();
        static constexpr uint16_t MAX_NOOP_MS = 200;
        uint16_t secondIndex = 0;
        for (size_t i = 0; i < availableTimings.size(); i++)
        {
            if (i % radioCount == idx)
            {
                const auto &slots = availableTimings[i]->timeSlots;
                const uint16_t offset = secondIndex * 1000;
                uint16_t prevEnd = offset;

                // Add main slots using their real timing positions within the second
                // Insert NOOP entries for any gap before each slot
                for (const auto &timing : slots)
                {
                    const uint16_t slotStart = static_cast<uint16_t>(offset + timing.start);
                    while (prevEnd < slotStart && !ctx.protocolTimings.full())
                    {
                        uint16_t fillEnd = etl::min(static_cast<uint16_t>(prevEnd + MAX_NOOP_MS), slotStart);
                        ctx.protocolTimings.emplace_back(nullptr, CountryRegulations::Channel::NOOP, prevEnd, fillEnd);
                        prevEnd = fillEnd;
                    }
                    if (!ctx.protocolTimings.full())
                    {
                        uint16_t cappedEnd = etl::min(timing.end, static_cast<uint16_t>(1000));
                        ctx.protocolTimings.emplace_back(availableTimings[i], timing.channel, slotStart, static_cast<uint16_t>(offset + cappedEnd));
                        prevEnd = offset + cappedEnd;
                    }
                }

                // Fill any remaining gap to end of this second
                const uint16_t secondEnd = static_cast<uint16_t>(offset + 1000);
                while (prevEnd < secondEnd && !ctx.protocolTimings.full())
                {
                    uint16_t fillEnd = etl::min(static_cast<uint16_t>(prevEnd + MAX_NOOP_MS), secondEnd);
                    ctx.protocolTimings.emplace_back(nullptr, CountryRegulations::Channel::NOOP, prevEnd, fillEnd);
                    prevEnd = fillEnd;
                }

                ctx.maxTimeMs = secondEnd;
                secondIndex++;
            }
        }

        // Fill NOOP gaps: prefer same-radio protocols first, then other radios
        for (auto &entry : ctx.protocolTimings)
        {
            if (entry.protocolTimeConfig != nullptr)
            {
                continue;
            }
            const uint16_t midMs = static_cast<uint16_t>((entry.start + entry.end) / 2);
            if (!fillNoop(entry, midMs, availableTimings, radioCount, idx, true))
            {
                fillNoop(entry, midMs, availableTimings, radioCount, idx, false);
            }
        }

        idx++;
    }
    releaseTasks();
}

bool RadioTunerRx::fillNoop(RxTiming &entry, uint16_t midMs, etl::span<const CountryRegulations::ProtocolRxTimeSlot *> timings, size_t radioCount, size_t radioIdx, bool sameRadio)
{
    for (size_t si = 0; si < timings.size(); si++)
    {
        bool isOwnRadio = (si % radioCount == radioIdx);
        if (isOwnRadio != sameRadio)
        {
            continue;
        }
        const auto *fitting = CountryRegulations::findFittingTiming(midMs, timings[si]->timeSlots);
        if (fitting != nullptr)
        {
            entry.protocolTimeConfig = timings[si];
            entry.channel = fitting->channel;
            return true;
        }
    }
    return false;
}

bool RadioTunerRx::blockTasks()
{
    eventSync.clear(BIT_EVENT_DONE);
    xTaskNotify(taskHandle, TaskState::BLOCK, eSetBits);

    if (!eventSync.wait(BIT_EVENT_DONE, pdMS_TO_TICKS(2000)))
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
