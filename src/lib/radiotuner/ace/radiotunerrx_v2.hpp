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
class RadioTunerRx : public BaseModule, public etl::message_router<RadioTunerRx, GATAS::OwnshipPositionMsg, GATAS::AircraftPositionMsg, GATAS::ConfigUpdatedMsg>
{
    using SlotReceive = etl::array<uint8_t, static_cast<uint8_t>(GATAS::DataSource::_ITEMS)>;

public:
    static constexpr const uint32_t UPDATE_ZONE_REGULATION_EVERY = 30'000'000; // Get new regulatory dataset every XXms
    // Offset applies to the timing so that the receiver is set on time to new frequencies and/or modes
    static constexpr const uint8_t GATAS_RX_OFFSET = 1;
    static constexpr const uint32_t BIT_EVENT_DONE = 1 << 0;
    static constexpr uint8_t MAX_EXTRA_SLOTS = 2; // Additional slots allowed for prioritisation

private:
    using TimeSlotVector = etl::vector<const CountryRegulations::ProtocolRxTimeSlot *, 3 + MAX_EXTRA_SLOTS>;

    // Each radio will get one task Context to handle
    struct RadioProtocolCtx
    {
        mutable struct
        {
            uint32_t taskActivity = 0;
        } statistics;

        RadioTunerRx *controller;
        uint8_t radioNo;
        TimeSlotVector protocolTimings = {};
        etl::circular_iterator<TimeSlotVector::const_iterator> protocolIterator;

        // Constructor
        RadioProtocolCtx(RadioTunerRx *controller_, uint8_t radioNo_) : controller(controller_), radioNo(radioNo_), protocolTimings()
        {
        }

        // Ensure we never copy
        RadioProtocolCtx(const RadioProtocolCtx &) = delete;
        RadioProtocolCtx &operator=(RadioProtocolCtx const &) = delete;

        void getData(etl::string_stream &stream) const
        {
            (void)stream; // Avoid unused parameter warning
            stream << ",\"radio_" << radioNo << "\":[";
            for (auto it = protocolTimings.begin(); it != protocolTimings.end(); ++it)
            {
                stream << "\"" << GATAS::toString((*it)->radioConfig.dataSource()) << "\"";
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
    SlotReceive slotReceive;

    // Keep track of one task per each radio
    etl::vector<RadioProtocolCtx, GATAS_MAX_RADIOS> radioCtxList={};
    // All datasources that needs to be received
    etl::vector<GATAS::DataSource, static_cast<uint8_t>(GATAS::DataSource::_TRANSPROTOCOLS)> dataSources={};

    enum TaskState : uint32_t
    {
        BLOCK = 1 << 2,   // Block all processing
        UNBLOCK = 1 << 3, // UnBlock all processing
    };

private:
    friend class message_router;

    // Current zone we are flying in
    Property<CountryRegulations::Zone> currentZone;
    EventSync eventSync;
    TaskHandle_t taskHandle;

private:
    static void radioTuneTask(void *arg);

    // ******************** Message bus receive handlers ********************
    // Handle receiving of zone information
    void on_receive(const GATAS::OwnshipPositionMsg &msg);
    // handle receiving of datasources
    void on_receive(const GATAS::AircraftPositionMsg &msg);
    // Handle configuration changes to protocols
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);
    void on_receive_unknown(const etl::imessage &msg);

public:
    static constexpr const etl::string_view NAME = "RadioTunerRx";

    RadioTunerRx(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
                                                                        currentZone(CountryRegulations::Zone::ZONE0)
    {
        dataSources = config.gaTasConfig().protocols;
    }
    virtual ~RadioTunerRx() = default;

    virtual GATAS::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

private:
    /**
     * @brief Assign the datasources to each radio. THis should only be used and called after startup, or when aircraft is changed
     *
     */
    void assignDataSources();

    /**
     * Return true igf for this protocol data was received;
     */
    bool hasReceived(GATAS::DataSource ds);

    bool blockTasks();
    void releaseTasks();
};
