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
#include "etl/algorithm.h"
#include "etl/utility.h"

#include "timers.h"

/**
 * This class is responsible for timimgs of sending location data for each configured protocol
 */
class RadioTunerTx : public BaseModule, public etl::message_router<RadioTunerTx, GATAS::OwnshipPositionMsg, GATAS::ConfigUpdatedMsg, GATAS::RadioControlMsg>
{
    struct DataSourceTxEvent
    {
        GATAS::DataSource dataSource;
        uint32_t atTime;
    };
    enum TaskState : uint32_t
    {
        BLOCK = 1 << 2,   // Block all processing
        UNBLOCK = 1 << 3, // UnBlock all processing
    };

public:
    static constexpr const uint32_t UPDATE_ZONE_REGULATION_EVERY = 30'000'000; // Get new regulatory dataset every XXms
    static constexpr const uint32_t BIT_EVENT_DONE = 1 << 0;

private:
    // Each radio will get one task Context to handle
    struct
    {
        uint32_t taskActivity = 0;
    } statistics;

    void getData(etl::string_stream &stream) const
    {
        (void)stream; // Avoid unused parameter warning
    }

private:
    friend class message_router;

    // Current zone we are flying in
    CountryRegulations::Zone currentZone;
    etl::array<uint8_t, static_cast<uint8_t>(GATAS::DataSource::_TRANSPROTOCOLS)> dataSourceToRadio = {};
    static constexpr size_t MaxQueueSize = static_cast<size_t>(GATAS::DataSource::_TRANSPROTOCOLS);
    etl::vector<DataSourceTxEvent, MaxQueueSize> dataSources;

    EventSync eventSync;
    TaskHandle_t taskHandle;
    uint8_t numRadios = 0;

private:
    static void radioTuneTaskTrampoline(void *arg);
    void radioTuneTask();

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
