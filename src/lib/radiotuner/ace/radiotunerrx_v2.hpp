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

#include "countryregulations2.hpp"

#include "etl/map.h"
#include "etl/message_bus.h"
#include "etl/vector.h"
#include "etl/array.h"
#include "etl/algorithm.h"
#include "etl/string.h"
#include "etl/bitset.h"
#include "etl/circular_iterator.h"
#include "etl/utility.h"

#include "timers.h"

// When set, also set's the PICO's rtc

/**
 * This class is responsible for controlling the radio's
 */
/**
 * Note: Currently using array's indexed on datasource to give fast lookups on statistics but will cost a bit more memory
 * The downside is that only one protocol can be configured on one radio so listening on the same protocol on two radios would not be possible
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
    // Each radio will get one task Context to handle
    struct RadioProtocolCtx
    {
        mutable struct
        {
            uint32_t taskActivity = 0;
        } statistics;

        using OnAirTimings = etl::vector<const CountryRegulations2::ProtocolTimeSlot2 *, (static_cast<uint8_t>(GATAS::DataSource::_TRANSPROTOCOLS) + MAX_EXTRA_SLOTS)>;

        OnAirTimings protocolTimings;
        uint8_t protocolTimingIdx = 0; // Index of the current timing in the timings vector

        // Pointers needed to control the radio
        RadioTunerRx *controller;
        uint8_t radioNo;

        etl::pair<uint8_t, CountryRegulations2::Channel> lastConfig = etl::make_pair(0, CountryRegulations2::Channel::NOP); // Last configured channel and protocol ID

        // The DataSources this radio will handle
        etl::vector<GATAS::DataSource, GATAS_MAX_SOURCE_PER_RADIO> dataSources;

        SlotReceive slotReceive = {};

        // Constructor
        RadioProtocolCtx(RadioTunerRx *controller_, uint8_t radioNo_) : controller(controller_), radioNo(radioNo_)
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
                stream << "\"" << GATAS::dataSourceToString((*it)->radioConfig.dataSource) << "\"";
                if (etl::next(it) != protocolTimings.end())
                {
                    stream << ",";
                }
            }
            stream << "]";
            stream << ",\"taskActivityRadio_" << radioNo << "\":" << statistics.taskActivity;
        }

        /**
         * @brief Add all datasources to the timing array, can only be called from the task or when blocked
         * TODO: handle prioritization of datasources based on slotReceive, for now just everything is added once
         */
        void prioritizeDatasources()
        {
            CountryRegulations2::Zone zone = controller->currentZone.value();
            protocolTimings.clear();

            auto extraSlotCnt = 0;
            for (const auto &ds : dataSources)
            {
                const auto &slot = CountryRegulations2::getSlot(zone, ds);
                // Don't add datasources that are NONE because do don't transmit anything nor receive
                if (slot.radioConfig.dataSource == GATAS::DataSource::NONE)
                {
                    continue;
                }
                // Add the slot to the dataSourceTimeSlots
                if (protocolTimings.full())
                {
#ifndef NDEBUG
                    printf("RadioTunerRx: Cannot add more slots for radio %d, max slots reached", radioNo);
#endif
                    continue;
                }
                protocolTimings.push_back(&slot);

                // When specific data is received, add it to extra receive slots
                // If we have just oen slot of just one protocol, it does not really matter if we add it or not
                if (controller->slotReceive[static_cast<uint8_t>(ds)] > 0 && extraSlotCnt < MAX_EXTRA_SLOTS)
                {
                    protocolTimings.push_back(&slot);
                    extraSlotCnt++;
                }
                controller->slotReceive[static_cast<uint8_t>(ds)] = 0;
            }

            protocolTimingIdx = 0;
            // Re-initialize the circular iterator after modifying the dataSourceTimeSlots
            // upcomingDataSource = CircularDataSourceIterator{timings.begin(), timings.end()};
        }
    };

    // Keep track if there is any traffic on the datasources
    SlotReceive slotReceive;

    // Keep track of one task per each radio
    etl::vector<RadioProtocolCtx, GATAS_MAX_RADIOS> radioTasks;

    enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        BLOCK = 1 << 2,    // Block all processing
        UNBLOCK = 1 << 3,  // UnBlock all processing
    };

private:
    friend class message_router;

    // Current zone we are flying in
    Property<CountryRegulations2::Zone> currentZone;
    EventSync eventSync;
    TaskHandle_t taskHandle;
    uint8_t numRadios = 0;

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
                                                                         currentZone(CountryRegulations2::Zone::ZONE0)
    {
        (void)config;
    }
    virtual ~RadioTunerRx() = default;

    virtual GATAS::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void stop() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

private:
    /**
     * @brief Assign the datasources to each radio. THis should only be used and called after startup, or when aircraft is changed
     *
     * @param datasources
     */
    void assignDataSources(const etl::ivector<GATAS::DataSource> &datasources);
};
