#include "../radiotunertx.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "etl/algorithm.h"

#include "pico/rand.h"

GATAS::PostConstruct RadioTunerTx::postConstruct()
{
    if (!moduleByName(*this, Radio::NAMES[0])) {
        return GATAS::PostConstruct::DEP_NOT_FOUND;
    }

    numRadios = 1;
    if (moduleByName(*this, Radio::NAMES[1]))
    {
        numRadios += 1;
    }
    return GATAS::PostConstruct::OK;
}

void RadioTunerTx::start()
{
    //    addDataSourceToTasks(dataSource, radio);

    Configuration *config = static_cast<Configuration *>(BaseModule::moduleByName(*this, Configuration::NAME));
    if (config)
    {
        enableDisableDatasources(config->gaTasConfig().protocols);
    }

    getBus().subscribe(*this);
};

void RadioTunerTx::stop()
{
    getBus().unsubscribe(*this);

    // Remove all DataSources that are not required
    for (auto const &it : txTasks)
    {

        xTimerDelete(it.timerHandle, TASK_DELAY_MS(2'000));
        xTaskNotify(it.taskHandle, TaskState::EXIT, eSetBits);
        while (eTaskGetState(it.taskHandle) != eDeleted)
        {
            vTaskDelay(TASK_DELAY_MS(50));
        }
    }
    txTasks.clear();
};

void RadioTunerTx::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"protocols\":[";
    for (auto it = txTasks.cbegin(); it != txTasks.cend(); ++it)
    {
        stream << "\"" << toString(it->source) << "\"";
        if (std::next(it) != txTasks.cend())
        {
            stream << ",";
        }
    }

    stream << "]";
    for (auto it = txTasks.cbegin(); it != txTasks.cend(); ++it)
    {
        it->getData(stream);
    }
    stream << ",\"zone\":\"" << CountryRegulations::zoneToString(currentZone) << "\"";
    stream << "}\n";
}

void RadioTunerTx::timerTxCallback(TimerHandle_t xTimer)
{
    RadioTunerTx::SendPositionCtx *taskCtx = (RadioTunerTx::SendPositionCtx *)pvTimerGetTimerID(xTimer);
    xTaskNotify(taskCtx->taskHandle, TaskState::TIMER, eSetBits);
}

void RadioTunerTx::radioTxTask(void *arg)
{
    constexpr uint16_t TIMEOUT_DELAY = 3'000; // Cannot be lower than one of the timeouts from CountryRegulations
    RadioTunerTx::SendPositionCtx *taskCtx = static_cast<RadioTunerTx::SendPositionCtx *>(arg);
    // uint32_t lastTx = 0;
    while (true)
    {
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(TIMEOUT_DELAY));
        if (notifyValue & TaskState::EXIT)
        {
            vTaskDelete(nullptr);
            return;
        }
        else if (notifyValue & TaskState::TIMER)
        {

            if (taskCtx->controller->currentZone == CountryRegulations::Zone::ZONE0)
            {
                continue;
            }

            const auto &protocolTimeSlot = CountryRegulations::protocolTimeslotById(taskCtx->protocolTimingIdx);

            // When zone changed get a new IDX
            if (protocolTimeSlot.zone != taskCtx->controller->currentZone)
            {
                taskCtx->protocolTimingIdx = CountryRegulations::getFirstSlotIdx(taskCtx->controller->currentZone, taskCtx->source);
            }

            // When a valid timingIdx, process
            if (taskCtx->protocolTimingIdx != CountryRegulations::NONE_DATASOURCE.idx)
            {
                // Create a message on the message bus to request positional message for a specific protocol
                uint32_t frequency = CountryRegulations::determineFrequency(protocolTimeSlot);
                auto msInSecond = CoreUtils::msInSecond();

//                printf("RadioTunerTX: %d %s currentMs:%d sinceLast:%d\n",taskCtx->radioNo, GATAS::toString(protocolTimeSlot.radioConfig.dataSource), msInSecond, (uint16_t)(CoreUtils::msSinceEpoch()-lastTx));
//                lastTx = CoreUtils::msSinceEpoch();

                taskCtx->controller->getBus().receive(
                    GATAS::RadioTxPositionRequestMsg
                {
                    Radio::RadioParameters{
                        protocolTimeSlot.radioConfig,
                        frequency,
                        protocolTimeSlot.frequency.powerdBm},
                    taskCtx->radioNo});
                
                auto nextTxTime = CountryRegulations::getNextTxTime(msInSecond, taskCtx->protocolTimingIdx);

                taskCtx->protocolTimingIdx = nextTxTime.idx;
                xTimerChangePeriod(taskCtx->timerHandle, TASK_DELAY_MS(nextTxTime.duration), TASK_DELAY_MS(10));
                taskCtx->statistics.txRequests += 1;
            }
        }
        else
        {
            // Always restart a timer
            xTimerChangePeriod(taskCtx->timerHandle, TASK_DELAY_MS(TIMEOUT_DELAY / 2), TASK_DELAY_MS(2));
            taskCtx->statistics.timerMissed += 1;
        }
    }
}

