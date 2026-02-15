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
#include "ace/bitcount.hpp"

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

/**
 * @brief THis was created to handle the raw Lora/GFSK frames from the radio tranceivers such that the tranceiver is quickly ofloaded.
 *        by doing this, each protocol does not have the need for each own tasks anymore and as such this will free up memory and
 *        removes a few tasks
 */
class RxDataFrameQueue : public BaseModule, public etl::message_router<RxDataFrameQueue, GATAS::DataFrameMsg>
{

private:
    friend class message_router;

    struct DataSourceMatch
    {
        GATAS::DataSource dataSource;
        uint8_t bitsToShift;
        uint8_t frameLength;
    };

    struct
    {
        uint32_t missedErr = 0;
        uint32_t totalIncoming = 0;
        uint32_t totalOutgoing = 0;
    } statistics;

    TaskHandle_t taskHandle;
    QueueHandle_t dataQueue;
    ;

    static void radioQueueTaskTrampoline(void *arg);
    void radioQueueTask(void *arg);

    // ******************** Message bus receive handlers ********************
    void on_receive(const GATAS::DataFrameMsg &msg);
    void on_receive_unknown(const etl::imessage &msg);

    static uint32_t CRCsyndrome(uint8_t Bit);
    static uint8_t FindCRCsyndrome(uint32_t Syndr);

public:
    static constexpr const etl::string_view NAME = "RxDataFrameQueue";

    RxDataFrameQueue(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME), taskHandle(nullptr), dataQueue(nullptr)
    {
        (void)config;
    }
    virtual ~RxDataFrameQueue() = default;

    virtual GATAS::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    DataSourceMatch decideDataSource(GATAS::DataSource ds, uint32_t frame[GATAS::RADIO_MAX_GFX_FRAME_WORD_LENGTH], uint8_t frameLength);
};
