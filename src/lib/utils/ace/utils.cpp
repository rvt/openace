#include "utils.hpp"
#include "stdio.h"

/**
 * Find a free pio and state machine and load the program into it.
 * Returns false if this fails
*/
bool add_pio_program(const pio_program_t *program, PIO *pio_hw, int *sm, uint *offset)
{
#if defined(PICO_RP2040) || defined(PICO_RP2350)
    *pio_hw = pio0;
    if (!pio_can_add_program(*pio_hw, program))
    {
        *pio_hw = pio1;
        if (!pio_can_add_program(*pio_hw, program))
        {
            *offset = -1;
            return false;
        }
    }
    *offset = pio_add_program(*pio_hw, program);
    *sm = (int8_t)pio_claim_unused_sm(*pio_hw, false);
    if (*sm < 0)
    {
        return false;
    }
#endif
    return true;
}


/**
 * Dump a buffer as a hexidecimal string for terminal output
*/
void print_buffer(uint8_t *buffer, uint8_t length)
{
    printf("Length(%d) ", length);
    for (uint8_t i = 0; i < length; i++)
    {
        printf("0x%02X", buffer[i]);
        if (i < length-1)
        {
            printf(", ");
        }
    }
}


