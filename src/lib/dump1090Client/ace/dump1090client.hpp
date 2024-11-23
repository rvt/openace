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
 * Note: This module is experimental
 */
class Dump1090Client : public BaseModule,
                       public etl::message_router<Dump1090Client, OpenAce::IdleMsg, OpenAce::WifiConnectionStateMsg>
{
    friend class message_router;

    struct
    {
        uint32_t totalReceived = 0;
    } statistics;

    etl::imessage_bus *bus;

    uint8_t stoppedCounter;
    bool wifiConnected;
    BinaryReceiver *receiver;
    TcpClient<24> tcpClient;

public:
    void on_receive(const OpenAce::WifiConnectionStateMsg &wcs);
    void on_receive_unknown(const etl::imessage &msg);
    void on_receive(const OpenAce::IdleMsg &msg);

public:
    static constexpr const char *NAME = "Dump1090Client";

    //  TcpClient::CallBackFunction::create([this](const char *sentence)
    //                                                                                                                         {
    //             statistics.totalReceived++;
    //               getBus().receive(OpenAce::ADSBMessage{sentence}); })

    Dump1090Client(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
                                                                          stoppedCounter(0),
                                                                          wifiConnected(false),
                                                                          receiver(nullptr),
                                                                          tcpClient(config.ipPortBypath(NAME), TcpClient<24>::CallBackFunction::create<Dump1090Client, &Dump1090Client::processNewSentence>(*this))
    {
    }

    virtual ~Dump1090Client() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    void processNewSentence(const char *sentence);

};