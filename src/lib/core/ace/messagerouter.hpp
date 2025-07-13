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
namespace GATAS
{
    template <uint_least8_t MAX_ROUTERS_>
    class ThreadSafeBus : public etl::imessage_bus
    {
        etl::vector<etl::imessage_router *, MAX_ROUTERS_> router_list;
        SemaphoreHandle_t xMutex;
#ifndef NDEBUG
        etl::array<uint8_t, 2> lastMsgPerCore;
#endif
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
            bool skipMutex = message.get_message_id() == 20; // <20> is ConfigUpdateMsg
            if (skipMutex || xSemaphoreTakeRecursive(xMutex, TASK_DELAY_MS(50)) == pdTRUE)
            {
                etl::imessage_bus::receive(message);

                if (!skipMutex)
                {
                    xSemaphoreGiveRecursive(xMutex);
                }
            }
            else
            {
#ifndef NDEBUG
                printf("Message not send current:%d:%d core0:%d core1:%d\n", get_core_num(), message.get_message_id(), lastMsgPerCore[0], lastMsgPerCore[1]);
#endif
            }
        }

        //*******************************************
        virtual void receive(const etl::imessage &message) override
        {

#ifndef NDEBUG
            auto previousPerCore = lastMsgPerCore;
            auto currentMsgId = message.get_message_id();
            auto coreNum = get_core_num();
            lastMsgPerCore[coreNum] = currentMsgId;
#endif

#ifndef NDEBUG
            auto measure = Measure("Message bus", 10'000);
            processMessage(message);
            if (measure)
            {
                printf("Bus Messages: current:%d:%d core0:%d core1:%d\n", get_core_num(), message.get_message_id(), previousPerCore[0], previousPerCore[1]);
            }
#else
            processMessage(message);
#endif

#ifndef NDEBUG
            lastMsgPerCore[get_core_num()] = 0;
#endif
        }

    //*******************************************
    virtual void
    receive(etl::shared_message shared_msg) override
    {
        // Note: Skipping Only for <20> / ConfigUpdatedMsg
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
}
;
