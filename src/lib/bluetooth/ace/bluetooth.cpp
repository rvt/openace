#include <stdio.h>

#include "bluetooth.hpp"
#include "ace/semaphoreguard.hpp"
#include "openace_gatt.h"

#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "pico/btstack_flash_bank.h"

#include "hci_event_builder.h"

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
// advertisement data, MAX 31 byte!
const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    2, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    8, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'O', 'p', 'e','n', 'A', 'c', 'e',
    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS, 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xe0, 0xff, 0x00, 0x00,
};

// clang-format on

static btstack_context_callback_registration_t callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

// SPP
// static uint8_t spp_service_buffer[100]; // SHowed as length to 91
// #ifdef ENABLE_GATT_OVER_CLASSIC
// static uint8_t gatt_service_buffer[70]; // SHowed as length to 58
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
    att_server_init(profile_data, NULL, NULL);

    // setup GATT Client
    gatt_client_init();

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(sizeof(adv_data), (uint8_t *)adv_data);
    gap_advertisements_enable(1);

    sm_event_callback_registration.callback = &smPacketHandler;
    sm_add_event_handler(&sm_event_callback_registration);

    // Configuration

    // Enable mandatory authentication for GATT Client
    // - if un-encrypted connections are not supported, e.g. when connecting to own device, this enforces authentication
    // gatt_client_set_required_security_level(LEVEL_2);

    /**
     * Choose ONE of the following configurations
     * Bonding is disabled to allow for repeated testing. It can be enabled by or'ing
     * SM_AUTHREQ_BONDING to the authentication requirements like this:
     * sm_set_authentication_requirements( X | SM_AUTHREQ_BONDING)
     */

    // LE Legacy Pairing, Just Works
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(0 | SM_AUTHREQ_BONDING);

    // LE Legacy Pairing, Passkey entry initiator enter, responder (us) displays
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    // sm_set_authentication_requirements(SM_AUTHREQ_MITM_PROTECTION);
    // sm_use_fixed_passkey_in_display_role(123456);

#ifdef ENABLE_LE_SECURE_CONNECTIONS

    // enable LE Secure Connections Only mode - disables Legacy pairing
    // sm_set_secure_connections_only_mode(true);

    // LE Secure Connections, Just Works
    // sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION);

    // LE Secure Connections, Numeric Comparison
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_YES_NO);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION|SM_AUTHREQ_MITM_PROTECTION);

    // LE Secure Pairing, Passkey entry initiator enter, responder (us) displays
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION|SM_AUTHREQ_MITM_PROTECTION);
    // sm_use_fixed_passkey_in_display_role(123456);

    // LE Secure Pairing, Passkey entry initiator displays, responder (us) enter
    // sm_set_io_capabilities(IO_CAPABILITY_KEYBOARD_ONLY);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION|SM_AUTHREQ_MITM_PROTECTION);
#endif

    // setup ATT server
    att_server_init(profile_data, nullptr, attWriteCallback);

    // register for ATT events
    att_server_register_packet_handler(attPacketHandler);

    // Registration for callback into bluetooth task
    callback_registration.callback = &pushQueueIntoConnectionBuffers;
    callback_registration.context = nullptr;

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
 * Only process data if there are any actual clients and when all client
 * buffers are empty, send a trigger to the BT thread to process the data.
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

bool Bluetooth::createNewConnection(hci_con_handle_t handle, uint16_t mtu, uint8_t readyState)
{
    if (auto guard = SemaphoreGuard<portMAX_DELAY>(Bluetooth::mutex))
    {
        if (!Bluetooth::connections.full())
        {
            Bluetooth::connections.emplace_back(BtContext{handle, mtu, readyState});
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
    uint8_t status;

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
    case SM_EVENT_PAIRING_STARTED:
        printf("Pairing started\n");
        break;
    case SM_EVENT_PAIRING_COMPLETE:
        switch (sm_event_pairing_complete_get_status(packet))
        {
        case ERROR_CODE_SUCCESS:
            printf("Pairing complete, success\n");
            break;
        case ERROR_CODE_CONNECTION_TIMEOUT:
            printf("Pairing failed, timeout\n");
            break;
        case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
            printf("Pairing failed, disconnected\n");
            break;
        case ERROR_CODE_AUTHENTICATION_FAILURE:
            printf("Pairing failed, authentication failure with reason = %u\n", sm_event_pairing_complete_get_reason(packet));
            break;
        default:
            break;
        }
        break;
    case SM_EVENT_REENCRYPTION_STARTED:
        sm_event_reencryption_complete_get_address(packet, addr);
        printf("Bonding information exists for addr type %u, identity addr %s -> re-encryption started\n",
               sm_event_reencryption_started_get_addr_type(packet), bd_addr_to_str(addr));
        break;
    case SM_EVENT_REENCRYPTION_COMPLETE:
        switch (sm_event_reencryption_complete_get_status(packet))
        {
        case ERROR_CODE_SUCCESS:
            printf("Re-encryption complete, success\n");
            break;
        case ERROR_CODE_CONNECTION_TIMEOUT:
            printf("Re-encryption failed, timeout\n");
            break;
        case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
            printf("Re-encryption failed, disconnected\n");
            break;
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
    case GATT_EVENT_QUERY_COMPLETE:
        status = gatt_event_query_complete_get_att_status(packet);
        switch (status)
        {
        case ATT_ERROR_INSUFFICIENT_ENCRYPTION:
            printf("GATT Query failed, Insufficient Encryption\n");
            break;
        case ATT_ERROR_INSUFFICIENT_AUTHENTICATION:
            printf("GATT Query failed, Insufficient Authentication\n");
            break;
        case ATT_ERROR_BONDING_INFORMATION_MISSING:
            printf("GATT Query failed, Bonding Information Missing\n");
            break;
        case ATT_ERROR_SUCCESS:
            printf("GATT Query successful\n");
            break;
        default:
            printf("GATT Query failed, status 0x%02x\n", gatt_event_query_complete_get_att_status(packet));
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
            // puts("ATT_EVENT_CONNECTED");
        }
        break;
        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
        {
            auto handle = att_event_mtu_exchange_complete_get_handle(packet);
            auto mtu = att_event_mtu_exchange_complete_get_MTU(packet);
            if (mtu > 24)
            {
                if (!createNewConnection(handle, mtu, 0b010))
                {
                    puts("ATT_EVENT_MTU_EXCHANGE_COMPLETE: Too many connections, disconnecting");
                    gap_disconnect(handle);
                } else {
                    hci_send_cmd(&hci_le_set_advertise_enable, 1);
                }
            }
            else
            {
                puts("ATT_EVENT_MTU_EXCHANGE_COMPLETE: Very small MTU detected, disconnecting");
                gap_disconnect(handle);
            }
        }
        break;

        case ATT_EVENT_CAN_SEND_NOW:
        {
            // puts("ATT_EVENT_CAN_SEND_NOW");
            for (const auto &it : Bluetooth::connections)
            {
                sendPackage(it.handle);
            }
        }
        break;

        case ATT_EVENT_DISCONNECTED:
        {
            // puts("ATT_EVENT_DISCONNECTED");
            // clang-format off
            Bluetooth::withHandle(att_event_disconnected_get_handle(packet),
                etl::delegate<void(BtContext &)>::create([](BtContext &ctx)
                { 
                    ctx.readyState = 0b000; 
                    ctx.handle = HCI_CON_HANDLE_INVALID; 
                }
            ));            
            // clang-format off
            Bluetooth::cleanupConnections();
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
