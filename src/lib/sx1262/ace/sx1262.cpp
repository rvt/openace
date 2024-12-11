#include "sx1262.hpp"

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* OpenACE. */
#include "etl/map.h"
#include "ace/manchester.hpp"
#include "ace/coreutils.hpp"

void Sx1262::start()
{
    getBus().subscribe(*this);
};

void Sx1262::stop()
{
    getBus().unsubscribe(*this);
    xTaskNotify(taskHandle, TaskState::DELETE, eSetBits);
};

OpenAce::PostConstruct Sx1262::postConstruct()
{
    spiHall = static_cast<SpiModule *>(BaseModule::moduleByName(*this, SpiModule::NAME));

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(csPin);
    gpio_set_dir(csPin, GPIO_OUT);
    gpio_put(csPin, 1);

    // Busy Pin has PullUp from SX1262 so kept as input only
    gpio_init(busyPin);
    gpio_set_dir(busyPin, GPIO_IN);

    // Make the SPI pins available to picotool
    bi_decl(bi_1pin_with_name(static_cast<uint32_t>(csPin), "Sx1262 CS"));

    // Read the device type
    char data[7];
    sx126x_read_register(this, 0x0320, (uint8_t *)data, 6);
    data[6] = 0;

    // Note that even for a SX1262, the version might come back as SX1261
    // https://forum.lora-developers.semtech.com/t/sx126x-device-id/1508
    if (strncmp(data, "SX126", 5) != 0)
    {
        printf("Expected SX126X, but found [%s] ", data);
        return OpenAce::PostConstruct::HARDWARE_NOT_FOUND;
    }
    printf(" found [%s] (Sx1261 is normal for a Sx1262) ", data);

    radioInit();

    if (xTaskCreate(sx1262Task, "sx1262Task", configMINIMAL_STACK_SIZE, this, tskIDLE_PRIORITY + 4, &taskHandle) != pdPASS)
    {
        return OpenAce::PostConstruct::TASK_ERROR;
    }
    registerPinInterrupt(dio1Pin, GPIO_IRQ_EDGE_RISE, taskHandle, TASK_VALUE_DIO1_INTERRUPT);

    printf("Initialised on cs:%d busy:%d dio1:%d ", csPin, busyPin, dio1Pin);
    return OpenAce::PostConstruct::OK;
}

void Sx1262::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"deviceErrors\":" << statistics.deviceErrors;
    stream << ",\"waitPacketTimeout\":" << statistics.waitPacketTimeout;
    stream << ",\"receivedPackets\":" << statistics.receivedPackets;
    stream << ",\"buzyWaitsTimeout\":" << statistics.buzyWaitsTimeout;
    stream << ",\"queueFull\":" << statistics.queueFull;
    stream << ",\"txTimeout\":" << statistics.txTimeout;
    stream << ",\"txOk\":" << statistics.txOk;
    stream << ",\"mode\":" << "\"" << Radio::modeString(statistics.mode) << "\"";
    stream << ",\"dataSource\":" << "\"" << OpenAce::dataSourceToString(statistics.dataSource) << "\"";
    stream << ",\"frequency\":" << statistics.frequency;
    stream << ",\"powerdBm\":" << statistics.powerdBm;
    stream << ",\"txEnabled\":" << txEnabled;
    stream << ",\"radio\":" << radioNo;
    stream << "}\n";
}

void Sx1262::on_receive(const OpenAce::RadioTxFrame &msg)
{
    if (msg.radioNo == radioNo && txEnabled)
    {
        txPacket(msg.txPacket);
    }
}

void Sx1262::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == NAMES[radioNo])
    {
        txEnabled = msg.config.valueByPath(true, NAMES[radioNo], "txEnabled");
        offsetHz = msg.config.valueByPath(true, NAMES[radioNo], "offset");
    }
}

/**
 * Apply methods
 */

