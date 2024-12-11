#pragma once

#include <stdint.h>

#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/pioserial.hpp"

#include "etl/map.h"
#include "etl/message_bus.h"


class SerialADSB : public BaseModule, public etl::message_router<SerialADSB>
{
private:
    friend class message_router;
    struct
    {
        uint32_t totalReceived=0;
    } statistics;

    void on_receive_unknown(const etl::imessage& msg)
    {
        (void)msg;
    }

    static void serialADSBTask(void *arg);

    static constexpr uint32_t SERIAL_BAUDRATE = 115200;

    PioSerial pioSerial;
    TaskHandle_t taskHandle;
public:
    static constexpr const etl::string_view NAME = "SerialADSB";
    SerialADSB(etl::imessage_bus& bus, const OpenAce::PinTypeMap& pins) :
        BaseModule(bus, NAME),
        pioSerial{pins, SERIAL_BAUDRATE, PioSerial::CallBackFunction::create<SerialADSB, &SerialADSB::processNewSentence>(*this)},
        taskHandle(nullptr)
    {
    }

    SerialADSB(etl::imessage_bus& bus, const Configuration &config)  : SerialADSB(bus, config.pinMap(NAME))
    {
    }

    virtual ~SerialADSB() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    void processNewSentence(const char *sentence);
};


