#pragma once

#include <stdint.h>
#include <sys/time.h>

#include "ace/constants.hpp"
#include "ace/models.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/coreutils.hpp"
#include "ace/messages.hpp"
#include "ace/messages.hpp"

#include "etl/map.h"
#include "etl/message_bus.h"

class DataPort : public BaseModule, public etl::message_router<DataPort, OpenAce::TrackedAircraftPositionMsg, OpenAce::OwnshipPositionMsg, OpenAce::ConfigUpdatedMsg, OpenAce::GPSSentenceMsg>
{
    friend class message_router;
    OpenAce::OwnshipPositionInfo ownshipPosition;
    OpenAce::Config::OpenAceConfiguration openAceConfiguration;
    OpenAce::AircraftAddress address;
    OpenAce::AircraftCategory category;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
    
    struct
    {
        uint32_t messages = 0;
    } statistics;

public:
    static constexpr const etl::string_view NAME = "DataPort";
    DataPort(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME)
    {
        (void)config;
    }

    virtual ~DataPort() = default;

    virtual OpenAce::PostConstruct postConstruct() override
    {
        return OpenAce::PostConstruct::OK;
    }

    virtual void start() override
    {
        getBus().subscribe(*this);
    };

    virtual void stop() override
    {
        getBus().unsubscribe(*this);
    };

    void on_receive(const OpenAce::TrackedAircraftPositionMsg &msg);

    void on_receive(const OpenAce::OwnshipPositionMsg &msg);

    void on_receive(const OpenAce::GPSSentenceMsg &msg);

    void on_receive(const OpenAce::ConfigUpdatedMsg &msg);
    
    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    /**
     * PFLAA – Data on other proximate aircraft
     * Periodicity:
     * Sent when available. Can be sent several times per second with information on several (but not always all) surrounding targets.
     */
    void sendPFLAA(const OpenAce::AircraftPositionInfo &position);

    uint8_t getPFLAASourceType(const OpenAce::AircraftPositionInfo &position);

    uint8_t getPFLAAAddressType(const OpenAce::AddressType type);

    void getPFLAAGroundSpeed(const OpenAce::AircraftPositionInfo &position, etl::string<6> &speed);

    void getPFLAAClimbRate(const OpenAce::AircraftPositionInfo &position, etl::string<7> &verticalSpeed);

    void getPFLAAAircraftCategory(const OpenAce::AircraftPositionInfo &position, etl::string<2> &type);

    /**
     * PFLAU – Heartbeat, status, and basic alarms
     *
     * Periodicity:
     * Sent once every second (1.8 s at maximum)
     */

    void sendPFLAU(const OpenAce::OwnshipPositionInfo &position);

    /**
     * PFLAE – Self-test result and errors codes
     *
     * Periodicity:
     * Sent once after startup and completion of self-test, when errors occur, and when requested
     *
     */
    void sendPFLAE(const OpenAce::AircraftPositionInfo &position);

    /**
     * PFLAV – Version information
     *
     * Periodicity:
     * Sent once after startup and completion of self-test and when requested
     *
     */
    void sendPFLAV(const OpenAce::AircraftPositionInfo &position);

    /**
     * PFLAR – Reset
     *
     * Periodicity:
     * Not applicable
     *
     */
    void sendPFLAR(const OpenAce::AircraftPositionInfo &position);

    /**
     * PFLAC – Device configuration
     *
     * Periodicity:
     * Sent when requested
     */

    void sendPFLAC(const OpenAce::AircraftPositionInfo &position);

    /**
     * PFLAN – Continuous Analyzer of Radio Performance (CARP).
     *
     * Periodicity:
     * Sent when requested
     */
    void sendPFLANC(const OpenAce::AircraftPositionInfo &position);

    void decimalDegreesToDMM(float degrees, int8_t &deg, float &min);

    /**
     * GPRMC – NMEA minimum recommended GPS navigation data
     */
    void sendGPRMC(const OpenAce::AircraftPositionInfo &position);

    void sendPGRMZ(const OpenAce::OwnshipPositionInfo &position);

    /**
     * GNGSA – GPS DOP and active satellites
     */
    void sendGNGSA(const OpenAce::AircraftPositionInfo &position);

    void sendGPGGA(const OpenAce::AircraftPositionInfo &position);

    /**
     * We seem to need this to get altitude in SkyDemon
     */
    void sendGPGSA();

    void sendLK8EX1();

};
