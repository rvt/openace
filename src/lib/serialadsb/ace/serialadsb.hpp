#pragma once

#include <stdint.h>

#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/pioserial.hpp"

#include "etl/map.h"
#include "etl/message_bus.h"
#include "etl/queue_spsc_atomic.h"

class SerialADSB : public BaseModule, public etl::message_router<SerialADSB>
{
    static constexpr uint8_t QUEUE_SIZE = 6;

private:
    friend class message_router;
        enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        NEW = 1 << 2,
    };
    struct
    {
        uint32_t totalReceived = 0;
        uint32_t queueFullErr = 0;
    } statistics;

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    static void serialADSBTask(void *arg);

    static constexpr uint32_t SERIAL_BAUDRATE = 115200;

    PioSerial pioSerial;
    TaskHandle_t taskHandle;
    etl::queue_spsc_atomic<GATAS::ADSBString, QUEUE_SIZE, etl::memory_model::MEMORY_MODEL_SMALL> queue;

public:
    static constexpr const etl::string_view NAME = "SerialADSB";
    SerialADSB(etl::imessage_bus &bus, const GATAS::PinTypeMap &pins) : BaseModule(bus, NAME),
                                                                          pioSerial{pins, SERIAL_BAUDRATE, PioSerial::CallBackFunction::create<SerialADSB, &SerialADSB::processNewSentence>(*this)},
                                                                          taskHandle(nullptr)
    {
    }

    SerialADSB(etl::imessage_bus &bus, const Configuration &config) : SerialADSB(bus, config.pinMap(NAME))
    {
    }

    virtual ~SerialADSB() = default;

    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    void processNewSentence(const etl::array_view<char>& sentence);
};
