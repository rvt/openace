#include "bmp280.hpp"
#include "ace/coreutils.hpp"

void Bmp280::start()
{
    xTaskCreate(bmp280Task, "Bmp280Task", configMINIMAL_STACK_SIZE + 128, this, tskIDLE_PRIORITY, &taskHandle);
    getBus().subscribe(*this);
};

void Bmp280::stop()
{
    getBus().unsubscribe(*this);
    xTaskNotify(taskHandle, 1, eSetBits);
};

void Bmp280::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == NAME)
    {
        compensation = msg.config.valueByPath(0, NAME, "compensation");
    }
}

void Bmp280::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void Bmp280::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"lastPressurehPa\":" << etl::format_spec{}.precision(1) << statistics.lastPressurehPa << OpenAce::RESET_FORMAT;
    stream << ",\"compensation\":" << compensation;
    stream << "}\n";
}

/* The following compensation functions are required to convert from the raw ADC
data from the chip to something usable. Each chip has a different set of
compensation parameters stored on the chip at point of manufacture, which are
read from the chip at startup and used in these routines.
*/
int32_t Bmp280::compensate_temp(int32_t adc_T)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}

uint32_t Bmp280::compensate_pressure(int32_t adc_P)
{
    int32_t var1, var2;
    uint32_t p;
    var1 = (((int32_t)t_fine) >> 1) - (int32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)dig_P6);
    var2 = var2 + ((var1 * ((int32_t)dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)dig_P4) << 16);
    var1 = (((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) + ((((int32_t)dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)dig_P1)) >> 15);
    if (var1 == 0)
        return 0;

    p = (((uint32_t)(((int32_t)1048576) - adc_P) - (var2 >> 12))) * 3125;
    if (p < 0x80000000)
        p = (p << 1) / ((uint32_t)var1);
    else
        p = (p / (uint32_t)var1) * 2;

    var1 = (((int32_t)dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)dig_P8)) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + dig_P7) >> 4));

    return p;
}

/* This function reads the manufacturing assigned compensation parameters from the device */
void Bmp280::read_compensation_parameters()
{
    SpiModule *aceSpi = static_cast<SpiModule *>(BaseModule::moduleByName(*this, SpiModule::NAME));
    uint8_t buffer[26];

    aceSpi->read_registers(cs, 0x88, buffer, 24, 20);

    dig_T1 = buffer[0] | (buffer[1] << 8);
    dig_T2 = buffer[2] | (buffer[3] << 8);
    dig_T3 = buffer[4] | (buffer[5] << 8);

    dig_P1 = buffer[6] | (buffer[7] << 8);
    dig_P2 = buffer[8] | (buffer[9] << 8);
    dig_P3 = buffer[10] | (buffer[11] << 8);
    dig_P4 = buffer[12] | (buffer[13] << 8);
    dig_P5 = buffer[14] | (buffer[15] << 8);
    dig_P6 = buffer[16] | (buffer[17] << 8);
    dig_P7 = buffer[18] | (buffer[19] << 8);
    dig_P8 = buffer[20] | (buffer[21] << 8);
    dig_P9 = buffer[22] | (buffer[23] << 8);
}

OpenAce::PostConstruct Bmp280::postConstruct()
{
    SpiModule *aceSpi = static_cast<SpiModule *>(BaseModule::moduleByName(*this, SpiModule::NAME));
    if (aceSpi == nullptr)
    {
        return OpenAce::PostConstruct::DEP_NOT_FOUND;
    }

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(cs);
    gpio_set_dir(cs, GPIO_OUT);
    gpio_put(cs, 1);

    // Make the SPI pins available to picotool
    bi_decl(bi_1pin_with_name(static_cast<uint32_t>(cs), "Bmp280 CS"));

    // See if SPI is working - interrograte the device for its I2C ID number, should be 0x60(BME280) or 0x58(BMP280)
    uint8_t chipId;
    aceSpi->read_registers(cs, 0xD0, &chipId, 1, 10);
    if (chipId != 0x58 && chipId != 0x60)
    {
        printf("Bmp280 Chip ID supports 0x58,0x60 but found 0x%x\n", chipId);
        return OpenAce::PostConstruct::HARDWARE_NOT_FOUND;
    }

    read_compensation_parameters();
    uint8_t buf[2] = {0xF4 & 0x7f, 0x27};
    aceSpi->write_array(cs, buf, sizeof(buf), 10); // Set rest of oversampling modes and run mode to normal

    printf("Initialised on cs:%d ChipID:0x%x ", cs, chipId);
    return OpenAce::PostConstruct::OK;
}

void Bmp280::bmp280Task(void *arg)
{
    Bmp280 *bmp280 = static_cast<Bmp280 *>(arg);
    SpiModule *aceSpi = static_cast<SpiModule *>(BaseModule::moduleByName(*bmp280, SpiModule::NAME));
    while (true)
    {
        if (uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(15'000)))
        {
            if ((notifyValue & 1) == 1)
            {
                vTaskDelete(nullptr);
                return;
            }

        } else {

            uint8_t buffer[8]; // I think this can be buffer[6] (No humidity needed)
            if (aceSpi->acquireSlotSyncCb(OPENOPENACE_SPI_DEFAULT_BUS_FREQUENCY, [&aceSpi, &bmp280, &buffer]()
            {
                aceSpi->read_registers_select(bmp280->cs, 0xF7);
                aceSpi->read_registers_read(bmp280->cs, buffer, sizeof(buffer));
            })) {
                int32_t pressure = ((uint32_t)buffer[0] << 12) | ((uint32_t)buffer[1] << 4) | (buffer[2] >> 4);
                int32_t temperature = ((uint32_t)buffer[3] << 12) | ((uint32_t)buffer[4] << 4) | (buffer[5] >> 4);

                temperature = bmp280->compensate_temp(temperature);
                pressure = bmp280->compensate_pressure(pressure);

                auto value = (pressure + bmp280->compensation) / 100.0f;
                bmp280->statistics.lastPressurehPa = value;
                bmp280->getBus().receive(OpenAce::BarometricPressure{value, CoreUtils::timeUs32()});
            };
        }
    }
}
