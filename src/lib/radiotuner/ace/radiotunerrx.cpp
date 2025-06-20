#include "radiotunerrx.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "radiotunertx.hpp"

#include "etl/algorithm.h"
#include "etl/pseudo_moving_average.h"

#include "pico/rand.h"

GATAS::PostConstruct RadioTunerRx::postConstruct()
{
    // Ensure at least one radio exists
    if (!moduleByName(*this, Radio::NAMES[0]))
    {
        return GATAS::PostConstruct::DEP_NOT_FOUND;
    }

    // Then count any other radios
    uint8_t numRadios = 1;
    for (int i = 1; i < GATAS_MAX_RADIOS; i++)
    {
        if (moduleByName(*this, Radio::NAMES[i]))
        {
            numRadios++;
        }
    }
    addRadioTasks(numRadios);
    return GATAS::PostConstruct::OK;
}

void RadioTunerRx::start()
{
    getBus().subscribe(*this);

    Configuration *config = static_cast<Configuration *>(BaseModule::moduleByName(*this, Configuration::NAME));
    if (config)
    {
        enableDisableDatasources(config->gaTasConfig().protocols);
    }
};

void RadioTunerRx::stop()
{
    getBus().unsubscribe(*this);

    for (auto& it : radioTasks)
    {
        it.stop();
    }
    radioTasks.clear();
};

/**
 * Create a tune task per radio
 */
