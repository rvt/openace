#pragma once

#include <stdint.h>
#include <cmath>


// Create abstract class for GeoPositio


// Create mock class for GeoPosition
class GeoPosition
{
public:
    struct Position
    {
        float latitude;
        float longitude;
        float course;
    };

public:
    virtual ~GeoPosition() = default;

    // Each take is a second step in the simulation
    virtual Position take() = 0;
};


class CircularPosition : public GeoPosition
{
    static constexpr float DEG_TO_RADS      = M_PI / 180.f;        // degrees to radians
    static constexpr float DEGREE_METERS = 111'111;
    const float radius;
    const float degreePerTake;

    GeoPosition::Position position;
    GeoPosition::Position center;
public:
// Constructor
    CircularPosition(float radius_, float degreePerTake_, GeoPosition::Position position) : radius(radius_), degreePerTake(degreePerTake_), position(position)
    {
        center = position;
        position.course = 0;
    }

    virtual ~CircularPosition() = default;

    virtual Position take() override
    {
        position.course += degreePerTake;
        if (position.course  >= 360.f)
        {
            position.course  -= 360.f;
        }

        float angle = (position.course - 90.0f) * DEG_TO_RADS;
        position.latitude = center.latitude + radius / 111000.0 * cosf(angle);
        position.longitude = center.longitude + radius / (111000.0 * cosf(center.latitude * DEG_TO_RADS)) * sinf(angle);
        return position;
    };
};

