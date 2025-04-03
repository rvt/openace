#pragma once

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "semphr.h"

#include "measure.hpp"

/* Vendor. */
#include "etl/message_router.h"
#include "etl/message_bus.h"
#include "etl/vector.h"

// The successor functionality is to allow routers to be chained together, so that if a message is not handled by the current router, then it will be passed on to the next.
// Should we use a queue ?? QueuedMessageRouter.cpp
namespace OpenAce
{
    static constexpr uint8_t SHOW_MESSAGEBUS_TIMING = true;

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

        void processMessage(const etl::imessage &message)
        {
            // Note: Skipping to mutex does not seem to harm performance
            bool skipMutex = (message.get_message_id() == 20);
            if (skipMutex || xSemaphoreTakeRecursive(xMutex, TASK_DELAY_MS(50)) == pdTRUE)
            {
                etl::imessage_bus::receive(message);

                if (!skipMutex)
                {
                    xSemaphoreGiveRecursive(xMutex);
                }
            } else {
                puts("Message not send");
            }
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

            if constexpr (SHOW_MESSAGEBUS_TIMING)
            {
                auto m = Measure("Message bus", 15000, message.get_message_id() );
                processMessage(message);        
            }
            else
            {
                processMessage(message);
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
