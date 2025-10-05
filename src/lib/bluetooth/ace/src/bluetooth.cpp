#include <stdio.h>

#include "../bluetooth.hpp"
#include "ace/semaphoreguard.hpp"
#include "ace/recursiveguard.hpp"
#include "ace/measure.hpp"
#include "ace/coreutils.hpp"
#include "ace/cobs.hpp"
#include "ace/binarymessages.hpp"
#include "gatas_gatt.h"

#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "pico/btstack_flash_bank.h"

#include "hci_event_builder.h"
#include "hci_dump_embedded_stdout.h"

#include "etl/algorithm.h"
#include "etl/set.h"
#include "etl/bit_stream.h"
#include <etl/algorithm.h>

#define RFCOMM_SERVER_CHANNEL 1

GATAS::PostConstruct Bluetooth::postConstruct()
{
    if (mutex != nullptr)
    {
        return GATAS::PostConstruct::MEMORY;
    }

    mutex = xSemaphoreCreateRecursiveMutex();
    if (mutex == nullptr)
    {
        return GATAS::PostConstruct::MUTEX_ERROR;
    }

    return GATAS::PostConstruct::OK;
}

void Bluetooth::stop()
{
    // TODO: proper stop BT stack
    getBus().unsubscribe(*this);
    gap_discoverable_control(0);
    gap_connectable_control(0);
    gap_advertisements_enable(0);
    hci_disconnect_all();

    hci_power_control(HCI_POWER_OFF);
    vSemaphoreDelete(mutex);
    mutex = nullptr;
};

void Bluetooth::eraseBonding()
{
    gap_delete_all_link_keys();
    printf("Bonding data erased successfully.\n");
}

void Bluetooth::start()
{
    l2cap_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, nullptr, attWriteCallback);

    // setup GATT Client
    gatt_client_init();

    // RFCOMM
    if (rfComm)
    {
        rfcomm_init();
        rfcomm_register_service(rfcommPacketHandler, RFCOMM_SERVER_CHANNEL, 0xffff);

        // init SDP, create record for SPP (Serial Port Profile) and register with SDP
        sdp_init();
        memset(Bluetooth::spp_service_buffer, 0, sizeof(Bluetooth::spp_service_buffer));
        spp_create_sdp_record(Bluetooth::spp_service_buffer, sdp_create_service_record_handle(), RFCOMM_SERVER_CHANNEL, localName.c_str());
        btstack_assert(de_get_len(Bluetooth::spp_service_buffer) <= sizeof(Bluetooth::spp_service_buffer));
        sdp_register_service(Bluetooth::spp_service_buffer);
        // RFCOMM
    }

    // setup advertisements
    uint16_t adv_int_min = 6;  // 0x0030; change dto 6/12 for possible fix Android very quick disconnect
    uint16_t adv_int_max = 12; // 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(advertiseData.size(), advertiseData.data());
    gap_advertisements_enable(1);

    gap_set_local_name(localName.c_str());

    Bluetooth::smEventCallback.callback = &smPacketHandler;
    sm_add_event_handler(&smEventCallback);

    // set one-shot btstack timer
    Bluetooth::heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, 250);
    btstack_run_loop_add_timer(&heartbeat);

    // Configuration

    // Enable mandatory authentication for GATT Client
    // - if un-encrypted connections are not supported, e.g. when connecting to own device, this enforces authentication
    // gatt_client_set_required_security_level(LEVEL_2);

    // LE Legacy Pairing, Just Works
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(0 | SM_AUTHREQ_BONDING);

    // register for ATT events
    att_server_register_packet_handler(attPacketHandler);

    // register for HCI events
    hciEventCallback.callback = &hciPacketHandler;
    hci_add_event_handler(&hciEventCallback);

    // Initialize HCI dump to log HCI packets to stdout via printf
#if defined(ENABLE_LOG_INFO) || defined(ENABLE_LOG_DEBUG) || defined(ENABLE_LOG_ERROR)
    hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif
    hci_power_control(HCI_POWER_ON);

    //    eraseBonding();
    getBus().subscribe(*this);
};

