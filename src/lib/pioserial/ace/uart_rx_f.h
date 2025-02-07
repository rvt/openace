#pragma once

#include "hardware/clocks.h"
#include "hardware/gpio.h"

static inline void uart_rx_program_init(PIO pio, uint sm, uint offset, uint pin, uint baudRate)
{
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
    pio_gpio_init(pio, pin);
    gpio_pull_up(pin);

    pio_sm_config c = uart_rx_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin); // for WAIT, IN
    sm_config_set_jmp_pin(&c, pin); // for JMP
    // Shift to right, autopush disabled
    sm_config_set_in_shift(&c, true, false, 32);
    // Deeper FIFO as we're not doing any TX
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    // SM transmits 1 bit per 8 execution cycles.
    float div = (float)clock_get_hz(clk_sys) / (8 * baudRate);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

/**
 * Test for characters on the uart.
 * If any characters they must be within rangeStart and rangeEnd to be considered valid (rangeStart must be > 0x00)
 * maximumScanTimeMs maximum time to wait for valid characters
 * numcharsConsideringValid        if this many characters are within rangeStart and rangeEnd then return true
 * ignoreFirstMs                   Ignore the first few bytes to ensure the uart is stable, we might get one or two bad bytes (5ms is aprox 6 bytes at 9600 baud)
 */
static inline uint8_t uart_rx_program_test(
    PIO pio,
    uint sm,
    char rangeStart,
    char rangeEnd,
    uint32_t maximumScanTimeMs,
    uint16_t numcharsConsideringValid = 64)
{
    uint16_t validCharCounter = 0;
    uint32_t start = time_us_32();
    uint32_t endTime = start + (maximumScanTimeMs * 1000);

    // Ignore first fifo fetches to ignore any stale data
    uint8_t ignoreFirstRounds = 5;
    while (true)
    {
        uint32_t currentTime = time_us_32();
        if (currentTime > endTime)
        {
            return 2;
        }

        // Only process if FIFO is not empty
        while (!pio_sm_is_rx_fifo_empty(pio, sm))
        {
            uint32_t data = pio_sm_get(pio, sm);
            char *bytePtr = (char *)&data;
            for (uint8_t i = 0; i < 4; i++)
            {
                if (ignoreFirstRounds == 0)
                {
                    char receivedChar = bytePtr[i];
                    if (receivedChar >= rangeStart && receivedChar <= rangeEnd)
                    {
                        validCharCounter++;
                        if (validCharCounter >= numcharsConsideringValid)
                        {
                            return 0;
                        }
                    }
                    else
                    {
                        return 1;
                    }
                }
                else
                {
                    ignoreFirstRounds--;
                }
            }
        }

    }
}

// Flush the RX buffer
static inline void uart_rx_flush(PIO pio, uint sm)
{
    while (!pio_sm_is_rx_fifo_empty(pio, sm))
    {
        pio_sm_get(pio, sm);
    }
}
