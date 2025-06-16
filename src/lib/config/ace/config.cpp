
#include "config.hpp"

#include "hardware/watchdog.h"
#include "pico/bootrom.h"

/* OpenACE. */
#include "ace/coreutils.hpp"
#include "etl/string.h"

static constexpr char SIGNATURE[] = "signature";

etl::string_view Config::loadLocationToString(LoadLocation location) const
{
    switch (location)
    {
    case LoadLocation::VOLATILE:
        return "Ram";
    case LoadLocation::PERSISTENT:
        return "Flash";
    case LoadLocation::DEFAULT:
        return "Default";
    default:
        return "-";
    }
}

OpenAce::PostConstruct Config::postConstruct()
{
    // Load a configuration in this order
    // volatileStore -> persistentStore -> Defaul configuration
    auto error = deserializeJson(doc, volatileStore.data());
    auto loadDefaultConfig = false; // Only usefull for developers, this ensures the default config is always loaded when set to true
    if (error || loadDefaultConfig)
    {
        error = deserializeJson(doc, permanentStore.data());
        auto signatureMismatch = doc[SIGNATURE].as<uint32_t>() != OPENACE_FLASH_SIGNATURE;

        // Still error, load default
        if (error || signatureMismatch || loadDefaultConfig)
        {
            volatileStore.write(defaultConfig, strlen((const char *)defaultConfig) + 1);
            deserializeJson(doc, volatileStore.data());
            statistics.location = DEFAULT;
        }
        else
        {
            statistics.location = PERSISTENT;
            statistics.persistentStoreSize = strlen((const char *)permanentStore.data()) + 1;
            serializeToVolatile();
        }
    }
    else
    {
        statistics.location = VOLATILE;
    }

    return OpenAce::PostConstruct::OK;
}

void Config::start()
{
    getBus().subscribe(*this);
}

void Config::stop()
{
    getBus().unsubscribe(*this);
}

void Config::getData(etl::string_stream &stream, const etl::string_view fullPath) const
{
    struct CustomWriter
    {
        etl::string_stream &stream;
        CustomWriter(etl::string_stream &stream_) : stream(stream_) {};

        // Writes one byte, returns the number of bytes written (0 or 1)
        size_t write(uint8_t c)
        {
            stream.str().append(1, (char)c);
            return 1;
        }

        // Writes several bytes, returns the number of bytes written
        size_t write(const uint8_t *buffer, size_t length)
        {
            etl::string_view view((const char *)buffer, length);
            stream << view;
            return length;
        }
    };

    auto [idx, path] = preparePath(fullPath);

    // Test for data from configuration, or from the module itself.
    if (path.size() == 0)
    {
        stream << "{";
        stream << "\"configuration\":\"" << loadLocationToString(statistics.location) << "\"";
        stream << ",\"pStoreSize\":" << statistics.persistentStoreSize;
        stream << "}\n";
    }
    else
    {
        auto src = configValueBypath<JsonVariantConst>(path);
        if (idx.has_value())
        {
            auto array = src.as<JsonArrayConst>();
            if (array)
            {
                src = array[idx.value()];
            }
            else
            {
                return;
            }
        }

        CustomWriter writer(stream);
        serializeJson(src, writer);
    }
}

