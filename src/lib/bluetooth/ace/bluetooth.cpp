#include <stdio.h>

#include "bluetooth.hpp"
#include "ace/semaphoreguard.hpp"
#include "openace_gatt.h"

#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"

// #include "btstack_run_loop_freertos.h"
#include "etl/algorithm.h"
#include "etl/set.h"

#define RFCOMM_SERVER_CHANNEL 1

extern const char *OpenAce_buildTime;

/*
 * @section Advertisements
 *
 * @text The Flags attribute in the Advertisement Data indicates if a device is dual-mode or le-only.
 */

// clang-format off
const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    2, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    // 17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS, 0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e,
    // 0x05, BLUETOOTH_DATA_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE, 0x0c, 0x00, 0x0c, 0x00,
};

const uint8_t adv_response_data[] = {
    // Name
    8, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'O', 'p', 'e','n', 'A', 'c', 'e',
    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS, 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xe0, 0xff, 0x00, 0x00,
};
// clang-format on

static btstack_context_callback_registration_t callback_registration;
static btstack_packet_callback_registration_t hci_event_callback_registration;

// SPP
static uint8_t spp_service_buffer[150];
// #ifdef ENABLE_GATT_OVER_CLASSIC
static uint8_t gatt_service_buffer[70];
// #endif

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

    hci_power_control(HCI_POWER_OFF);
    vSemaphoreDelete(mutex);
    mutex = nullptr;
};

void Bluetooth::start()
{
    l2cap_init();

    rfcomm_init();
    rfcomm_register_service(packetHandler, RFCOMM_SERVER_CHANNEL, 0xffff);

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, sdp_create_service_record_handle(), RFCOMM_SERVER_CHANNEL, "OpenAce");
    sdp_register_service(spp_service_buffer);

    // #ifdef ENABLE_GATT_OVER_CLASSIC
    memset(gatt_service_buffer, 0, sizeof(gatt_service_buffer));
    gatt_create_sdp_record(gatt_service_buffer, sdp_create_service_record_handle(), ATT_SERVICE_GATT_SERVICE_START_HANDLE, ATT_SERVICE_GATT_SERVICE_END_HANDLE);
    sdp_register_service(gatt_service_buffer);
    // #endif

    gap_set_local_name("OpenAce"); // Should be classic, but I think this will also appear as ib BLE advertisements?
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    // https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1735400416371 2.8 Class of Device
    //    gap_set_class_of_device(0b1'0000'0001'0001'0000); // bit starts counting from 0
    gap_set_class_of_device(0);

    gap_discoverable_control(1);

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, nullptr, attWriteCallback);

    // register for HCI events
    hci_event_callback_registration.callback = &packetHandler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for ATT events
    att_server_register_packet_handler(packetHandler);

    // setup battery service
    battery_service_server_init(100);

    // setup device information service
    device_information_service_server_init();
    device_information_service_server_set_manufacturer_name("OpenAce");
    // device_information_service_server_set_model_number("model_number");
    // device_information_service_server_set_serial_number("serial_number");
    // device_information_service_server_set_hardware_revision("hardware_revision");
    // device_information_service_server_set_firmware_revision("firmware_revision");
    device_information_service_server_set_software_revision(OpenAce_buildTime);
    // device_information_service_server_set_system_id(0x01, 0x02);
    // device_information_service_server_set_ieee_regulatory_certification(0x03, 0x04);
    // device_information_service_server_set_pnp_id(0x05, 0x06, 0x07, 0x08);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_scan_response_set_data(sizeof(adv_response_data), (uint8_t *)adv_response_data);
    gap_advertisements_set_data(sizeof(adv_data), (uint8_t *)adv_data);
    gap_advertisements_enable(1);

    // Registration for cb into bluetooth task
    callback_registration.callback = &pushQueueIntoConnectionBuffers;
    callback_registration.context = nullptr;

    // hci_dump_init(hci_dump_embedded_stdout_get_instance());

    // turn on!
    hci_power_control(HCI_POWER_ON);

    getBus().subscribe(*this);
};

void Bluetooth::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"connections\":" << connections.size();
    stream << ",\"bufferOverrunErr\":" << statistics.bufferOverrunErr;
    stream << "}\n";
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
 */
void Bluetooth::on_receive(const OpenAce::DataPortMsg &msg)
{
    if (Bluetooth::connections.empty())
    {
        return;
    }

    if (queue.full())
    {
        statistics.bufferOverrunErr++;
        btstack_run_loop_execute_on_main_thread(&callback_registration);
        return;
    }
    queue.push(msg.sentence);

    // When all the buffers where empty, it can be assumed that BTstack might be stalled by detecting this
    // a trigger is send to the BT thread to call pushQueueIntoConnectionBuffers, which in return call up the right connections to send data
    bool allBufferEmpty = true;
    if (auto guard = SemaphoreGuard<portMAX_DELAY>(mutex))
    {
        for (auto &it : Bluetooth::connections)
        {
            allBufferEmpty &= it.buffer.empty();
        }
    }

    if (allBufferEmpty)
    {
        btstack_run_loop_execute_on_main_thread(&callback_registration);
    }
}

