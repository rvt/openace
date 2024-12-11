#include <stdio.h>

#include "ubloxm8n.hpp"
#include "message_buffer.h"

#include "string.h"

#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/coreutils.hpp"
#include "ace/constants.hpp"
#include "ace/utils.hpp"

#include "etl/map.h"
#include "etl/string.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "ace/utils.hpp"

// *INDENT-OFF*
inline constexpr uint8_t UbloxM8N_baudrate[] = {
    0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00,
    0x00, 0xC2, 0x01, 0x00, 0x23, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x5E}; // CFG_PRT Baudrate 115200

inline constexpr uint8_t UbloxM8N_warmstart[] = {
    0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x64}; // CFG_RST warm start

inline constexpr uint8_t UbloxM8N_saveBBR[] = {
    0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x1B, 0xA9}; // CFG_CFG, Safe into BBR

inline constexpr uint8_t UbloxM8N_saveBBR_FLASH[] = {
    0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x1D, 0xAB};

inline constexpr uint8_t UbloxM8N_defaultCfg[] = {
    0xB5, 0x62, 0x06, 0x07, 0x14, 0x00, 0x40, 0x42, 0x0F, 0x00, 0x18, 0x73, 0x01, 0x00, // CFG_CFG, reset default
    0x01, 0x01, 0x00, 0x00, 0x34, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0xF4};

constexpr uint8_t UbloxM8N_m8nConfig_size = 13;
inline constexpr uint8_t *UbloxM8N_m8nConfig[UbloxM8N_m8nConfig_size] = {
    (uint8_t[]){12, 0xB5, 0x62, 0x06, 0x13, 0x04, 0x00, 0x1F, 0x00, 0x0F, 0x64, 0xAF, 0xCB}, // CFG_ANT Settings Enable Voltage + Short Circuit + Open Circuit

    (uint8_t[]){10, 0xB5, 0x62, 0x06, 0x06, 0x02, 0x00, 0x00, 0x00, 0x0E, 0x4A}, // CFG_DAT WSG84

    (uint8_t[]){16, 0xB5, 0x62, 0x06, 0x3E, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x10, 0x00, 0x01, 0x00, 0x00, 0x01, 0x6B, 0x50}, // CFG_GNSS GPS
    (uint8_t[]){16, 0xB5, 0x62, 0x06, 0x3E, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x01, 0x58, 0xD9}, // CFG_GNSS SBAS
    (uint8_t[]){16, 0xB5, 0x62, 0x06, 0x3E, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x08, 0x08, 0x00, 0x01, 0x00, 0x00, 0x01, 0x65, 0x30}, // CFG_GNSS Galileo
    (uint8_t[]){16, 0xB5, 0x62, 0x06, 0x3E, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x55, 0xCC}, // CFG_GNSS Beidu off
    (uint8_t[]){16, 0xB5, 0x62, 0x06, 0x3E, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x56, 0xD4}, // CFG_GNSS IMES off
    (uint8_t[]){16, 0xB5, 0x62, 0x06, 0x3E, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x57, 0xDC}, // CFG_GNSS QZSS off
    (uint8_t[]){16, 0xB5, 0x62, 0x06, 0x3E, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x06, 0x08, 0x10, 0x00, 0x01, 0x00, 0x00, 0x01, 0x71, 0x80}, // CFG_GNSS GLONASS

    (uint8_t[]){16, 0xB5, 0x62, 0x06, 0x16, 0x08, 0x00, 0x01, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0xC}, // CFG_SBAS

    (uint8_t[]){28, 0xB5, 0x62, 0x06, 0x07, 0x14, 0x00, 0x40, 0x42, 0x0F, 0x00, 0x30, 0x1B, 0x0F, 0x00, // CFG_TP Timepulse
                0x01, 0x01, 0x00, 0x00, 0x34, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x10},

    (uint8_t[]){28, 0xB5, 0x62, 0x06, 0x17, 0x14, 0x00, 0x00, 0x21, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, // CFG NMEA 2.1, MAIN talker ID GP
                0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0x0C},

    (uint8_t[]){14, 0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A} // CFG_RATE 200ms GPS time
};
// *INDENT-ON*

// TODO: For some reason casting to RTC did not work, so we use a global pointer PicoRtc, properly some casting did not go well
RtcModule *UbloxM8N_rtc = nullptr;

// Call RTC in interrupt to native of when last second pulse happened
void __time_critical_func(UbloxM8N_pps_callback)(uint32_t events)
{
    (void)events;
    UbloxM8N_rtc->ppsEvent();
}

