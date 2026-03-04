#include <stdio.h>
#include <inttypes.h>

#include "etl/string.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "../AbstractGnss.hpp"

#include "ace/basemodule.hpp"
#include "ace/coreutils.hpp"
#include "ace/bitutils.hpp"

// Global pointert to PicoRTC so the PPS event can be called from the iterrupt
RtcModule *AbstractGnss_rtc = nullptr;

#if ABSTRACT_GNSS_MEASURE_SOFTPPS_LAG == 1
static volatile uint32_t softPpsMeasureStart = 0;
#endif

// Call RTC in interrupt to native of when last second pulse happened
void __time_critical_func(AbstractGnss_pps_callback)(uint32_t events)
{
    (void)events;
    AbstractGnss_rtc->ppsEvent(0);
#if ABSTRACT_GNSS_MEASURE_SOFTPPS_LAG == 1
    softPpsMeasureStart = CoreUtils::timeUs32Raw();
#endif
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

    // Disable PPS to ensure no false detection is made when the user decides to use Software PPS
    if (!softwarebasedPPS || ABSTRACT_GNSS_MEASURE_SOFTPPS_LAG)
    {
        gpio_init(ppsPin);
        gpio_set_dir(ppsPin, GPIO_IN);
        gpio_pull_up(ppsPin);
    }

    if (softwarebasedPPS)
    {
        puts(" Software based PPS enabled");
    }

    return GATAS::PostConstruct::OK;
}

void AbstractGnss::start()
{
    // Needs a larger stack because GPS is part of a chain of events
    xTaskCreate(receiveTask, AbstractGnss::NAME.cbegin(),
                configMINIMAL_STACK_SIZE + 768, this, tskIDLE_PRIORITY + 3, &taskHandle);

    pioSerial.start();
    if (!softwarebasedPPS || ABSTRACT_GNSS_MEASURE_SOFTPPS_LAG)
    {
        registerPinInterrupt(ppsPin, GPIO_IRQ_EDGE_RISE, AbstractGnss_pps_callback);
    }
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
                        abstractGnss->getBus().receive(GATAS::GPSSentenceMsg{sentence});
                        abstractGnss->statistics.totalReceived += 1;

                        if (ABSTRACT_GNSS_MEASURE_SOFTPPS_LAG && abstractGnss->statistics.totalReceived % 10 == 0)
                        {
                            // Note: Using printf so it will show up in a release build as well
                            printf("Software PPS lags %" PRIu32 "us behind\n", abstractGnss->softPPSlagUs);
                        }
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
#if ABSTRACT_GNSS_MEASURE_SOFTPPS_LAG == 1
    stream << ",\"softPPSlagUs\":" << softPPSlagUs;
#endif    
    stream << "}";
}

void __time_critical_func(AbstractGnss::processNewSentence)(const etl::array_view<char> &sentence)
{
    // Not even enough characters for a valid sentence type ("$GPxxx...")
    if (sentence.size() < 17)
    {
        return;
    }

    // Bail early if queue can't take anything
    if (queue.full())
    {
        statistics.queueFullErr += 1;
        xTaskNotifyFromISR(taskHandle, TaskState::NEW, eSetBits, nullptr);
        return;
    }

    // Extract the NMEA sentence type letters at positions 4 and 5
    const char t0 = sentence[4];
    const char t1 = sentence[5];

    // PRIORITY CHECK
    //
    // RMC = "MC"
    // GGA = "GA"
    bool isRMC = (t0 == 'M' && t1 == 'C');
    bool isGGA = (t0 == 'G' && t1 == 'A');
    bool isPriority = (isRMC || isGGA);

    // SOFTWARE PPS
    if (softwarebasedPPS && isRMC && sentence.size() > 16)
    {

        // Detect ".00" at sentence[13..15] because RMC messages can happenmultiple times per second
        // Note: As far as I know, RMC is send always at .00 for L78B and Ublox M8N
        // Example message we want to detect: $GNRMC,183744.000,A,5000.5459,N,00400.1711,E,0.00,146.53,151125,,,D*70
        if (sentence[13] == '.' &&
            sentence[14] == '0' &&
            sentence[15] == '0')
        {
#if ABSTRACT_GNSS_MEASURE_SOFTPPS_LAG == 1
            softPPSlagUs = CoreUtils::timeUs32Raw() - softPpsMeasureStart;
#else
            AbstractGnss_rtc->ppsEvent(softPPSlagUs);
#endif
        }
    }

    queue.push(sentence.data());
    if (isPriority || queue.size() > (QUEUE_SIZE / 2))
    {
        xTaskNotifyFromISR(taskHandle, TaskState::NEW, eSetBits, nullptr);
    }
}
