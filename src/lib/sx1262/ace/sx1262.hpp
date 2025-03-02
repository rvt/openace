#pragma once

#include "../driver/src/sx126x.h"
#include "../driver/src/sx126x_hal.h"

/* System. */
#include <stdint.h>

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* PICO. */
#include "pico/binary_info.h"
// #include "hardware/spi.h"
#include "pico/stdlib.h"

/* Vendor. */
#include "etl/message_bus.h"
#include "etl/pseudo_moving_average.h"

/* OpenAce Libraries */
#include "ace/constants.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"

// https://www.waveshare.com/wiki/SX1262_XXXM_LoRaWAN/GNSS_HAT


/**
 * Client that can connect to a host and a port and expect to receive line terminated NMEA Messages
 * Part of this code taken from the example from Raspbery
 */
class Sx1262 : public Radio, public etl::message_router<Sx1262, OpenAce::RadioTxFrameMsg, OpenAce::ConfigUpdatedMsg, OpenAce::GpsStatsMsg>
{
    static constexpr uint32_t MAX_LISTEN_TIMEOUT = 150000; // maximum time we listen for packages before we timeout and reset the Sx1262
    static constexpr uint8_t MANCHESTER = 2;               // Used to just clarify why we sometime multiply by 2
    static constexpr uint8_t CRCBYTES = 2;                 // Used for clarifications in calculations

    enum TaskState : uint8_t
    {
        DELETE = 1 << 0,
        TASK_VALUE_DIO1_INTERRUPT = 1 << 2,
        FAILSAVE_LISTEN_MODE = 1 << 3
    };

    mutable struct
    {
        uint16_t deviceErrors = 0;
        uint32_t waitPacketTimeout = 0; // THis is more of a indication that we did not receive a packet within OPENACE_SX126X_MAX_RX_TIME, not an error
        uint32_t receivedPackets = 0;
        uint32_t buzyWaitsTimeout = 0;
        uint32_t queueFull = 0;
        uint32_t txTimeout = 0;
        uint32_t txOk = 0;
        Radio::Mode mode = Radio::Mode::NONE;
        OpenAce::DataSource dataSource = OpenAce::DataSource::NONE;
        uint32_t frequency = 0;
        int8_t powerdBm = -100;
    } statistics;

    // ************************************************************************************
    // 13.1.14 SetPaConfig
    // Table 13-21: PA Operating Modes with Optimal Settings
    static constexpr sx126x_pa_cfg_params_s DEFAULT_HIGH_POWER_PA_CFG =
        {
            .pa_duty_cycle = 0x04, // Never increase above 0x04, device might damage
            .hp_max = 0x07,        // never ever increase above 0x07, device might damage
            .device_sel = 0x00,    // sx1262
            .pa_lut = 0x01,        // Always 1
        };

    // ************************************************************************************
    // GFSK

    // GFSK packet is setup such that we get the last byte from teh syncword as the first byte of the packet
    // 13.4.6 SetPacketParams
    static constexpr sx126x_pkt_params_gfsk_t DEFAULT_PKG_PARAMS_GFSK =
        {
            .preamble_len_in_bits = 0,                                    // SET per protocol
            .preamble_detector = SX126X_GFSK_PREAMBLE_DETECTOR_MIN_8BITS, // SET per protocol
            .sync_word_len_in_bits = 0,                                   // SET per protocol
            .address_filtering = SX126X_GFSK_ADDRESS_FILTERING_DISABLE,
            .header_type = SX126X_GFSK_PKT_FIX_LEN,
            .pld_len_in_bytes = 0,           // SET per protocol
            .crc_type = SX126X_GFSK_CRC_OFF, // Manchester decoding used. so no CRC possible
            .dc_free = SX126X_GFSK_DC_FREE_OFF};

    // Verified, looks ok
    // 13.4.5 SetModulationParams
    static constexpr sx126x_mod_params_gfsk_t DEFAULT_MOD_PARAMS_GFSK =
        {
            .br_in_bps = 100000,                          // 50kbps*2 (Manchester) = 100000
            .fdev_in_hz = 50000,                          //
            .pulse_shape = SX126X_GFSK_PULSE_SHAPE_BT_05, // Gaussian BT 0.5
            .bw_dsb_param = SX126X_GFSK_BW_234300         //
        };

