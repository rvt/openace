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

    return OpenAce::PostConstruct::OK;
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
    // Fast detection of msgType 17 and hexStrToByteArray to reduce resources
    if (sentence != nullptr && sentence[1] == '8' && (sentence[2] == 'D' || sentence[2] == 'A' || sentence[2] == '0'))
    {
        uint8_t hexSize = strlen(sentence) - 2;
        if (hexSize == 30) // 30 happens to be the size of message we are interested in
        {
            // puts(sentence);
            OpenAce::ADSBMessageBin msg;
            hexStrToByteArray(sentence + 1, (hexSize - 2), msg.data.data());
            receiver->receiveBinary(msg.data.data(), msg.data.size());
            statistics.totalReceived++;
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

void Dump1090Client::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void Dump1090Client::on_receive(const OpenAce::IdleMsg &msg)
{
    (void)msg;
    if (wifiConnected && (stoppedCounter++ == 2) && tcpClient.isStopped())
    {
        stoppedCounter = 0;
        tcpClient.start();
    }
}

void Dump1090Client::on_receive(const OpenAce::WifiConnectionStateMsg &wcs)
{
    wifiConnected = wcs.connected;
}