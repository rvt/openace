#pragma once

#include <stdint.h>

#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/coreutils.hpp"
#include "ace/models.hpp"
#include "ace/eventsync.hpp"
#include "ace/property.hpp"

#include "countryregulations_v2.hpp"

#include "etl/message_bus.h"
#include "etl/algorithm.h"
#include "etl/utility.h"
#include "etl/priority_queue.h"

#include "timers.h"

/**
 * @brief Holds a queue of timings for all enabled protocols.
 * The queue is ordered bu which datasource needs the first TX to be handled.
 * Due to teh design, not a priritoy queue was choosen but a set
 * 
 */
class DataSourceTxQueue {
public:
    struct DataSourceTxEvent {
        const CountryRegulations::ProtocolTimeSlot* timeSlot;
        uint32_t atTime;
        GATAS::DataSource dataSource;

        // Reverse for min-heap: earlier atTime = higher priority
        bool operator<(const DataSourceTxEvent& other) const {
            return atTime > other.atTime;
        }
    };

private:
    static constexpr size_t MaxQueueSize = static_cast<size_t>(GATAS::DataSource::_TRANSPROTOCOLS);

    using EventQueue = etl::priority_queue<DataSourceTxEvent, MaxQueueSize>;

    EventQueue queue;

    void addDatasource(CountryRegulations::Zone zone, GATAS::DataSource ds) {
        const auto& slot = CountryRegulations::getSlot(zone, ds);
        uint32_t atTime = CountryRegulations::nextRandomTime(slot);
        queue.push({&slot, atTime, ds});
    }

public:
    void initTimings(CountryRegulations::Zone zone, const etl::span<GATAS::DataSource>& dataSources) {
        queue.clear();
        for (auto& ds : dataSources) 
        {
            if(!queue.full()) {
                addDatasource(zone, ds);
            }
        }
    }

    etl::optional<DataSourceTxEvent> peek() {
        if (queue.empty()) 
        {
            return {};
        }
        return queue.top();
    }

    // Reschedules this Datasource in the given zone
    void reschedule(CountryRegulations::Zone zone) {
        if (queue.empty())
        {
            return;
        }

        auto top = queue.top();
        queue.pop();
        addDatasource(zone, top.dataSource);
    }
};



/**
 * This class is responsible for timimgs of sending location data for each configured protocol
 */
class RadioTunerTx : public BaseModule, public etl::message_router<RadioTunerTx, GATAS::OwnshipPositionMsg, GATAS::ConfigUpdatedMsg, GATAS::RadioControlMsg>
{
public:
    static constexpr const uint32_t UPDATE_ZONE_REGULATION_EVERY = 30'000'000; // Get new regulatory dataset every XXms
    static constexpr const uint32_t BIT_EVENT_DONE = 1 << 0;

private:
    // Each radio will get one task Context to handle
    struct
    {
        uint32_t taskActivity = 0;
    } statistics;

    DataSourceTxQueue dataSourceTxQueue;
    
    void getData(etl::string_stream &stream) const
    {
        (void)stream; // Avoid unused parameter warning
    }

    /**
     * @brief Add all dataSources to the timing array, can only be called from the task or when blocked
     * TODO: handle prioritization of dataSources based on slotReceive, for now just everything is added once
     */
    void updateTimings()
    {
    }

    enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        BLOCK = 1 << 2,    // Block all processing
        UNBLOCK = 1 << 3,  // UnBlock all processing
    };

private:
    friend class message_router;

    // Current zone we are flying in
    Property<CountryRegulations::Zone> currentZone;
    etl::array<uint8_t, static_cast<uint8_t>(GATAS::DataSource::_TRANSPROTOCOLS)> dataSourceToRadio;

    EventSync eventSync;
    TaskHandle_t taskHandle;
    uint8_t numRadios = 0;

private:
    static void radioTuneTask(void *arg);

    // ******************** Message bus receive handlers ********************
    // Handle receiving of zone information
    void on_receive(const GATAS::OwnshipPositionMsg &msg);
    // Handle configuration changes to protocols
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);
    void on_receive(const GATAS::RadioControlMsg &msg);
    void on_receive_unknown(const etl::imessage &msg);

public:
    static constexpr const etl::string_view NAME = "RadioTunerTx";

    RadioTunerTx(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
                                                                         currentZone(CountryRegulations::Zone::ZONE0)
    {
        (void)config;
    }
    virtual ~RadioTunerTx() = default;

    virtual GATAS::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

private:
    /**
     * @brief Assign the dataSources to each radio. THis should only be used and called after startup, or when aircraft is changed
     *
     * @param dataSources
     */
    void assignDataSources(const etl::span<GATAS::DataSource> &dataSources);
};
