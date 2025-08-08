#include "../radiotunertx_v2.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "../countryregulations_v2.hpp"

#include "etl/algorithm.h"

GATAS::PostConstruct RadioTunerTx::postConstruct()
{
    if (xTaskCreate(radioTuneTask, RadioTunerTx::NAME.cbegin(), configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY + 2, &taskHandle) != pdPASS)
    {
        return GATAS::PostConstruct::TASK_ERROR;
    }

    return GATAS::PostConstruct::OK;
}

void RadioTunerTx::start()
{
    getBus().subscribe(*this);

    Configuration *configuration = static_cast<Configuration *>(BaseModule::moduleByName(*this, Configuration::NAME));
    if (configuration)
    {
        auto gaTasConfiguration = configuration->gaTasConfig();
        assignDataSources(gaTasConfiguration.protocols);
    }
};

void RadioTunerTx::stop()
{
    getBus().unsubscribe(*this);
};

void RadioTunerTx::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"_dummy\": 0";
    stream << ",\"zone\":\"" << CountryRegulations::zoneToString(currentZone.value()) << "\"";
    stream << "}\n";
}

//*********************** Tuner tasks ***********************

void RadioTunerTx::radioTuneTask(void *arg)
{
    int32_t nextDelay = 200;
    RadioTunerTx *radioTunerRx = static_cast<RadioTunerTx *>(arg);
    bool taskBlock = false;
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
            // Disable the tranceivers during reconfiguration
            // For now it's always assumed that the RadioTunerRx will take care of this
            radioTunerRx->eventSync.set(BIT_EVENT_DONE);
        }

        if (!taskBlock)
        {
            // When notifyValue == 0, it means the timeout happened and thus our slot happaned
            if (notifyValue == 0)
            {
                while (true)
                {
                    const auto &opionalTiming = radioTunerRx->dataSourceTxQueue.peek();

                    if (opionalTiming.has_value())
                    {
                        const auto &timing = opionalTiming.value();
                        nextDelay = timing.atTime - CoreUtils::timeMs32();

                        if (nextDelay < 10)
                        {
                            radioTunerRx->dataSourceTxQueue.reschedule(radioTunerRx->currentZone.value());

                            auto channel = timing.timeSlot->timing[(timing.atTime % 1000) / CountryRegulations::SLOT_MS];
                            if (channel == CountryRegulations::Channel::NOOP)
                            {
                                // puts("RadioTunerTx: IS NOOP");
                                continue;
                            }
                            // printf("RadioTunerTx: ds:%s delay:%ld ms:%ld\n", GATAS::toString(timing.timeSlot->radioConfig.dataSource) , nextDelay,timing.atTime % 1000 );

                            auto frequency = CountryRegulations::getFrequency(timing.timeSlot->frequency, channel);
                            radioTunerRx->getBus().receive(
                                GATAS::RadioTxPositionRequestMsg{
                                    Radio::RadioParameters{
                                        &timing.timeSlot->radioConfig,
                                        frequency,
                                        timing.timeSlot->frequency.powerdBm},
                                    radioTunerRx->dataSourceToRadio[static_cast<uint8_t>(timing.dataSource)]});

                            continue;
                        }
                        break;
                    }
                    // No protocols
                    nextDelay = 1000;
                    break;
                }
                nextDelay -= 2; // -2 to allow for some timing inaccurasies
                // Fail safe timing to keep any error within limits
                if (nextDelay > 5000 || nextDelay < 1)
                {
                    nextDelay = 50;
                }
            }
        }
    }
}

// ******************** Message bus receive handlers ********************

void RadioTunerTx::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    static auto lastTime = 0;
    // Update ZONE every 30 seconds, or when still at ZONE0
    if (static_cast<uint8_t>(currentZone.value()) == static_cast<uint8_t>(CountryRegulations::Zone::ZONE0) || CoreUtils::isUsReached(lastTime))
    {
        lastTime = CoreUtils::timeUs32() + UPDATE_ZONE_REGULATION_EVERY;
        currentZone.set(CountryRegulations::zone(msg.position.lat, msg.position.lon));
    }
}

void RadioTunerTx::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::CONFIG)
    {
        auto gaTasConfiguration = msg.config.gaTasConfig();
        assignDataSources(gaTasConfiguration.protocols);
    }
}

void RadioTunerTx::on_receive(const GATAS::RadioControlMsg &msg)
{
    dataSourceToRadio.at(static_cast<uint8_t>(msg.radioParameters.config->dataSource)) = msg.radioNo;
}

void RadioTunerTx::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void RadioTunerTx::assignDataSources(const etl::span<GATAS::DataSource> &dataSources)
{
    eventSync.clear(BIT_EVENT_DONE);
    xTaskNotify(taskHandle, TaskState::BLOCK, eSetBits);

    // Expetced is 200ms per tick, we wait 10 times as long, much much longer
    if (!eventSync.wait(BIT_EVENT_DONE, pdMS_TO_TICKS(200 * 20)))
    {
        puts("============================== FAILED");
        return;
    }

    dataSourceTxQueue.initTimings(currentZone.value(), dataSources);

    xTaskNotify(taskHandle, TaskState::UNBLOCK, eSetBits);
}
