#include "sx1262.hpp"

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* ETL. */
#include "etl/map.h"

/* OpenACE. */
#include "ace/manchester.hpp"
#include "ace/coreutils.hpp"
#include "ace/measure.hpp"

/* PICO */
#include "hardware/spi.h"

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

    if (spiHall == nullptr)
    {
        return OpenAce::PostConstruct::DEP_NOT_FOUND;
    }

    xMutex = xSemaphoreCreateMutex();
    if (xMutex == nullptr)
    {
        return OpenAce::PostConstruct::MUTEX_ERROR;
    }

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(csPin);
    gpio_set_dir(csPin, GPIO_OUT);
    gpio_put(csPin, 1);

    // Busy Pin has PullUp from SX1262 so kept as input only
    gpio_init(busyPin);
    gpio_set_dir(busyPin, GPIO_IN);

    gpio_init(dio1Pin);
    gpio_set_dir(dio1Pin, GPIO_IN);

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

    // Added 512byte because Fanet can be up to 256 byte
    if (xTaskCreate(sx1262Task, "sx1262Task", configMINIMAL_STACK_SIZE + 512, this, tskIDLE_PRIORITY + 4, &taskHandle) != pdPASS)
    {
        return OpenAce::PostConstruct::TASK_ERROR;
    }
    registerPinInterrupt(dio1Pin, GPIO_IRQ_EDGE_RISE, taskHandle, 0);

    printf("Initialised on cs:%d busy:%d dio1:%d ", csPin, busyPin, dio1Pin);

    // Make the SPI pins available to picotool
    bi_decl(bi_1pin_with_name(static_cast<uint32_t>(csPin), NAMES[radioNo].cbegin()));
    bi_decl(bi_1pin_with_name(static_cast<uint32_t>(busyPin), NAMES[radioNo].cbegin()));
    bi_decl(bi_1pin_with_name(static_cast<uint32_t>(dio1Pin), NAMES[radioNo].cbegin()));

    return OpenAce::PostConstruct::OK;
}

void Sx1262::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"deviceErrors\":" << statistics.deviceErrors;
    stream << ",\"spiNo\":" << spiHall->spiNum();
    stream << ",\"taskTimeout\":" << statistics.taskTimeout;
    stream << ",\"receivedPackets\":" << statistics.receivedPackets;
    stream << ",\"transmittedPackets\":" << statistics.transmittedPackets;
    stream << ",\"buzyWaitsTimeout\":" << statistics.buzyWaitsTimeout;
    stream << ",\"txQueueFull\":" << statistics.queueFull;
    stream << ",\"txQueueSize\":" << txQueue.size();
    stream << ",\"mode\":" << "\"" << Radio::modeString(rxRadioParameters.config.mode) << "\"";
    stream << ",\"dataSource\":" << "\"" << OpenAce::dataSourceToString(rxRadioParameters.config.dataSource) << "\"";
    stream << ",\"frequency\":" << rxRadioParameters.frequency;
    stream << ",\"powerdBm\":" << rxRadioParameters.powerdBm;
    stream << ",\"txEnabled\":" << txEnabled;
    stream << ",\"radio\":" << radioNo;
    stream << ",\"hasGpsFix\":" << hasGpsFix;
    stream << ",\"isTransmitting\":" << (hasGpsFix && txEnabled);
    stream << "}\n";
}

