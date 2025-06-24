#include "../driver/src/sx126x_hal.h"

#include "sx1262.hpp"

/* FreeRTOS. */
#include "FreeRTOS.h"

/* PICO. */
#include "hardware/spi.h"
#include "pico/stdlib.h"

/* GATAS. */
#include "ace/basemodule.hpp"
#include "ace/coreutils.hpp"

/**
 * Wait for BUZY pin to go low or timeout.
 * When BUZY is low, the SX1262 is ready for new commands
 * returns 0 for sucess, 1 for timeout
 * This currently uses a spinlock with a timeoiut function
 */
uint8_t sx126x_buzy_wait(uint8_t busyPin)
{
    auto timeoutTime = CoreUtils::timeUs32Raw() + 1'000'000;
    uint16_t counter = 1; // to reduce the number of calls to timer functions
    while (gpio_get(busyPin))
    {
        if (counter % 100 == 0) {
            if (CoreUtils::isUsReachedRaw(timeoutTime))
            {
                return 1;
            }
        }
        counter++;
    }
    return 0;
}

sx126x_hal_status_t sx126x_hal_write(const void *context, const uint8_t *command, const uint16_t command_length,
                                     const uint8_t *data, const uint16_t data_length)
{
    Sx1262 *sx1262 = (Sx1262 *)context;
    SpiModule *spi = sx1262->spi();

    sx126x_hal_status_t ret = SX126X_HAL_STATUS_OK;
    if (sx126x_buzy_wait(sx1262->busy()))
    {
        puts("hal write, Wait busy timeout");
        ret = SX126X_HAL_STATUS_ERROR;
    }
    else
    {
        spi->cs_select(sx1262->cs());
        spi_write_blocking(spi->spiNum()?spi1:spi0, command, command_length);
        if (data_length != 0)
        {
            spi_write_blocking(spi->spiNum()?spi1:spi0, data, data_length);
        }
    }
    spi->cs_deselect(sx1262->cs());
    return ret;
}

sx126x_hal_status_t sx126x_hal_read(const void *context, const uint8_t *command, const uint16_t command_length,
                                    uint8_t *data, const uint16_t data_length)
{
    Sx1262 *sx1262 = (Sx1262 *)context;
    SpiModule *spi = sx1262->spi();

    sx126x_hal_status_t ret = SX126X_HAL_STATUS_OK;
    if (sx126x_buzy_wait(sx1262->busy()))
    {
        puts("hal read, Wait busy timeout");
        ret = SX126X_HAL_STATUS_ERROR;
    }
    else
    {
        spi->cs_select(sx1262->cs());
        int length = spi_write_blocking(spi->spiNum()?spi1:spi0, command, command_length);
        if (length != command_length)
        {
            puts("sx126x_hal_read write error");
            ret = SX126X_HAL_STATUS_ERROR;
        }
        else
        {
            length = spi_read_blocking(spi->spiNum()?spi1:spi0, 0, data, data_length);
            if (length != data_length)
            {
                puts("sx126x_hal_read read error");
                ret = SX126X_HAL_STATUS_ERROR;
            }
        }
    }
    spi->cs_deselect(sx1262->cs());
    return ret;
}

sx126x_hal_status_t sx126x_hal_reset(const void *context)
{
    // Reset is already given once when the SPI starts up
    (void)context;
    puts("SX1262 Reset called");
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_wakeup(const void *context)
{
    (void)context;
    puts("SX1262 wakeup called");
    return SX126X_HAL_STATUS_OK;
}
