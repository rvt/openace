#pragma once

#include "constants.hpp"
#include "models.hpp"

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
const char *postConstructToString(OpenAce::PostConstruct reason);

class Configuration;
class BaseModule
{
    static constexpr uint8_t MAX_MODULES = 40;

private:
    // Mutex to be used during load/unloading and changes in interrupts
    inline static SemaphoreHandle_t xMutex;

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
    inline static etl::map<uint8_t, BaseModule::pinInterruptHandler, 8> __scratch_y("OpenAceMem") pinInterruptHandlers;

public:
    static void initBase()
    {
        xMutex = xSemaphoreCreateMutex();
        if (xMutex == nullptr)
        {
            panic("Failed to create xMutex");
        }
    }
    using ModuleLoadFunction = etl::delegate<BaseModule *(etl::imessage_bus &, const Configuration &)>;
    struct ModuleStatus
    {
        ModuleLoadFunction loadFunction;
        OpenAce::PostConstruct result;
        BaseModule *module;
        bool hwCheck;
    };

protected:
    using ModuleLoadMap = etl::map<const etl::string_view, ModuleStatus, MAX_MODULES /*, CharPtrComparator*/>;
    inline static ModuleLoadMap moduleLoaderMap;

private:
    etl::imessage_bus &bus;
    const etl::string_view moduleName;

public:
    BaseModule(etl::imessage_bus &bus_, const etl::string_view name_);

    virtual ~BaseModule()
    {
        BaseModule::moduleLoaderMap[moduleName].module = nullptr;
    }

    static BaseModule *moduleByName(const BaseModule &that, const etl::string_view requesting);

    static void registerModule(const etl::string_view name, bool hwCheck, ModuleLoadFunction loader)
    {
        moduleLoaderMap[name] = {loader, OpenAce::PostConstruct::NA, nullptr, hwCheck};
    }

    /**
     * Set the final evaluation of the module. b
     */
    static void setModuleStatus(const etl::string_view name, BaseModule *base, OpenAce::PostConstruct result)
    {
        moduleLoaderMap[name].result = result;
        moduleLoaderMap[name].module = base;
    }
    static const ModuleLoadMap &registeredModules()
    {
        return moduleLoaderMap;
    }

    // Called after construction but before running
    virtual OpenAce::PostConstruct postConstruct() = 0;

    // Called when all objects are initialised and ready to run
    virtual void start() = 0;

    // Called when this is going to be removed
    virtual void stop() = 0;

    // Called when this is going to be removed

    const etl::string_view name() const
    {
        return moduleName;
    }

    etl::imessage_bus &getBus() const
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
        return false;
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
    virtual void handle(const OpenAce::NMEAString &message) = 0;
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
    virtual void ppsEvent() = 0;
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

    /**
     * Squire access to the SPI buss
     * \sa acquireSlotSyncCb()
     * \sa releaseSlotSync()
     */
    virtual bool acquireSlotSync(uint8_t busFrequencyMhz) = 0;
    /**
     * Alternative to acquireSlotSync that will acquire access to the SPI bus, calls the delegate and release it in one function call
     * \sa releaseSlotSync()
     */
    virtual bool acquireSlotSyncCb(uint8_t busFrequencyMhz, const etl::delegate<void()> &delegate) = 0;
    virtual void releaseSlotSync() = 0;
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

    enum class Mode
    {
        NONE,
        GFSK,
        LORA,
    };

    static const char *modeString(Mode mode)
    {
        switch (mode)
        {
        case Mode::GFSK:
            return "GFSK";
        case Mode::LORA:
            return "LORA";
        default:
            return "UNK";
        }
    }

    struct ProtocolConfig
    {
        Radio::Mode mode;               // Mode of the radio
        OpenAce::DataSource dataSource; // Data source
        uint8_t packetLength;           // Total packet length including CRC
        uint8_t preambleLength;         // Preamble length in bits
        uint8_t codingRate;             // Coding rate for LORA packages
        uint8_t syncLength;
        etl::array<uint8_t, 10> syncWord; // Sync word

