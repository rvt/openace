#pragma once

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "etl/message_router.h"
#include "etl/message_bus.h"

#include "ace/constants.hpp"
#include "ace/messages.hpp"
#include "ace/pioserial.hpp"

#include "etl/queue_spsc_atomic.h"

class UbloxM8N : public BaseModule, public etl::message_router<UbloxM8N>
{
private:
    static constexpr uint8_t QUEUE_SIZE = 6;
    friend class message_router;
    
    enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        NEW = 1 << 2,
    };
        struct
    {
        uint32_t totalReceived=0;
        uint32_t baudrate = 0;
        uint32_t queueFullErr = 0;
        etl::string<16> status;
    } statistics;


    void on_receive_unknown(const etl::imessage& msg)
    {
        (void)msg;
    }

    static void ubloxM8NTask(void *arg);

    bool detectAndConfigureGPS();

    static constexpr uint32_t GPS_BAUDRATE = 115200; // If you change this, you need to change the baudrate in the ublox config as well

    PioSerial pioSerial;
    uint8_t ppsPin;
    TaskHandle_t taskHandle;
    etl::queue_spsc_atomic<OpenAce::NMEAString, QUEUE_SIZE, etl::memory_model::MEMORY_MODEL_SMALL> queue;

public:
    static constexpr const etl::string_view NAME = "UbloxM8N";
    UbloxM8N(etl::imessage_bus& bus, const OpenAce::PinTypeMap& pins) :
        BaseModule(bus, NAME),
        pioSerial{pins, GPS_BAUDRATE, PioSerial::CallBackFunction::create<UbloxM8N, &UbloxM8N::processNewSentence>(*this)},
        ppsPin(pins.at(OpenAce::PinType::BUSY)),
        taskHandle(nullptr)
    {
    }
    UbloxM8N(etl::imessage_bus& bus, const Configuration &config)  : UbloxM8N(bus, config.pinMap(NAME))
    {

    }

    virtual ~UbloxM8N() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    void processNewSentence(const char *sentence);

};