bool Bluetooth::createNewConnection(hci_con_handle_t handle, uint16_t mtu)
{
    if (auto guard = SemaphoreGuard<portMAX_DELAY>(Bluetooth::mutex))
    {
        if (!Bluetooth::connections.full())
        {
            Bluetooth::connections.emplace_back(BtContext{handle, mtu});
            return true;
        }
    }
    return false;
}

void Bluetooth::cleanupConnections()
{
    if (auto guard = SemaphoreGuard<portMAX_DELAY>(Bluetooth::mutex))
    {

        Bluetooth::connections.erase(
            etl::remove_if(
                Bluetooth::connections.begin(),
                Bluetooth::connections.end(),
                [](const BtContext &ctx)
                { return ctx.handle == HCI_CON_HANDLE_INVALID; }),
            Bluetooth::connections.end());
    }
}

/**
 * Moved data from the queue into the buffers of each connection
 * Note: a bit of a hack on arg, when arg is null, it was called via btstack_run_loop_execute_on_main_thread.
 * When not null it came from sendNotify. However, from 'sendNotify' it's not good to call att_server_request_can_send_now_event
 * this is to reduce the number of calls to btstack_run_loop_execute_on_main_thread and att_server_request_can_send_now_event.
 * att_server_request_can_send_now_event will only be called when requiresNotification, eg, the buffers where exhausted
 * and thus teh stack willbe informed via btstack_run_loop_execute_on_main_thread to continue sending data.
 */
void Bluetooth::pushQueueIntoConnectionBuffers(void *arg)
{
    (void)arg;

    while (!Bluetooth::queue.empty())
    {
        OpenAce::NMEAString sentence;
        Bluetooth::queue.pop(sentence);

        for (auto &it : Bluetooth::connections)
        {
            if (it.buffer.available() >= sentence.size())
            {
                it.buffer.push(sentence.c_str(), sentence.size());
            }
        }
    }

    if (arg == nullptr)
    {
        for (auto &it : Bluetooth::connections)
        {
            if (it.requiresNotification)
            {
                if ((it.readyState & RFCOM_READYSTATE) == RFCOM_READYSTATE)
                {
                    rfcomm_request_can_send_now_event(it.rfcommChannelId);
                }
                else if ((it.readyState & ATT_READYSTATE) == ATT_READYSTATE)
                {
                    att_server_request_can_send_now_event(it.handle);
                }
            }
        }
    }
}