void RadioTunerTx::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    static uint16_t checkEvery = 0;
    // TODO: Change this to some proper timing
    if (currentZone == CountryRegulations::Zone::ZONE0 || checkEvery++ > 30)
    {
        checkEvery = 0;
        currentZone = CountryRegulations::zone(msg.position.lat, msg.position.lon);
    }
}

void RadioTunerTx::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::CONFIG)
    {
        const auto gaTasConfiguration = msg.config.gaTasConfig();
        enableDisableDatasources(gaTasConfiguration.protocols);
    }
}

void RadioTunerTx::enableDisableDatasources(const etl::ivector<GATAS::DataSource> &datasources)
{
    // Remove all DataSources that are not required
    etl::array<int8_t, GATAS_MAX_RADIOS> radioOccupation={};
    radioOccupation.fill(-1);

    for (auto it = txTasks.begin(); it != txTasks.end();)
    {
        bool taskSourceInDatasource = etl::find_if(datasources.cbegin(), datasources.cend(), [&it](const GATAS::DataSource &source)
        {
            return it->source == source;
        }) != datasources.cend();

        if (!taskSourceInDatasource)
        {
            xTimerDelete(it->timerHandle, TASK_DELAY_MS(2'000));
            xTaskNotify(it->taskHandle, TaskState::EXIT, eSetBits);
            while (eTaskGetState(it->taskHandle) != eDeleted)
            {
                vTaskDelay(TASK_DELAY_MS(50));
            }
            it = txTasks.erase(it);
        }
        else
        {
            radioOccupation[it->radioNo] += 1;
            ++it;
        }
    }

    // Add all DataSources that are required, but not yet running
    for (auto dataSource : datasources)
    {
        bool isRunning = etl::find_if(txTasks.cbegin(), txTasks.cend(), [dataSource](const SendPositionCtx &ctx)
        {
            return ctx.source == dataSource;
        }) != txTasks.cend();

        if (!isRunning)
        {
            if (!txTasks.full())
            { 
                // Get the least occupied radio
                // TODO: HEre we could add an additional check based on conditions, for example two protocols not at one radio
                auto minIt = etl::min_element(radioOccupation.begin(), radioOccupation.end());
                std::size_t numRadio = etl::distance(radioOccupation.begin(), minIt);

                radioOccupation[numRadio] += 1;
                auto &ref = txTasks.emplace_back(dataSource, this, numRadio);

                ref.timerHandle = xTimerCreate("txTaskTimer", TASK_DELAY_MS(250), pdFALSE /* Must not be autostart */, &ref, timerTxCallback);
                if (ref.timerHandle == nullptr)
                {
                    txTasks.pop_back();
                    continue;
                }

                // HAd to increase this due to FANET
                xTaskCreate(radioTxTask, RadioTunerTx::NAME.cbegin(), configMINIMAL_STACK_SIZE + 512, &ref, tskIDLE_PRIORITY + 2, &ref.taskHandle);
                if (ref.taskHandle == nullptr)
                {
                    xTimerDelete(ref.timerHandle, TASK_DELAY_MS(250));
                    txTasks.pop_back();
                    continue;
                }
            }
        }
    }
}

void RadioTunerTx::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

etl::vector<GATAS::DataSource, 6> RadioTunerTx::datasourcesOnRadio(uint8_t radioNo) const {
    etl::vector<GATAS::DataSource, 6> datasources;
    for (const auto &it : txTasks) {
        if (it.radioNo == radioNo) {
            datasources.emplace_back(it.source);
        }
    }
    return datasources;
}
