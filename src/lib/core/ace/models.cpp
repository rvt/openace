#include "models.hpp"

namespace OpenAce
{

    struct PinMapping
    {
        const char *name;
        PinType type;
    };
    constexpr Mapping<PinType, const char *> pinMappings[] =
    {
        {PinType::CLK, "CLK"},
        {PinType::MOSI, "MOSI"},
        {PinType::MISO, "MISO"},
        {PinType::RST, "RST"},
        {PinType::TX, "TX"},
        {PinType::RX, "RX"},
        {PinType::BUSY, "BUSY"},
        {PinType::CS, "CS"},
        {PinType::O0, "O0"},
        {PinType::I0, "I0"},
        {PinType::DIO1, "DIO1"},
        {PinType::P0, "P0"},
        {PinType::P1, "P1"},
        {PinType::P2, "P2"},
        {PinType::SPI, "SPI"},
        {PinType::AD0, "AD0"}
    };

    PinType stringToPinType(const char *str)
    {
        return stringToEnum(pinMappings, str, PinType::UNKNOWN);
    }

    constexpr Mapping<AddressType, const char *> addressMappings[] =
    {
        {AddressType::RANDOM, "RANDOM"},
        {AddressType::ICAO, "ICAO"},
        {AddressType::FLARM, "FLARM"},
        {AddressType::OGN, "OGN"},
        {AddressType::FANET, "FANET"},
        {AddressType::RESERVED, "RESERVED"},
        {AddressType::UNKNOWN, "UNKNOWN"}
    };

    const char *addressTypeToString(AddressType ds)
    {
        return enumToString(addressMappings, ds, "UNKNOWN");
    };

    AddressType stringToAddressType(const char *str)
    {
        return stringToEnum(addressMappings, str, AddressType::UNKNOWN);
    };

    constexpr Mapping<DataSource, const char *> dataSourceMappings1[] =
    {
        {DataSource::NONE, "-"},
        {DataSource::FLARM, "FLARM"},
        {DataSource::ADSL, "ADSL"},
        {DataSource::FANET, "FANET"},
        {DataSource::ADSB, "ADSB"},
        {DataSource::MODES, "MODES"},
        {DataSource::OGN1, "OGN"},
        {DataSource::PAW, "PAW"}
    };
    constexpr Mapping<DataSource, const char *> dataSourceMappings2[] =
    {
        {DataSource::NONE, "-"},
        {DataSource::FLARM, "F"},
        {DataSource::ADSL, "L"},
        {DataSource::FANET, "N"},
        {DataSource::ADSB, "A"},
        {DataSource::MODES, "M"},
        {DataSource::OGN1, "O"},
        {DataSource::PAW, "P"}
    };

    const char *dataSourceToString(DataSource ds)
    {
        return enumToString(dataSourceMappings1, ds, "UNKNOWN");
    }
    DataSource stringToDataSource(const char *str)
    {
        return stringToEnum(dataSourceMappings1, str, DataSource::NONE);
    }

    const char *dataSourceToChar(DataSource ds)
    {
        return enumToString(dataSourceMappings2, ds, "U");
    }

    constexpr Mapping<AircraftCategory, const char *> aircraftMappings[] =
    {
        {AircraftCategory::Unknown, "Unknown"},
        {AircraftCategory::GliderMotorGlider, "GliderMotorGlider"},
        {AircraftCategory::TowPlane, "TowPlane"},
        {AircraftCategory::Helicopter, "Helicopter"},
        {AircraftCategory::Skydiver, "Skydiver"},
        {AircraftCategory::DropPlane, "DropPlane"},
        {AircraftCategory::HangGlider, "HangGlider"},
        {AircraftCategory::Paraglider, "Paraglider"},
        {AircraftCategory::ReciprocatingEngine, "ReciprocatingEngine"},
        {AircraftCategory::JetTurbopropEngine, "JetTurbopropEngine"},
        {AircraftCategory::Reserved, "Reserved"},
        {AircraftCategory::Balloon, "Balloon"},
        {AircraftCategory::Airship, "Airship"},
        {AircraftCategory::Uav, "Uav"},
        {AircraftCategory::ReservedE, "ReservedE"},
        {AircraftCategory::StaticObstacle, "StaticObstacle"}
    };

    AircraftCategory stringToAircraftCategory(const char *str)
    {
        return stringToEnum(aircraftMappings, str, AircraftCategory::Unknown);
    }

    const char *aircraftTypeToString(OpenAce::AircraftCategory at)
    {
        return enumToString(aircraftMappings, at, "Unknown");
    }

    struct pDopInterpretationMapping
    {
        pDopInterpretation type;
        const char *name;
    };

    constexpr Mapping<pDopInterpretation, const char *> interpretationMappings[] =
    {
        {pDopInterpretation::IDEAL, "Ideal"},
        {pDopInterpretation::EXCELLENT, "Excellent"},
        {pDopInterpretation::GOOD, "Good"},
        {pDopInterpretation::MODERATE, "Moderate"},
        {pDopInterpretation::FAIR, "Fair"},
        {pDopInterpretation::POOR, "Poor"}
    };

    const char *DOPInterpretationToString(pDopInterpretation interpretation)
    {
        return enumToString(interpretationMappings, interpretation, "Poor");
    }

    pDopInterpretation floatToDOPInterpretation(float dop)
    {
        if (dop < static_cast<uint8_t>(pDopInterpretation::EXCELLENT))
            return pDopInterpretation::IDEAL;
        if (dop < static_cast<uint8_t>(pDopInterpretation::GOOD))
            return pDopInterpretation::EXCELLENT;
        if (dop < static_cast<uint8_t>(pDopInterpretation::MODERATE))
            return pDopInterpretation::GOOD;
        if (dop < static_cast<uint8_t>(pDopInterpretation::FAIR))
            return pDopInterpretation::MODERATE;
        if (dop < static_cast<uint8_t>(pDopInterpretation::POOR))
            return pDopInterpretation::FAIR;
        return pDopInterpretation::POOR;
    }
}
