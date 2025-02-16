#include <stdio.h>

#include "pico/stdlib.h"

#include "../fanet/fanet_common.hpp"
#include "../fanet/fanet_packet.hpp"
#include "../fanet/fanet_builder.hpp"

#include "fanetace.hpp"

#include "ace/messagerouter.hpp"
#include "ace/coreutils.hpp"

void FanetAce::start()
{
    xTaskCreate(FanetAceTask, "FanetAceTask", configMINIMAL_STACK_SIZE, this, tskIDLE_PRIORITY + 2, &taskHandle);
};

void FanetAce::stop()
{
    xTaskNotify(taskHandle, TaskState::EXIT, eSetBits);
};

OpenAce::PostConstruct FanetAce::postConstruct()
{
    return OpenAce::PostConstruct::OK;
}

void FanetAce::FanetAceTask(void *arg)
{
    (void)arg;
    // FanetAce *FanetAce = static_cast<FanetAce *>(arg);
    // FanetAce->statistics.queueFullErr = 0;
    // OpenAce::ADSBString sentence;
    while (true)
    {
        if (uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY))
        {
            (void)notifyValue;
//             if (notifyValue & TaskState::EXIT)
//             {
//                 vTaskDelete(nullptr);
//                 return;
//             }

//             if (notifyValue & TaskState::NEW)
//             {
//                 while (FanetAce->queue.pop(sentence))
//                 {
//                     FanetAce->statistics.totalReceived++;
//                     // Finalise implementation to create the binary message here and send
// //                    FanetAce->getBus().receive(OpenAce::ADSBMessageBin{sentence});
//                 }
//             }
        }
    }
}

void FanetAce::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"totalReceived\":" << statistics.totalReceived;
    stream << ",\"queueFullErr\":" << statistics.queueFullErr;
    stream << "}\n";
}
