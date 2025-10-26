#pragma once

#include "ace/basemodule.hpp"

class MockConfig : public Configuration
{

public:
    MockConfig(etl::imessage_bus &bus) : Configuration(bus)
    {
    }
    virtual ~MockConfig() = default;

    virtual void start() override
    {
    }
    virtual GATAS::PostConstruct postConstruct() override
    {
        return GATAS::PostConstruct::OK;
    }

    virtual const GATAS::Config::GaTasConfiguration gaTasConfig() const override
    {
        return GATAS::Config::GaTasConfiguration{
            {0x000000,
             GATAS::AircraftCategory::LIGHT,
             GATAS::AddressType::ICAO,
             false,
             false},
            {GATAS::DataSource::OGN1}};
    }

    virtual const GATAS::PinTypeMap pinMap(const etl::string_view moduleName) const override
    {
        GATAS::PinTypeMap m;
        return m;
    };

    virtual bool deleteData(const etl::string_view fullPath) override
    {
        return true;
    }

    virtual int valueByPath(int defaultValue, const etl::string_view pathToValue, const etl::string_view key) const override
    {
        return 0;
    };

    virtual const GATAS::ConfigString strValueByPath(const etl::string_view defaultValue, const etl::string_view pathToValue, const etl::string_view key) const override
    {
        return "";
    }

    virtual bool isModuleEnabled(const etl::string_view moduleName) const override
    {
        return true;
    }

    // virtual const GATAS::Config::WifiNamePassword wifiClient() const override
    // {
    //     return GATAS::Config::WifiNamePassword
    //     {
    //         "",
    //         ""
    //     };
    // }

    virtual const GATAS::Config::WifiServiceData wifiService() const override
    {
        return GATAS::Config::WifiServiceData{};
    }

    virtual const GATAS::Config::IpPort ipPortBypath(const etl::string_view pathToValue, const etl::string_view key) const override
    {
        return {0, 0};
    };

    virtual const GATAS::BinaryStore *internalStore() const override
    {
        static GATAS::BinaryStore store{GATAS::BinaryStore::MAGIC, 0xCAFEBABE};

        return &store;
    }

    virtual void internalStore(const GATAS::BinaryStore &store) override
    {
    }

    virtual GATAS::CallSign getCallSignFromHex(uint32_t) const override
    {
        return "PH-XXX";
    }

    virtual void setValueBypath(const etl::string_view pathToValue, etl::string_view value) override
    {
    }

    virtual void setValueBypath(const etl::string_view pathToValue, uint64_t value) override
    {
    }
};
