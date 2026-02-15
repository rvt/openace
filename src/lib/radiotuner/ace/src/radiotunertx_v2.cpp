#include "../radiotunertx_v2.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "../countryregulations_v2.hpp"

#include "etl/algorithm.h"
#include "ace/measure.hpp"

GATAS::PostConstruct RadioTunerTx::postConstruct()
{
    const auto result = CountryRegulations::validateProtocolTxTimings();
    static_assert(result == 0, "ProtocolTxTimeSlot table is INVALID");

    if (xTaskCreate(radioTuneTaskTrampoline, RadioTunerTx::NAME.cbegin(), configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY + 2, &taskHandle) != pdPASS)
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

void RadioTunerTx::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"_dummy\": 0";
    stream << ",\"zone\":\"ZONE" << static_cast<uint8_t>(currentZone) << "\"";
    stream << "}";
}

//*********************** Tuner tasks ***********************

void RadioTunerTx::radioTuneTaskTrampoline(void *arg)
{
    RadioTunerTx *radioTunerRx = static_cast<RadioTunerTx *>(arg);
    radioTunerRx->radioTuneTask();
}

void RadioTunerTx::radioTuneTask()
{
    int32_t nextDelayMs = 200;
    bool taskBlock = false;
    while (true)
    {
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(nextDelayMs));
        if (notifyValue & TaskState::UNBLOCK)
        {
            taskBlock = false;
            eventSync.set(BIT_EVENT_DONE);
        }
        if (notifyValue & TaskState::BLOCK)
        {
            taskBlock = true;
            // Disable the tranceivers during reconfiguration
            // For now it's always assumed that the RadioTunerRx will take care of this
            eventSync.set(BIT_EVENT_DONE);
        }

        if (!taskBlock)
        {
            uint32_t currentTime = CoreUtils::timeUs32();
            for (auto &&ds : dataSources)
            {
                if (CoreUtils::isUsReached(ds.atTime))
                {
                    const auto &timing = CountryRegulations::getProtocolTxTimings(CountryRegulations::Zone::ZONE1, ds.dataSource);
                    if (timing.radioConfig.mode != GATAS::Modulation::NONE)
                    {
                        auto dsId = static_cast<uint8_t>(ds.dataSource);
                        auto timeSlot = CountryRegulations::findFittingTiming(CoreUtils::msInSecond(), timing.timeSlots);
                        if (timeSlot != nullptr)
                        {
                            auto frequency = CountryRegulations::getFrequency(timing.frequency, timeSlot->channel);

                            //                            GATAS_LOG("DS: %s radio:%d\n", GATAS::toString(ds.dataSource), dataSourceToRadio[dsId]);
                            GATAS_MEASURE("Request TX", 2000);
                            getBus().receive(
                                GATAS::RadioTxPositionRequestMsg{
                                    Radio::RadioParameters{
                                        &timing.radioConfig,
                                        frequency,
                                        timing.frequency.powerdBm},
                                    dataSourceToRadio[dsId]});
                        }
                        else
                        {
                            GATAS_INFO("Warning: No timeslot for %s found %d", GATAS::toString(ds.dataSource), CoreUtils::msInSecond());
                        }

                        auto delayMs = CountryRegulations::nextRandomTxTime(timing);
                        currentTime = CoreUtils::timeUs32();
                        if (delayMs != UINT32_MAX)
                        {
                            ds.atTime = currentTime + delayMs * 1000;
                        }
                        else
                        {
                            GATAS_INFO("Warning: Next random no timing found %s\n", GATAS::toString(ds.dataSource));
                            ds.atTime = currentTime + 950'000;
                        }
                    }
                    else
                    {
                        // Try this DS again in 5 seconds when no TX was found
                        ds.atTime = currentTime + 5'000'000;
                    }
                }
            }

            // Decide the protcol that should be send next
            currentTime = CoreUtils::timeUs32();
            int32_t nextUpIn = INT32_MAX;
            for (auto &&ds : dataSources)
            {
                auto toRef = CoreUtils::usToReference(ds.atTime, currentTime);
                if (toRef < nextUpIn)
                {
                    nextUpIn = toRef;
                }
            }

            if (nextUpIn < 0)
            {
                nextDelayMs = 10;
                // This message we might occasionally see when the device starts up due to GPS time beeing set and correct
                // They don't cause any harm
                if (nextUpIn < -1000)
                {
                    GATAS_INFO("Event in the past should normally not happen %ldms", nextUpIn / 1000);
                }
            }
            else
            {
                // clap to max 500 seconds incase datasources waas empty
                nextDelayMs = etl::min(nextUpIn / 1000, static_cast<int32_t>(5000));
            }
        }
    }
}

// ******************** Message bus receive handlers ********************

void RadioTunerTx::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    static auto lastTime = CoreUtils::timeUs32Raw();
    // Update ZONE every 30 seconds, or when still at ZONE0
    if (CoreUtils::isUsReachedRaw(lastTime) || static_cast<uint8_t>(currentZone) == static_cast<uint8_t>(CountryRegulations::Zone::ZONE0))
    {
        lastTime = CoreUtils::timeUs32Raw() + UPDATE_ZONE_REGULATION_EVERY;
        currentZone = CountryRegulations::zone(msg.position.lat, msg.position.lon);
    }
}

void RadioTunerTx::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::NAME)
    {
        auto gaTasConfiguration = msg.config.gaTasConfig();
        assignDataSources(gaTasConfiguration.protocols);
    }
}

void RadioTunerTx::on_receive(const GATAS::RadioControlMsg &msg)
{
    uint8_t dsId = static_cast<uint8_t>(msg.radioParameters.config->dataSource());
    if (dsId < static_cast<uint8_t>(GATAS::DataSource::_TRANSPROTOCOLS))
    {
        dataSourceToRadio[dsId] = msg.radioNo;
    }
    else
    {
        printf("DS: %d ", dsId);
//        GATAS_ASSERT(false, "Not expected to be full");
    }
}

void RadioTunerTx::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void RadioTunerTx::assignDataSources(const etl::span<GATAS::DataSource> &newDataSources)
{
    eventSync.clear(BIT_EVENT_DONE);
    xTaskNotify(taskHandle, TaskState::BLOCK, eSetBits);

    // Expetced is 200ms per tick, we wait 10 times as long, much much longer
    if (!eventSync.wait(BIT_EVENT_DONE, pdMS_TO_TICKS(200 * 10)))
    {
        GATAS_INFO("RadioTunerTx: Failed to wait for event sync");
        return;
    }

    dataSources.clear();
    for (auto ds : newDataSources)
    {
        dataSources.emplace_back(ds, CoreUtils::timeUs32());
    }

    xTaskNotify(taskHandle, TaskState::UNBLOCK, eSetBits);
}
