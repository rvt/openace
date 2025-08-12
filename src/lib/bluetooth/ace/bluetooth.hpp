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
#include "etl/bit_stream.h"

/* GaTas */
#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/circularbuffer.hpp"
#include "ace/cobsstreamhandler.hpp"

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
 * The only one that requires a mutex is to see if we need a notification in on_receive(const GATAS::DataPortMsg &msg)
 * The queue is a lock free queue so no mutex needed
 */
class Bluetooth : public BaseModule, public etl::message_router<Bluetooth, GATAS::DataPortMsg>
{
    static constexpr uint8_t RFCOM_READYSTATE = 0b101;
    static constexpr uint8_t ATT_READYSTATE = 0b011;
    static constexpr uint8_t CONN_READY = 0b001;
    static constexpr uint16_t CONNECTIONS_BUFFER_SIZE = 1536; // TODO: Tune buffer, should be > MTU which is 255 bytes for BLE witj etxnded data length
    static constexpr uint8_t MINIMUM_BLE_PACKET_SIZE = 180;   // Minimum size of a BLE packet, to optimise performance

    inline static Bluetooth *instance;

    // clang-format off
    // advertisement data, MAX 31 byte!
    etl::vector<uint8_t, 31> advertiseData;

    friend class message_router;
    struct
    {
        uint32_t dataPortMsgMissedErr=0;
        uint32_t cobsErr= 0;
    } statistics;

    struct BtContext
    {
        union
        {
            hci_con_handle_t hciHandle;
            uint16_t rfcommChannelId;
        };
        uint8_t readyState; // Simple binary state machine, 0b01 = notification enabled, 0b100 = rfcomm channel opened, 0b010 = att channel open
        uint16_t mtu;
        uint16_t attrHandle; // Used for ATT connections only
        uint16_t bufferOverrunErr;
        uint32_t nextCheckNotification;
        btstack_context_callback_registration_t callBack;
        CircularBuffer<CONNECTIONS_BUFFER_SIZE> buffer; // connection private data
        BtContext(hci_con_handle_t hciHandle_, uint16_t mtu_, uint8_t readyState_, void (*callBack_)(void * context)) : hciHandle(hciHandle_),
                                                                                                                                     readyState(readyState_),
                                                                                                                                     mtu(mtu_),
                                                                                                                                     attrHandle(0),
                                                                                                                                     bufferOverrunErr(0),
                                                                                                                                     nextCheckNotification(0)
        {
            callBack.context = this;
            callBack.callback = callBack_;
        };

        // Disallow copy
        BtContext(const BtContext &) = delete;
        BtContext &operator=(const BtContext &) = delete;

        // Disallow move
        BtContext(BtContext &&) = delete;
        BtContext &operator=(BtContext &&) = delete;
    };

private:
    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    void on_receive(const GATAS::DataPortMsg &msg);

    void on_receive_unknown(const etl::imessage &msg);

    void createAdvData();

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    // START: methods within this block as running within the BLE task
    static void smPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void attContextCallback(void *context);
    static void attPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void rfcommPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static int attWriteCallback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
    static uint16_t attReadCallback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
    // Create a new connection in the connections list
    static bool createConnection(hci_con_handle_t handle, uint16_t mtu, uint8_t readyState);
    // Remove any old connections
    static void removeConnection(uint16_t handle);
    // END: methods within this block as running within the BLE task
    static void eraseBonding();
    static void heartbeat_handler(struct btstack_timer_source *ts);

    // Lists of bluetooth contexts
    using BluetoothConnections = etl::list<BtContext, GATAS_MAX_BLUETOOTH_CONNECTIONS>;
    inline static BluetoothConnections connections;
    /**
     * Get the connections context by Bluetooth handle
     */
    static BluetoothConnections::iterator ctxByHandle(hci_con_handle_t hciHandle)
    {
        // clang-format off
        return etl::find_if(connections.begin(), connections.end(),
            [hciHandle](const BtContext &ctx)
            {
                return ctx.hciHandle == hciHandle;
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

    inline static btstack_packet_callback_registration_t smEventCallback;
    inline static btstack_timer_source_t heartbeat;
    inline static uint8_t spp_service_buffer[100]; // SPP (Serial Port Profile) Showed as length to 91

    // Semaphore to ensure checking data and is synchronized with the BT thread.
    // TODO: Perhaps this can be optimized by using a atomic (allBufferEmpty), all we use it for is to validate if all buffers ar empty
    inline static SemaphoreHandle_t mutex = nullptr;
    GATAS::SsidOrPasswdStr localName;
    bool rfComm;
    CobsStreamHandler cobsStreamHandler;
public:

    static constexpr const char *NAME = "Bluetooth";
    Bluetooth(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME), rfComm(false), cobsStreamHandler(CobsStreamHandler(bus))
    {
        instance = this;
        localName = config.strValueByPath("GaTas", NAME, "localName");
        rfComm = config.valueByPath(0, NAME, "rfComm");
        createAdvData();
    }

    virtual ~Bluetooth() = default;
};