void Sx1262::on_receive(const OpenAce::RadioTxFrameMsg &msg)
{
    if (msg.radioNo == radioNo && txEnabled)
    {
        if (txQueue.full())
        {
            statistics.queueFull++;
        }
        else if (hasGpsFix)
        {
            txQueue.push(msg.txPacket);
        }
        xTaskNotify(taskHandle, TaskState::HANDLETX, eSetBits);
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

void Sx1262::on_receive(const OpenAce::GpsStatsMsg &msg)
{
    hasGpsFix = msg.fixType == 3;
}

void Sx1262::on_receive(const OpenAce::RadioControlMsg &msg)
{
    if (msg.radioNo == radioNo)
    {
        if (auto guard = SemaphoreGuard<5>(xMutex))
        {
            newRxRadioParameters = msg.radioParameters;
            xTaskNotify(taskHandle, TaskState::HANDLE_NEW_CONFIG, eSetBits);
        }
    }
}

/**
 * Apply methods
 */

void Sx1262::radioInit()
{
    // .365
    waitBusy(50);

    sx126x_set_dio_irq_params(this, SX126X_IRQ_NONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE);
    sx126x_clear_irq_status(this, SX126X_IRQ_ALL);

    // Must be called before any calibration
    sx126x_set_dio3_as_tcxo_ctrl(this, SX126X_TCXO_CTRL_1_6V, 5000.f / 15.625);
    sx126x_set_dio2_as_rf_sw_ctrl(this, true);

    // Start Calibration

    // 9.4 Standby (STDBY) Mode DCDC must be set in SX126X_STANDBY_CFG_RC mode
    // 13.1.12 Calibrate Function must happen in SX126X_STANDBY_CFG_RC
    sx126x_set_standby(this, SX126X_STANDBY_CFG_RC);
    sx126x_set_reg_mode(this, SX126X_REG_MODE_DCDC);
    sx126x_cal(this, SX126X_CAL_ALL);
    waitBusy(25);
    checkAndClearDeviceErrors();
    sx126x_cal_img_in_mhz(this, 868 - 2, 868 + 2); // 13.1.13 CalibrateImage
    waitBusy(100);
    checkAndClearDeviceErrors();

    // Calibration done, use stanby
    standBy();

    // 15.2.2 Better Resistance of the SX1262 Tx to Antenna Mismatch
    uint8_t clamp;
    sx126x_read_register(this, 0x08D8, &clamp, 1);
    clamp |= 0x1E;
    sx126x_write_register(this, 0x08D8, &clamp, 1);

    // TX Base at 0x00  RX Base at 0x80
    sx126x_set_buffer_base_address(this, 0x00, 0x80);

    sx126x_set_rx_tx_fallback_mode(this, SX126X_FALLBACK_STDBY_XOSC);
    sx126x_set_cad_params(this, &DEFAULT_CAD_PARAMS);

    sx126x_set_pa_cfg(this, &DEFAULT_HIGH_POWER_PA_CFG);
    sx126x_set_ocp_value(this, (uint8_t)(120.0 / 2.5));
}

void Sx1262::configureSx1262(const RadioParameters &newParameters, bool forTx)
{
    // 9.8 Transceiver Circuit Modes Graphical Illustration
    standBy();
    // This routines takes about 250us
    if (newParameters.config.mode == Radio::Mode::GFSK)
    {
        sx126x_set_pkt_type(this, SX126X_PKT_TYPE_GFSK);
        sx126x_set_gfsk_mod_params(this, &DEFAULT_MOD_PARAMS_GFSK);

        auto pkt_params_gfsk = DEFAULT_PKG_PARAMS_GFSK;
        pkt_params_gfsk.pld_len_in_bytes = newParameters.config.packetLength * MANCHESTER;
        pkt_params_gfsk.preamble_len_in_bits = newParameters.config.preambleLength;                         // 1 * 8 newParameters.config.preambleLength;
        pkt_params_gfsk.preamble_detector = static_cast<sx126x_gfsk_preamble_detector_t>((1 + 3) & 0b1100); // Must be set to 1 for now (even though ADS-L has 2...)
        pkt_params_gfsk.sync_word_len_in_bits = (newParameters.config.syncLength) * 8;
        sx126x_set_gfsk_pkt_params(this, &pkt_params_gfsk);
        // TO TEST, newParameters.config.preambleLength / 8 THIS MIGHT HAVE FUCKED UP ADSL SENDING OR RECEIVING this used to be newParameters.config.preambleLength
        sx126x_set_gfsk_sync_word(this, newParameters.config.syncWord.data() + newParameters.config.preambleLength / 8, newParameters.config.syncLength);
        // printf("Radio %d changed from %s to %s\n", radioNo, OpenAce::dataSourceToString(lastParameters.config.dataSource), OpenAce::dataSourceToString(newParameters.config.dataSource));
    }
    else if (newParameters.config.mode == Radio::Mode::LORA)
    {
        sx126x_set_pkt_type(this, SX126X_PKT_TYPE_LORA);
        if (!forTx)
        {
            sx126x_set_lora_mod_params(this, &DEFAULT_MOD_PARAMS_LORA);
            sx126x_set_lora_pkt_params(this, &DEFAULT_PKG_PARAMS_LORA);
        }
        sx126x_write_register(this, 0x0740, newParameters.config.syncWord.data(), newParameters.config.syncLength);
        // printf("Radio %d changed frequency from %ld to %ld\n", radioNo, lastParameters.frequency, newParameters.frequency);
    }

    if (newParameters.frequency != rxRadioParameters.frequency || true)
    {
        sx126x_set_rf_freq(this, newParameters.frequency + offsetHz);
    }

    // printf("Radio %d changed frequency from %ld to %ld\n", radioNo, lastParameters.frequency, newParameters.frequency);
    checkAndClearDeviceErrors();
}

void Sx1262::listen()
{
    // https://forum.lora-developers.semtech.com/t/sx1262-reduced-rx-sensitivity-packet-reception-fails/162/12
    // Need to call SetFs() and then RxBoosted() periodically to fix a issue with receiver gain
    sx126x_set_fs(this);
    sx126x_cfg_rx_boosted(this, true);

    sx126x_set_dio_irq_params(this, SX126X_IRQ_RX_DONE, SX126X_IRQ_RX_DONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE);
    sx126x_clear_irq_status(this, SX126X_IRQ_ALL);
    enablePinInterrupt(dio1Pin, DIO1_RX_DONE);
    sx126x_set_rx(this, SX126X_MAX_TIMEOUT_IN_MS);
    // printf("Radio %d set to listen\n", radioNo);
}

void Sx1262::sendGFSKPacket(const RadioParameters &parameters, const uint8_t *data, uint8_t length)
{
    (void)parameters;
    sx126x_set_dio_irq_params(this, SX126X_IRQ_TX_DONE, SX126X_IRQ_TX_DONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE);
    sx126x_clear_irq_status(this, SX126X_IRQ_ALL);
    enablePinInterrupt(dio1Pin, DIO1_TX_DONE);

    sx126x_set_tx_params(this, parameters.powerdBm, SX126X_RAMP_200_US);
    sx126x_write_buffer(this, 0x00, data, length);
    // 13.1.14 SetTx
    sx126x_set_tx(this, SX126X_MAX_TIMEOUT_IN_MS);
}

void Sx1262::sendLORAPacket(const RadioParameters &parameters, const uint8_t *data, uint8_t length)
{

    auto LORA_PARAMS = DEFAULT_MOD_PARAMS_LORA;
    switch (parameters.config.codingRate)
    {
    case 5:
        LORA_PARAMS.cr = SX126X_LORA_CR_4_5;
        break;
    case 6:
        LORA_PARAMS.cr = SX126X_LORA_CR_4_6;
        break;
    case 7:
        LORA_PARAMS.cr = SX126X_LORA_CR_4_7;
        break;
    case 8:
        LORA_PARAMS.cr = SX126X_LORA_CR_4_8;
        break;
    default:
        LORA_PARAMS.cr = SX126X_LORA_CR_4_8;
        break;
    }
    sx126x_set_lora_mod_params(this, &LORA_PARAMS);

    auto pkg = DEFAULT_PKG_PARAMS_LORA;
    pkg.pld_len_in_bytes = length;
    sx126x_set_lora_pkt_params(this, &pkg);

    sx126x_set_tx_params(this, parameters.powerdBm, SX126X_RAMP_200_US);

    // Wait until CAD done
    // uint32_t start = CoreUtils::timeUs32();
    // disablePinInterrupt(dio1Pin); // We are just waiting for CAD
    sx126x_set_dio_irq_params(this, SX126X_IRQ_TX_DONE, SX126X_IRQ_TX_DONE, SX126X_IRQ_NONE, SX126X_IRQ_NONE);
    sx126x_clear_irq_status(this, SX126X_IRQ_ALL);
    enablePinInterrupt(dio1Pin, DIO1_TX_DONE); // Disable interrupt because sending is aSync

    // sx126x_set_cad(this);
    // while (!gpio_get(dio1Pin))
    // {
    //     tight_loop_contents();
    // }

    // auto irqStatus = getIrqStatus();
    // if (irqStatus & SX126X_IRQ_CAD_DETECTED)
    // {
    //     // CAD detected, bail out
    //     printf("CAD detected, aborting TX took: %ld\n", CoreUtils::timeUs32() - start);
    //     return;
    // }

    // 13.1.14 SetTx
    sx126x_write_buffer(this, 0x00, data, length);
    sx126x_set_tx(this, SX126X_MAX_TIMEOUT_IN_MS);
}

void Sx1262::receiveGFSKPacket()
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

            rxGfskMsg = OpenAce::RadioRxGfskMsg{(uint8_t)(receivedFrameLength / MANCHESTER), CoreUtils::secondsSinceEpoch(), pkt_status.rssi_avg, rxRadioParameters.frequency, rxRadioParameters.config.dataSource};

            // Seems like all GFSK packets are Manchester encoded, so we just decode here directly
            manchesterDecode((uint8_t *)rxGfskMsg.frame, (uint8_t *)rxGfskMsg.err, data, receivedFrameLength);
            // dumpBuffer((uint8_t *)RadioRxGfskMsg.frame, RadioRxGfskMsg.length);
        }
        else
        {
            // If we get here, there is some mis configuration going on
            printf("Incorrect frame length expected:%d got:%d\n", maxFrameLength, receivedFrameLength);
        }
    }
    // printf("\n");
}

