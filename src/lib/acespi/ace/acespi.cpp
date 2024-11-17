#include "acespi.hpp"

OpenAce::PostConstruct AceSpi::postConstruct()
{
    // Make the SPI pins available to picotool
    bi_decl(bi_3pins_with_func(static_cast<uint32_t>(miso), static_cast<uint32_t>(mosi), static_cast<uint32_t>(clk), GPIO_FUNC_SPI));

    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr)
    {
        return OpenAce::PostConstruct::MUTEX_ERROR;
    }

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(rst);
    gpio_set_dir(rst, GPIO_OUT);
    gpio_put(rst, 1);

    // Set default SPI bus frequency
    spi_init(OPENACE_SPI_DEFAULT, OPENOPENACE_SPI_DEFAULT_BUS_FREQUENCY * 1000 * 1000);
    gpio_set_function(miso, GPIO_FUNC_SPI);
    gpio_set_function(clk, GPIO_FUNC_SPI);
    gpio_set_function(mosi, GPIO_FUNC_SPI);

    // Reset ALL devices
    resetDevices();
    printf("Initialised on miso:%d clk:%d mosi:%d rst:%d (devices reset) ", miso, clk, mosi, rst);
    return OpenAce::PostConstruct::OK;
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
    spi_write_blocking(OPENACE_SPI_DEFAULT, &reg, 1);
    vTaskDelay(TASK_DELAY_MS(delayMs));
    spi_read_blocking(OPENACE_SPI_DEFAULT, 0, buf, len);
    cs_deselect(cs);
    vTaskDelay(TASK_DELAY_MS(delayMs));
}

void AceSpi::read_registers_select(uint8_t cs, uint8_t reg) const
{
    reg |= READ_BIT;
    cs_select(cs);
    spi_write_blocking(OPENACE_SPI_DEFAULT, &reg, 1);
}

void AceSpi::read_registers_read(uint8_t cs, uint8_t *buf, uint16_t len) const
{
    spi_read_blocking(OPENACE_SPI_DEFAULT, 0, buf, len);
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
    spi_write_blocking(OPENACE_SPI_DEFAULT, data, length);
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

bool AceSpi::acquireSlotSyncCb(uint8_t busFrequencyMhz, const etl::delegate<void()>& delegate) {
    
    if (acquireSlotSync(busFrequencyMhz))
    {
        delegate();
        releaseSlotSync();
        return true;
    }

    return false;
}

bool AceSpi::acquireSlotSync(uint8_t busFrequencyMhz)
{
    if (xSemaphoreTake(mutex, TASK_DELAY_MS(10)) == pdTRUE)
    {
        if (lastBusFrequency != busFrequencyMhz)
        {
            lastBusFrequency = busFrequencyMhz;
            spi_init(OPENACE_SPI_DEFAULT, lastBusFrequency * 1000'000);
        }
        return true;
    }

    return false;
}

void AceSpi::releaseSlotSync()
{
    xSemaphoreGive(mutex);
}

void AceSpi::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}