void Bluetooth::packetHandler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    switch (packet_type)
    {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet))
        {
        /********************* Just Works *********************/
        // case SM_EVENT_JUST_WORKS_REQUEST:
        // {
        //     printf("Just Works requested\n");
        //     sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        // }
        // break;
        // case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
        // {
        //     printf("Confirming numeric comparison: %lu\n", sm_event_numeric_comparison_request_get_passkey(packet));
        //     sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
        // }
        // break;
        // case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
        // {
        //     printf("Display Passkey: %lu\n", sm_event_passkey_display_number_get_passkey(packet));
        // }
        // break;

        /********************* HCI *********************/
        case HCI_EVENT_PIN_CODE_REQUEST:
        {
            bd_addr_t event_address;
            // inform about pin code request
            // printf("Pin code request - using '0000'\n");
            hci_event_pin_code_request_get_bd_addr(packet, event_address);
            gap_pin_code_response(event_address, "0000");
        }
        break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        {
            // inform about user confirmation request
            // printf("SSP User Confirmation Request with numeric value '%06" PRIu32 "'\n", little_endian_read_32(packet, 8));
            // printf("SSP User Confirmation Auto accept\n");
        }
        break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
        {
            // puts("HCI_EVENT_DISCONNECTION_COMPLETE");
            cleanupConnections();
            break;
        }
        case HCI_EVENT_LE_META:
        {
            switch (hci_event_le_meta_get_subevent_code(packet))
            {
            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
            {
                auto con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                gap_request_connection_parameter_update(con_handle, 12, 12, 4, 0x0048);
            }
            break;
            }
        }
        break;

        /********************* ATT *********************/
        case ATT_EVENT_CONNECTED:
        {
            auto handle = att_event_connected_get_handle(packet);
            auto mtu = att_server_get_mtu(handle);
            if (mtu > 12)
            {
                if (!createNewConnection(handle, mtu - 12))
                {
                    gap_disconnect(handle);
                }
            }
            // Reenable advertising
            hci_send_cmd(&hci_le_set_advertise_enable, 1);
        }
        break;

        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
        {
            // clang-format off
            Bluetooth::withHandle(att_event_disconnected_get_handle(packet),
                etl::delegate<void(BtContext &)>::create([packet](BtContext &ctx)
                {
                    auto mtu = att_event_mtu_exchange_complete_get_MTU(packet);
                    if (mtu > 12)
                    {
                        ctx.mtu = static_cast<uint16_t>(mtu - 12);
                        ctx.readyState |= 0b010;
                    }
                    else
                    {
                        printf("ATT_EVENT_MTU_EXCHANGE_COMPLETE: Very small MTU detected, disconnecting\n");
                        gap_disconnect(ctx.handle);
                        ctx.handle = HCI_CON_HANDLE_INVALID;
                    }
                }
            ));
            // clang-format off
        }
        break;

        case ATT_EVENT_CAN_SEND_NOW:
        {
            for (const auto &it : Bluetooth::connections)
            {
                sendPackage(it.handle);
            }
        }
        break;

        case ATT_EVENT_DISCONNECTED:
        {
            // clang-format off
            Bluetooth::withHandle(att_event_disconnected_get_handle(packet),
                etl::delegate<void(BtContext &)>::create([](BtContext &ctx)
                { 
                    ctx.readyState = 0b000; 
                    ctx.handle = HCI_CON_HANDLE_INVALID; 
                }
            ));
            // clang-format off
        }
        break;

        /********************* RFCOMM *********************/
        case RFCOMM_EVENT_INCOMING_CONNECTION:
        {
            auto channelId = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
            if (createNewConnection(channelId, 24))
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
                // clang-format off
                Bluetooth::withHandle(channelId,
                    etl::delegate<void(BtContext &)>::create([](BtContext &ctx)
                    { 
                        ctx.handle = HCI_CON_HANDLE_INVALID; 
                    }));
                // clang-format on
            }
            else
            {
                // clang-format off
                Bluetooth::withHandle(channelId,
                    etl::delegate<void(BtContext &)>::create([packet](BtContext &ctx)
                    {
                auto rfcomm_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        ctx.mtu = static_cast<uint16_t>(rfcomm_mtu);
                        ctx.readyState |= 0b101;
                        rfcomm_request_can_send_now_event(ctx.rfcommChannelId); 
                    }
                ));
                // clang-format on
            }
        }
        break;

        case RFCOMM_EVENT_CAN_SEND_NOW:
        {
            sendPackage(rfcomm_event_can_send_now_get_rfcomm_cid(packet));
        }
        break;

        case RFCOMM_EVENT_CHANNEL_CLOSED:
        {
            // clang-format off
            Bluetooth::withHandle(rfcomm_event_channel_closed_get_rfcomm_cid(packet),
                etl::delegate<void(BtContext &)>::create([](BtContext &ctx)
                { 
                    ctx.handle = HCI_CON_HANDLE_INVALID;
                }));
            // clang-format on
        }
        break;

        default:
            break;
        }
        break;

    case RFCOMM_DATA_PACKET:
    {
        // printf("RCV: '");
        // for (int i = 0; i < size; i++)
        // {
        //     putchar(packet[i]);
        // }
        // printf("'\n");
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
                ctx.readyState |= little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION ? 0b001 : 0b000;
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

void Bluetooth::sendPackage(uint16_t handle)
{
    pushQueueIntoConnectionBuffers((void *)1);

    auto it = Bluetooth::ctxByHandle(handle);
    if (it == connections.end())
    {
        return;
    }

    auto [part, peekLength] = it->buffer.peek();
    peekLength = etl::min(peekLength, static_cast<size_t>(it->mtu));
    if (peekLength > 0)
    {
        if ((it->readyState & RFCOM_READYSTATE) == RFCOM_READYSTATE)
        {
            rfcomm_send(handle, (uint8_t *)part, peekLength);
        }
        else if ((it->readyState & ATT_READYSTATE) == ATT_READYSTATE)
        {
            att_server_notify(handle, it->attrHandle, reinterpret_cast<const uint8_t *>(part), peekLength);
        }
        it->buffer.accepted(peekLength);

        // As long as there is data in the connections buffer, immediately request for sending for more data,
        // otherwise request set requireForNotification so the request will be done as soon as new data will be available
        if (!it->buffer.empty())
        {
            if ((it->readyState & RFCOM_READYSTATE) == RFCOM_READYSTATE)
            {
                it->requiresNotification = false;
                rfcomm_request_can_send_now_event(handle);
            }
            else if ((it->readyState & ATT_READYSTATE) == ATT_READYSTATE)
            {
                it->requiresNotification = false;
                att_server_request_can_send_now_event(handle);
            }

            return;
        }
    }
    it->requiresNotification = true;
}
