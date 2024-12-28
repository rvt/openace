#pragma once

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "semphr.h"

#include "coreutils.hpp"

/* Vendor. */
#include "etl/message_router.h"
#include "etl/message_bus.h"
#include "etl/vector.h"

// The successor functionality is to allow routers to be chained together, so that if a message is not handled by the current router, then it will be passed on to the next.
// Should we use a queue ?? QueuedMessageRouter.cpp
namespace OpenAce
{

    template <uint_least8_t MAX_ROUTERS_>
    class ThreadSafeBus : public etl::imessage_bus
    {
        struct
        {
            uint32_t mutexErr = 0;
            uint32_t totalMessages = 0;
        } statistics;

        etl::vector<etl::imessage_router *, MAX_ROUTERS_> router_list;
        SemaphoreHandle_t xMutex;
        uint32_t lastMessages;
        uint32_t lastTime;

    public:
        ThreadSafeBus() : etl::imessage_bus(router_list), xMutex(nullptr), lastMessages(0), lastTime(0)
        {
            xMutex = xSemaphoreCreateRecursiveMutex();
        }

        ThreadSafeBus(etl::imessage_router &successor) : etl::imessage_bus(router_list, successor), xMutex(nullptr), lastMessages(0), lastTime(0)
        {
            xMutex = xSemaphoreCreateRecursiveMutex();
        }

        virtual ~ThreadSafeBus()
        {
            // MessageBus will be active for a lifetime
            vSemaphoreDelete(xMutex);
        }

        /**
         * Calculate total messages per second that goes over the messagebus
         * Returns 0 if no calculation could be made
         */
        uint16_t messagesPerSec()
        {
            auto usBoot = CoreUtils::timeUs32();
            auto elapsed = CoreUtils::usToReference(lastTime, usBoot);
            if (elapsed < -100'000)
            {
                // Calculate number of messages per second
                lastMessages = statistics.totalMessages;
                lastTime = usBoot;
                return (statistics.totalMessages - lastMessages) * (1'000'000 / elapsed);
            }
            return 0;
        }

        //*******************************************
        virtual void receive(const etl::imessage &message) override
        {
            /**
             * Specify extra delay for configuration changes to ensure they are delivered
             * TODO: For development purpose create statistics of the message ID's per
             */

            // When updating configurations the mutex did not work, Not sure yet why this was
            // So it's now hacked to not use a mutex
            auto skipMutex = message.get_message_id() == 20;
            if (skipMutex || (xSemaphoreTakeRecursive(xMutex, TASK_DELAY_MS(10)) == pdTRUE))
            {
                statistics.totalMessages++;
                etl::imessage_bus::receive(etl::imessage_router::ALL_MESSAGE_ROUTERS, message);
                if (!skipMutex)
                {
                    xSemaphoreGiveRecursive(xMutex);
                }
            }
        }

        //*******************************************
        virtual void receive(etl::shared_message shared_msg) override
        {

            /**
             * Specify extra delay for configuration changes to ensure they are delivered
             */
            // When updating configurations the mutex did not work, Not sure yet why this was
            // So it's now hacked to not use a mutex
            auto skipMutex = shared_msg.get_message().get_message_id() == 20;
            if (skipMutex || (xSemaphoreTakeRecursive(xMutex, TASK_DELAY_MS(10)) == pdTRUE))
            {
                statistics.totalMessages++;
                etl::imessage_bus::receive(etl::imessage_router::ALL_MESSAGE_ROUTERS, shared_msg);
                if (!skipMutex)
                {
                    xSemaphoreGiveRecursive(xMutex);
                }
            }
        }
    };
};
