#include "radiotunerrx_v2.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "radiotunertx.hpp"

#include "etl/algorithm.h"
#include "etl/pseudo_moving_average.h"

#include "pico/rand.h"

GATAS::PostConstruct RadioTunerRx::postConstruct()
{
    if (moduleByName(*this, Radio::NAMES[0]))
    {
        numRadios = 1;
    }
    else
    {
        return GATAS::PostConstruct::DEP_NOT_FOUND;
    }
    if (moduleByName(*this, Radio::NAMES[1]))
    {
        numRadios++;
    }

    radioTasks.clear();
    for (auto radioNo = 0; radioNo < numRadios; radioNo++)
    {
        radioTasks.emplace_back(this, radioNo);
    }

    if (xTaskCreate(radioTuneTask, RadioTunerRx::NAME.cbegin(), configMINIMAL_STACK_SIZE + 64, this, tskIDLE_PRIORITY + 2, &taskHandle) != pdPASS)
    {
        return GATAS::PostConstruct::TASK_ERROR;
    }

    return GATAS::PostConstruct::OK;
}

void RadioTunerRx::start()
{
    getBus().subscribe(*this);

    Configuration *config = static_cast<Configuration *>(BaseModule::moduleByName(*this, Configuration::NAME));
    if (config)
    {
        assignDataSources(config->gaTasConfig().protocols);
    }
};

void RadioTunerRx::stop()
{
    getBus().unsubscribe(*this);

    radioTasks.clear();
};

void RadioTunerRx::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"_dummy\": 0";
    for (auto it = radioTasks.cbegin(); it != radioTasks.cend(); ++it)
    {
        it->getData(stream);
    }
    stream << ",\"zone\":\"" << CountryRegulations2::zoneToString(currentZone.value()) << "\"";
    stream << "}\n";
}

//*********************** Tuner tasks ***********************

void RadioTunerRx::radioTuneTask(void *arg)
{
    uint16_t nextDelay = 200;
    RadioTunerRx *radioTunerRx = static_cast<RadioTunerRx *>(arg);
    bool taskBlock = false;
    uint16_t priorityCounter = 0;
    bool performPriority = false;
    while (true)
    {
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(nextDelay));
        if (notifyValue & TaskState::EXIT)
        {
            vTaskDelete(nullptr);
            return;
        }
        if (notifyValue & TaskState::UNBLOCK)
        {
            taskBlock = false;
            radioTunerRx->eventSync.set(BIT_EVENT_DONE);
        }
        if (notifyValue & TaskState::BLOCK)
        {
            taskBlock = true;
            // Disable the ranceivers during reconfiguration
            for (auto &ref : radioTunerRx->radioTasks)
            {
                radioTunerRx->getBus().receive(GATAS::RadioControlMsg{
                    Radio::RadioParameters{CountryRegulations2::timings2[0].radioConfig, CountryRegulations2::Europe.baseFrequency, 0}, ref.radioNo});
            }
            radioTunerRx->eventSync.set(BIT_EVENT_DONE);
        }

        if (!taskBlock)
        {
            // When notifyValue == 0, it means the timeout happened and thus our slot happaned
            if (notifyValue == 0)
            {
                // Should be under normal conditions around every 5 seconds
                // 5 ticks per seconds * 5 = 25 ticks for 5 seconds
                if (++priorityCounter > 25)
                {
                    performPriority = true;
                }

                // Add 50ms to the current time to ensure we are wel within the current slot,
                // this avoids timings issues where CoreUtils::msInSecond()  might report a few ms to eurly including the -5ms
                auto currentSlot = ((CoreUtils::msInSecond() + 50) / 200) % 5;
                for (auto &ref : radioTunerRx->radioTasks)
                {
                    // First prioritize the datasources, this will ensure that the datasources are set correctly, but could be empty if teh zoen changed
                    if (performPriority)
                    {
                        ref.prioritizeDatasources();
                        priorityCounter = 0;
                    }

                    if (ref.protocolTimings.empty())
                    {
                        // No protocol timings available skip everything
                        continue;
                    }

                    ref.protocolTimingIdx = (ref.protocolTimingIdx + 1) % ref.protocolTimings.size();
                    auto const timeSlot = ref.protocolTimings[ref.protocolTimingIdx];
                    etl::pair<uint8_t, CountryRegulations2::Channel> current = etl::make_pair(timeSlot->ptsId, timeSlot->timing[currentSlot]);

                    if (current != ref.lastConfig && timeSlot->timing[currentSlot] != CountryRegulations2::Channel::NOP)
                    {
                        // Calculate delay to next slot
                        auto frequency = CountryRegulations2::getFrequency(timeSlot->frequency, timeSlot->timing[currentSlot]);
                        radioTunerRx->getBus().receive(GATAS::RadioControlMsg{
                            Radio::RadioParameters{timeSlot->radioConfig, frequency, timeSlot->frequency.powerdBm}, ref.radioNo});

                        ref.statistics.taskActivity++;
                        //                        printf("RadioTunerRx: %d, protocol: %s slot %d, ms:%d fr:%ld\n", ref.radioNo, GATAS::dataSourceToString(timeSlot->radioConfig.dataSource), currentSlot, CoreUtils::msInSecond(), frequency);

                        ref.lastConfig = current;
                    }
                }
                performPriority = false;

                // Calculate delay to next slot
                nextDelay = CoreUtils::msDelayToReference((currentSlot + 1) * 200 - 5, CoreUtils::msInSecond()); // Takes about 5ms to set, so we set the next delay to 5ms before the next slot starts
            }
        }
    }
}