void UbloxM8N::start()
{
    xTaskCreate(ubloxM8NTask, "UbloxM8N"
                              "Task",
                configMINIMAL_STACK_SIZE + 128, this, tskIDLE_PRIORITY + 2, &taskHandle);

    pioSerial.start();
    // ublox uses rising pulse to trigger
    // https://portal.u-blox.com/s/question/0D52p0000D35wjlCQA/how-to-minimize-serial-output-time-variance
    // Note: when we really have a GPS without PPS, perhaps we can just call UbloxM8N_rtc->ppsEvent();
    // after detecting GMC? and just 'add' a few us to compensate for incomming GPS time messages?
    registerPinInterrupt(ppsPin, GPIO_IRQ_EDGE_RISE, UbloxM8N_pps_callback);
};

void UbloxM8N::stop()
{
    pioSerial.stop();
    unregisterPinInterrupt(ppsPin);
    xTaskNotify(taskHandle, TaskState::EXIT, eSetBits);
};

void UbloxM8N::ubloxM8NTask(void *arg)
{
    UbloxM8N *ubloxM8N = static_cast<UbloxM8N *>(arg);

    // Initialise and find the GPS
    while (!ubloxM8N->detectAndConfigureGPS())
    {
        vTaskDelay(TASK_DELAY_MS(1000));
    }

    ubloxM8N->statistics.queueFullErr = 0;
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
                while (!ubloxM8N->queue.empty())
                {
                    ubloxM8N->queue.pop(sentence);
                    ubloxM8N->statistics.totalReceived++;
                    ubloxM8N->getBus().receive(OpenAce::GPSSentenceMsg{sentence});
                }
            }
        }
    }
}

bool UbloxM8N::detectAndConfigureGPS()
{
    // Initialise the GPS hardware
    statistics.status = "Search";
    uint32_t scanBaudRate = pioSerial.findBaudRate(25000);
    if (!scanBaudRate)
    {
        statistics.status = "NO GPS";
        return false;
    }
    statistics.baudrate = scanBaudRate;
    if (scanBaudRate != GPS_BAUDRATE)
    {
        statistics.status = "Found";
        // printf("GPS found at %ldBd setting to %ldBd, waiting for GPS to come back on... ", scanBaudRate, GPS_BAUDRATE);
        if (!pioSerial.enableTx(scanBaudRate))
        {
            puts("enableTx failed");
            return false;
        }

        pioSerial.sendBlocking(UbloxM8N_baudrate, sizeof(UbloxM8N_baudrate));
        pioSerial.rxFlush(100);
        pioSerial.sendBlocking(UbloxM8N_warmstart, sizeof(UbloxM8N_warmstart));
        pioSerial.rxFlush(100);

        for (uint8_t i = 0; i < 60; i++)
        {
            statistics.status = "NO GPS";
            if (pioSerial.testUartAtBaudrate(GPS_BAUDRATE, 250))
            {
                break;
            }
        }
        scanBaudRate = pioSerial.findBaudRate(25000);
        statistics.baudrate = scanBaudRate;
        if (scanBaudRate != GPS_BAUDRATE)
        {
            statistics.status = "Cfg Err";
            return false;
        }
        // Save to BBR so we don't have slow startup delays finding the uart
        // Temporary disabled to test finding of uBlox
        statistics.status = "BBR";
        pioSerial.sendBlocking(UbloxM8N_saveBBR, sizeof(UbloxM8N_saveBBR));
        vTaskDelay(50);
        pioSerial.rxFlush();
    }

    // Configure GPS
    if (!pioSerial.enableTx(scanBaudRate))
    {
        statistics.status = "Cfg err m8nCfg";
        return false;
    }
    for (uint8_t i = 0; i < UbloxM8N_m8nConfig_size; i++)
    {
        // printf("Send configuration %d\n", i);
        pioSerial.sendBlocking(&UbloxM8N_m8nConfig[i][1], UbloxM8N_m8nConfig[i][0]);
        // TODO: Wait for 'ok' reply, this requires modification in pioserial
        vTaskDelay(250);
        pioSerial.rxFlush(100);
    }

    statistics.status = "Configured";
    statistics.baudrate = scanBaudRate;
    return true;
}

OpenAce::PostConstruct UbloxM8N::postConstruct()
{
    pioSerial.postConstruct();
    UbloxM8N_rtc = static_cast<RtcModule *>(moduleByName(*this, RtcModule::NAME));
    return OpenAce::PostConstruct::OK;
}

void UbloxM8N::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"totalReceived\":" << statistics.totalReceived;
    stream << ",\"queueFullErr\":" << statistics.queueFullErr;
    stream << ",\"status\":\"" << statistics.status << "\"";
    stream << ",\"baudrate\":" << statistics.baudrate;
    stream << "}\n";
}

void __time_critical_func(UbloxM8N::processNewSentence)(const char *sentence)
{
    if (!queue.full())
    {
        queue.push(sentence);
    }
    else
    {
        statistics.queueFullErr++;
    }
    if (queue.size() > QUEUE_SIZE - 2)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(taskHandle, TaskState::NEW, eSetBits, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}