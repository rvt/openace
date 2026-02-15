#pragma once

#include "constants.hpp"
#include "models.hpp"
#include "semaphoreguard.hpp"

#include "pico.h"

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Vendor. */
#include "etl/map.h"
#include "etl/array.h"
#include "etl/string.h"
#include "etl/message_bus.h"
#include "etl/span.h"
#include "etl/delegate.h"

// TODO: Change to a etl::delegate
typedef std::function<void(const uint32_t)> pinIntrCallback_t;

// function to transform a reason into text
const char *postConstructToString(GATAS::PostConstruct reason);

class Configuration;
class BaseModule
{
    static constexpr uint8_t MAX_MODULES = 40;
    // Mutex to be used during load/unloading and changes in interrupts
    inline static SemaphoreHandle_t baseMutex = nullptr;

public:
    static void initBase()
    {
        baseMutex = xSemaphoreCreateMutex();
        if (baseMutex == nullptr)
        {
            panic("Failed to create BaseModule_mutex");
        }
    }
    using ModuleLoadFunction = etl::delegate<BaseModule *(etl::imessage_bus &, const Configuration &)>;
    struct ModuleStatus
    {
        GATAS::PostConstruct result;
        bool hwCheck;
        BaseModule *module;
    };

private:
    struct pinInterruptHandler
    {
        uint32_t event;
        TaskHandle_t handler;
        pinIntrCallback_t callback;
        uint32_t notificationValue;
        bool enabled;
        pinInterruptHandler(uint32_t _event, TaskHandle_t _handler, uint32_t _notificationValue) : event(_event), handler(_handler), callback(nullptr), notificationValue(_notificationValue), enabled(true) {}
        pinInterruptHandler(uint32_t _event, pinIntrCallback_t _callback) : event(_event), handler(nullptr), callback(_callback), notificationValue(0x00), enabled(true) {}
        pinInterruptHandler() : event(0x00), handler(nullptr), callback(nullptr), notificationValue(0x00), enabled(true) {}
    };
    inline static etl::map<uint8_t, BaseModule::pinInterruptHandler, 8> pinInterruptHandlers;

    using ModuleLoadMap = etl::map<const etl::string_view, ModuleStatus, MAX_MODULES /*, CharPtrComparator*/>;
    inline static ModuleLoadMap moduleLoaderMap;

    etl::imessage_bus &bus;
    const etl::string_view moduleName;

public:
    BaseModule(etl::imessage_bus &bus_, const etl::string_view name_);

    virtual ~BaseModule()
    {
        BaseModule::moduleLoaderMap[moduleName].module = nullptr;
    }

    static BaseModule *moduleByName(const BaseModule &that, const etl::string_view requesting);

    static void registerModule(const etl::string_view name, bool hwCheck)
    {
        moduleLoaderMap[name] = {GATAS::PostConstruct::NA, hwCheck, nullptr};
    }

    /**
     * Set the final evaluation of the module. b
     */
    static void setModuleStatus(const etl::string_view name, BaseModule *base)
    {
        moduleLoaderMap[name].result = GATAS::PostConstruct::OK;
        moduleLoaderMap[name].module = base;
    }
    static void setModuleStatus(const etl::string_view name, GATAS::PostConstruct result)
    {
        GATAS_ASSERT(result != GATAS::PostConstruct::OK, "setModuleStatus(const etl::string_view name, BaseModule *base) to set OK status!");
        moduleLoaderMap[name].result = result;
        moduleLoaderMap[name].module = nullptr;
    }

    static const ModuleLoadMap &registeredModules()
    {
        return moduleLoaderMap;
    }

    // Called after construction but before running
    virtual GATAS::PostConstruct postConstruct() = 0;

    // Called when all objects are initialised and ready to run
    virtual void start() = 0;

    const etl::string_view name() const
    {
        return moduleName;
    }

    inline etl::imessage_bus &getBus() const
    {
        return bus;
    }

    /**
     * get any data out of a module. THis will usually show some internal informatoion about an module
     *
     * @param stream the string stream to store data. Usually JSON is used for this unless there is a good reason to use some other format
     * @param path the path to the data
     *
     * @return void
     *
     * @throws None
     */
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const
    {
        (void)path;
        stream << "{}";
    }