void Sx1262::radioInit()
{
    // .365
    waitBusy(50);

    sx126x_set_dio_irq_params(this, 0, 0, 0, 0);
    sx126x_clear_irq_status(this, SX126X_IRQ_ALL);

    // Must be called before any calibration
    sx126x_set_dio3_as_tcxo_ctrl(this, SX126X_TCXO_CTRL_1_6V, 5000.f / 15.625);
    sx126x_set_dio2_as_rf_sw_ctrl(this, true);

    // 9.4 Standby (STDBY) Mode DCDC must be set in SX126X_STANDBY_CFG_RC mode
    // 13.1.12 Calibrate Function must happen in SX126X_STANDBY_CFG_RC
    sx126x_set_standby(this, SX126X_STANDBY_CFG_RC);
    sx126x_set_reg_mode(this, SX126X_REG_MODE_DCDC);

    // Start Calibration
    sx126x_cal(this, SX126X_CAL_ALL);
    waitBusy(25);
    checkAndClearDeviceErrors();
    sx126x_cal_img_in_mhz(this, 868 - 2, 868 + 2); // 13.1.13 CalibrateImage
    waitBusy(100);
    checkAndClearDeviceErrors();

    // Better Resistance of the SX1262 Tx to Antenna Mismatch
    uint8_t clamp;
    sx126x_read_register(this, 0x08D8, &clamp, 1);
    clamp |= 0x1E;
    sx126x_write_register(this, 0x08D8, &clamp, 1);

    sx126x_set_buffer_base_address(this, 0x00, 0x80);

    standBy();
}

uint8_t Sx1262::receivedPacketLength() const
{
    sx126x_rx_buffer_status_t rx_buffer_status;
    sx126x_get_rx_buffer_status(this, &rx_buffer_status);
    return rx_buffer_status.pld_len_in_bytes;
}

bool Sx1262::applyNewLoraParameters(const Radio::ProtocolConfig &config)
{
    (void)config;
    // // Set the Sync Word
    // LoRa Sync Word, Differentiate the LoRa® signal for Public or Private Network
    uint8_t data[] = {0xF4, 0x14};
    sx126x_write_register(this, 0x0740, data, 2); // SX126X_REG_LR_SYNCWORD

    // Optimizing the Inverted IQ Operation
    sx126x_read_register(this, 0x0736, data, 1); // SX126X_REG_IQ_POLARITY
    data[0] |= 0x04;
    sx126x_write_register(this, 0x0736, data, 1);

    //     if (sx126x_buzy_wait(busyPin, 150000)) {
    //         statistics.buzyWaitsTimeout++;
    //         printf("Wait busy timeout (switchToGFSK) ");
    //     }
    //     // 13.1.8 SetCAD (Only Lora Mode)
    //     sx126x_set_cad_params(this, &cad_params_lora);
    //     sx126x_set_cad(this);

    return true;
}

void Sx1262::waitBusy(uint16_t minimumDelay) const
{
    if (sx126x_buzy_wait(busyPin))
    {
        statistics.buzyWaitsTimeout++;
    }
    if (minimumDelay)
    {
        vTaskDelay(TASK_DELAY_MS(minimumDelay));
    }
}

void Sx1262::standBy()
{
    waitBusy(0);

    // .715
    sx126x_set_standby(this, SX126X_STANDBY_CFG_XOSC);
}

void Sx1262::checkAndClearDeviceErrors()
{
    sx126x_errors_mask_t errors;
    sx126x_status_t status = sx126x_get_device_errors(this, &errors);
    if (status != SX126X_STATUS_OK && ((uint8_t)errors) != 0)
    {
        statistics.deviceErrors++;
        sx126x_clear_device_errors(this);
        printf("Device Error: %d\n", errors);
    }
}

void Sx1262::configureSx1262(const RadioParameters &newParameters)
{
    standBy();
    // THis routines takes about 250us
    if (newParameters.config.mode == Radio::Mode::GFSK)
    {
        sx126x_set_pkt_type(this, SX126X_PKT_TYPE_GFSK);
        sx126x_set_gfsk_mod_params(this, &DEFAULT_MOD_PARAMS_GFSK);
        sx126x_set_rx_tx_fallback_mode(this, SX126X_FALLBACK_STDBY_XOSC);
        statistics.mode = newParameters.config.mode;

        auto pkt_params_gfsk = DEFAULT_PKG_PARAMS_GFSK;
        pkt_params_gfsk.pld_len_in_bytes = newParameters.config.packetLength * MANCHESTER;
        pkt_params_gfsk.preamble_len_in_bits = 1 * 8;
        pkt_params_gfsk.preamble_detector = static_cast<sx126x_gfsk_preamble_detector_t>((1 + 3) & 0b1100); // Must be set to 1 for now (even though ADS-L has 2...)
        pkt_params_gfsk.sync_word_len_in_bits = (newParameters.config.syncLength) * 8;
        sx126x_set_gfsk_pkt_params(this, &pkt_params_gfsk);
        sx126x_set_gfsk_sync_word(this, newParameters.config.syncWord.data() + newParameters.config.preambleLength, newParameters.config.syncLength);
        // printf("Radio %d changed from %s to %s\n", radioNo, OpenAce::dataSourceToString(lastParameters.config.dataSource), OpenAce::dataSourceToString(newParameters.config.dataSource));
        statistics.dataSource = newParameters.config.dataSource;
    }
    else if (newParameters.config.mode == Radio::Mode::LORA)
    {
        // to implement
        applyNewLoraParameters(newParameters.config);
        statistics.mode = newParameters.config.mode;

        printf("Lora not yet supported");
    }

    sx126x_set_rf_freq(this, newParameters.frequency + offsetHz);
    statistics.frequency = newParameters.frequency + offsetHz;

    // printf("Radio %d changed frequency from %ld to %ld\n", radioNo, lastParameters.frequency, newParameters.frequency);
    checkAndClearDeviceErrors();
}

