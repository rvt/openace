#pragma once

#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* ETLCPP */
#include "etl/message_bus.h"
#include "etl/list.h"
#include "etl/string.h"
#include "etl/queue_spsc_atomic.h"

/* OpenAce */
#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/circularbuffer.hpp"

/* BT Stack*/
#include "btstack.h"

/**
 * Bluetooth protocol for EFB's supports BlueTooth connections
 * Both BLE and Classic are supported
 *
 * For documentation on the btstack see:
 * https://bluekitchen-gmbh.com/btstack/#
 *
 * Note for developers: btStack is single threaded in FreeRTOS. It's not needed to uses mutexes to protect the data structures.
 * The only one that requires a mutex is to see if we need a notification in on_receive(const OpenAce::DataPortMsg &msg)
 * The queue is a lock free queue so no mutex needed
 */
class Bluetooth : public BaseModule, public etl::message_router<Bluetooth, OpenAce::DataPortMsg, OpenAce::IdleMsg>
{
    static constexpr uint8_t RFCOM_READYSTATE = 0b101;
    static constexpr uint8_t ATT_READYSTATE = 0b011;
    static constexpr uint8_t CONN_READY = 0b001;

    static constexpr uint16_t CONNECTIONS_BUFFER_SIZE = 512; // TODO: Tune buffer, should be > MTU which seems to be 255
    friend class message_router;
    struct
    {
        uint32_t toManyClients = 0;
        uint32_t bufferOverrunErr = 0;
        uint16_t lastMtu = 0;
    } statistics;

private:
    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    void on_receive(const OpenAce::DataPortMsg &msg);

    void on_receive(const OpenAce::IdleMsg &msg);

    void on_receive_unknown(const etl::imessage &msg);

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    // START: methods within this block as running within the BLE task
    static void smPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void attPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void sendPackage(hci_con_handle_t handle);
    static int attWriteCallback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
    // Create a new connection in the connections list
    static bool createNewConnection(hci_con_handle_t handle, uint16_t mtu, uint8_t readyState);
    // Remove any old connections
    static void cleanupConnections();
    static void pushQueueIntoConnectionBuffers(void *arg);
    // END: methods within this block as running within the BLE task
    static void eraseBonding();

    struct BtContext
    {
        union {
            hci_con_handle_t handle;
            uint16_t rfcommChannelId;
        };
        uint8_t readyState;                 // Simple binary state machine, 0b01 = notification enabled, 0b100 = rfcomm channel opened, 0b010 = att channel open
        bool requiresNotification;
        uint16_t mtu;
        uint16_t attrHandle;                // Used for ATT connections only
        CircularBuffer<CONNECTIONS_BUFFER_SIZE> buffer; // connection private data
        BtContext(hci_con_handle_t handle_, uint16_t mtu_, uint8_t readyState_) : handle(handle_),
                                                             readyState(readyState_),
                                                             requiresNotification(true),
                                                             mtu(mtu_),
                                                             attrHandle(0) {};

        BtContext(const BtContext &) = delete;
        BtContext &operator=(const BtContext &) = delete;

        // Move constructor
        BtContext(BtContext &&other) noexcept
            : handle(other.handle),
            readyState(other.readyState),
              requiresNotification(other.requiresNotification),
              mtu(other.mtu),
              attrHandle(other.attrHandle),
              buffer(etl::move(other.buffer))
        {
        }

        // Move assignment operator
        BtContext &operator=(BtContext &&other) noexcept
        {
            if (this != &other)
            {
                handle = other.handle;
                readyState = other.readyState;
                requiresNotification = other.requiresNotification;
                mtu = other.mtu;
                attrHandle = other.attrHandle;
                buffer = etl::move(other.buffer);
            }
            return *this;
        }
    };

    // Lists of bluetooth contexts
    using BlueTooConnections = etl::vector<BtContext, OPENACE_MAX_BLUETOOTH_CONNECTIONS>;
    inline static BlueTooConnections connections;
    /**
     * Get the connections context by Bluetooth handle
     */
    static BlueTooConnections::iterator ctxByHandle(hci_con_handle_t handle)
    {
        return etl::find_if(connections.begin(), connections.end(),
                            [handle](const BtContext &ctx)
                            {
                                return ctx.handle == handle;
                            });
    }

    /**
     * Call back a provided lambda with the context of the connection if found.
     * ote; Ensure to not call any BT API's or other time consuming calls
     */
    using BtContextCallback = etl::delegate<void(BtContext &)>;
    static void withHandle(hci_con_handle_t conn_handle, BtContextCallback callback)
    {
        auto it = ctxByHandle(conn_handle);
        if (it != connections.end())
        {
            callback(*it);
        }
    }

    inline static etl::queue_spsc_atomic<OpenAce::NMEAString, 8> queue;
    // Semaphore to ensure checking data and is synchronized with the BT thread. 
    // TODO: Perhaps this can be optimized by using a atomic (allBufferEmpty), all we use it for is to validate if all buffers ar empty
    inline static SemaphoreHandle_t mutex = nullptr;
    OpenAce::SsidOrPasswdStr localName;
public:
    static constexpr const char *NAME = "Bluetooth";
    Bluetooth(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME)
    {
        localName = config.strValueByPath("OpenAce", NAME, "localName");
    }

    virtual ~Bluetooth() = default;
};
