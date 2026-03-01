#pragma once

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "semphr.h"

#include "measure.hpp"
#include "constants.hpp"

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
#if GATAS_DEBUG == 1
        etl::array<uint8_t, 2> lastMsgPerCore;
#endif
        GATAS::GlobalPoolConfiguration *pool = nullptr;

    public:
        ThreadSafeBus() : etl::imessage_bus(router_list), xMutex(xSemaphoreCreateRecursiveMutex())
        {
        }

        ThreadSafeBus(etl::imessage_router &successor) : etl::imessage_bus(router_list, successor), xMutex(xSemaphoreCreateRecursiveMutex())
        {
        }

        void setPool(GATAS::GlobalPoolConfiguration *pool_)
        {
            pool = pool_;
        }

        void processMessage(const etl::imessage &message)
        {
            auto msgId = message.get_message_id();
            bool skipMutex;
            uint16_t blockTime;
            switch (msgId)
            {
            // These are message that must be delivered with high guarantee
            // therefor setting a higher lock timeout
            case 21: // AccessPointClientsMsg
            case 24: // WifiConnectionStateMsg
            case 29: // Every1SecMsg
                blockTime = 1000;
                skipMutex = false;
                break;
            case 20: // ConfigUpdatedMsg
                blockTime = 1;
                skipMutex = true;
                break;
            default:
                skipMutex = false;
                blockTime = 50;
            }
            if (skipMutex || xSemaphoreTakeRecursive(xMutex, TASK_DELAY_MS(blockTime)) == pdTRUE)
            {
                etl::imessage_bus::receive(message);

                if (!skipMutex)
                {
                    xSemaphoreGiveRecursive(xMutex);
                }
            }
            else
            {
                if (msgId >= 200 && msgId < 255 && pool != nullptr)
                {
                    reinterpret_cast<GATAS::PoolRelease &>(const_cast<etl::imessage &>(message)).releasePool(*pool);
                }
                GATAS_WARN("Message not send current:%d:%d core0:%d core1:%d", get_core_num(), message.get_message_id(), lastMsgPerCore[0], lastMsgPerCore[1]);
            }
        }

        //*******************************************
        virtual void receive(const etl::imessage &message) override
        {

#if GATAS_DEBUG == 1
            auto previousPerCore = lastMsgPerCore;
            (void)previousPerCore;
            auto currentMsgId = message.get_message_id();
            auto coreNum = get_core_num();
            lastMsgPerCore[coreNum] = currentMsgId;
#endif

#if GATAS_DEBUG == 1
            GATAS_MEASURE_M("", 20'000 /* 10'000 */);
            processMessage(message);
            if (measure)
            {
                printf("Bus Messages: current:%d:%d core0:%d core1:%d\n", get_core_num(), message.get_message_id(), previousPerCore[0], previousPerCore[1]);
            }
#else
            processMessage(message);
#endif

#if GATAS_DEBUG == 1
            lastMsgPerCore[get_core_num()] = 0;
#endif
        }

        //*******************************************
        virtual void
        receive(etl::shared_message shared_msg) override
        {
            // Note: Skipping Only for <20> / ConfigUpdatedMsg
            auto msgId = shared_msg.get_message().get_message_id();
            auto skipMutex = msgId == 20;
            if (skipMutex || (xSemaphoreTakeRecursive(xMutex, TASK_DELAY_MS(10)) == pdTRUE))
            {
                etl::imessage_bus::receive(shared_msg);
                if (!skipMutex)
                {
                    xSemaphoreGiveRecursive(xMutex);
                }
            }
            else
            {
                if (msgId >= 200 && msgId < 255 && pool != nullptr)
                {
                    reinterpret_cast<GATAS::PoolRelease &>(const_cast<etl::imessage &>(shared_msg.get_message())).releasePool(*pool);
                }
                GATAS_WARN("Message not send current:%d:%d core0:%d core1:%d", get_core_num(), msgId, lastMsgPerCore[0], lastMsgPerCore[1]);
            }
        }
    };
};
