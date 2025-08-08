#include <stdint.h>
#include "../flashstore.hpp"

#include "pico/flash.h"

extern char __flash_binary_end;

typedef struct
{
    size_t size;
    uint32_t address;
    const uint8_t *p1;
} mutation_operation_t;

FlashStore::FlashStore(uint16_t size_, uint32_t offsetFromEnd_) : ConfigStore(),
    size(((size_+FLASH_SECTOR_SIZE-1) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE),
    offsetFromEnd(((offsetFromEnd_ + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE)
{
#if defined(RUN_FREERTOS_ON_CORE)
    flash_safe_execute_core_init();
#endif
}

uint32_t FlashStore::address() const
{
    return (PICO_FLASH_SIZE_BYTES - size - offsetFromEnd);
}

void FlashStore::rewind()
{
    // NOOP
}

size_t FlashStore::write(uint8_t c)
{
    (void)c;
    panic("Operation of one byte not supported in FlashStore");
}

/**
 * Flashing must be run out of memory, hence using __not_in_flash_func
 */
void __not_in_flash_func(pico_flash_bank_perform_flash_mutation_operation)(void *param)
{
    const mutation_operation_t *mop = (const mutation_operation_t *)param;
    uint16_t length = ((mop->size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;
    uint16_t bytesWritten = 0;

    for (uint32_t page = 1; page<=((length + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE); page++)
    {
        flash_range_erase(mop->address, FLASH_SECTOR_SIZE * page);
    }

    while (bytesWritten < length)
    {
        flash_range_program(mop->address + bytesWritten, mop->p1 + bytesWritten, FLASH_PAGE_SIZE);
        bytesWritten += FLASH_PAGE_SIZE;
    }
}


size_t FlashStore::write(const uint8_t *buffer, size_t length)
{
    mutation_operation_t mop =
    {
        .size = length,
        .address = address(),
        .p1 = buffer
    };
    flash_safe_execute(pico_flash_bank_perform_flash_mutation_operation, &mop, UINT32_MAX);

    return length;
}

const uint8_t *FlashStore::data() const
{
    return (const uint8_t *)(XIP_BASE + address());
}