void Bluetooth::getData(etl::string_stream &stream, const etl::string_view path) const
{
    if (auto guard = SemaphoreGuard<10>(Bluetooth::mutex))
    {
        (void)path;
        stream << "{";
        stream << "\"connections\":" << connections.size();
        stream << ",\"dataPortMsgMissedErr\":" << statistics.dataPortMsgMissedErr;
        stream << ",\"cobsErr\":" << statistics.cobsErr;
        stream << ",\"ctxBufferOverrunErr\":[";
        for (auto &it : Bluetooth::connections)
        {
            stream << it.bufferOverrunErr << ",";
        }
        stream << "-1],\"mtu\":[";
        for (auto &it : Bluetooth::connections)
        {
            stream << it.mtu << ",";
        }
        stream << "-1]}\n"; // Adding minus one to ensure the array is valid
    }
}

void Bluetooth::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void Bluetooth::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition.store(msg.position, etl::memory_order_release);
}

void Bluetooth::createAdvData()
{
    // clang-format off
    static const uint8_t serviceUUID[16] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xe0, 0xff, 0x00, 0x00};
    // clang-format on

    advertiseData.clear();
    advertiseData.push_back(2); // length
    advertiseData.push_back(BLUETOOTH_DATA_TYPE_FLAGS);
    // https://tinyurl.com/yvvw6avx
    // bit 0 LE Limited Discoverable Mode
    // bit 1 LE General Discoverable Mode
    // bit 2 LE BR/EDR Not Supported. Bit 37 of LMP Feature Mask Definitions (Page 0)
    // bit 3 LE Simultaneous LE and BR/EDR to Same Device Capable (Controller). Bit 49 of LMP Feature Mask Definitions (Page 0)
    // bit 4 Previously Used
    advertiseData.push_back(rfComm ? 0x02 : 0x06);

    uint8_t maxSize = etl::min((size_t)8, localName.size());
    advertiseData.push_back(static_cast<uint8_t>(1 + maxSize)); // length = type + name length
    advertiseData.push_back(BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME);
    advertiseData.insert(advertiseData.end(), localName.begin(), localName.begin() + maxSize);

    advertiseData.push_back(1 + sizeof(serviceUUID));
    advertiseData.push_back(BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS);
    advertiseData.insert(advertiseData.end(), std::begin(serviceUUID), std::end(serviceUUID));
}

/**
 * Receive dataport messages and send it to all clients
 * Only process data if there are any actual clients and when all client
 * buffers are empty, send a trigger to the BT thread to process the data.
 */
void Bluetooth::on_receive(const GATAS::DataPortMsg &msg)
{
    if (auto guard = SemaphoreGuard<10>(Bluetooth::mutex))
    {
        for (auto &ctx : Bluetooth::connections)
        {
            if (!ctx.buffer.setString(msg.sentence))
            {
                ctx.bufferOverrunErr += 1;
            }
        }
    }
    else
    {
        statistics.dataPortMsgMissedErr += 1;
    }
}

bool Bluetooth::createConnection(hci_con_handle_t handle, uint16_t mtu, uint8_t readyState)
{
    if (auto guard = SemaphoreGuard<10000, true>(Bluetooth::mutex))
    {
        if (!Bluetooth::connections.full())
        {
            Bluetooth::connections.emplace_back(handle, mtu, readyState, &Bluetooth::attContextCallback);
            return true;
        }
    }
    return false;
}

void Bluetooth::removeConnection(uint16_t hciHandle)
{
    if (auto guard = SemaphoreGuard<10000, true>(Bluetooth::mutex))
    {
        Bluetooth::connections.remove_if([hciHandle](const BtContext &ctx)
                                         { return ctx.hciHandle == hciHandle; });
    }
}

/**
 * The heartbeat handler will be called from within an BT Task, so there is nu need to place a mutex around it?
 */
void Bluetooth::heartbeat_handler(struct btstack_timer_source *ts)
{
    (void)ts;
    uint32_t delay = 2000; // make delay based if there is a connection or not, to better use resources when BlueTooth is not used
    for (auto &btContext : Bluetooth::connections)
    {
        delay = 250;
        if (btContext.buffer.used() > MINIMUM_BLE_PACKET_SIZE)
        {
            if (btContext.hciHandle)
            {
                att_server_request_to_send_notification(&btContext.callBack, btContext.hciHandle);
            }
            else if (btContext.rfcommChannelId)
            {
                rfcomm_request_can_send_now_event(btContext.rfcommChannelId);
            }
        }
    }
    btstack_run_loop_set_timer(&heartbeat, delay);
    btstack_run_loop_add_timer(&heartbeat);
}

