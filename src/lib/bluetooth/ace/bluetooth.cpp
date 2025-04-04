#include <stdio.h>

#include "bluetooth.hpp"
#include "ace/semaphoreguard.hpp"
#include "ace/measure.hpp"
#include "ace/coreutils.hpp"
#include "openace_gatt.h"

#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "pico/btstack_flash_bank.h"

#include "hci_event_builder.h"
#include "hci_dump_embedded_stdout.h"

#include "etl/algorithm.h"
#include "etl/set.h"

#define RFCOMM_SERVER_CHANNEL 1

extern const char *OpenAce_buildTime;

OpenAce::PostConstruct Bluetooth::postConstruct()
{
    if (mutex != nullptr)
    {
        return OpenAce::PostConstruct::MEMORY;
    }

    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr)
    {
        return OpenAce::PostConstruct::MUTEX_ERROR;
    }

    return OpenAce::PostConstruct::OK;
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
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    if (rfComm)
    {
        gap_advertisements_set_data(sizeof(rfCommAdvData), (uint8_t *)rfCommAdvData);
    }
    else
    {
        gap_advertisements_set_data(sizeof(leAdvData), (uint8_t *)leAdvData);
    }
    gap_advertisements_enable(1);

    gap_set_local_name(localName.c_str());

    Bluetooth::smEventCallback.callback = &smPacketHandler;
    sm_add_event_handler(&smEventCallback);

    // set one-shot btstack timer
    Bluetooth::heartbeat.process = &heartbeat_handler;
    Bluetooth::heartbeat.context = nullptr;
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

    // Initialize HCI dump to log HCI packets to stdout via printf
#if defined(ENABLE_LOG_INFO) || defined(ENABLE_LOG_DEBUG) || defined(ENABLE_LOG_ERROR)
    hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif
    hci_power_control(HCI_POWER_ON);

    getBus().subscribe(*this);
};

void Bluetooth::getData(etl::string_stream &stream, const etl::string_view path) const
{
    if (auto guard = SemaphoreGuard<5>(mutex))
    {
        (void)path;
        stream << "{";
        stream << "\"connections\":" << connections.size();
        stream << ",\"dataPortMsgMissedErr\":" << statistics.dataPortMsgMissedErr;
        stream << ",\"ctxBufferOverrunErr\":[";
        for (auto &it : Bluetooth::connections)
        {
            stream << it.bufferOverrunErr << ",";
        }
        stream << "-1]}\n"; // Adding minus one to ensure the array is valid
    }
}

void Bluetooth::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

/**
 * Starts and stop LWiP when wifi connects or disconnects to cleanup any resources
 */
void Bluetooth::on_receive(const OpenAce::IdleMsg &msg)
{
    (void)msg;
}

/**
 * Receive dataport messages and send it to all clients
 * Only process data if there are any actual clients and when all client
 * buffers are empty, send a trigger to the BT thread to process the data.
 */
void Bluetooth::on_receive(const OpenAce::DataPortMsg &msg)
{
    if (auto guard = SemaphoreGuard<5>(mutex))
    {
        for (auto &ctx : Bluetooth::connections)
        {
            if (!ctx.buffer.push(msg.sentence.c_str(), msg.sentence.size()))
            {
                ctx.bufferOverrunErr++;
            }
        }
    }
    else
    {
        statistics.dataPortMsgMissedErr++;
    }
}

bool Bluetooth::createConnection(hci_con_handle_t handle, uint16_t mtu, uint8_t readyState)
{
    if (auto guard = SemaphoreGuard<portMAX_DELAY>(Bluetooth::mutex))
    {
        if (!Bluetooth::connections.full())
        {
            btstack_context_callback_registration_t callBack;
            callBack.callback = &Bluetooth::attContextCallback;
            Bluetooth::connections.emplace_back(handle, mtu, readyState, callBack);
            return true;
        }
    }
    return false;
}

void Bluetooth::removeConnection(uint16_t handle)
{
    if (auto guard = SemaphoreGuard<portMAX_DELAY>(Bluetooth::mutex))
    {
        Bluetooth::connections.remove_if([handle](const BtContext &ctx)
                                         { return ctx.handle == handle; });
    }
}

void Bluetooth::heartbeat_handler(struct btstack_timer_source *ts)
{
    (void)ts;

    notifyAttServer();

    btstack_run_loop_set_timer(ts, 50);
    btstack_run_loop_add_timer(ts);
}