    /**
     * Set any data on a module. THis should be used to modify a modules function in runtime that is not persisted.
     * Any data that needs to be persisted needs to use the configuration and use the Config during initalisation of the module
     *
     * @param data the data to set, usually this would be in JSON form bout could contain any other data type
     * @param path the path to set the data
     *
     * @return true if the data was accepted and processed
     *
     * @throws None
     */
    virtual bool setData(const etl::string_view data, const etl::string_view path)
    {
        (void)data;
        (void)path;
        return true;
    }

    /**
     * Interrupt handler for GPIO pins that can call back over a task notification or a callback function
     */
    static void gpioInterrupt(uint pin, uint32_t event);

    /**
     * Register a pin interrupt handler with task notification
     */
    void registerPinInterrupt(uint8_t pin, uint32_t events, TaskHandle_t handler, uint32_t notificationValue);

    /**
     * Register a pin interrupt handler with callback function
     */
    void registerPinInterrupt(uint8_t pin, uint32_t events, pinIntrCallback_t callback);

    /**
     * Register a pin interrupt handler with callback function
     */
    void disablePinInterrupt(uint8_t pin);
    void enablePinInterrupt(uint8_t pin, uint32_t notificationValue);

    /**
     * Unregister a pin interrupt handler
     */
    void unregisterPinInterrupt(uint8_t pin);
};

/**
 * Object can handle NMEA strings
 */
class StringHandler
{
    friend class StringProvider;

protected:
    virtual void handle(const GATAS::NMEAString &message) = 0;
};

/**
 * Object that can provide NMEA Strings.
 * You can use this to make a TCP socket or Serial provider that just passes them into a StringHandler
 */
class BinaryReceiver : public BaseModule
{
public:
    BinaryReceiver(etl::imessage_bus &bus, const etl::string_view name) : BaseModule(bus, name)
    {
    }
    virtual void receiveBinary(const uint8_t *data, uint8_t length) = 0;
};

class RtcModule : public BaseModule
{
public:
    static constexpr const etl::string_view NAME = "_RtcModule";
    // constructor
    RtcModule(etl::imessage_bus &bus) : BaseModule(bus, NAME)
    {
    }
    virtual void ppsEvent(int32_t offsetUs) = 0;
};

class SpiModule : public BaseModule
{
public:
    static constexpr uint32_t SPI_BUS_READY = 1 << 30;
    static constexpr const etl::string_view NAME = "_SPI";

    SpiModule(etl::imessage_bus &bus) : BaseModule(bus, NAME)
    {
    }
    virtual ~SpiModule() = default;

    virtual void read_registers(uint8_t cs, uint8_t reg, uint8_t *buf, uint16_t len, uint8_t delayMs) const = 0;
    virtual void read_registers_select(uint8_t cs, uint8_t reg) const = 0;
    virtual void read_registers_read(uint8_t cs, uint8_t *buf, uint16_t len) const = 0;

    virtual void cs_select(uint8_t cs) const = 0;
    virtual void cs_deselect(uint8_t cs) const = 0;

    virtual void write_array(uint8_t cs, uint8_t *data, uint8_t length, uint8_t delayMs) const = 0;
    virtual void write_byte(uint8_t cs, uint8_t data, uint8_t delayMs) const = 0;

    virtual SpiGuard getLock(bool &locked) = 0;
    virtual uint8_t spiNum() const = 0;
};

/**
 * A Radio baseclase that 'real' Radio's like the SX1262 should extend so they can be controlled in a generic way
 */
class Radio : public BaseModule
{
public:
    static constexpr etl::array<etl::string_view, 4> NAMES{"_Radio_0", "_Radio_1", "_Radio_2", "_Radio_3"};
    Radio(etl::imessage_bus &bus, const etl::string_view name) : BaseModule(bus, name)
    {
    }

    struct RadioParameters
    {
        const GATAS::ProtocolConfig *config;
        uint32_t frequency;
        int8_t powerdBm;
        uint8_t codingRate = 8; // Coding rate for LORA packages

