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
// Note to self: We can never queue messages here and we always must pass them without making a copy since we use messages that references local data structures.
// As long as we stick to that rule, we should never have dangling references
namespace GATAS
{
    template <uint_least8_t MAX_ROUTERS_>
    class ThreadSafeBus : public etl::imessage_bus
    {
        etl::vector<etl::imessage_router *, MAX_ROUTERS_> router_list;
#if GATAS_DEBUG == 1
        etl::array<uint8_t, 2> lastMsgPerCore;
#endif

    public:
        ThreadSafeBus() : etl::imessage_bus(router_list)
        {
        }

        ThreadSafeBus(etl::imessage_router &successor) : etl::imessage_bus(router_list, successor)
        {
        }

        void processMessage(const etl::imessage &message)
        {
            etl::imessage_bus::receive(message);
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

            GATAS_MEASURE_M("", 20'000 /* 10'000 */);
            processMessage(message);
            if (measure)
            {
                printf("Bus Messages: current:%d:%d core0:%d core1:%d\n", get_core_num(), message.get_message_id(), previousPerCore[0], previousPerCore[1]);
            }
            lastMsgPerCore[get_core_num()] = 0;
#else
            processMessage(message);
#endif
        }

        //*******************************************
        virtual void
        receive(etl::shared_message shared_msg) override
        {
            // , this should be avoided since it can lead to dangling references if not used carefully
            GATAS_WARN("Shared message received");
            etl::imessage_bus::receive(shared_msg);
        }
    };
};