void Sx1262::Listen()
{
    sx126x_set_dio_irq_params(this, SX126X_IRQ_RX_DONE, SX126X_IRQ_RX_DONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE);
    sx126x_clear_irq_status(this, SX126X_IRQ_ALL);
    enablePinInterrupt(dio1Pin);

    // https://forum.lora-developers.semtech.com/t/sx1262-reduced-rx-sensitivity-packet-reception-fails/162/12
    // Need to call SetFs() and then RxBoosted() periodically to fix a issue with receiver gain
    sx126x_cfg_rx_boosted(this, true);

    // sx126x_set_rx_with_timeout_in_rtc_step( this, SX126X_RX_CONTINUOUS );
    //  Device is out into listen with timeout because there where reports
    //  that sensitivity goes down at some point
    sx126x_set_rx(this, SX126X_MAX_TIMEOUT_IN_MS);
    // printf("Radio %d set to listen\n", radioNo);
}

void Sx1262::sendGFSKPacket(const RadioParameters &parameters, const uint8_t *data, uint8_t length)
{
    disablePinInterrupt(dio1Pin); // Disable interrupt because sending is aSync
    sx126x_set_dio_irq_params(this, SX126X_IRQ_TX_DONE, SX126X_IRQ_TX_DONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE);
    sx126x_clear_irq_status(this, SX126X_IRQ_ALL);

    sx126x_set_pa_cfg(this, &DEFAULT_HIGH_POWER_PA_CFG);
    // OCP (Over Current Protection) Configuration must be set after sx126x_set_pa_cfg
    sx126x_set_ocp_value(this, (uint8_t)(120.0 / 2.5));

    statistics.powerdBm = parameters.powerdBm;
    sx126x_set_tx_params(this, parameters.powerdBm, SX126X_RAMP_200_US);

    sx126x_write_buffer(this, 0x00, data, length);
    sx126x_set_tx(this, SX126X_MAX_TIMEOUT_IN_MS);

    // 8.3
    // In TX, BUSY will go low when the PA has ramped-up and transmission of preamble starts.
    // So wait until busy get's high first
    // From STBY_XOSC to TX around 105us
    // 13.3.2.1 DioxMask
    // Wiat for rising edge when TX is done
    while (!gpio_get(busyPin))
    {
        tight_loop_contents();
    }

    // Sending a packet takes around 4..5ms so allow RTOS to switch
    vTaskDelay(pdMS_TO_TICKS(5));

    // Wait until raising edge for TX to be done
    while (!gpio_get(dio1Pin))
    {
        tight_loop_contents();
    }

    sx126x_set_dio_irq_params(this, SX126X_IRQ_NONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE);
    sx126x_clear_irq_status(this, SX126X_IRQ_ALL);
}

void Sx1262::receiveGFSKPacket(Radio::RadioParameters const &parameters)
{
    // 13.5.3 GetPacketStatus
    sx126x_pkt_status_gfsk_t pkt_status;
    sx126x_get_gfsk_pkt_status(this, &pkt_status);
    if (pkt_status.rx_status.pkt_received && pkt_status.rx_status.abort_error == 0)
    {
        statistics.receivedPackets++;
        uint8_t receivedFrameLength = receivedPacketLength();
        constexpr uint8_t maxFrameLength = OpenAce::RADIO_MAX_FRAME_LENGTH * MANCHESTER;
        if (receivedFrameLength > 0 && receivedFrameLength <= maxFrameLength)
        {
            uint8_t data[maxFrameLength];
            sx126x_read_buffer(this, 0x80, data, receivedFrameLength);

            OpenAce::RadioRxFrame RadioRxFrame{(uint8_t)(receivedFrameLength / MANCHESTER), CoreUtils::secondsSinceEpoch(), (int8_t)(-pkt_status.rssi_sync / 2), parameters.frequency, parameters.config.dataSource};

            // Seems like all GFSK packets are Manchester encoded, so we just decode here directly
            manchesterDecode((uint8_t *)RadioRxFrame.frame, (uint8_t *)RadioRxFrame.err, data, receivedFrameLength);
            sendToBus(RadioRxFrame);
            // dumpBuffer((uint8_t *)RadioRxFrame.frame, RadioRxFrame.length);
        }
        else
        {
            // We get here if the radio received noise, this should normally not happen
            printf("Incorrect frame length expected:%d got:%d\n", maxFrameLength, receivedFrameLength);
        }
    }
    else
    {
        // Normally we don't end up here, but if we do we package is ignored
    }
    // printf("\n");
}

