#pragma once

/* System. */
#include <stdint.h>

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "timers.h"

/* OpenACE. */
#include "ace/constants.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/tcpclient.hpp"

/**
 * Dump1090 Client for development purposes transform a NMEA String into a ADSB Message
 */
class Dump1090Client : public BaseModule,
    public etl::message_router<Dump1090Client>
{
    struct
    {
        uint32_t totalReceived = 0;
    } statistics;

    etl::imessage_bus *bus;

    TimerHandle_t timerHandle;
    uint8_t stoppedCounter;
    BinaryReceiver *receiver;
    TaskHandle_t taskHandle;
    TcpClient<24> tcpClient;
public:
    static constexpr const char *NAME = "Dump1090Client";

    //  TcpClient::CallBackFunction::create([this](const char *sentence)
    //                                                                                                                         {
    //             statistics.totalReceived++;
    //               getBus().receive(OpenAce::ADSBMessage{sentence}); })

    Dump1090Client(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
        timerHandle(nullptr),
        stoppedCounter(0),
        receiver(nullptr),
        taskHandle(nullptr),
        tcpClient(config.ipPortBypath(NAME), TcpClient<24>::CallBackFunction::create<Dump1090Client, &Dump1090Client::processNewSentence>(*this))
    {
    }

    virtual ~Dump1090Client() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    void processNewSentence(const char *sentence)
    {
        // Fast detection of msgType 17 and hexStrToByteArray to reduce resources
        if (sentence != nullptr && sentence[1] == '8' && (sentence[2] == 'D' || sentence[2] == 'A' || sentence[2] == '0'))
        {
            uint8_t hexSize = strlen(sentence) - 2;
            if (hexSize == 30) // 30 happens to be the size of message we are interested in
            {
                //puts(sentence);
                OpenAce::ADSBMessageBin msg;
                hexStrToByteArray(sentence + 1, (hexSize - 2), msg.data.data());
                receiver->receiveBinary(msg.data.data(), msg.data.size());
                statistics.totalReceived++;
            }
        }
    }

    static void dump1090Task(void *arg);
};