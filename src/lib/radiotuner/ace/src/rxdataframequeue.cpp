#include "../rxdataframequeue.hpp"
#include "ace/manchester.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "etl/algorithm.h"

GATAS::PostConstruct RxDataFrameQueue::postConstruct()
{
    dataQueue = xQueueCreate(2, sizeof(GATAS::DataFrame));
    if (dataQueue == nullptr)
    {
        return GATAS::PostConstruct::XQUEUE_ERROR;
    }

    if (xTaskCreate(radioQueueTask, RxDataFrameQueue::NAME.cbegin(), configMINIMAL_STACK_SIZE + 512, this, tskIDLE_PRIORITY + 2, &taskHandle) != pdPASS)
    {
        vQueueDelete(dataQueue);
        return GATAS::PostConstruct::TASK_ERROR;
    }

    return GATAS::PostConstruct::OK;
}

void RxDataFrameQueue::start()
{
    getBus().subscribe(*this);
};

void RxDataFrameQueue::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"missedErr\":\"" << statistics.missedErr << "\"";
    stream << ",\"totalIncoming\":\"" << statistics.totalIncoming << "\"";
    stream << ",\"totalOutgoing\":\"" << statistics.totalOutgoing << "\"";
    stream << "}";
}

void RxDataFrameQueue::on_receive(const GATAS::DataFrameMsg &msg)
{
    statistics.totalIncoming += 1;
    if (xQueueSendToBack(dataQueue, &msg.dataFrame, TASK_DELAY_MS(5)) != pdPASS)
    {
        statistics.missedErr += 1;
    }
}

//*********************** Tuner tasks ***********************
void RxDataFrameQueue::radioQueueTask(void *arg)
{
    RxDataFrameQueue *rxQueue = static_cast<RxDataFrameQueue *>(arg);
    GATAS::DataFrame frame;
    while (true)
    {
        if (xQueueReceive(rxQueue->dataQueue, &frame, portMAX_DELAY) == pdPASS)
        {
            rxQueue->statistics.totalOutgoing += 1;
            if (frame.modulation == GATAS::Modulation::GFSK)
            {
                auto rxGfskMsg = GATAS::RadioRxGfskMsg{static_cast<uint8_t>(frame.length >> 1), frame.epochSeconds, frame.rssidBm, frame.frequency, frame.dataSource};
                manchesterDecode((uint8_t *)rxGfskMsg.frame, (uint8_t *)rxGfskMsg.err, frame.data, frame.length);
                rxQueue->getBus().receive(rxGfskMsg);
            }
            else if (frame.modulation == GATAS::Modulation::LORA)
            {
                auto rxLoraMsg = GATAS::RadioRxLoraMsg(frame.epochSeconds, frame.rssidBm, frame.frequency, frame.dataSource);
                rxLoraMsg.frame.assign(frame.data, frame.data + frame.length);
                rxQueue->getBus().receive(rxLoraMsg);
            }
            else
            {
                // Ignore unknown modulation types
            }
        }
    }
}

// ******************** Message bus receive handlers ********************
void RxDataFrameQueue::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}