bool Config::setData(const etl::string_view data, const etl::string_view fullPath)
{
    auto [idx, path] = preparePath(fullPath);
    bool dataMutated = false;

    if (idx.has_value())
    {
        // Set data by index
        auto src = configValueBypath<JsonVariant>(path);
        auto array = src.as<JsonArray>();
        if (array)
        {
            deserializeJson(array[idx.value()], data.cbegin(), data.cend() - data.cbegin());
            dataMutated = true;
        }
    }
    else
    {
        if (path.size() > 0)
        {
            if (path.back() == "SaveBr")
            {
                doc["config"]["_dirty"] = false;
                serializeToVolatile();
                serializeToPersistent();
            }
            // TODO: See if its possible to make something that these two are not in the config
            else if (path.back() == "Restart")
            {
                watchdog_reboot(0, 0, 0);
                while (true)
                {
                    tight_loop_contents();
                }
            }
            // Restart the Pico in USB mode so that the user can upload a new firmware
            else if (path.back() == "UsbBoot")
            {
                reset_usb_boot(0, 0);
                while (true)
                {
                    tight_loop_contents();
                } 
            }
            else
            {
                // Test of key exists, if not, create an entry
                auto src = configValueBypath<JsonVariant>(path);

                if (src == nullptr)
                {
                    // When key does not exist, create it firs at
                    auto const key = path.back();
                    path.pop_back();
                    // TODO: Move check for epty path and return doc to configValueBypath
                    if (path.size() == 0)
                    {
                        src = doc;
                    }
                    else
                    {
                        src = configValueBypath<JsonVariant>(path);
                    }

                    // Must add a non const ptr for ArduinoJson so a copy will be made instead of reference
                    src = src[const_cast<char *>(key.c_str())].to<JsonObject>();
                }
                deserializeJson(src, data.cbegin(), data.size());
                dataMutated = true;
            }
        }
    }

    if (dataMutated)
    {
        doc["config"]["_dirty"] = true;
        serializeToVolatile();
    }

    return dataMutated;
}

void Config::serializeToVolatile()
{
    volatileStore.rewind();
    serializeJson(doc, volatileStore);
    volatileStore.write(0);
}

void Config::serializeToPersistent()
{
    // One byte operations not yet supported, and arduinoJson wants to do these
    // It might not work at all due to complexity of flash (no interrupts etc)
    // So for now we do this from volatileStore

    permanentStore.rewind();
    permanentStore.write(volatileStore.data(), strlen((const char *)volatileStore.data()) + 1);
    statistics.persistentStoreSize = strlen((char *)permanentStore.data()) + 1;
}

bool Config::deleteData(const etl::string_view fullPath)
{
    bool dataMutated = false;
    auto [idx, path] = preparePath(fullPath);

    JsonVariant src = configValueBypath<JsonVariant>(path);
    if (idx.has_value())
    {
        auto array = src.as<JsonArray>();
        if (array)
        {
            src.remove(idx.value());
            dataMutated = true;
        }
    }
    else
    {
        dataMutated = true;
        auto key = path.back();
        path.pop_back();
        src = configValueBypath<JsonVariant>(path);
        src.remove(key.c_str());
    }

    if (dataMutated)
    {
        doc["config"]["_dirty"] = true;
        serializeToVolatile();
    }
    return dataMutated;
}

void Config::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

const OpenAce::PinTypeMap Config::pinMap(const etl::string_view moduleName) const
{
    OpenAce::PinTypeMap map;
    ccharptr hardware = (ccharptr)doc["hardware"]["type"];
    if (JsonObjectConst moduleConfig = doc[hardware][moduleName]; !moduleConfig.isNull())
    {
        for (JsonPairConst kv : moduleConfig)
        {
            auto pinType = OpenAce::stringToPinType(kv.key().c_str());
            if (pinType != OpenAce::PinType::UNKNOWN)
            {
                map[pinType] = kv.value().as<uint8_t>();
            }
        }
    }
    return map;
}

const OpenAce::Config::WifiServiceData Config::wifiService() const
{
    auto wifi = doc["WifiService"];
    OpenAce::Config::WifiServiceData wifiService;

    OpenAce::SsidOrPasswdStr ssid = (ccharptr)wifi["ap"]["ssid"];
    OpenAce::SsidOrPasswdStr password = (ccharptr)wifi["ap"]["password"];

    if (ssid.size() < 4 || password.size() < 8)
    {
        // Set default Ssid and passsword if non was found
        wifiService.ap.ssid = "OpenAce";
        wifiService.ap.password = "12345678";
    }
    else
    {
        wifiService.ap.ssid = ssid;
        wifiService.ap.password = password;
    }

    wifiService.apDisabled = (bool)wifi["ap"]["disabled"];

    // Set clients usernames and passwords
    for (auto client : wifi["clients"].as<JsonArrayConst>())
    {
        if (!wifiService.clients.full())
        {
            wifiService.clients.emplace_back((ccharptr)client["ssid"], (ccharptr)client["password"]);
        }
    }

    return wifiService;
};

