#include <stdio.h>

#include "etl/string.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "AbstractGnss.hpp"

#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/coreutils.hpp"
#include "ace/utils.hpp"


// TODO: For some reason casting to RTC did not work, so we use a global pointer PicoRtc, properly some casting did not go well
RtcModule *AbstractGnss_rtc = nullptr;

// Call RTC in interrupt to native of when last second pulse happened
void __time_critical_func(AbstractGnss_pps_callback)(uint32_t events)
{
    (void)events;
    AbstractGnss_rtc->ppsEvent();
    // CoreUtils::setPPS();
}

void AbstractGnss::start()
{
    // TODO: Check if there is a better way to reduce stack size.
    // This seem to have because the ublox is the beginning of messages through the complete system?
    // Can we use a reference to strings instead of copy?
    xTaskCreate(receiveTask, "AbstractGnss"
                              "Task",
                configMINIMAL_STACK_SIZE + 768, this, tskIDLE_PRIORITY + 3, &taskHandle);

    pioSerial.start();
    // ublox uses rising pulse to trigger
    // https://portal.u-blox.com/s/question/0D52p0000D35wjlCQA/how-to-minimize-serial-output-time-variance
    // Note: when we really have a GPS without PPS, perhaps we can just call AbstractGnss_rtc->ppsEvent();
    // after detecting GMC? and just 'add' a few us to compensate for incomming GPS time messages?
    registerPinInterrupt(ppsPin, GPIO_IRQ_EDGE_RISE, AbstractGnss_pps_callback);
};

void AbstractGnss::stop()
{
    pioSerial.stop();
    unregisterPinInterrupt(ppsPin);
    xTaskNotify(taskHandle, TaskState::EXIT, eSetBits);
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
    OpenAce::NMEAString sentence;
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
                    // Note: if in case this get's removed because the messages are needed,
                    // then this filter needs to be added in DataPort that passes through GPS messages
                    // Otherwise it create indeed traffic over TCP/IP or Bluetooth
                    // "SkyDemon processes RMC, GGA, GSA and GSV, however GSV is not really used any more internally."
                    // GSA : GPS DOP and active satellites
                    // GSV : GPS Satellites in view
                    if (abstractGnss->preProcessSentence(sentence)) {
                        abstractGnss->getBus().receive(OpenAce::GPSSentenceMsg{sentence});
                        abstractGnss->statistics.totalReceived++;
                    }
                }
            }
        }
    }
}

OpenAce::PostConstruct AbstractGnss::postConstruct()
{
    pioSerial.postConstruct();
    AbstractGnss_rtc = static_cast<RtcModule *>(moduleByName(*this, RtcModule::NAME));
    return OpenAce::PostConstruct::OK;
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

void __time_critical_func(AbstractGnss::processNewSentence)(const etl::array_view<char>& sentence)
{
    constexpr etl::string_view target1 = "$GPGSV";
    constexpr etl::string_view target2 = "$GPVTG";
    
    // BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (!queue.full())
    {
        // Note: if in case this get's removed because the messages are needed,
        // then this filter needs to be added in DataPort that passes through GPS messages
        // Otherwise it create indeed traffic over TCP/IP or Bluetooth
        // "SkyDemon processes RMC, GGA, GSA and GSV, however GSV is not really used any more internally."
        // GSA : GPS DOP and active satellites
        // GSV : GPS Satellites in view
        bool found = (etl::search(sentence.begin(), sentence.end(), target1.begin(), target1.end()) != sentence.end()) ||
                     (etl::search(sentence.begin(), sentence.end(), target2.begin(), target2.end()) != sentence.end());

        if (!found) {
            queue.push(sentence.data());
        }
    }
    else
    {
        statistics.queueFullErr++;
        //xTaskNotifyFromISR(taskHandle, TaskState::NEW, eSetBits, nullptr);
        //portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }

    // To reduce some FreeRTOS switches only send when queue is nearly full
    // Since this is a continues NMEA stream, this should be fine and accurate enough as
    // longs as the queue is relative small
    if (queue.size() > 3)
    {
        xTaskNotifyFromISR(taskHandle, TaskState::NEW, eSetBits, nullptr);
        //portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}