sx126x_irq_mask_t Sx1262::getIrqStatus()
{
    sx126x_irq_mask_t mask;
    sx126x_get_irq_status(this, &mask);
    return mask;
}

void Sx1262::rxMode(const RxMode &rxMode)
{
    if (spiHall->acquireSlotSync(OPENOPENACE_SPI_DEFAULT_BUS_FREQUENCY))
    {
//        printf("Radio %d RX: %s timeMs:%d\n", radioNo, OpenAce::dataSourceToString(lastRadioParameters.config.dataSource), CoreUtils::msInSecond());
        configureSx1262(rxMode.radioParameters);
        Listen();
        lastRadioParameters = rxMode.radioParameters;
        spiHall->releaseSlotSync();
    } else {
        //puts("Missed");
    }
}

void Sx1262::txPacket(const TxPacket &txPacket)
{

    if (spiHall->acquireSlotSync(OPENOPENACE_SPI_DEFAULT_BUS_FREQUENCY))
    {
        // printf("Radio %d TX %s timeMs:%d\n", sx1262->radioNo, OpenAce::dataSourceToString(command.txPacket.radioParameters.config.dataSource), CoreUtils::msInSecond());
        configureSx1262(txPacket.radioParameters);

        uint8_t frame[OpenAce::RADIO_MAX_FRAME_LENGTH * 2];
        manchesterEncode(frame, txPacket.data.data(), txPacket.length);

        sendGFSKPacket(txPacket.radioParameters, frame, txPacket.length * 2);

        configureSx1262(lastRadioParameters);
        Listen();
        spiHall->releaseSlotSync();
    } else {
        // puts("Missed");
    }

}

void Sx1262::sx1262Task(void *arg)
{
    Sx1262 *sx1262 = static_cast<Sx1262 *>(arg);
    SpiModule *aceSpi = static_cast<SpiModule *>(BaseModule::moduleByName(*sx1262, SpiModule::NAME));

    while (true)
    {
        if (uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(10000)))
        {
            if (notifyValue & TaskState::DELETE)
            {
                vTaskDelete(nullptr);
                return;
            }

            aceSpi->acquireSlotSyncCb(OPENOPENACE_SPI_DEFAULT_BUS_FREQUENCY, [&sx1262, notifyValue]() 
            {

                // Read device status to validate if there is anything to do
                auto irqStatus = sx1262->getIrqStatus();
                // printf("Radio %d IRQ Status: %d %ld\n", sx1262->radioNo, irqStatus, notifyValue);
                if (irqStatus & SX126X_IRQ_RX_DONE)
                {
                    // Reading the data takes about 4ms
//                    printf("Radio %d Packet RX: %s timeMs:%d\n", sx1262->radioNo, OpenAce::dataSourceToString(sx1262->lastRadioParameters.config.dataSource), CoreUtils::msInSecond());
                    sx1262->receiveGFSKPacket(sx1262->lastRadioParameters);
                    sx1262->configureSx1262(sx1262->lastRadioParameters);
                    sx1262->Listen();
                }

                if ((notifyValue & TaskState::FAILSAVE_LISTEN_MODE) && (sx1262->lastRadioParameters.config.dataSource != OpenAce::DataSource::NONE))
                {
                    sx1262->configureSx1262(sx1262->lastRadioParameters);
                    sx1262->Listen();
                    // printf("Radio %d FailSafe %d\n", sx1262->radioNo, CoreUtils::msInSecond());
                }
            });
        }
        else
        {
            // End up here when timeout, only count in statistics so we know this is happening
            if (sx1262->lastRadioParameters.config.dataSource != OpenAce::DataSource::NONE)
            {
                sx1262->statistics.waitPacketTimeout++;
            }
        }
    }
}