const OpenAce::Config::OpenAceConfiguration Config::openAceConfig() const
{
    ccharptr aircraft = (ccharptr)doc["config"]["aircraftId"];
    JsonObjectConst aircraftConfig = doc["aircraft"][aircraft];

    // Default if no aircraft config was found
    etl::vector<OpenAce::DataSource, static_cast<uint8_t>(OpenAce::DataSource::_TRANSPROTOCOLS)> protocols;
    if (aircraftConfig.isNull())
    {
        return {
            .category = OpenAce::AircraftCategory::ReciprocatingEngine,
            .addressType = OpenAce::AddressType::ADSL,
            .address = 0,
            .stealth = false,
            .noTrack = false,
            .protocols = protocols};
    }

    for (auto protocol : aircraftConfig["protocols"].as<JsonArrayConst>())
    {
        protocols.push_back(OpenAce::stringToDataSource(protocol.as<const char *>()));
    }

    return {
        .category = OpenAce::stringToAircraftCategory((ccharptr)aircraftConfig["category"]),
        .addressType = OpenAce::stringToAddressType((ccharptr)aircraftConfig["addressType"]),
        .address = aircraftConfig["address"],
        .stealth = aircraftConfig["stealth"],
        .noTrack = aircraftConfig["noTrack"],
        .protocols = protocols};
};

bool Config::isModuleEnabled(const etl::string_view moduleName) const
{
    using Token = etl::optional<etl::string_view>;
    Token token;
    etl::string_view view = doc["modules"].as<const char *>();
    while ((token = etl::get_token(view, ",", token, true)))
    {
        if (token.value() == moduleName)
        {
            return true;
        }
    }
    return false;
};

int Config::valueByPath(int defaultValue, const etl::string_view pathToValue, const etl::string_view key) const
{
    auto path = CoreUtils::parsePath(pathToValue);
    auto src = configValueBypath<JsonVariantConst>(path);
    if (key.size())
    {
        src = src[key];
    }

    if (src.isNull())
    {
        return defaultValue;
    }
    else
    {
        return src.as<int>();
    }
}

const OpenAce::ConfigString Config::strValueByPath(const etl::string_view defaultValue, const etl::string_view pathToValue, const etl::string_view key) const
{
    auto path = CoreUtils::parsePath(pathToValue);
    auto src = configValueBypath<JsonVariantConst>(path);
    if (key.size())
    {
        src = src[key];
    }

    if (src.isNull())
    {
        return OpenAce::ConfigString(defaultValue);
    }
    else
    {
        auto string = src.as<const char *>();
        return OpenAce::ConfigString(string);
    }
};

const OpenAce::Config::IpPort Config::ipPortBypath(const etl::string_view pathToValue, const etl::string_view key) const
{
    OpenAce::ConfigPathString fullPath(pathToValue);
    if (key.size() > 0)
    {
        etl::string_stream fullPath_stream(fullPath);
        fullPath_stream << "/" << key;
    }

    auto path = CoreUtils::parsePath(fullPath);
    auto src = configValueBypath<JsonVariantConst>(path);
    // if (key.size())
    // {
    //     src = src[key];
    // }

    if (src.isNull())
    {
        return OpenAce::Config::IpPort{IPADDR_NONE, 0};
    }
    else
    {
        auto ip = src["ip"].as<const char *>();
        auto port = src["port"].as<uint16_t>();
        return OpenAce::Config::IpPort{
            inet_addr(ip),
            port};
    }
}