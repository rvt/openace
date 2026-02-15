#pragma once

#include <stdint.h>
#include <sys/time.h>

#include "ace/constants.hpp"
#include "ace/models.hpp"
#include "ace/basemodule.hpp"
#include "ace/coreutils.hpp"
#include "ace/messages.hpp"
#include "ace/messages.hpp"
#include "ace/spinlockguard.hpp"

#include "etl/map.h"
#include "etl/message_bus.h"

class DataPort : public BaseModule, public etl::message_router<DataPort, GATAS::TrackedAircraftPositionMsg, GATAS::OwnshipPositionMsg, GATAS::GPSSentenceMsg, GATAS::Every30SecMsg, GATAS::WifiConnectionStateMsg>
{
    friend class message_router;
    GATAS::OwnshipMinimalPositionInfo ownshipPosition;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    struct
    {
        uint32_t messages = 0;
    } statistics;

    uint32_t gatasIp;

public:
    static constexpr const etl::string_view NAME = "DataPort";
    DataPort(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME), /*spinLock(0),*/ gatasIp(0)
    {
        (void)config;
    }

    virtual ~DataPort() = default;

    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override
    {
        getBus().subscribe(*this);
    };

    void on_receive(const GATAS::TrackedAircraftPositionMsg &msg);

    void on_receive(const GATAS::OwnshipPositionMsg &msg);

    void on_receive(const GATAS::GPSSentenceMsg &msg);

    void on_receive(const GATAS::WifiConnectionStateMsg &msg);

    void on_receive(const GATAS::Every30SecMsg &msg);

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    /**
     * PFLAA – Data on other proximate aircraft
     * Periodicity:
     * Sent when available. Can be sent several times per second with information on several (but not always all) surrounding targets.
     */
    void sendPFLAA(const GATAS::AircraftPositionInfo &position);

    uint8_t getPFLAASourceType(const GATAS::AircraftPositionInfo &position);

    uint8_t getPFLAAAddressType(const GATAS::AddressType type);

    void getPFLAAGroundSpeed(const GATAS::AircraftPositionInfo &position, etl::string<6> &speed);

    void getPFLAAClimbRate(const GATAS::AircraftPositionInfo &position, etl::string<7> &verticalSpeed);

    etl::string_view getPFLAAAircraftCategory(const GATAS::AircraftPositionInfo &position) const;

    /**
     * PFLAU – Heartbeat, status, and basic alarms
     *
     * Periodicity:
     * Sent once every second (1.8 s at maximum)
     */

    void sendPFLAU(const GATAS::OwnshipPositionInfo &position);

    /**
     * PFLAE – Self-test result and errors codes
     *
     * Periodicity:
     * Sent once after startup and completion of self-test, when errors occur, and when requested
     *
     */
    void sendPFLAE(const GATAS::AircraftPositionInfo &position);

    /**
     * PFLAV – Version information
     *
     * Periodicity:
     * Sent once after startup and completion of self-test and when requested
     *
     */
    void sendPFLAV(const GATAS::AircraftPositionInfo &position);

    /**
     * PFLAR – Reset
     *
     * Periodicity:
     * Not applicable
     *
     */
    void sendPFLAR(const GATAS::AircraftPositionInfo &position);

    /**
     * PFLAC – Device configuration
     *
     * Periodicity:
     * Sent when requested
     */

    void sendPFLAC(const GATAS::AircraftPositionInfo &position);

    /**
     * PFLAN – Continuous Analyzer of Radio Performance (CARP).
     *
     * Periodicity:
     * Sent when requested
     */
    void sendPFLANC(const GATAS::AircraftPositionInfo &position);

    void decimalDegreesToDMM(float degrees, int8_t &deg, float &min);

    /**
     * GPRMC – NMEA minimum recommended GPS navigation data
     */
    // void sendGPRMC(const GATAS::OwnshipPositionInfo &position);

    // void sendPGRMZ(const GATAS::OwnshipPositionInfo &position);

    /**
     * GNGSA – GPS DOP and active satellites
     */
    // void sendGNGSA(const GATAS::AircraftPositionInfo &position);

    // void sendGPGGA(const GATAS::OwnshipPositionInfo &position);

    /**
     * We seem to need this to get altitude in SkyDemon
     */
    void sendGPGSA();

    void sendLK8EX1();
};