void Sx1262::receiveLORAPacket()
{
    // 13.5.3 GetPacketStatus
    sx126x_chip_status_t chip_status;
    sx126x_get_status(this, &chip_status);
    if (chip_status.cmd_status == SX126X_CMD_STATUS_DATA_AVAILABLE)
    {
        sx126x_pkt_status_lora_t pkt_status;
        sx126x_get_lora_pkt_status(this, &pkt_status);

        statistics.receivedPackets++;
        uint8_t receivedFrameLength = receivedPacketLength();
        if (receivedFrameLength > 0 && receivedFrameLength <= OpenAce::MAX_LORA_MSG_SIZE)
        {
            rxLoraMsg = OpenAce::RadioRxLoraMsg(CoreUtils::secondsSinceEpoch(), pkt_status.signal_rssi_pkt_in_dbm, rxRadioParameters.frequency, rxRadioParameters.config.dataSource);
            rxLoraMsg.frame.resize(receivedFrameLength);
            sx126x_read_buffer(this, 0x80, rxLoraMsg.frame.data(), receivedFrameLength);
            // dumpBuffer((uint8_t *)RadioRxGfskMsg.frame, RadioRxGfskMsg.length);
        }
    }
}

uint8_t Sx1262::receivedPacketLength() const
{
    sx126x_rx_buffer_status_t rx_buffer_status;
    sx126x_get_rx_buffer_status(this, &rx_buffer_status);
    return rx_buffer_status.pld_len_in_bytes;
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
    // This takes about100us
    waitBusy();

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

sx126x_irq_mask_t Sx1262::getIrqStatus()
{
    sx126x_irq_mask_t mask;
    sx126x_get_irq_status(this, &mask);
    return mask;
}

void Sx1262::sendPacket(const TxPacket &txPacket)
{

    // printf("Radio %d TX %s timeMs:%d\n", sx1262->radioNo, OpenAce::dataSourceToString(command.txPacket.radioParameters.config.dataSource), CoreUtils::msInSecond());
    configureSx1262(txPacket.radioParameters, true);

    if (txPacket.radioParameters.config.mode == Radio::Mode::GFSK)
    {
        uint8_t frame[OpenAce::RADIO_MAX_FRAME_LENGTH * 2];
        manchesterEncode(frame, txPacket.data.data(), txPacket.length);
        sendGFSKPacket(txPacket.radioParameters, frame, txPacket.length * 2);
    }
    else if (txPacket.radioParameters.config.mode == Radio::Mode::LORA)
    {
        sendLORAPacket(txPacket.radioParameters, txPacket.data.data(), txPacket.length);
    }
}

void Sx1262::sx1262Task(void *arg)
{
    Sx1262 *sx1262 = static_cast<Sx1262 *>(arg);
    SpiModule *aceSpi = static_cast<SpiModule *>(BaseModule::moduleByName(*sx1262, SpiModule::NAME));
    uint32_t txExpiration = 0;
    bool hasNewConfig = false;
    while (true)
    {
        if (uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(2000)))
        {
            if (notifyValue & TaskState::DELETE)
            {
                vTaskDelete(nullptr);
                return;
            }

            // When a new configuration mark it with a boolean as it needs to be processed later
            if (notifyValue & TaskState::HANDLE_NEW_CONFIG)
            {
                hasNewConfig = true;
            }

            // After TX, go back to RX
            if (notifyValue & TaskState::DIO1_TX_DONE)
            {
                bool _;
                if (auto guard = aceSpi->getLock(_))
                {
                    // printf("%8ld Listen Packet after TX ds:%s\n", CoreUtils::timeUs32() / 1000, OpenAce::dataSourceToString(sx1262->rxRadioParameters.config.dataSource));
                    sx1262->configureSx1262(sx1262->rxRadioParameters);
                    sx1262->listen();
                }
                txExpiration = 0;
            }

            // When in TX mode, the tranceiver cannot be reconfigured and we need to wait for the TX to finish
            if (txExpiration)
            {
                // Test for TX in progress, if so continue and wait until DIO1_TX_DONE or any other notification and this will be tested
                if (!CoreUtils::isUsReachedRaw(txExpiration))
                {
                    continue;
                }
                txExpiration = 0;
            }

            // When a packet is received, receive it and directly reconfigure the transceiver.. then send it to the bus
            if (notifyValue & TaskState::DIO1_RX_DONE)
            {
                // printf("Radio %d Packet RX: %s timeMs:%d\n", sx1262->radioNo, OpenAce::dataSourceToString(sx1262->rxRadioParameters.config.dataSource), CoreUtils::msInSecond());
                if (sx1262->rxRadioParameters.config.mode == Radio::Mode::GFSK)
                {
                    auto m = Measure("Receive receiveGFSKPacket (takes about 1.4ms)", 0);
                    bool doSend;
                    if (auto guard = aceSpi->getLock(doSend))
                    {
                        sx1262->receiveGFSKPacket();
                        sx1262->configureSx1262(sx1262->rxRadioParameters);
                        sx1262->listen();
                    }
                    if (doSend)
                    {
                        sx1262->sendToBus(sx1262->rxGfskMsg);
                    }
                }
                // clang-format on
                else if (sx1262->rxRadioParameters.config.mode == Radio::Mode::LORA)
                {
                    auto m = Measure("Receive receiveLoraPacket", 0);
                    bool doSend;
                    if (auto guard = aceSpi->getLock(doSend))
                    {
                        sx1262->receiveLORAPacket();
                        sx1262->configureSx1262(sx1262->rxRadioParameters);
                        sx1262->listen();
                    }
                    if (doSend)
                    {
                        sx1262->sendToBus(sx1262->rxLoraMsg);
                    }
                }
                // printf("%8ld RX Packet ds:%s\n", CoreUtils::timeUs32() / 1000, OpenAce::dataSourceToString(sx1262->rxRadioParameters.config.dataSource));
            };

            // Only in TX 
            if (TxPacket txPacket; sx1262->txQueue.pop(txPacket))
            {
                bool _;
                if (auto guard = aceSpi->getLock(_))
                {
                    auto m = Measure("Send Packet");
                    // printf("%8ld TX Packet ds:%s\n", CoreUtils::timeUs32() / 1000, OpenAce::dataSourceToString(txPacket.radioParameters.config.dataSource));
                    txExpiration = CoreUtils::timeUs32Raw() + 55000; // 55ms is longest packet expect (LORA)
                    sx1262->sendPacket(txPacket);
                    sx1262->statistics.transmittedPackets++;
                    continue; // Need to wait for TX done
                }
            }

            // Finally, if only a new configuration was set, reconfigure the tranceiver
            if (hasNewConfig)
            {
                if (auto g = SemaphoreGuard<5>(sx1262->xMutex))
                {
                    sx1262->rxRadioParameters = sx1262->newRxRadioParameters;
                    // printf("%8ld New Config ds:%s\n", CoreUtils::timeUs32() / 1000, OpenAce::dataSourceToString(sx1262->rxRadioParameters.config.dataSource));
                    hasNewConfig = false;
                }
                bool _;
                if (auto guard = aceSpi->getLock(_))
                {
                    sx1262->configureSx1262(sx1262->rxRadioParameters);
                    sx1262->listen();
                }
            }
        }
        else
        {
            // End up here when timeout, only count in statistics so we know this is happening
            if (sx1262->rxRadioParameters.config.dataSource != OpenAce::DataSource::NONE)
            {
                sx1262->statistics.taskTimeout++;
            }
        }
    }
}