void Bluetooth::attContextCallback(void *context)
{
    auto btContext = static_cast<Bluetooth::BtContext *>(context);

    // When calling att_server_request_to_send_notification, it might be it will directly call back om
    // this functions, when that happens we only allow 8 recursive calls else there will be stack overflow issues.
    auto rGuard = RecursiveGuard<8>(btContext->guardCounter, "RecursiveGuard: attContextCallback");
    if (!rGuard)
    {
        return;
    }

    etl::span<uint8_t> data;
    if (auto guard = SemaphoreGuard<10, true>(Bluetooth::mutex))
    {
        btContext->buffer.read(data, btContext->mtu);
    }

    if (data.size() > 0)
    {
        uint8_t sendStatus = !ERROR_CODE_SUCCESS;
        if (btContext->hciHandle && (btContext->readyState & ATT_READYSTATE) == ATT_READYSTATE)
        {
            sendStatus = att_server_notify(btContext->hciHandle, btContext->attrHandle, data.data(), data.size());
        }
        else if (btContext->rfcommChannelId && (btContext->readyState & RFCOM_READYSTATE) == RFCOM_READYSTATE)
        {
            sendStatus = rfcomm_send(btContext->rfcommChannelId, data.data(), data.size());
        }

        size_t used=0;
        if (auto guard = SemaphoreGuard<1000, true>(Bluetooth::mutex))
        {
            btContext->buffer.compact();
            used = btContext->buffer.used();
        }

        if (sendStatus == ERROR_CODE_SUCCESS && used > MINIMUM_BLE_PACKET_SIZE)
        {
            if ((btContext->readyState & ATT_READYSTATE) == ATT_READYSTATE)
            {
                att_server_request_to_send_notification(&btContext->callBack, btContext->hciHandle);
            }
            else if ((btContext->readyState & RFCOM_READYSTATE) == RFCOM_READYSTATE)
            {
                rfcomm_request_can_send_now_event(btContext->rfcommChannelId);
            }
        }

#if GATAS_DEBUG == 1
        if (sendStatus != ERROR_CODE_SUCCESS)
        {
            puts("Bluetooth: Send Failed");
        }
#endif
    }
}

/*
 * @section HCI Packet Handler
 *
 * @text The packet handler is used to handle new connections, can trigger Security Request
 */
void Bluetooth::attPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    switch (packet_type)
    {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet))
        {
        case ATT_EVENT_CONNECTED:
        {
            auto handle = att_event_connected_get_handle(packet);
            auto mtu = att_server_get_mtu(handle) - 4;
            if (createConnection(handle, mtu, 0b010))
            {
                printf("ATT_EVENT_CONNECTED Handle:%d MTU:%d\n", handle, mtu);
                // Only re-advertise when it's possible to accept new connections
                if (!Bluetooth::connections.full())
                {
                    hci_send_cmd(&hci_le_set_advertise_enable, 1);
                }
            }
            else
            {
                gap_disconnect(handle);
            }
        }
        break;
        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
        {

            // clang-format off
            // printf("ATT_EVENT_MTU_EXCHANGE_COMPLETE Handle:%d MTU:%d\n", att_event_mtu_exchange_complete_get_handle(packet), att_event_mtu_exchange_complete_get_MTU(packet));
            Bluetooth::withHandle(att_event_mtu_exchange_complete_get_handle(packet), 
                etl::delegate<void(BtContext &)>::create([packet](BtContext &ctx)
                {
                    // We remove minus 16 because of additional header data that needs to fit
                    // This is different from what I was reading in the documentation, but this worked for us.
                    ctx.mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 16; 
                })
            );
            // clang-format on
        }
        break;

        case ATT_EVENT_DISCONNECTED:
        {
            // puts("ATT_EVENT_DISCONNECTED");
            Bluetooth::removeConnection(att_event_disconnected_get_handle(packet));
            gap_disconnect(att_event_disconnected_get_handle(packet));
        }
        break;

        default:
            break;
        }
        break;
    default:
        break;
    }
}

