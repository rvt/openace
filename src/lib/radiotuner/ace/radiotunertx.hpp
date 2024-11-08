#pragma once

#include <stdint.h>

#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/coreutils.hpp"

#include "countryregulations.hpp"

#include "etl/message_bus.h"
#include "etl/list.h"
#include "etl/string.h"
#include "etl/functional.h"

#include "timers.h"

/**
 * This class is responsible for controlling the radio's
 */
/**
 * Note: Currently using array's indexed on datasource to give fast lookups on statistics but will cost a bit more memory
 * The downside is that only one protocol can be configured on one radio so listening on the same protocol on two radios would not be possible
 */
class RadioTunerTx : public BaseModule, public etl::message_router<RadioTunerTx, OpenAce::OwnshipPositionMsg, OpenAce::ConfigUpdatedMsg>
{
    static constexpr const size_t MAX_PROTOCOLS = 6;                      // Maximum number of datasources per radio
    static constexpr const uint32_t UPDATE_ZONE_REGULATION_EVERY = 30000; // Get new regulatory dataset every XXms

private:
    enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        TIMER = 1 << 1,
    };

    // Keep track of the last timestamp a message was send, nextTimeSet is used ot keep track if a timer was already set
    struct SendPositionCtx
    {
        mutable struct
        {
            uint32_t txRequests = 0;
            uint32_t timerMissed = 0;
        } statistics;

        OpenAce::DataSource source;
        RadioTunerTx *controller;
        uint8_t radioNo;
        TimerHandle_t timerHandle;
        TaskHandle_t taskHandle;
        uint8_t protocolTimingIdx;
        SendPositionCtx(OpenAce::DataSource _source, RadioTunerTx *_controller, uint8_t _radioNo)
            : source(_source), controller(_controller), radioNo(_radioNo), timerHandle(nullptr), taskHandle(nullptr), protocolTimingIdx(CountryRegulations::NONE_DATASOURCE.idx) {}

        // Ensure we never copy
        SendPositionCtx(const SendPositionCtx&) = delete;
        SendPositionCtx& operator=(SendPositionCtx const&) = delete;

        void getData(etl::string_stream &stream) const
        {
            stream << ",\"timerMissed_" << dataSourceToString(source) << "\":" << statistics.timerMissed;
            stream << ",\"txRequests_" << dataSourceToString(source) << "\":" << statistics.txRequests;
        }
    };

    // All tasks that run on each radio
    etl::list<SendPositionCtx, MAX_PROTOCOLS> txTasks = {};

private:
    friend class message_router;

    // Current zone we are flying in
    volatile CountryRegulations::Zone currentZone;
    uint8_t numRadios;

private:
    static void timerTxCallback(TimerHandle_t xTimer);
    static void radioTxTask(void *arg);

    void on_receive(const OpenAce::ConfigUpdatedMsg &msg);
    void on_receive(const OpenAce::OwnshipPositionMsg &msg);
    void on_receive_unknown(const etl::imessage &msg);
    void enableDisableDatasources(const etl::ivector<OpenAce::DataSource> &datasources);

public:
    static constexpr const etl::string_view NAME = "RadioTunerTx";
    RadioTunerTx(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
        currentZone(CountryRegulations::Zone::ZONE0),
        numRadios(0)
    {
        (void)config;
    }
    virtual ~RadioTunerTx() = default;

    virtual OpenAce::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void stop() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    /*
    * Returns the datasources for a specific radio
    */
    etl::vector<OpenAce::DataSource, MAX_PROTOCOLS> datasourcesOnRadio(uint8_t radioNo) const;
};
