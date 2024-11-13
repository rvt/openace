#include <stdio.h>

#include "dump1090client.hpp"

OpenAce::PostConstruct Dump1090Client::postConstruct()
{
    receiver = static_cast<BinaryReceiver *>(BaseModule::moduleByName(*this, "ADSBDecoder", false));
    if (receiver == nullptr)
    {
        return OpenAce::PostConstruct::DEP_NOT_FOUND;
    }

    auto result = tcpClient.postConstruct();
    if (result != OpenAce::PostConstruct::OK)
    {
        return result;
    }

    timerHandle = xTimerCreate("dump1090Timer", TASK_DELAY_MS(2'000), pdTRUE, this, dump1090Timer);
    if (timerHandle == nullptr)
    {
        return OpenAce::PostConstruct::TIMER_ERROR;
    }

    return OpenAce::PostConstruct::OK;
}

void Dump1090Client::dump1090Timer(TimerHandle_t xTimer)
{
    auto *handle = (Dump1090Client *)pvTimerGetTimerID(xTimer);
    if (handle->tcpClient.isStopped() && (handle->stoppedCounter++ == 2))
    {
        handle->stoppedCounter = 0;
        handle->tcpClient.start();
    }
}

void Dump1090Client::stop()
{
    // xTimerDelete(timerHandle, TASK_DELAY_MS(250));
    tcpClient.stop();
    getBus().unsubscribe(*this);
};

void Dump1090Client::start()
{
    xTaskCreate(dump1090Task, "Bmp280Task", configMINIMAL_STACK_SIZE + 128, this, tskIDLE_PRIORITY + 1, &taskHandle);
    getBus().subscribe(*this);
};

void Dump1090Client::dump1090Task(void *arg)
{
    Dump1090Client *dump1090 = static_cast<Dump1090Client *>(arg);
    while (true)
    {

        if (ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(1'000)))
        {
            // TODO: Handle shutdown
        }
        else
        {
            if (dump1090->tcpClient.isStopped() && (dump1090->stoppedCounter++ == 2))
            {
                dump1090->stoppedCounter = 0;
                dump1090->tcpClient.start();
            }
        }
    }
}

void Dump1090Client::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"totalReceived\":" << statistics.totalReceived;
    stream << "}\n";
}
