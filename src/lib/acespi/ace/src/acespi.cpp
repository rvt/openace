#include "../acespi.hpp"

GATAS::PostConstruct AceSpi::postConstruct()
{
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr)
    {
        return GATAS::PostConstruct::MUTEX_ERROR;
    }

    // Reset is active-low, so we'll initialise it to a driven-high state
    gpio_init(rst);
    gpio_set_dir(rst, GPIO_OUT);
    gpio_put(rst, 1);

    // Set default SPI bus frequency
    spi_init(spi?spi1:spi0, GATAS_SPI_DEFAULT_BUS_FREQUENCY * 1000 * 1000);
    gpio_set_function(miso, GPIO_FUNC_SPI);
    gpio_set_function(clk, GPIO_FUNC_SPI);
    gpio_set_function(mosi, GPIO_FUNC_SPI);

    // Reset ALL devices
    resetDevices();
    GATAS_LOG("Initialised on miso:%d mosi:%d clk:%d  rst:%d spi:%d -> (devices reset)", miso, mosi, clk, rst, spi);

    // Make the SPI pins available to picotool
    bi_decl(bi_3pins_with_func(static_cast<uint32_t>(miso), static_cast<uint32_t>(mosi), static_cast<uint32_t>(clk), GPIO_FUNC_SPI));
    return GATAS::PostConstruct::OK;
}

void AceSpi::start()
{
    getBus().subscribe(*this);
};

void AceSpi::stop()
{
    getBus().unsubscribe(*this);
};

void AceSpi::read_registers(uint8_t cs, uint8_t reg, uint8_t *buf, uint16_t len, uint8_t delayMs) const
{
    reg |= READ_BIT;
    cs_select(cs);
    spi_write_blocking(spi?spi1:spi0, &reg, 1);
    vTaskDelay(TASK_DELAY_MS(delayMs));
    spi_read_blocking(spi?spi1:spi0, 0, buf, len);
    cs_deselect(cs);
    vTaskDelay(TASK_DELAY_MS(delayMs));
}

void AceSpi::read_registers_select(uint8_t cs, uint8_t reg) const
{
    reg |= READ_BIT;
    cs_select(cs);
    spi_write_blocking(spi?spi1:spi0, &reg, 1);
}

void AceSpi::read_registers_read(uint8_t cs, uint8_t *buf, uint16_t len) const
{
    spi_read_blocking(spi?spi1:spi0, 0, buf, len);
    cs_deselect(cs);
}

void AceSpi::cs_select(uint8_t cs) const
{
    gpio_put(cs, 0); // Active low
}

void AceSpi::cs_deselect(uint8_t cs) const
{
    gpio_put(cs, 1);
}

void AceSpi::write_array(uint8_t cs, uint8_t *data, uint8_t length, uint8_t delayMs) const
{
    cs_select(cs);
    spi_write_blocking(spi?spi1:spi0, data, length);
    cs_deselect(cs);
    vTaskDelay(TASK_DELAY_MS(delayMs));
}

void AceSpi::write_byte(uint8_t cs, uint8_t data, uint8_t delayMs) const
{
    write_array(cs, &data, 1, delayMs);
}

/**
 * Reset all attached devices that is using the rst pin
 */
void AceSpi::resetDevices() const
{
    gpio_put(rst, 0);
    vTaskDelay(TASK_DELAY_MS(100));
    gpio_put(rst, 1);
}

SpiGuard AceSpi::getLock(bool &locked){
    // if (lastBusFrequency != busFrequencyMhz)
    // {
    //     lastBusFrequency = busFrequencyMhz;
    //     spi_init(spi?spi1:spi0, lastBusFrequency * 1'000'000);
    // }
    return SpiGuard(mutex, locked);
};

void AceSpi::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

uint8_t AceSpi::spiNum() const {
    return spi;
};