/**
 * Moved data from the queue into the buffers of each connection
 * Note: a bit of a hack on arg, when arg is null, it was called via btstack_run_loop_execute_on_main_thread.
 * When not null it came from sendNotify. However, from 'sendNotify' it's not good to call att_server_request_can_send_now_event
 * this is to reduce the number of calls to btstack_run_loop_execute_on_main_thread and att_server_request_can_send_now_event.
 * att_server_request_can_send_now_event will only be called when requiresNotification, eg, the buffers where exhausted
 * and thus the stack willbe informed via btstack_run_loop_execute_on_main_thread to continue sending data.
 */
void Bluetooth::notifyAttServer()
{
    for (auto &btContext : Bluetooth::connections)
    {
        bool hasData = false;
        
        // Check if there's data in the buffer
        if (auto guard = SemaphoreGuard<5>(mutex))
        {
            hasData = !btContext.buffer.empty();
        }
        
        // Only request notification if we have data to send or we're already flagged for notification
        if (hasData || btContext.requiresNotification) 
        {
            bool requestSuccess = false;
            
            if (btContext.handle)
            {
                requestSuccess = (att_server_request_to_send_notification(&btContext.callBack, btContext.handle) == ERROR_CODE_SUCCESS);
            }
            else if (btContext.rfcommChannelId)
            {
                rfcomm_request_can_send_now_event(btContext.rfcommChannelId);
                requestSuccess = true;
            }
            
            // Only mark as not requiring notification if request was successful
            btContext.requiresNotification = !requestSuccess;
        }
    }
}

void Bluetooth::attContextCallback(void *context)
{
    auto btContext = static_cast<Bluetooth::BtContext *>(context);
    
    // Try to send as much data as possible up to the MTU
    const char *part = nullptr;
    size_t sendLength = 0;
    bool bufferHasMoreData = false;

    if (auto guard = SemaphoreGuard<10>(mutex))
    {
        auto [peekPart, peekLength] = btContext->buffer.peek();
        part = peekPart;
        
        // Ensure we don't exceed the MTU and we actually have data
        if (peekPart && peekLength > 0) {
            sendLength = static_cast<uint8_t>(etl::min(peekLength, static_cast<size_t>(btContext->mtu)));
            bufferHasMoreData = (peekLength > sendLength);
        }
    }

    if (sendLength > 0)
    {
        bool sendSuccess = false;
        
        if (btContext->handle && (btContext->readyState & ATT_READYSTATE) == ATT_READYSTATE)
        {
            sendSuccess = (att_server_notify(btContext->handle, btContext->attrHandle, 
                           reinterpret_cast<const uint8_t *>(part), sendLength) == ERROR_CODE_SUCCESS);
        }
        else if (btContext->rfcommChannelId && (btContext->readyState & RFCOM_READYSTATE) == RFCOM_READYSTATE)
        {
            sendSuccess = (rfcomm_send(btContext->rfcommChannelId, (uint8_t *)part, sendLength) == ERROR_CODE_SUCCESS);
        }

        if (sendSuccess)
        {
            // Only update the buffer if send was successful
            if (auto guard = SemaphoreGuard<10>(mutex))
            {
                btContext->buffer.accepted(sendLength);
                bufferHasMoreData = !btContext->buffer.empty();
            }

            // Immediately request to send more if we have more data
            if (bufferHasMoreData)
            {
                btContext->requiresNotification = false;
                
                if ((btContext->readyState & ATT_READYSTATE) == ATT_READYSTATE)
                {
                    if (att_server_request_to_send_notification(&btContext->callBack, btContext->handle) != ERROR_CODE_SUCCESS)
                    {
                        btContext->requiresNotification = true;
                    }
                }
                else if ((btContext->readyState & RFCOM_READYSTATE) == RFCOM_READYSTATE)
                {
                    rfcomm_request_can_send_now_event(btContext->rfcommChannelId);
                }
                return;
            }
        }
    }
    
    // If we reach here, either we couldn't send, had nothing to send, or emptied the buffer
    btContext->requiresNotification = true;
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
    UNUSED(buffer_size);

    if (transaction_mode != ATT_TRANSACTION_MODE_NONE)
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
        for (auto i = 0; i < buffer_size; i++)
        {
            putchar(buffer[i]);
        }
        break;
    default:
        break;
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
    //    uint8_t status;

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
    //     status = gatt_event_query_complete_get_att_status(packet);
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
    //     break;
    default:
        break;
    }
}