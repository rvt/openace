#include <stdio.h>

#include "etl/string.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "../AbstractGnss.hpp"

#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/coreutils.hpp"
#include "ace/utils.hpp"

// Global pointert to PicoRTC so the PPS event can be called from the iterrupt
RtcModule *AbstractGnss_rtc = nullptr;

// Call RTC in interrupt to native of when last second pulse happened
void __time_critical_func(AbstractGnss_pps_callback)(uint32_t events)
{
    (void)events;
    AbstractGnss_rtc->ppsEvent();
}

GATAS::PostConstruct AbstractGnss::postConstruct()
{
    if (pioSerial.postConstruct() != GATAS::PostConstruct::OK)
    {
        return GATAS::PostConstruct::HARDWARE_ERROR;
    }

    AbstractGnss_rtc = static_cast<RtcModule *>(moduleByName(*this, RtcModule::NAME));
    if (AbstractGnss_rtc == nullptr)
    {
        return GATAS::PostConstruct::DEP_NOT_FOUND;
    }

    gpio_init(ppsPin);
    gpio_set_dir(ppsPin, GPIO_IN);
    gpio_pull_up(ppsPin);

    return GATAS::PostConstruct::OK;
}

void AbstractGnss::start()
{
    // Needs a larger stack because GPS is part of a chain of events
    xTaskCreate(receiveTask, AbstractGnss::NAME.cbegin(),
                configMINIMAL_STACK_SIZE + 768, this, tskIDLE_PRIORITY + 3, &taskHandle);

    pioSerial.start();
    registerPinInterrupt(ppsPin, GPIO_IRQ_EDGE_RISE, AbstractGnss_pps_callback);
};

void AbstractGnss::receiveTask(void *arg)
{
    AbstractGnss *abstractGnss = static_cast<AbstractGnss *>(arg);

    //    Initialise and find the GPS
    while (!abstractGnss->configureGnss())
    {
        vTaskDelay(TASK_DELAY_MS(1000));
    }

    abstractGnss->statistics.queueFullErr = 0;
    GATAS::NMEAString sentence;
    while (true)
    {
        if (uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY))
        {
            if (notifyValue & TaskState::EXIT)
            {
                vTaskDelete(nullptr);
                return;
            }

            if (notifyValue & TaskState::NEW)
            {
                while (abstractGnss->queue.pop(sentence))
                {
                    if (abstractGnss->preProcessSentence(sentence))
                    {
                        // puts(sentence.c_str());
                        abstractGnss->getBus().receive(GATAS::GPSSentenceMsg{sentence});
                        abstractGnss->statistics.totalReceived += 1;
                    }
                }
            }
        }
    }
}

void AbstractGnss::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"totalReceived\":" << statistics.totalReceived;
    stream << ",\"queueFullErr\":" << statistics.queueFullErr;
    stream << ",\"status\":\"" << statistics.status << "\"";
    stream << ",\"baudrate\":" << statistics.baudrate;
    stream << "}\n";
}

void __time_critical_func(AbstractGnss::processNewSentence)(const etl::array_view<char> &sentence)
{
    // Note: if in case this get's removed because the messages are needed,
    // then this filter needs to be added in DataPort that passes through GPS messages
    // Otherwise it create indeed traffic over TCP/IP or Bluetooth
    // "SkyDemon processes RMC, GGA, GSA and GSV, however GSV is not really used any more internally."
    // GSA : GPS DOP and active satellites
    // GSV : GPS Satellites in view

    bool isPriority = false;

    // Only check if the queue has room
    if (!queue.full())
    {
        if (sentence.size() >= 6)
        {
            // Check the message type at position 3–5
            char type[4] = {sentence[4], sentence[5], '\0'};

            if (etl::string_view(type) == "SV" || // GSV
                etl::string_view(type) == "TG" || // VTG
                etl::string_view(type) == "LL")   // GLL
            {
                // Ignore this sentence
                return;
            }

            // Check for RMC/GGA to give it more priority
            if (etl::string_view(type) == "MC" || etl::string_view(type) == "GA")
            {
                isPriority = true;
            }

            // Push the sentence
            queue.push(sentence.data());
        }
    }
    else
    {
        statistics.queueFullErr += 1;
    }

    // Notify only when queue has enough items or it's an RMC
    if (queue.size() > QUEUE_SIZE / 2 || isPriority)
    {
        xTaskNotifyFromISR(taskHandle, TaskState::NEW, eSetBits, nullptr);
    }
}
