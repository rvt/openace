#include "../models.hpp"

namespace GATAS
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
        {PinType::AD0, "AD0"},
        {PinType::SPI, "SPI"}
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
        {AddressType::ADSL, "ADSL"},
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

    constexpr Mapping<DataSource, const char *> dataSourceMappings[] =
    {
        {DataSource::FLARM, "Flarm"},
        {DataSource::ADSLM, "ADSL"}, // <-- Don;t change to ADSM, this is also used frok teh frontend
        {DataSource::ADSLO_HDR, "ADSL Hdr"},
        {DataSource::ADSLFLARM, "ADSL FLARM"},
        {DataSource::ADSLOGN, "ADSL OGN"},
        {DataSource::FANET, "Fanet"},
        {DataSource::ADSB, "ADSB"},
        {DataSource::OGN1, "OGN"},
        {DataSource::PAW, "PAW"},
        {DataSource::NONE, "NONE"}
    };

    const char *dataSourceIntToString(uint8_t ds)
    {
        return toString(static_cast<DataSource>(ds));
    }
    const char *toString(DataSource ds)
    {
        return enumToString(dataSourceMappings, ds, "UNKNOWN");
    }
    DataSource stringToDataSource(const char *str)
    {
        return stringToEnum(dataSourceMappings, str, DataSource::NONE);
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

    constexpr Mapping<Modulation, const char *> modulationMapping[] =
    {
        {Modulation::GFSK, "Gfsk"},
        {Modulation::LORA, "Lora"},
        {Modulation::NONE, "None"},
    };

    const char *modulationToString(GATAS::Modulation at)
    {
        return enumToString(modulationMapping, at, "-");
    }

}