void Bluetooth::hciPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    switch (hci_event_packet_get_type(packet))
    {
    case HCI_EVENT_META_GAP:
        switch (hci_event_gap_meta_get_subevent_code(packet))
        {
        case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
        {
            hci_con_handle_t con_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
            // request min con interval 15 ms for iOS 11+
            gap_request_connection_parameter_update(con_handle, 12, 12, 4, 0x0048);
            break;
        }
        default:
            break;
        }
        break;
    default:
        break;
    }
}

/*
 * @section HCI Packet Handler
 *
 * @text The packet handler is used to handle new connections, can trigger Security Request
 */
void Bluetooth::rfcommPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    switch (packet_type)
    {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet))
        {
        case RFCOMM_EVENT_INCOMING_CONNECTION:
        {
            auto channelId = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
            if (createConnection(channelId, 24, 0b100))
            {
                rfcomm_accept_connection(channelId);
            }
            else
            {
                rfcomm_decline_connection(channelId);
            }
        }
        break;

        case RFCOMM_EVENT_CHANNEL_OPENED:
        {
            auto channelId = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
            if (rfcomm_event_channel_opened_get_status(packet))
            {
                Bluetooth::removeConnection(channelId);
            }
            else
            {
                // clang-format off
                Bluetooth::withHandle(channelId,
                    etl::delegate<void(BtContext &)>::create([packet](BtContext &ctx)
                    {
                        auto rfcomm_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        ctx.mtu = static_cast<uint16_t>(rfcomm_mtu);
                        ctx.readyState |= 0b001;
                        rfcomm_request_can_send_now_event(ctx.rfcommChannelId);
                    })
                );
                // clang-format on
            }
        }
        break;

        case RFCOMM_EVENT_CAN_SEND_NOW:
        {
            // sendPackage(rfcomm_event_can_send_now_get_rfcomm_cid(packet));
        }
        break;

        case RFCOMM_EVENT_CHANNEL_CLOSED:
        {
            Bluetooth::removeConnection(rfcomm_event_channel_closed_get_rfcomm_cid(packet));
        }
        break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

int Bluetooth::attWriteCallback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    UNUSED(offset);

    if (transaction_mode != ATT_TRANSACTION_MODE_NONE || transaction_mode == ATT_TRANSACTION_MODE_CANCEL)
    {
        return 0;
    }
    switch (att_handle)
    {
    case ATT_CHARACTERISTIC_0000ffe1_0000_1000_8000_00805f9b34fb_01_CLIENT_CONFIGURATION_HANDLE:
    {
        // clang-format off
        Bluetooth::withHandle(con_handle, 
            etl::delegate<void(BtContext &)>::create([buffer](BtContext &ctx)
            {
                // puts("ATT_CHARACTERISTIC_0000ffe1_0000_1000_8000_00805f9b34fb_01_CLIENT_CONFIGURATION_HANDLE");
                ctx.readyState |= little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION ? Bluetooth::CONN_READY : 0b000;
                ctx.attrHandle = ATT_CHARACTERISTIC_0000ffe1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE;
            }
        ));
        // clang-format on
    }
    break;
    case ATT_CHARACTERISTIC_0000ffe1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    {
        // Expected to receives COBS from bluetooth
        // TODO: Create a bluetooth channel for this?
        auto ownship = Bluetooth::instance->ownshipPosition.load(etl::memory_order_acquire);
        Bluetooth::instance->cobsStreamHandler.handle(ownship.lat, ownship.lon, etl::span<uint8_t>(buffer, buffer_size));
    }
    break;
    default:
        break;
    }
    return 0;
}

uint16_t Bluetooth::attReadCallback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    UNUSED(connection_handle);
    if (att_handle == ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_VALUE_HANDLE)
    {
        puts("Read requested");
        return att_read_callback_handle_blob((const uint8_t *)Bluetooth::instance->localName.c_str(), Bluetooth::instance->localName.size(), offset, buffer, buffer_size);
    }
    return 0;
}

/*
 * @section Security Manager Packet Handler
 *
 * @text The packet handler is used to handle Security Manager events
 */

