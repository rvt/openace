#pragma once

#include "FreeRTOS.h"
#include "event_groups.h"

/**
 * @brief Wrapper class around FreeRTOS event groups for synchronization.
 * Usr this to syn event's between different tasks For an example check radiotunerrx.hpp
 */
class EventSync {
public:
    EventSync() {
        handle = xEventGroupCreate();
    }

    ~EventSync() {
        if (handle) {
            vEventGroupDelete(handle);
        }
    }

    // Prevent copying
    EventSync(const EventSync&) = delete;
    EventSync& operator=(const EventSync&) = delete;
    EventSync(EventSync&& other) = delete;
    EventSync& operator=(EventSync&& other) = delete;

    /**
     * @brief Set a bit indicating a event has been complated
     * 
     * @param bits 
     */
    void set(uint32_t bits) const {
        xEventGroupSetBits(handle, bits);
    }

    /**
     * @brief Clear the bit's, use this as a start condition
     * 
     * @param bits 
     */
    void clear(uint32_t bits) const {
        xEventGroupClearBits(handle, bits);
    }

    /**
     * @brief Wait untill the bit's have been set by set
     * 
     * @param bits 
     * @param timeout 
     * @return true  When the event was received,
     * @return false When teh event came into timeout
     */
    bool wait(uint32_t bits, TickType_t timeout) const {
        EventBits_t result = xEventGroupWaitBits(
            handle,
            bits,
            pdTRUE,     // clear on exit
            pdFALSE,    // wait for any
            timeout
        );
        return (result & bits) != 0;
    }

private:
    EventGroupHandle_t handle;
};