        constexpr ProtocolConfig(Radio::Mode mode_, OpenAce::DataSource dataSource_, uint8_t packetLength_, uint8_t preambleLength_, uint8_t codingRate_, uint8_t syncLength_, etl::array<uint8_t, 10> syncWord_)
            : mode(mode_), dataSource(dataSource_), packetLength(packetLength_), preambleLength(preambleLength_), codingRate(codingRate_), syncLength(syncLength_), syncWord(syncWord_) {}

        constexpr ProtocolConfig(const ProtocolConfig &other)
            : mode(other.mode),
              dataSource(other.dataSource),
              packetLength(other.packetLength),
              preambleLength(other.preambleLength),
              codingRate(other.codingRate),
              syncLength(other.syncLength),
              syncWord(other.syncWord) {}

        ProtocolConfig() = default;
    };

    struct RadioParameters
    {
        Radio::ProtocolConfig config;
        uint32_t frequency;
        int8_t powerdBm;

        constexpr RadioParameters(const Radio::ProtocolConfig &_config, uint32_t _frequency, int8_t _powerdBm) : config(_config), frequency(_frequency), powerdBm(_powerdBm) {}
        constexpr RadioParameters(const Radio::RadioParameters &_params) : config(_params.config), frequency(_params.frequency), powerdBm(_params.powerdBm) {}
        RadioParameters() = default;
        RadioParameters &operator=(const RadioParameters &other) = default;
    };

    struct TxPacket
    {
        RadioParameters radioParameters;
        uint8_t length;
        OpenAce::TxPacketType data;
        TxPacket(const RadioParameters &radioParameters_, etl::span<const uint8_t> dataSpan)
            : radioParameters(radioParameters_), length(static_cast<uint8_t>(dataSpan.size()))
        {
            if (dataSpan.size() > data.size())
            {
                puts("TxPacket: Frame length too large for this packet, clearing out");
                memset(data.data(), 0, data.size());
            } else {
                memcpy(data.data(), dataSpan.data(), dataSpan.size());
            }
        }

        TxPacket(const RadioParameters &radioParameters_, uint8_t length_, const void *data_)
            : TxPacket(radioParameters_, etl::span<const uint8_t>(static_cast<const uint8_t *>(data_), length_)) {}
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
    static constexpr const etl::string_view NAME = "_Configuration";
    Configuration(etl::imessage_bus &bus) : BaseModule(bus, NAME)
    {
    }
    virtual ~Configuration() = default;

    virtual const OpenAce::Config::OpenAceConfiguration openAceConfig() const = 0;
    virtual const OpenAce::PinTypeMap pinMap(const etl::string_view moduleName) const = 0;

    virtual bool deleteData(const etl::string_view fullPath) = 0;
    virtual int valueByPath(int defaultValue, const etl::string_view pathToValue, const etl::string_view key) const = 0;
    int valueByPath(int defaultValue, const etl::string_view pathToValue) const
    {
        return valueByPath(defaultValue, pathToValue, "");
    };

    virtual const OpenAce::ConfigString strValueByPath(const etl::string_view defaultValue, const etl::string_view pathToValue, const etl::string_view key) const = 0;
    const OpenAce::ConfigString strValueByPath(const etl::string_view defaultValue, const etl::string_view pathToValue) const
    {
        return strValueByPath(defaultValue, pathToValue, "");
    }

    virtual bool isModuleEnabled(const etl::string_view moduleName) const = 0;
    virtual const OpenAce::Config::WifiServiceData wifiService() const = 0;
    virtual const OpenAce::Config::IpPort ipPortBypath(const etl::string_view pathToValue, const etl::string_view key) const = 0;
    const OpenAce::Config::IpPort ipPortBypath(const etl::string_view pathToValue) const
    {
        return ipPortBypath(pathToValue, "");
    }
};