// ******************** Message bus receive handlers ********************

void RadioTunerRx::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    static auto lastTime = 0;
    // Update ZONE every 30 seconds, or when still at ZONE0
    // Does not require to often since this module requiresZONE information
    if (static_cast<uint8_t>(currentZone.value()) == static_cast<uint8_t>(CountryRegulations::Zone::ZONE0) || CoreUtils::isUsReached(lastTime))
    {
        lastTime = CoreUtils::timeUs32() + UPDATE_ZONE_REGULATION_EVERY;
        currentZone.set(CountryRegulations2::zone(msg.position.lat, msg.position.lon));
    }
}

void RadioTunerRx::on_receive(const GATAS::AircraftPositionMsg &msg)
{
    slotReceive[(uint8_t)msg.position.dataSource]++;
}

void RadioTunerRx::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::CONFIG)
    {
        const auto gaTasConfiguration = msg.config.gaTasConfig();
        assignDataSources(gaTasConfiguration.protocols);
    }
}

void RadioTunerRx::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void RadioTunerRx::assignDataSources(const etl::ivector<GATAS::DataSource> &dataSources)
{
    eventSync.clear(BIT_EVENT_DONE);
    xTaskNotify(taskHandle, TaskState::BLOCK, eSetBits);

    // Expetced is 200ms per tick, we wait 10 times as long, much much longer 
    // We 'should' never end up here??
    if (!eventSync.wait(BIT_EVENT_DONE, pdMS_TO_TICKS(200 * 20))) 
    {
        return;
    }

    const size_t itemsPerRadio = dataSources.size() / radioTasks.size();
    auto it = dataSources.cbegin();
    for (uint8_t i = 0; i < radioTasks.size(); ++i)
    {
        auto &taskCtx = radioTasks[i];
        taskCtx.dataSources.clear();

        if (i == radioTasks.size() - 1)
        {
            taskCtx.dataSources.assign(it, dataSources.cend()); // take remaining
        }
        else
        {
            taskCtx.dataSources.assign(it, it + itemsPerRadio);
            it += itemsPerRadio;
        }

        taskCtx.prioritizeDatasources();
    }

    xTaskNotify(taskHandle, TaskState::UNBLOCK, eSetBits);
}
