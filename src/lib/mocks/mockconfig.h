#pragma once

#include "ace/basemodule.hpp"

class MockConfig : public Configuration
{

public:
    MockConfig(etl::imessage_bus& bus) : Configuration(bus)
    {
    }
    virtual ~MockConfig() = default;

    virtual void start() override
    {
    }
    virtual void stop()override
    {
    }
    virtual OpenAce::PostConstruct postConstruct() override
    {
        return OpenAce::PostConstruct::OK;
    }

    virtual const OpenAce::Config::OpenAceConfiguration openAceConfig() const override
    {
        return OpenAce::Config::OpenAceConfiguration
        {
            OpenAce::AircraftCategory::ReciprocatingEngine,
            OpenAce::AddressType::ICAO,
            0x123456,
            false,
            false,
            {OpenAce::DataSource::OGN1}
        };
    }

    virtual const OpenAce::PinTypeMap pinMap(const etl::string_view moduleName) const override
    {
        OpenAce::PinTypeMap m;
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


    virtual const OpenAce::ConfigString strValueByPath(const etl::string_view defaultValue, const etl::string_view pathToValue, const etl::string_view key) const override
    {
        return "";
    }

    virtual bool isModuleEnabled(const etl::string_view moduleName) const override
    {
        return true;
    }

    // virtual const OpenAce::Config::WifiNamePassword wifiClient() const override
    // {
    //     return OpenAce::Config::WifiNamePassword
    //     {
    //         "",
    //         ""
    //     };
    // }

    virtual const OpenAce::Config::WifiServiceData wifiService() const override
    {
        return OpenAce::Config::WifiServiceData{};
    }

    virtual const OpenAce::Config::IpPort ipPortBypath(const etl::string_view pathToValue, const etl::string_view key) const override
    {
        return {0, 0};
    };


};
