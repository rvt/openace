#include <stdio.h>

#include "../dump1090client.hpp"
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

void Dump1090Client::start()
{
    getBus().subscribe(*this);
};

void Dump1090Client::processNewSentence(etl::span<uint8_t> data)
{
    auto sentence = std::string_view(reinterpret_cast<const char*>(data.data()), data.size()-1);

    constexpr uint8_t ADSBDATALENGTH = 28;
    constexpr uint8_t ADSBDATALENGTH_HEXSIZE = ADSBDATALENGTH / 2;
    constexpr uint8_t ADSBDATALENGTH_SP = ADSBDATALENGTH + 2;
    GATAS_ASSERT(GATAS::ADSBMessageBin::MAX_BINARY_LENGTH * 2 == ADSBDATALENGTH, "Must be equal");

    // Fast detection of msgType 17 and hexStrToByteArray to reduce resources
    if (sentence[1] == '8' && (sentence[2] == 'D' || sentence[2] == 'A' || sentence[2] == '0'))
    {
        // [*]8D4CADC458BF02B09CCB3E499B92[;] <- 28 chars without * and ;
        if (sentence.size() == ADSBDATALENGTH_SP)
        {
            GATAS::ADSBMessageBin msg;
            CoreUtils::hexStrToByteArray(sentence.cbegin() + 1, GATAS::ADSBMessageBin::MAX_BINARY_LENGTH*2, msg.data);
            receiver->receiveBinary(msg.data, ADSBDATALENGTH_HEXSIZE);
            statistics.totalReceived += 1;
        }
    }
}

void Dump1090Client::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"totalReceived:k\":" << statistics.totalReceived;
    stream << ",\"reConnects\":" << statistics.reConnects;
    stream << "}";
}

void Dump1090Client::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void Dump1090Client::on_receive(const GATAS::WifiConnectionStateMsg &wcs)
{
    wifiConnected = wcs.wifiMode != GATAS::WifiMode::NC;
    networkAddress = wcs.gatasIp & 0xFFFFFF;
}