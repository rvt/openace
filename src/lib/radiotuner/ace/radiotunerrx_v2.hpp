#pragma once

#include <stdint.h>

#include "ace/constants.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/coreutils.hpp"
#include "ace/models.hpp"
#include "ace/eventsync.hpp"
#include "ace/property.hpp"

#include "countryregulations_v2.hpp"

#include "etl/message_bus.h"
#include "etl/vector.h"
#include "etl/array.h"
#include "etl/algorithm.h"
#include "etl/string.h"
#include "etl/utility.h"

#include "timers.h"

/**
 * This class is responsible for setting up timings to receive data for each protocol
 */
class RadioTunerRx : public BaseModule, public etl::message_router<RadioTunerRx, GATAS::OwnshipPositionMsg, GATAS::IngressAircraftPositionMsg, GATAS::ConfigUpdatedMsg>
{
    using SlotReceive = etl::array<uint8_t, static_cast<uint8_t>(GATAS::DataSource::_RADIO)>;
    using AvailableTimingsSpan = etl::span<const CountryRegulations::ProtocolRxTimeSlot *>;

public:
    static constexpr const uint32_t UPDATE_ZONE_REGULATION_EVERY = 30'000'000; // Get new regulatory dataset every XXms
    static constexpr const uint8_t GATAS_RX_OFFSET = 1;
    static constexpr const uint32_t BIT_EVENT_DONE = 1 << 0;
    // Max pre-expanded RxTiming entries per radio (5 protocols × up to 3 segments each, rounded up)
    static constexpr uint8_t MAX_RX_TIMINGS = 16;

    // A single, non-wrapping listening window for one radio
    struct RxTiming
    {
        const CountryRegulations::ProtocolRxTimeSlot *protocolTimeConfig = nullptr;
        CountryRegulations::Channel channel=CountryRegulations::Channel::NOOP;
        uint16_t start = 0;
        uint16_t end = 0;
    };

private:
    using TimeSlotVector = etl::vector<RxTiming, MAX_RX_TIMINGS>;

    // Each radio will get one task Context to handle
    struct RadioProtocolCtx
    {
        mutable struct
        {
            uint32_t taskActivity = 0;
        } statistics;

        RadioTunerRx *controller;
        uint8_t radioNo;

        //
        TimeSlotVector protocolTimings = {};
        size_t currentProtocolIdx = std::numeric_limits<size_t>::max();
        uint32_t maxTimeMs;

        // Constructor
        RadioProtocolCtx(RadioTunerRx *controller_, uint8_t radioNo_) : controller(controller_), radioNo(radioNo_), protocolTimings()
        {
            clear();
        }

        // Ensure we never copy
        RadioProtocolCtx(const RadioProtocolCtx &) = delete;
        RadioProtocolCtx &operator=(RadioProtocolCtx const &) = delete;

        void clear() {
            protocolTimings.clear();
            currentProtocolIdx = std::numeric_limits<size_t>::max();
            maxTimeMs = 0;
        }

        void getData(etl::string_stream &stream) const
        {
            stream << ",\"radio_" << radioNo << ":rrx\":[";
            for (auto it = protocolTimings.begin(); it != protocolTimings.end(); ++it)
            {
                const auto &ts = *it;
                const char *dsName = (ts.protocolTimeConfig != nullptr) ? GATAS::toString(ts.protocolTimeConfig->radioConfig.dataSource()) : "NOOP";
                stream << "{\"ds\":\"" << dsName << "\",\"s\":" << ts.start << ",\"e\":" << ts.end << ",\"ch\":" << static_cast<uint8_t>(ts.channel) << "}";
                if (etl::next(it) != protocolTimings.end())
                {
                    stream << ",";
                }
            }
            stream << "]";
            stream << ",\"taskActivityRadio_" << radioNo << ":k\":" << statistics.taskActivity;
        }
    };

    // Keep track if there is any traffic on the datasources
    SlotReceive slotReceive={};

    // Keep track of one task per each radio
    etl::vector<RadioProtocolCtx, GATAS_MAX_RADIOS> radioCtxList = {};
    // All datasources that needs to be received
    etl::vector<GATAS::DataSourceConfig, static_cast<uint8_t>(GATAS::DataSource::_TRANSPROTOCOLS)> configuredDatasources = {};

    enum TaskState : uint32_t
    {
        BLOCK = 1 << 2,   // Block all processing
        UNBLOCK = 1 << 3, // UnBlock all processing
    };

private:
    friend class message_router;

    // Current zone we are flying in
    Property<CountryRegulations::Zone> currentZone;
    EventSync eventSync={};
    TaskHandle_t taskHandle=nullptr;

private:
    static void radioTuneTask(void *arg);

    // ******************** Message bus receive handlers ********************
    // Handle receiving of zone information
    void on_receive(const GATAS::OwnshipPositionMsg &msg);
    // handle receiving of datasources
    void on_receive(const GATAS::IngressAircraftPositionMsg &msg);
    // Handle configuration changes to protocols
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);
    void on_receive_unknown(const etl::imessage &msg);

public:
    static constexpr const etl::string_view NAME = "RadioTunerRx";

    RadioTunerRx(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
                                                                        currentZone(CountryRegulations::Zone::ZONE0)
    {
        configuredDatasources = config.gaTasConfig().protocols;
        currentZone.set(CountryRegulations::Zone::ZONE0);
    }
    virtual ~RadioTunerRx() = default;

    virtual GATAS::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

private:
    /**
     * @brief Assign the datasources to each radio. Should only be called after startup or when zone/config changes.
     */
    void assignDataSources();
    bool belongsToRadio(size_t timingIndex, size_t radioIndex, size_t radioCount)
    {
        return (timingIndex % radioCount) == radioIndex;
    }

    /**
     * Return true if for this protocol data was received;
     */
    bool hasReceived(GATAS::DataSource ds);

    static bool fillNoop(RxTiming &entry, uint16_t midMs, etl::span<const CountryRegulations::ProtocolRxTimeSlot *> timings, size_t radioCount, size_t radioIdx, bool sameRadio);
    bool blockTasks();
    void releaseTasks();
};