        constexpr RadioParameters(const GATAS::ProtocolConfig *config_, uint32_t frequency_, int8_t powerdBm_, uint8_t codingRate_) : config(config_), frequency(frequency_), powerdBm(powerdBm_), codingRate(codingRate_) {}
        constexpr RadioParameters(const GATAS::ProtocolConfig *config_, uint32_t frequency_, int8_t powerdBm_) : config(config_), frequency(frequency_), powerdBm(powerdBm_) {}
        constexpr RadioParameters(const Radio::RadioParameters &params) : config(params.config), frequency(params.frequency), powerdBm(params.powerdBm), codingRate(params.codingRate) {}
        RadioParameters() = default;
        RadioParameters &operator=(const RadioParameters &other) = default;
    };

    struct TxPacket
    {
        RadioParameters radioParameters;
        size_t length; // In bytes
        union
        {
            GATAS::TxPacketType32 data32;
            GATAS::TxPacketType data;
        };

        TxPacket() = default;
        TxPacket(const RadioParameters &radioParameters_, etl::span<const uint8_t> dataSpan)
            : radioParameters(radioParameters_), length(dataSpan.size())
        {
            // Default to writing into .data (assumed default member)
            if (dataSpan.size() > sizeof(data))
            {
                GATAS_INFO("TxPacket: Frame length too large for this packet, clearing out");
                memset(&data, 0, sizeof(data));
            }
            else
            {
                memcpy(&data, dataSpan.data(), dataSpan.size());
            }
        }

        TxPacket(const RadioParameters &radioParameters_, size_t length_, const void *data_)
            : TxPacket(radioParameters_, etl::span<const uint8_t>(static_cast<const uint8_t *>(data_), length_))
        {
        }
    };

    struct RxMode
    {
        const RadioParameters radioParameters;
    };

    virtual ~Radio() = default;
    virtual uint8_t radio() const = 0;
};

class Configuration : public BaseModule
{

public:
    static constexpr const etl::string_view NAME = "Config";
    Configuration(etl::imessage_bus &bus) : BaseModule(bus, NAME)
    {
    }
    virtual ~Configuration() = default;

    virtual const GATAS::Config::GaTasConfiguration gaTasConfig() const = 0;
    virtual const GATAS::PinTypeMap pinMap(const etl::string_view moduleName) const = 0;

    virtual bool deleteData(const etl::string_view fullPath) = 0;
    virtual int valueByPath(int defaultValue, const etl::string_view pathToValue, const etl::string_view key) const = 0;
    int valueByPath(int defaultValue, const etl::string_view pathToValue) const
    {
        return valueByPath(defaultValue, pathToValue, "");
    };

    /**
     * Returns a value from path
     * example: auto value = config.strValueByPath("-", "GDLoverUDP/ips/0", "ip");
     *
     */
    virtual const GATAS::ConfigString strValueByPath(const etl::string_view defaultValue, const etl::string_view pathToValue, const etl::string_view key) const = 0;
    const GATAS::ConfigString strValueByPath(const etl::string_view defaultValue, const etl::string_view pathToValue) const
    {
        return strValueByPath(defaultValue, pathToValue, "");
    }

    /**
     * Validate if a module is enabled or not
     */
    virtual bool isModuleEnabled(const etl::string_view moduleName) const = 0;

    /**
     * Returns teh WIFI Service configuration
     */
    virtual const GATAS::Config::WifiServiceData wifiService() const = 0;

    /**
     * Returns an IP and Port combination
     */
    virtual const GATAS::Config::IpPort ipPortBypath(const etl::string_view pathToValue, const etl::string_view key) const = 0;
    const GATAS::Config::IpPort ipPortBypath(const etl::string_view pathToValue) const
    {
        return ipPortBypath(pathToValue, "");
    }

    /**
     * Returns a pointer to the flash mempry to the internal storage
     * Internal Store is used for program counters, antenna measurements etc..
     */
    virtual const GATAS::BinaryStore *internalStore() const = 0;

    /**
     * Store the complete data of the binaryStore
     */
    virtual void internalStore(const GATAS::BinaryStore &store) = 0;

    /**
     * Find a aircraft from it's transponder code
     */
    virtual GATAS::CallSign getCallSignFromHex(uint32_t transponderId) const = 0;

    virtual void setValueBypath(const etl::string_view pathToValue, etl::string_view value) = 0;
    virtual void setValueBypath(const etl::string_view pathToValue, uint64_t value) = 0;
};
