#pragma once

#include <stdint.h>

#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "etl/message_bus.h"
#include "etl/pseudo_moving_average.h"

#include "ace/constants.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"

/**
 * Client that can connect to a host and a port and expect to receive line terminated NMEA Messages
 * Part of this code taken from the example from Raspbery
 */
class Bmp280 : public BaseModule, public etl::message_router<Bmp280, GATAS::ConfigUpdatedMsg, GATAS::Every30SecMsg>
{
    friend class message_router;
    struct
    {
        float lastPressurehPa = 1013;
    } statistics;

    static constexpr uint8_t READ_BIT = 0x80;

    const uint8_t cs;
    int16_t compensation;

    int32_t t_fine = 0;
    uint16_t dig_T1 = 0;
    int16_t dig_T2 = 0, dig_T3 = 0;
    uint16_t dig_P1 = 0;
    int16_t dig_P2 = 0, dig_P3 = 0, dig_P4 = 0, dig_P5 = 0, dig_P6 = 0, dig_P7 = 0, dig_P8 = 0, dig_P9 = 0;
    SpiModule *aceSpi = nullptr;
    
private:
    /* The following compensation functions are required to convert from the raw ADC
    data from the chip to something usable. Each chip has a different set of
    compensation parameters stored on the chip at point of manufacture, which are
    read from the chip at startup and used in these routines.
    */
    int32_t compensate_temp(int32_t adc_T);
    uint32_t compensate_pressure(int32_t adc_P);
    /* This function reads the manufacturing assigned compensation parameters from the device */
    void read_compensation_parameters();

    void on_receive_unknown(const etl::imessage &msg);

    void on_receive(const GATAS::ConfigUpdatedMsg &msg);

    void on_receive(const GATAS::Every30SecMsg &msg);

public:
    static constexpr const etl::string_view NAME = "Bmp280";
    Bmp280(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
                                                                  cs(config.pinMap(NAME).at(GATAS::PinType::CS))
    {
        compensation = config.valueByPath(0, NAME, "compensation");
    }

    virtual ~Bmp280() = default;

    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
};