/* LISTING_START(packetHandler): Security Manager Packet Handler */
void Bluetooth::smPacketHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    bd_addr_t addr;
    bd_addr_type_t addr_type;

    switch (hci_event_packet_get_type(packet))
    {
    case SM_EVENT_JUST_WORKS_REQUEST:
        printf("Just Works requested\n");
        sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        break;
    case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
        printf("Confirming numeric comparison: %lu\n", sm_event_numeric_comparison_request_get_passkey(packet));
        sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
        break;
    case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
        printf("Display Passkey: %lu\n", sm_event_passkey_display_number_get_passkey(packet));
        break;
    case SM_EVENT_IDENTITY_CREATED:
        sm_event_identity_created_get_identity_address(packet, addr);
        printf("Identity created: type %u address %s\n", sm_event_identity_created_get_identity_addr_type(packet), bd_addr_to_str(addr));
        break;
    case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
        sm_event_identity_resolving_succeeded_get_identity_address(packet, addr);
        printf("Identity resolved: type %u address %s\n", sm_event_identity_resolving_succeeded_get_identity_addr_type(packet), bd_addr_to_str(addr));
        break;
    case SM_EVENT_IDENTITY_RESOLVING_FAILED:
        sm_event_identity_created_get_address(packet, addr);
        printf("Identity resolving failed\n");
        break;
    // case SM_EVENT_PAIRING_STARTED:
    //     printf("Pairing started\n");
    //     break;
    // case SM_EVENT_PAIRING_COMPLETE:
    //     switch (sm_event_pairing_complete_get_status(packet))
    //     {
    //     case ERROR_CODE_SUCCESS:
    //         printf("Pairing complete, success\n");
    //         break;
    //     case ERROR_CODE_CONNECTION_TIMEOUT:
    //         printf("Pairing failed, timeout\n");
    //         break;
    //     case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
    //         printf("Pairing failed, disconnected\n");
    //         break;
    //     case ERROR_CODE_AUTHENTICATION_FAILURE:
    //         printf("Pairing failed, authentication failure with reason = %u\n", sm_event_pairing_complete_get_reason(packet));
    //         break;
    //     default:
    //         break;
    //     }
    //     break;
    case SM_EVENT_REENCRYPTION_STARTED:
        sm_event_reencryption_complete_get_address(packet, addr);
        printf("Bonding information exists for addr type %u, identity addr %s -> re-encryption started\n",
               sm_event_reencryption_started_get_addr_type(packet), bd_addr_to_str(addr));
        break;
    case SM_EVENT_REENCRYPTION_COMPLETE:
        switch (sm_event_reencryption_complete_get_status(packet))
        {
        // case ERROR_CODE_SUCCESS:
        //     printf("Re-encryption complete, success\n");
        //     break;
        // case ERROR_CODE_CONNECTION_TIMEOUT:
        //     printf("Re-encryption failed, timeout\n");
        //     break;
        // case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
        //     printf("Re-encryption failed, disconnected\n");
        //     break;
        case ERROR_CODE_PIN_OR_KEY_MISSING:
            printf("Re-encryption failed, bonding information missing\n\n");
            printf("Assuming remote lost bonding information\n");
            printf("Deleting local bonding information to allow for new pairing...\n");
            sm_event_reencryption_complete_get_address(packet, addr);
            addr_type = (bd_addr_type_t)(sm_event_reencryption_started_get_addr_type(packet));
            gap_delete_bonding(addr_type, addr);
            break;
        default:
            break;
        }
        break;
    // case GATT_EVENT_QUERY_COMPLETE:
    // {
    //     auto status = gatt_event_query_complete_get_att_status(packet);
    //     switch (status)
    //     {
    //     case ATT_ERROR_INSUFFICIENT_ENCRYPTION:
    //         printf("GATT Query failed, Insufficient Encryption\n");
    //         break;
    //     case ATT_ERROR_INSUFFICIENT_AUTHENTICATION:
    //         printf("GATT Query failed, Insufficient Authentication\n");
    //         break;
    //     case ATT_ERROR_BONDING_INFORMATION_MISSING:
    //         printf("GATT Query failed, Bonding Information Missing\n");
    //         break;
    //     case ATT_ERROR_SUCCESS:
    //         printf("GATT Query successful\n");
    //         break;
    //     default:
    //         printf("GATT Query failed, status 0x%02x\n", gatt_event_query_complete_get_att_status(packet));
    //         break;
    //     }
    // }
    // break;
    default:
        break;
    }
}
