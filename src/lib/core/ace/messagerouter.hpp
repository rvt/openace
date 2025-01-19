#pragma once

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "semphr.h"

//#include "coreutils.hpp"

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
        etl::vector<etl::imessage_router *, MAX_ROUTERS_> router_list;
        SemaphoreHandle_t xMutex;

    public:
        ThreadSafeBus() : etl::imessage_bus(router_list), xMutex(nullptr)
        {
            xMutex = xSemaphoreCreateRecursiveMutex();
        }

        ThreadSafeBus(etl::imessage_router &successor) : etl::imessage_bus(router_list, successor), xMutex(nullptr)
        {
            xMutex = xSemaphoreCreateRecursiveMutex();
        }

        virtual ~ThreadSafeBus()
        {
            // MessageBus will be active for a lifetime
            vSemaphoreDelete(xMutex);
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
                etl::imessage_bus::receive(message);
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
                etl::imessage_bus::receive(shared_msg);
                if (!skipMutex)
                {
                    xSemaphoreGiveRecursive(xMutex);
                }
            }
        }
    };
};
