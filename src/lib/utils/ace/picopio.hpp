#pragma once

#include "hardware/pio.h"
#include "stdint.h"

// Find a free pio and state machine and load the program into it.
// Returns false if this fails
bool add_pio_program(const pio_program_t *program, PIO *pio_hw, int *sm, uint *offset);

