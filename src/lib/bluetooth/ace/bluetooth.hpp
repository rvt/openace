#pragma once

#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* ETLCPP */
#include "etl/message_bus.h"
#include "etl/list.h"
#include "etl/string.h"
#include "etl/span.h"

/* GaTas */
#include "ace/constants.hpp"
#include "ace/basemodule.hpp"
#include "ace/binarymessages.hpp"
#include "ace/gulp.hpp"
#include "ace/messages.hpp"
#include "ace/packetbuffer.hpp"

/* BT Stack*/
#include "btstack.h"

/**
 * Bluetooth transport for GATAS.
 *
 * The Bluetooth module keeps the NMEA and binary COBS traffic separated into
 * two characteristics. Binary payloads are carried through GatasConnectTx /
 * GatasConnectRx while NMEA continues to use DataPortMsg.
 */
class Bluetooth : public BaseModule, public etl::message_router<Bluetooth, GATAS::DataPortMsg, GATAS::OwnshipPositionMsg, GATAS::GatasConnectTx>
{
    static constexpr uint8_t ATT_READYSTATE = 0b011;
    static constexpr uint8_t CONN_READY = 0b001;
    static constexpr uint16_t CONNECTIONS_BUFFER_SIZE = 2048; // TODO: Tune buffer, should be > MTU which is 255 bytes for BLE witj etxnded data length
    static constexpr uint8_t MINIMUM_BLE_PACKET_SIZE = 180;   // Minimum size of a BLE packet, to better use the BLE bandwith

    inline static Bluetooth *instance;

    // advertisement data, MAX 31 byte
    etl::vector<uint8_t, 31> advertiseData;

    friend class message_router;
    struct
    {
        uint32_t nmeaPortMsgMissedErr = 0;
        uint32_t cobsMsgMissedErr = 0;
        uint32_t cobsMsgReceived = 0;
        uint32_t nmeaMsgReceived = 0;
    } statistics;

    using TxBuffer = PacketBuffer<CONNECTIONS_BUFFER_SIZE, (CONNECTIONS_BUFFER_SIZE / 32) * 2>;

    struct BtContext
    {
        union
        {
            hci_con_handle_t hciHandle;
        };
        uint8_t nmeaReadyState;
        uint8_t binaryReadyState;
        uint16_t mtu;
        uint16_t nmeaAttrHandle;
        uint16_t binaryAttrHandle;
        uint16_t nmeaWriteBufferErr;
        uint16_t cobsWriteBufferErr;
        uint8_t guardCounter;
        btstack_context_callback_registration_t callBack;

        // Per-connection NMEA stream splitter; keeps partial sentences across BLE writes.
        etl::vector<uint8_t, GATAS::NMEA_MAX_LENGTH> nmeaGulpBuffer;
        Gulp nmeaGulp;
        TxBuffer nmeaWriteBuffer;

        // Per-connection COBS stream splitter; keeps partial binary frames across BLE writes.
        etl::vector<uint8_t, BinaryMessages::MAX_COBS_FRAME_SIZE> binaryGulpBuffer;
        Gulp binaryGulp;
        TxBuffer cobsWriteBuffer;

        BtContext(hci_con_handle_t hciHandle_, uint16_t mtu_, uint8_t readyState_, void (*callBack_)(void *context))
            : hciHandle(hciHandle_),
              nmeaReadyState(readyState_),
              binaryReadyState(readyState_),
              mtu(mtu_),
              nmeaAttrHandle(0),
              binaryAttrHandle(0),
              nmeaWriteBufferErr(0),
              cobsWriteBufferErr(0),
              guardCounter(0),
              nmeaGulp{nmeaGulpBuffer, DelimiterBitmap::CRLF()},
              binaryGulp{binaryGulpBuffer, DelimiterBitmap::Null()}
        {
            callBack.context = this;
            callBack.callback = callBack_;
        }

        void getData(etl::string_stream &stream, const etl::string_view path) const
        {
            (void)path;
            stream << "{";
            stream << "\"hciHandle\":" << hciHandle;
            stream << ",\"nmeaReadyState\":" << static_cast<uint32_t>(nmeaReadyState);
            stream << ",\"binaryReadyState\":" << static_cast<uint32_t>(binaryReadyState);
            stream << ",\"mtu\":" << mtu;
            stream << ",\"nmeaWriteBufferErr\":" << nmeaWriteBufferErr;
            stream << ",\"cobsWriteBufferErr\":" << cobsWriteBufferErr;
            stream << "}";
        }

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

    void on_receive(const GATAS::DataPortMsg &msg);
    void on_receive_unknown(const etl::imessage &msg);
    void on_receive(const GATAS::OwnshipPositionMsg &msg);
    void on_receive(const GATAS::GatasConnectTx &msg);

    void createAdvData();
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    // START: methods within this block as running within the BLE task
    static void smPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void attContextCallback(void *context);
    static void attPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static void hciPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
    static int attWriteCallback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
    static uint16_t attReadCallback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
    // Create a new connection in the connections list
    static bool createConnection(hci_con_handle_t handle, uint16_t mtu, uint8_t readyState);
    // Remove any old connections
    static void removeConnection(uint16_t handle);
    // END: methods within this block as running within the BLE task
    static void heartbeat_handler(struct btstack_timer_source *ts);

    // Lists of bluetooth contexts
    using BluetoothConnections = etl::list<BtContext, GATAS_MAX_BLUETOOTH_CONNECTIONS>;
    BluetoothConnections connections;

    /**
     * Get the connections context by Bluetooth handle
     */
    static BluetoothConnections::iterator ctxByHandle(hci_con_handle_t hciHandle)
    {
        // clang-format off
        return etl::find_if(instance->connections.begin(), instance->connections.end(),
                            [hciHandle](const BtContext &ctx)
                            { return ctx.hciHandle == hciHandle; });
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
        if (it != instance->connections.end())
        {
            callback(*it);
        }
    }

    btstack_packet_callback_registration_t hciEventCallback;
    btstack_packet_callback_registration_t smEventCallback;
    btstack_timer_source_t heartbeat;
    uint8_t spp_service_buffer[100]; // SPP (Serial Port Profile) Showed as length to 91
    GATAS::OwnshipMinimalPositionInfo ownshipPosition;
    GATAS::SsidOrPasswdStr localName;

    SemaphoreHandle_t mutex;

    static bool sendBuffer(BtContext &ctx, TxBuffer &buffer, uint16_t attrHandle, uint8_t readyState);

public:
    static constexpr const char *NAME = "Bluetooth";
    Bluetooth(etl::imessage_bus &bus, Configuration &config) : BaseModule(bus, NAME), mutex(nullptr)
    {
        instance = this;
        localName = config.strValueByPath("GaTas", NAME, "localName");
        createAdvData();
    }

    virtual ~Bluetooth() = default;
};
