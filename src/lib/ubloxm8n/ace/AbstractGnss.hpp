#pragma once

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "etl/message_router.h"
#include "etl/message_bus.h"
#include "etl/string_view.h"

#include "ace/constants.hpp"
#include "ace/messages.hpp"
#include "ace/pioserial.hpp"

#include "etl/queue_spsc_atomic.h"

class AbstractGnss : public BaseModule
{
private:
    static constexpr uint8_t QUEUE_SIZE = 6;
    static constexpr uint32_t REQUIRED_GPS_BAUDRATE = 115200; // If you change this, you need to change the baudrate in the ublox config as well

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

    static void receiveTask(void *arg);

    PioSerial pioSerial;
    uint8_t ppsPin;
    TaskHandle_t taskHandle;
    etl::queue_spsc_atomic<OpenAce::NMEAString, QUEUE_SIZE, etl::memory_model::MEMORY_MODEL_SMALL> queue;

protected:
    PioSerial &getSerial() {
        return pioSerial;
    }
    void processNewSentence(const etl::array_view<char>& sentence);


    /** What a implementation needs to override */
    void setStatus(const etl::string_view &status) { 
        statistics.status.assign(status.cbegin(), status.cend());
    }

    void setStatusBaud(uint32_t baud) {
        statistics.baudrate = baud;
    }

    /*
    This method can be overwritten to cleanup teh sebtence if needed
    */
    virtual bool preProcessSentence(OpenAce::NMEAString &sentence) {
        (void)sentence;
        return true;
    }

    virtual bool configureGnss() = 0;



public:
    AbstractGnss(etl::imessage_bus& bus, const etl::string_view name, const OpenAce::PinTypeMap& pins) :
        BaseModule(bus, name),
        pioSerial{pins, REQUIRED_GPS_BAUDRATE, PioSerial::CallBackFunction::create<AbstractGnss, &AbstractGnss::processNewSentence>(*this)},
        ppsPin(pins.at(OpenAce::PinType::BUSY)),
        taskHandle(nullptr)
    {
    }

    virtual ~AbstractGnss() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;


};


