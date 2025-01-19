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

    // clang-format off
    // advertisement data, MAX 31 byte!
    static constexpr uint8_t leAdvData[] = {
        2, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, // https://tinyurl.com/yvvw6avx
        8, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'O', 'p', 'e','n', 'A', 'c', 'e',
        17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS, 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xe0, 0xff, 0x00, 0x00,
    };
    static constexpr uint8_t rfCommAdvData[] = {
        2, BLUETOOTH_DATA_TYPE_FLAGS, 0x02, // https://tinyurl.com/yvvw6avx
        8, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'O', 'p', 'e','n', 'A', 'c', 'e',
        17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS, 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xe0, 0xff, 0x00, 0x00,
    };
    // clang-format on

    friend class message_router;
    struct
    {
        uint32_t toManyClients = 0;
        uint32_t CircularBufferOverrunErr = 0;
        uint16_t lastMtu = 0;
    } statistics;

    struct BtContext
    {
        union
        {
            hci_con_handle_t handle;
            uint16_t rfcommChannelId;
        };
        uint8_t readyState; // Simple binary state machine, 0b01 = notification enabled, 0b100 = rfcomm channel opened, 0b010 = att channel open
        bool requiresNotification;
        uint16_t mtu;
        uint16_t attrHandle; // Used for ATT connections only
        uint16_t bufferOverrunErr;
        btstack_context_callback_registration_t callBack;
        CircularBuffer<CONNECTIONS_BUFFER_SIZE> buffer; // connection private data
        BtContext(hci_con_handle_t handle_, uint16_t mtu_, uint8_t readyState_, btstack_context_callback_registration_t callBack_) : handle(handle_),
                                                                                                                                     readyState(readyState_),
                                                                                                                                     requiresNotification(true),
                                                                                                                                     mtu(mtu_),
                                                                                                                                     attrHandle(0),
                                                                                                                                     bufferOverrunErr(0),
                                                                                                                                     callBack(callBack_)
        {
            callBack.context = this;
        };

        BtContext(const BtContext &) = delete;
        BtContext &operator=(const BtContext &) = delete;

        // Move constructor
        BtContext(BtContext &&other) noexcept
            : handle(other.handle),
              readyState(other.readyState),
              requiresNotification(other.requiresNotification),
              mtu(other.mtu),
              attrHandle(other.attrHandle),
              bufferOverrunErr(other.bufferOverrunErr),
              callBack(other.callBack),
              buffer(etl::move(other.buffer))
        {
            callBack.context = this;
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
                bufferOverrunErr = other.bufferOverrunErr;
                callBack = other.callBack;
                buffer = etl::move(other.buffer);
                callBack.context = this;
            }
            return *this;
        }
    };

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
    static void attContextCallback(void *context);
    static void attPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void rfcommPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void sendNotification(Bluetooth::BtContext *context);
    static int attWriteCallback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
    // Create a new connection in the connections list
    static bool createConnection(hci_con_handle_t handle, uint16_t mtu, uint8_t readyState);
    // Remove any old connections
    static void removeConnection(uint16_t handle);
    static void pushQueueIntoConnectionBuffers(void *arg);
    // END: methods within this block as running within the BLE task
    static void eraseBonding();

    // Lists of bluetooth contexts
    using BlueTooConnections = etl::list<BtContext, OPENACE_MAX_BLUETOOTH_CONNECTIONS>;
    inline static BlueTooConnections connections;
    /**
     * Get the connections context by Bluetooth handle
     */
    static BlueTooConnections::iterator ctxByHandle(hci_con_handle_t handle)
    {
        // clang-format off
        return etl::find_if(connections.begin(), connections.end(),
            [handle](const BtContext &ctx)
            {
                return ctx.handle == handle;
            });
        // clang-format on 
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
    inline static btstack_context_callback_registration_t pushIntoQueueReg;
    inline static btstack_packet_callback_registration_t smEventCallback;
    inline static uint8_t spp_service_buffer[100]; // SPP (Serial Port Profile) Showed as length to 91

    // Semaphore to ensure checking data and is synchronized with the BT thread.
    // TODO: Perhaps this can be optimized by using a atomic (allBufferEmpty), all we use it for is to validate if all buffers ar empty
    inline static SemaphoreHandle_t mutex = nullptr;
    OpenAce::SsidOrPasswdStr localName;
    bool rfComm;

public:
    static constexpr const char *NAME = "Bluetooth";
    Bluetooth(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME), rfComm(false)
    {
        localName = config.strValueByPath("OpenAce", NAME, "localName");
        rfComm = config.valueByPath(0, NAME, "rfComm");
    }

    virtual ~Bluetooth() = default;
};