void RadioTunerRx::addRadioTasks(uint8_t numRadios)
{
    for (uint8_t radioNo = 0; radioNo < numRadios; radioNo++)
    {

        auto radio = static_cast<Radio *>(moduleByName(*this, Radio::NAMES[radioNo]));
        auto &ref = radioTasks.emplace_back(this, radio);

        ref.timerHandle = xTimerCreate("rxTaskTimer", TASK_DELAY_MS(1'000), pdFALSE /* Must not be autostart */, &ref, timerTuneCallback);
        if (ref.timerHandle == nullptr)
        {
            radioTasks.pop_back();
            puts("RadioTunerRx: Failed to create timer.");
            continue;
        }

        xTaskCreate(radioTuneTask, RadioTunerRx::NAME.cbegin(), configMINIMAL_STACK_SIZE + 128, &ref, tskIDLE_PRIORITY + 2, &ref.taskHandle);
        if (ref.taskHandle == nullptr)
        {
            radioTasks.pop_back();
            puts("RadioTunerRx: Failed to create task.");
            continue;
        }
    }
}

void RadioTunerRx::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"_dummy\": 0";
    for (auto it = radioTasks.cbegin(); it != radioTasks.cend(); ++it)
    {
        it->getData(stream);
    }
    stream << ",\"zone\":\"" << CountryRegulations::zoneToString(currentZone) << "\"";
    stream << "}\n";
}

//*********************** Tuner tasks ***********************

/**
 * Call back the task that handle the radio indicating that the wait period is completed and that the radio needs to change to the new datasource
 */
void RadioTunerRx::timerTuneCallback(TimerHandle_t xTimer)
{
    RadioTunerRx::RadioProtocolCtx *taskCtx = (RadioTunerRx::RadioProtocolCtx *)pvTimerGetTimerID(xTimer);
    xTaskNotify(taskCtx->taskHandle, TaskState::TIMER, eSetBits);
}

void RadioTunerRx::radioTuneTask(void *arg)
{
    constexpr uint16_t TIMEOUT_DELAY = 2'000;
    RadioTunerRx::RadioProtocolCtx *taskCtx = static_cast<RadioTunerRx::RadioProtocolCtx *>(arg);
    bool taskBlock = false;
    while (true)
    {
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(TIMEOUT_DELAY));
        if (notifyValue & TaskState::EXIT)
        {
            vTaskDelete(nullptr);
            return;
        }
        else if (notifyValue & TaskState::BLOCK)
        {
            taskBlock = true;
        }
        else if (notifyValue & TaskState::UNBLOCK)
        {
            taskBlock = false;
        }

        if (!taskBlock) {
            if (notifyValue & TaskState::TIMER)
            {
                if (taskCtx->upcomingTimeslot != CountryRegulations::NONE_DATASOURCE.idx)
                {
    
                    // printf("Set frequency to f:%ld ms:%d zone:%d source:%s\n", frequency, CoreUtils::msInSecond(), taskCtx->nextTimeSlot.zone, GATAS::dataSourceToString(radioTask->nextTimeSlot.source));
                    auto thisTimeSlot = CountryRegulations::protocolTimeslotById(taskCtx->upcomingTimeslot);
                    auto frequency = CountryRegulations::determineFrequency(thisTimeSlot);
    
                    // Send a message to the radio to indicate to switch and listen to a different protocol
                    taskCtx->controller->getBus().receive(GATAS::RadioControlMsg{
                        Radio::RadioParameters{thisTimeSlot.radioConfig, frequency, thisTimeSlot.frequency.powerdBm}, taskCtx->radio->radio()});

                    auto delay = taskCtx->advanceReceiveSlot() - GATAS_RX_OFFSET;
                    xTimerChangePeriod(taskCtx->timerHandle, TASK_DELAY_MS(delay < 1 ? 1 : delay), TASK_DELAY_MS(1));
    
                    // printf("RadioTunerRx: next: radio:%s protocol: %s Freq:%ld ms:%d delay:%d\n",
                    //        taskCtx->radio->name().cbegin(), dataSourceToString(thisTimeSlot.radioConfig.dataSource), frequency, CoreUtils::msInSecond(), delay);
    
                    taskCtx->statistics.rxRequests++;
                }
            }
            else
            {
                // Only count if it was not a NONE datasource
                if (taskCtx->upcomingTimeslot != CountryRegulations::NONE_DATASOURCE.idx)
                {
                    taskCtx->statistics.timerMissed++;
                }
    
                // Try to find a next slot
                taskCtx->advanceReceiveSlot();
                xTimerChangePeriod(taskCtx->timerHandle, TASK_DELAY_MS(900), TASK_DELAY_MS(10));
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
    if (currentZone == CountryRegulations::Zone::ZONE0 || CoreUtils::isUsReached(lastTime))
    {
        lastTime = CoreUtils::timeUs32() + 30'000'000;
        currentZone = CountryRegulations::zone(msg.position.lat, msg.position.lon);
    }
}

void RadioTunerRx::on_receive(const GATAS::AircraftPositionMsg &msg)
{
    static uint32_t lastTime = 0;
    slotReceive[(uint8_t)msg.position.dataSource]++;

    // Update the tasks at least every seconds, but not on each and every aircraft message
    // The positions are only used for statistics, not for positional information
    if (CoreUtils::isUsReached(lastTime))
    {
        lastTime = CoreUtils::timeUs32() + 1'000'000;
        for (auto &taskCtx : radioTasks)
        {
            taskCtx.updateSlotReceive(slotReceive);
        }
    }
}

void RadioTunerRx::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void RadioTunerRx::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::CONFIG)
    {
        const auto gaTasConfiguration = msg.config.gaTasConfig();
        enableDisableDatasources(gaTasConfiguration.protocols);
    }
}

void RadioTunerRx::enableDisableDatasources(const etl::ivector<GATAS::DataSource> &dataSources)
{
    // Block all tasks first
    for (auto &taskCtx : radioTasks)
    {
        xTaskNotify(taskCtx.taskHandle, TaskState::BLOCK, eSetBits);
    }

    // Precompute how many protocols each radio should handle
    uint8_t dsPerRadio = dataSources.size() / radioTasks.size();
    uint8_t remainingSources = dataSources.size() % radioTasks.size(); // Handle remainder for last radio
    uint8_t newDsPos = 0;

    for (auto &taskCtx : radioTasks)
    {

        // Distribute data sources across radios
        uint8_t sourcesForThisRadio = dsPerRadio;

        // Distribute the remainder to the last radio
        if (&taskCtx == &radioTasks.back())
        {
            sourcesForThisRadio += remainingSources;
        }

        sourcesForThisRadio = etl::min(static_cast<uint8_t>(GATAS_MAX_SOURCE_PER_RADIO), sourcesForThisRadio);
        // Copy the assigned data sources for this radio
        etl::vector<GATAS::DataSource, GATAS_MAX_SOURCE_PER_RADIO> newDataSources;
        for (uint8_t i = 0; i < sourcesForThisRadio; ++i)
        {
            newDataSources.emplace_back(dataSources[newDsPos]);
            newDsPos++;
        }

        taskCtx.updateDataSources(newDataSources);

        // Unblock the task
        xTaskNotify(taskCtx.taskHandle, TaskState::UNBLOCK, eSetBits);
    }
}
