
#include "../config.hpp"

#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "etl/string_utilities.h"

/* GATAS. */
#include "ace/coreutils.hpp"
#include "ace/binarymessages.hpp"
#include "etl/string.h"

#include "pico/rand.h"

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

GATAS::PostConstruct Config::postConstruct()
{
    auto iStore = internalStore();
    if (iStore->magic != GATAS::BinaryStore::MAGIC)
    {
        GATAS::BinaryStore defaultStore;
        defaultStore.magic = GATAS::BinaryStore::MAGIC;
        defaultStore.gatasId = get_rand_64();
        internalStore(defaultStore);
    }
    
    // Load a configuration in this order
    auto loadDefaultConfig = false; // Only usefull for developers, this ensures the default config is always loaded when set to true
    if (loadDefaultConfig)
    {
        volatileStore.rewind();
        volatileStore.write(defaultConfig, strlen((const char *)defaultConfig) + 1);
        deserializeJson(doc, volatileStore.data());
        // Don't store here yet, let the user save it for himself first
        //        serializeToPersistent();
        statistics.location = DEFAULT;
    }

    volatileStore.rewind();
    permanentStore.rewind();
    auto error = deserializeJson(doc, volatileStore.data());
    auto signatureMismatch = doc[SIGNATURE].as<uint32_t>() != GATAS_FLASH_SIGNATURE;
    if (error || signatureMismatch)
    {
        error = deserializeJson(doc, permanentStore.data());
        signatureMismatch = doc[SIGNATURE].as<uint32_t>() != GATAS_FLASH_SIGNATURE;

        // Still error, load default
        if (error || signatureMismatch)
        {
            volatileStore.rewind();
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

    return GATAS::PostConstruct::OK;
}

void Config::start()
{
    getBus().subscribe(*this);
}

void Config::getData(etl::string_stream &stream, const etl::string_view fullPath) const
{
    struct CustomWriter
    {
        etl::string_stream &stream;
        CustomWriter(etl::string_stream &stream_) : stream(stream_) {};

        size_t write(uint8_t c)
        {
            stream.str().append(1, (char)c);
            return 1;
        }

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
        stream << ",\"gatasId\":" << static_cast<uint32_t>(internalStore()->gatasId);
        stream << "}";
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

const GATAS::PinTypeMap Config::pinMap(const etl::string_view moduleName) const
{
    GATAS::PinTypeMap map;
    ccharptr hardware = (ccharptr)doc["hardware"]["type"];
    if (JsonObjectConst moduleConfig = doc[hardware][moduleName]; !moduleConfig.isNull())
    {
        for (JsonPairConst kv : moduleConfig)
        {
            auto pinType = GATAS::stringToPinType(kv.key().c_str());
            if (pinType != GATAS::PinType::UNKNOWN)
            {
                map[pinType] = kv.value().as<uint8_t>();
            }
        }
    }
    return map;
}

const GATAS::Config::WifiServiceData Config::wifiService() const
{
    auto wifi = doc["WifiService"];
    GATAS::Config::WifiServiceData wifiService;

    GATAS::SsidOrPasswdStr ssid = (ccharptr)wifi["ap"]["ssid"];
    GATAS::SsidOrPasswdStr password = (ccharptr)wifi["ap"]["password"];

    if (ssid.size() < 3 || password.size() < 8)
    {
        // Set default Ssid and passsword if non was found
        wifiService.ap.ssid = "GATAS";
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

const GATAS::Config::GaTasConfiguration Config::gaTasConfig() const
{
    ccharptr aircraftId = (ccharptr)doc["config"]["aircraftId"];
    JsonObjectConst aircraftConfig = doc["aircraft"][aircraftId];

    // Default if no aircraft config was found
    etl::vector<GATAS::DataSource, static_cast<uint8_t>(GATAS::DataSource::_TRANSPROTOCOLS)> protocols;
    if (aircraftConfig.isNull())
    {
        return {
            .conspicuity = {
                .icaoAddress = 0,
                .category = GATAS::AircraftCategory::LIGHT,
                .addressType = GATAS::AddressType::ADSL,
                .stealth = false,
                .noTrack = false},
            .protocols = protocols,
            .allIcaoAddresses = {}};
    }

    for (auto protocol : aircraftConfig["protocols"].as<JsonArrayConst>())
    {
        if (!protocols.full())
        {
            protocols.push_back(GATAS::stringToDataSource(protocol.as<const char *>()));
        }
    }

    etl::vector<uint32_t, GATAS::MAX_AIRCRAFT_CONFIGURATIONS> allIcaoAddresses;
    auto aircrafts = doc["aircraft"];
    for (JsonPairConst kv : aircrafts.as<JsonObjectConst>())
    {
        if (!allIcaoAddresses.full())
        {
            uint32_t address = kv.value()["address"];
            allIcaoAddresses.push_back(address);
        }
    }

    return {
        .conspicuity = {
            .icaoAddress = aircraftConfig["address"],
            .category = BinaryMessages::mapAircraftCategoryToType((ccharptr)aircraftConfig["category"]),
            .addressType = GATAS::stringToAddressType((ccharptr)aircraftConfig["addressType"]),
            .stealth = aircraftConfig["stealth"],
            .noTrack = aircraftConfig["noTrack"],
        },
        .protocols = protocols,
        .allIcaoAddresses = allIcaoAddresses};
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
    auto path = CoreUtils::parsePath(pathToValue, key);
    auto src = configValueBypath<JsonVariantConst>(path);

    if (src.isNull())
    {
        return defaultValue;
    }
    else
    {
        return src.as<int>();
    }
}

const GATAS::ConfigString Config::strValueByPath(const etl::string_view defaultValue, const etl::string_view pathToValue, const etl::string_view key) const
{
    auto path = CoreUtils::parsePath(pathToValue, key);
    auto src = configValueBypath<JsonVariantConst>(path);

    if (src.isNull())
    {
        return GATAS::ConfigString(defaultValue);
    }
    else
    {
        auto string = src.as<const char *>();
        return GATAS::ConfigString(string);
    }
};

/**
 * Simple IPv4 parser, Not sure where to put this but di dnot want to include LWiP into Config
 */
uint32_t Config::parseIpv4String(const etl::string_view ipStr, uint32_t defaultValue) const
{
    using Token = etl::optional<etl::string_view>;
    uint32_t ip = 0;
    uint8_t shift = 0;
    Token token;
    while ((token = etl::get_token(ipStr, ".", token, true)))
    {
        uint32_t value = atoi(token.value().cbegin());
        if (value > 255)
        {
            return defaultValue;
        }
        ip |= (value << shift);
        shift += 8;
    }

    return ip;
}

const GATAS::Config::IpPort Config::ipPortBypath(const etl::string_view pathToValue, const etl::string_view key) const
{
    auto path = CoreUtils::parsePath(pathToValue, key);
    auto src = configValueBypath<JsonVariantConst>(path);

    if (src.isNull())
    {
        return GATAS::Config::IpPort{0xffffffffUL, 0};
    }
    else
    {
        auto ipStr = src["ip"].as<const char *>();
        auto ip = parseIpv4String(ipStr, 0xffffffffUL);
        auto port = src["port"].as<uint16_t>();
        return GATAS::Config::IpPort{
            ip,
            port};
    }
}

const GATAS::BinaryStore *Config::internalStore() const
{
    return reinterpret_cast<const GATAS::BinaryStore *>(binaryStore.data());
}

void Config::internalStore(const GATAS::BinaryStore &store)
{
    binaryStore.write(reinterpret_cast<const uint8_t *>(&store), sizeof(GATAS::BinaryStore));
}

GATAS::CallSign Config::getCallSignFromHex(uint32_t transponderHex) const
{
    auto aircrafts = doc["aircraft"];
    for (JsonPairConst kv : aircrafts.as<JsonObjectConst>())
    {
        uint32_t address = kv.value()["address"];
        if (transponderHex == address)
        {
            return kv.value()["callSign"].as<const char *>();
        }
    }

    return "";
}

void Config::setValueBypath(const etl::string_view pathToValue, etl::string_view value)
{
    auto path = CoreUtils::parsePath(pathToValue, "");
    auto src = configValueBypath<JsonVariant>(path);
    src.set(value);
}

void Config::setValueBypath(const etl::string_view pathToValue, uint64_t value)
{
    auto path = CoreUtils::parsePath(pathToValue, "");
    auto src = configValueBypath<JsonVariant>(path);
    src.set(value);
}