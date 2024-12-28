#include <stdio.h>

#include "pioserial.hpp"

#include "ace/utils.hpp"

OpenAce::PostConstruct PioSerial::postConstruct()
{

    int numElements = sizeof(interruptHandlers) / sizeof(interruptHandlers[0]);
    while (handlerIdx < numElements && interruptHandlers[handlerIdx] != nullptr)
    {
        ++handlerIdx;
    }
    if (handlerIdx >= numElements)
    {
        return OpenAce::PostConstruct::HARDWARE_ERROR;
    }
    interruptHandlers[handlerIdx] = this;

    // Set tx to out to prevent it from floating. Attached devices might receive random data
    gpio_init(txPin);
    gpio_set_dir(txPin, GPIO_OUT);
    gpio_put(txPin, 1);

    // Enable Rx
    if (!enableRx())
    {
        return OpenAce::PostConstruct::HARDWARE_ERROR;
    }

    static_assert(PIO0_IRQ_1 == PIO0_IRQ_0 + 1 && PIO1_IRQ_1 == PIO1_IRQ_0 + 1, "");
    uint8_t pio_irq = (rxPio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0; // pio_irq will become 7,8,9,10
    if (irq_get_exclusive_handler(pio_irq))
    {
        pio_irq++;
        if (irq_get_exclusive_handler(pio_irq))
        {
            return OpenAce::PostConstruct::HARDWARE_ERROR;
        }
    }

    switch (pio_irq)
    {
    case PIO0_IRQ_0:
        handler = pio0_irq0_func_handler;
        break;
    case PIO0_IRQ_1:
        handler = pio0_irq1_func_handler;
        break;
    case PIO1_IRQ_0:
        handler = pio1_irq0_func_handler;
        break;
    case PIO1_IRQ_1:
        handler = pio1_irq1_func_handler;
        break;
    default:
        return OpenAce::PostConstruct::HARDWARE_NOT_FOUND;
        ;
    }
    return OpenAce::PostConstruct::OK;
}

void PioSerial::start()
{
    // Enable interrupt
    uint8_t pio_irq = (rxPio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0; // pio_irq will become 7,8,9,10
    uint8_t irq_index = pio_irq - ((rxPio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0);
    irq_add_shared_handler(pio_irq, handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);                                     // Add a shared IRQ handler
    irq_set_enabled(pio_irq, true);                                                                                               // Enable the IRQ
    pio_set_irqn_source_enabled(rxPio, irq_index, static_cast<pio_interrupt_source>(pis_sm0_rx_fifo_not_empty + rxSmIndx), true); // Set pio to tell us when the FIFO is NOT empty
};

void PioSerial::stop()
{
    uint8_t pio_irq = (rxPio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0; // pio_irq will become 7,8,9,10
    uint8_t irq_index = pio_irq - ((rxPio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0);

    // Disable interrupt
    pio_set_irqn_source_enabled(rxPio, irq_index, static_cast<pio_interrupt_source>(pis_sm0_rx_fifo_not_empty + rxSmIndx), false);
    irq_set_enabled(pio_irq, false);
    irq_remove_handler(pio_irq, handler);

    // Disable Rx
    disableRx();

    // Disable Tx
    disableTx();
};

bool PioSerial::enableRx()
{

    if (rxPio == nullptr)
    {
        // Set up the state machine to use to use
        if (!add_pio_program(&uart_rx_program, &rxPio, &rxSmIndx, &rxOffset))
        {
            return false;
        }
        uart_rx_program_init(rxPio, rxSmIndx, rxOffset, rxPin, baudrate);
    }
    return true;
}

void PioSerial::disableRx()
{

    if (rxPio != nullptr)
    {
        // Cleanup Pio
        pio_sm_set_enabled(rxPio, rxSmIndx, false);
        pio_remove_program(rxPio, &uart_rx_program, rxOffset);
        pio_sm_unclaim(rxPio, rxSmIndx);

        // Remove the handler
        rxSmIndx = -1;
        rxOffset = 0;
    }
}

/**
 * IRQ called when the pio fifo is not empty, i.e. there are some characters on the uart
 * When a NMEA string is found, it will send a message using FReeRTOS
 * The message is guaranteed to be zero terminated
 */
void __isr __time_critical_func(PioSerial::pio_irq_func)(uint8_t irqHandlerIndex)
{
    UBaseType_t savedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

    PioSerial &pioSerial = *interruptHandlers[irqHandlerIndex];
    while (!pio_sm_is_rx_fifo_empty(pioSerial.rxPio, pioSerial.rxSmIndx))
    {
        uint32_t data =  pioSerial.rxPio->rxf[pioSerial.rxSmIndx];
        char *bytePtr = (char*)&data;
        for (int i = 0; i < 4; i++)
        {
            char c = bytePtr[i];
            if (c == '\n' || c == '\r')
            {
                if (pioSerial.charIndex > 1)
                {
                    pioSerial.buffer[pioSerial.charIndex] = '\0';
                    pioSerial.callback(pioSerial.buffer);
                }
                pioSerial.charIndex = 0;
            }
            else if (pioSerial.charIndex >= OpenAce::NMEA_MAX_LENGTH)
            {
                pioSerial.charIndex = 0;
            }
            else
            {
                pioSerial.buffer[pioSerial.charIndex++] = c;
            }
        }
    }

    taskEXIT_CRITICAL_FROM_ISR(savedInterruptStatus);
}

void PioSerial::sendBlocking(const uint8_t *data, uint16_t length)
{
    uart_tx_program_put(txPio, txSmIndx, data, length);
}

bool PioSerial::enableTx(uint32_t givenBaudRate)
{
    if (txPio == nullptr)
    {
        if (!add_pio_program(&uart_tx_program, &txPio, &txSmIndx, &txOffset))
        {
            puts("failed to setup pio for tx");
            return false;
        }
    }
    uart_tx_program_init(txPio, txSmIndx, txOffset, txPin, givenBaudRate);
    return true;
}

void PioSerial::disableTx()
{
    if (txPio != nullptr)
    {
        pio_sm_set_enabled(txPio, txSmIndx, false);
        pio_remove_program(txPio, &uart_tx_program, txOffset);
        pio_sm_unclaim(txPio, txSmIndx);

        txPio = nullptr;
        txOffset = 0;
        txSmIndx = 0;
    }
}

bool PioSerial::setBaudRate(uint32_t baudRate)
{
    if (rxPio != nullptr)
    {
        pio_sm_set_enabled(rxPio, rxSmIndx, false);
        uart_rx_program_init(rxPio, rxSmIndx, rxOffset, rxPin, baudRate);
        return true;
    }
    return false;
}

/**
 * Validate if the uart is receiving any valid data at the given baudrate
 */
bool PioSerial::testUartAtBaudrate(uint32_t testBaudRate, uint32_t maximumScanTimeMs, uint32_t ignoreFirstMs, uint16_t numcharsConsideringValid)
{
    if (rxPio != nullptr)
    {
        setBaudRate(testBaudRate);
        bool hasData = uart_rx_program_test(rxPio, rxSmIndx, 0x0a, 0x80, maximumScanTimeMs, ignoreFirstMs, numcharsConsideringValid);
        setBaudRate(baudrate);
        return hasData;
    }
    return false;
}

/**
 * Find a baudrate where the uart is sending data on
 */
uint32_t PioSerial::findBaudRate(uint32_t maxTimeOutMs)
{
    for (uint32_t baudRate : commonBaudrates)
    {
        // printf("Scanning %ldBd\n", baudRate);
        if (testUartAtBaudrate(baudRate, maxTimeOutMs))
        {
            return baudRate;
        }
    }
    return 0;
}

bool PioSerial::rxFlush(uint32_t timeOutMs)
{
    return uart_rx_flush(rxPio, rxSmIndx, timeOutMs);
}
