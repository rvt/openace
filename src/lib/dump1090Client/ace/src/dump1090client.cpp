#include <stdio.h>

#include "../dump1090client.hpp"
#include "ace/assert.hpp"
#include "ace/coreutils.hpp"
#include "etl/error_handler.h"

GATAS::PostConstruct Dump1090Client::postConstruct()
{
    receiver = static_cast<BinaryReceiver *>(BaseModule::moduleByName(*this, "ADSBDecoder"));
    if (receiver == nullptr)
    {
        return GATAS::PostConstruct::DEP_NOT_FOUND;
    }

    auto result = tcpClient.postConstruct();
    if (result != GATAS::PostConstruct::OK)
    {
        return result;
    }

    return GATAS::PostConstruct::OK;
}

void Dump1090Client::stop()
{
    tcpClient.stop();
    getBus().unsubscribe(*this);
};

void Dump1090Client::start()
{
    getBus().subscribe(*this);
};

void Dump1090Client::processNewSentence(const char *sentence)
{
    constexpr uint8_t ADSBDATALENGTH = 28;
    static_assert(GATAS::ADSBMessageBin::MAX_BINARY_LENGTH * 2 == ADSBDATALENGTH, "Must be equal");

    // Fast detection of msgType 17 and hexStrToByteArray to reduce resources
    if (sentence != nullptr && sentence[1] == '8' && (sentence[2] == 'D' || sentence[2] == 'A' || sentence[2] == '0'))
    {
        uint8_t hexSize = strlen(sentence) - 2;
        // [*]8D4CADC458BF02B09CCB3E499B92[;] <- 28 chars without * and ;
        if (hexSize == ADSBDATALENGTH)
        {
            auto halfSize = hexSize / 2;
            // puts(sentence);
            GATAS::ADSBMessageBin msg;
            CoreUtils::hexStrToByteArray(sentence + 1, GATAS::ADSBMessageBin::MAX_BINARY_LENGTH*2, msg.data);
            receiver->receiveBinary(msg.data, halfSize);
            statistics.totalReceived += 1;
        }
    }
}

void Dump1090Client::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"totalReceived\":" << statistics.totalReceived;
    stream << ",\"reConnects\":" << statistics.reConnects;
    stream << "}\n";
}

void Dump1090Client::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void Dump1090Client::on_receive(const GATAS::Every5SecMsg &msg)
{
    (void)msg;
    // printf("%d %d %d \n", wifiConnected , tcpClient.isStopped() , (stoppedCounter % 4 == 0));
    if (wifiConnected && tcpClient.isStopped() && (stoppedCounter % 4 == 0) && ((tcpClient.ip() & 0xFFFFFF) == networkAddress))
    {
        stoppedCounter = 0;
        tcpClient.start();
        statistics.reConnects += 1;
    }
    stoppedCounter += 1;
}

void Dump1090Client::on_receive(const GATAS::WifiConnectionStateMsg &wcs)
{
    wifiConnected = wcs.connected;
    networkAddress = wcs.networkAddress;
}