    // ************************************************************************************
    // LORA

    // 13.1.8 SetCAD
    // CAD is only used by LORA
    static constexpr sx126x_cad_params_t cad_params_lora =
        {
            .cad_symb_nb = SX126X_CAD_08_SYMB,
            .cad_detect_peak = 0x14,
            .cad_detect_min = 0X0A,
            .cad_exit_mode = SX126X_CAD_ONLY,
            .cad_timeout = 0x00000000,
        };

    static constexpr Radio::ProtocolConfig PROTOCOL_NONE{Radio::Mode::NONE, OpenAce::DataSource::NONE, 0, 1, 1, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

    const uint8_t csPin;
    const uint8_t busyPin;
    const uint8_t dio1Pin;
    const uint8_t radioNo;
    uint32_t offsetHz;
    bool txEnabled;
    volatile bool hasGpsFix;
    SpiModule *spiHall;
    TaskHandle_t taskHandle;
    Radio::RadioParameters lastRadioParameters{PROTOCOL_NONE, 868'000'000, -100};


public:
    static constexpr etl::array<etl::string_view, 2> NAMES{"Sx1262_0", "Sx1262_1"};

    Sx1262(etl::imessage_bus &bus, const OpenAce::PinTypeMap &pins, uint8_t radioNo_, bool txEnabled_, uint32_t offsetHz_) : Radio(bus, Radio::NAMES[radioNo_]),
                                                                                                                             csPin(pins.at(OpenAce::PinType::CS)),
                                                                                                                             busyPin(pins.at(OpenAce::PinType::BUSY)),
                                                                                                                             dio1Pin(pins.at(OpenAce::PinType::DIO1)),
                                                                                                                             radioNo(radioNo_),
                                                                                                                             offsetHz(offsetHz_),
                                                                                                                             txEnabled(txEnabled_),
                                                                                                                             hasGpsFix(false),
                                                                                                                             spiHall(nullptr),
                                                                                                                             taskHandle(nullptr)
    {
        //        assert(num >=0 && num <= 1);
    }
    Sx1262(etl::imessage_bus &bus, const Configuration &config, uint8_t radioNo_) : Sx1262(bus, config.pinMap(NAMES[radioNo_]),
                                                                                           radioNo_,
                                                                                           config.valueByPath(true, NAMES[radioNo_], "txEnabled"),
                                                                                           config.valueByPath(true, NAMES[radioNo_], "offset"))
    {
    }

    virtual ~Sx1262() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    // virtual const char* name() const override;

    /**
     * SPI access for SX1262 driver, don't use for anything else
     */
    inline uint8_t cs() const
    {
        return csPin;
    }
    inline uint8_t busy() const
    {
        return busyPin;
    }
    inline SpiModule *spi()
    {
        return spiHall;
    }
    virtual uint8_t radio() const
    {
        return radioNo;
    }

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    inline void sendToBus(OpenAce::RadioRxFrameMsg &frame)
    {
        getBus().receive(frame);
    };

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    void on_receive(const OpenAce::RadioTxFrameMsg &msg);
    void on_receive(const OpenAce::ConfigUpdatedMsg &msg);
    void on_receive(const  OpenAce::GpsStatsMsg &msg);
   

    void radioInit();
    void checkAndClearDeviceErrors();
    void receiveGFSKPacket(Radio::RadioParameters const &parameters);
    void sendGFSKPacket(const RadioParameters &parameters, const uint8_t *data, uint8_t length);
    void configureSx1262(const RadioParameters &newParameters);
    bool applyNewLoraParameters(const Radio::ProtocolConfig &parameters);
    sx126x_irq_mask_t getIrqStatus();

    void Listen();
    void standBy();

    static void sx1262Task(void *arg);

    uint8_t receivedPacketLength() const;

    virtual void rxMode(const RxMode &rxMode) override;
    virtual void txPacket(const TxPacket &txpacket) override;

    void waitBusy(uint16_t minimumDelay) const;

};
