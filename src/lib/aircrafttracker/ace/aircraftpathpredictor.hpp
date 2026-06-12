#pragma once

#include <stdint.h>
#include <limits.h>
#include <math.h>

#include "ace/constants.hpp"
#include "ace/coreutils.hpp"
#include "ace/models.hpp"

#include "etl/algorithm.h"
#include "etl/array.h"
#include "etl/unordered_map.h"

/**
 * Lightweight per-aircraft path predictor for short radio reception gaps.
 *
 * The predictor keeps up to three recent samples per aircraft and extrapolates
 * only over a short horizon. The covered prediction modes are:
 * - straight horizontal motion with constant ground speed
 * - horizontal constant-turn motion when a valid turn rate is available
 * - derived turn-rate prediction from recent heading history when direct turn
 *   rate is not available
 * - constant vertical-speed climb or descent
 *
 * To keep memory use down on embedded targets, the predictor stores only the
 * kinematic fields needed for prediction. It is intended as a helper that can
 * later be merged into TrackerData rather than as a full aircraft-state cache.
 *
 * The predictor does not recompute ownship-relative distance, so callers must
 * refresh distanceFromOwn after extrapolating.
 */
template <size_t SIZE>
class AircraftPathPredictor
{
public:
    static constexpr size_t HISTORY_SIZE = 3;
    static constexpr uint32_t MAX_PREDICTION_AGE_US = 10'000'000;

private:
    static constexpr float MAX_TURN_RATE_DEG_PER_SEC = 15.0f;
    static constexpr float MIN_TURN_RATE_FOR_ARC_DEG_PER_SEC = 0.1f;
    static constexpr float MIN_GROUNDSPEED_FOR_TURNRATE_MS = 3.0f;
    static constexpr uint32_t MIN_TURNRATE_DT_US = 200'000;
    static constexpr uint32_t MAX_TURNRATE_DT_US = 5'000'000;

    struct TrackSample
    {
        uint32_t timestamp = 0;
        float lat = 0.0f;
        float lon = 0.0f;
        int32_t ellipseHeight = 0;
        float verticalSpeed = 0.0f;
        float groundSpeed = 0.0f;
        int16_t track = 0;
        float hTurnRate = 0.0f;
        bool turnRateValid = false;
    };

    struct TrackState
    {
        etl::array<TrackSample, HISTORY_SIZE> history = {};
        size_t count = 0;
        size_t nextIndex = 0;
        float estimatedTurnRateDegPerSec = 0.0f;
        bool estimatedTurnRateValid = false;

        void push(const TrackSample &sample)
        {
            history[nextIndex] = sample;
            nextIndex = (nextIndex + 1) % HISTORY_SIZE;
            if (count < HISTORY_SIZE)
            {
                count += 1;
            }
        }

        TrackSample &latest()
        {
            return history[(nextIndex + HISTORY_SIZE - 1) % HISTORY_SIZE];
        }

        const TrackSample &latest() const
        {
            return history[(nextIndex + HISTORY_SIZE - 1) % HISTORY_SIZE];
        }

        const TrackSample *previous(size_t stepsBack) const
        {
            if (stepsBack >= count)
            {
                return nullptr;
            }

            return &history[(nextIndex + HISTORY_SIZE - 1 - stepsBack) % HISTORY_SIZE];
        }
    };

    etl::unordered_map<GATAS::AircraftAddress, TrackState, SIZE> tracks_;

    static float clampTurnRate(float turnRateDegPerSec)
    {
        return etl::clamp(turnRateDegPerSec, -MAX_TURN_RATE_DEG_PER_SEC, MAX_TURN_RATE_DEG_PER_SEC);
    }

    static int16_t normalizeTrack(float trackDegrees)
    {
        return static_cast<int16_t>(CoreUtils::toBearing(trackDegrees + 0.5f));
    }

    static float dtSeconds(uint32_t fromUs, uint32_t toUs)
    {
        return static_cast<float>(static_cast<int32_t>(toUs - fromUs)) * 1e-6f;
    }

    static void invalidateRelativeDistance(GATAS::AircraftPositionInfo &position)
    {
        position.distanceFromOwn = static_cast<uint32_t>(INT32_MIN);
    }

    static bool isExpired(uint32_t lastTimestampUs, uint32_t nowUs)
    {
        return CoreUtils::isUsReachedRaw(lastTimestampUs + MAX_PREDICTION_AGE_US, nowUs);
    }

    static TrackSample buildSample(uint32_t timeStampUs,
                                   float lat,
                                   float lon,
                                   int32_t ellipseHeight,
                                   float verticalSpeed,
                                   float groundSpeed,
                                   int16_t track,
                                   float hTurnRate,
                                   bool turnRateValid)
    {
        TrackSample sample;
        sample.timestamp = timeStampUs;
        sample.lat = lat;
        sample.lon = lon;
        sample.ellipseHeight = ellipseHeight;
        sample.verticalSpeed = verticalSpeed;
        sample.groundSpeed = groundSpeed;
        sample.track = normalizeTrack(static_cast<float>(track));
        sample.hTurnRate = clampTurnRate(hTurnRate);
        sample.turnRateValid = turnRateValid;
        return sample;
    }

    /**
     * Derive a horizontal turn rate from two consecutive track samples.
     *
     * Only segments with a usable age and sufficient groundspeed are accepted.
     * Heading deltas are normalized across the 0/360 wrap so short turns around
     * north are handled correctly.
     *
     * @param from Older sample.
     * @param to Newer sample.
     * @param turnRateDegPerSec Output turn rate in degrees per second.
     * @return true when a usable rate could be derived, otherwise false.
     */
    static bool turnRateFromSegment(const TrackSample &from, const TrackSample &to, float &turnRateDegPerSec)
    {
        int32_t dtUs = static_cast<int32_t>(to.timestamp - from.timestamp);
        if (dtUs < static_cast<int32_t>(MIN_TURNRATE_DT_US) || dtUs > static_cast<int32_t>(MAX_TURNRATE_DT_US))
        {
            return false;
        }

        if (from.groundSpeed < MIN_GROUNDSPEED_FOR_TURNRATE_MS || to.groundSpeed < MIN_GROUNDSPEED_FOR_TURNRATE_MS)
        {
            return false;
        }

        float deltaTrack = CoreUtils::wrapLonDelta(static_cast<float>(to.track - from.track));
        turnRateDegPerSec = deltaTrack / (static_cast<float>(dtUs) * 1e-6f);
        return true;
    }

    /**
     * Refresh the effective turn-rate estimate for one aircraft track.
     *
     * Priority is:
     * 1. Use the latest directly supplied turn rate when it is marked valid.
     * 2. Otherwise derive a rate from the most recent one or two history
     *    segments and apply a simple weighted average biased toward the newest
     *    segment.
     * 3. Otherwise clear the estimate so stale curvature cannot leak into later
     *    predictions.
     *
     * @param state Track history and estimated motion state for one aircraft.
     */
    static void refreshTurnRate(TrackState &state)
    {
        if (state.count == 0)
        {
            state.estimatedTurnRateDegPerSec = 0.0f;
            state.estimatedTurnRateValid = false;
            return;
        }

        const TrackSample &latest = state.latest();
        if (latest.turnRateValid)
        {
            state.estimatedTurnRateDegPerSec = clampTurnRate(latest.hTurnRate);
            state.estimatedTurnRateValid = true;
            return;
        }

        float recentRate = 0.0f;
        float olderRate = 0.0f;
        float weightedSum = 0.0f;
        float totalWeight = 0.0f;

        if (const TrackSample *previous = state.previous(1))
        {
            if (turnRateFromSegment(*previous, latest, recentRate))
            {
                weightedSum += recentRate * 2.0f;
                totalWeight += 2.0f;
            }

            if (const TrackSample *older = state.previous(2))
            {
                if (turnRateFromSegment(*older, *previous, olderRate))
                {
                    weightedSum += olderRate;
                    totalWeight += 1.0f;
                }
            }
        }

        if (totalWeight > 0.0f)
        {
            state.estimatedTurnRateDegPerSec = clampTurnRate(weightedSum / totalWeight);
            state.estimatedTurnRateValid = true;
        }
        else
        {
            state.estimatedTurnRateDegPerSec = 0.0f;
            state.estimatedTurnRateValid = false;
        }
    }

    /**
     * Project a short local north/east offset back into latitude/longitude.
     *
     * This uses the same short-distance approximation style already used in the
     * codebase and is intended for small extrapolation intervals, not long-range
     * navigation.
     *
     * @param origin Last known aircraft position.
     * @param northMeters Offset toward geographic north in meters.
     * @param eastMeters Offset toward geographic east in meters.
     * @param lat Output latitude in degrees.
     * @param lon Output longitude in degrees.
     */
    static void projectLocal(const GATAS::AircraftPositionInfo &origin, float northMeters, float eastMeters, float &lat, float &lon)
    {
        lat = origin.lat + northMeters / 111139.0f;

        float cosLat = cosf(origin.lat * DEG_TO_RADS);
        if (fabsf(cosLat) < 0.01f)
        {
            cosLat = cosLat >= 0.0f ? 0.01f : -0.01f;
        }

        lon = CoreUtils::wrapLonDelta(origin.lon + eastMeters / (111321.0f * cosLat));
    }

public:
    /**
     * Insert or replace a track sample using explicit kinematic fields.
     *
     * When the address already exists and the timestamp is identical to the
     * newest stored sample, that newest sample is replaced in place. Older
     * out-of-order updates are rejected.
     *
     * @return true when the sample was accepted, otherwise false.
     */
    bool update(uint32_t timeStampUs,
                GATAS::AircraftAddress address,
                float lat,
                float lon,
                int32_t ellipseHeight,
                float verticalSpeed,
                float groundSpeed,
                int16_t track,
                float hTurnRate,
                bool turnRateValid)
    {
        maintenance(timeStampUs);

        TrackSample sample = buildSample(timeStampUs, lat, lon, ellipseHeight, verticalSpeed, groundSpeed, track, hTurnRate, turnRateValid);

        auto it = tracks_.find(address);
        if (it == tracks_.end())
        {
            if (tracks_.full())
            {
                return false;
            }

            TrackState state;
            state.push(sample);
            refreshTurnRate(state);
            tracks_.insert({address, state});
            return true;
        }

        TrackState &state = it->second;
        if (state.count > 0)
        {
            const TrackSample &latest = state.latest();
            int32_t orderUs = static_cast<int32_t>(timeStampUs - latest.timestamp);
            if (orderUs < 0)
            {
                return false;
            }

            if (orderUs == 0)
            {
                state.latest() = sample;
            }
            else
            {
                state.push(sample);
            }
        }
        else
        {
            state.push(sample);
        }

        refreshTurnRate(state);
        return true;
    }

    /**
     * Insert or replace a track sample from AircraftPositionInfo.
     *
     * Only the kinematic fields used by the predictor are retained.
     *
     * @param position Complete aircraft sample to store.
     * @param turnRateValid True when position.hTurnRate is trusted as measured.
     * @return true when the sample was accepted, otherwise false.
     */
    bool update(const GATAS::AircraftPositionInfo &position, bool turnRateValid)
    {
        maintenance(position.timestamp);

        TrackSample sample = buildSample(position.timestamp,
                                         position.lat,
                                         position.lon,
                                         position.ellipseHeight,
                                         position.verticalSpeed,
                                         position.groundSpeed,
                                         position.track,
                                         position.hTurnRate,
                                         turnRateValid);

        auto it = tracks_.find(position.address);
        if (it == tracks_.end())
        {
            if (tracks_.full())
            {
                return false;
            }

            TrackState state;
            state.push(sample);
            refreshTurnRate(state);
            tracks_.insert({position.address, state});
            return true;
        }

        TrackState &state = it->second;
        if (state.count > 0)
        {
            const TrackSample &latest = state.latest();
            int32_t orderUs = static_cast<int32_t>(sample.timestamp - latest.timestamp);
            if (orderUs < 0)
            {
                return false;
            }

            if (orderUs == 0)
            {
                state.latest() = sample;
            }
            else
            {
                state.push(sample);
            }
        }
        else
        {
            state.push(sample);
        }

        refreshTurnRate(state);
        return true;
    }

    /**
     * Extrapolate an aircraft position at the requested time.
     *
     * Horizontal prediction uses straight-line motion for near-zero turn rates
     * and constant-turn circular-arc motion otherwise. Vertical prediction uses
     * constant vertical speed. Predictions are rejected once the track reaches
     * the configured age limit.
     *
     * Because this standalone predictor has no ownship context, the returned
     * position preserves caller-provided metadata but invalidates distanceFromOwn.
     *
     * @param timeStampUs Requested prediction timestamp.
     * @param position Input/output prediction object. The caller must set
     * position.address to select the aircraft to predict.
     * @return true when a prediction could be produced, otherwise false.
     */
    bool extrapolatedPos(uint32_t timeStampUs, GATAS::AircraftPositionInfo &position) const
    {
        GATAS::AircraftAddress address = position.address;
        auto it = tracks_.find(address);
        if (it == tracks_.end())
        {
            return false;
        }

        const TrackState &state = it->second;
        if (state.count == 0)
        {
            return false;
        }

        const TrackSample &latest = state.latest();
        int32_t ageUs = static_cast<int32_t>(timeStampUs - latest.timestamp);
        if (ageUs < 0)
        {
            return false;
        }

        if (isExpired(latest.timestamp, timeStampUs))
        {
            return false;
        }

        float dt = dtSeconds(latest.timestamp, timeStampUs);
        float turnRateDegPerSec = state.estimatedTurnRateValid ? state.estimatedTurnRateDegPerSec : 0.0f;
        float turnRateRadPerSec = turnRateDegPerSec * DEG_TO_RADS;
        float headingRad = static_cast<float>(latest.track) * DEG_TO_RADS;
        float groundSpeed = latest.groundSpeed;
        float northMeters = 0.0f;
        float eastMeters = 0.0f;

        if (fabsf(turnRateDegPerSec) < MIN_TURN_RATE_FOR_ARC_DEG_PER_SEC)
        {
            northMeters = groundSpeed * dt * cosf(headingRad);
            eastMeters = groundSpeed * dt * sinf(headingRad);
        }
        else
        {
            float headingEnd = headingRad + turnRateRadPerSec * dt;
            float radiusMeters = groundSpeed / turnRateRadPerSec;
            northMeters = radiusMeters * (sinf(headingEnd) - sinf(headingRad));
            eastMeters = radiusMeters * (cosf(headingRad) - cosf(headingEnd));
        }

        position.timestamp = timeStampUs;
        position.address = address;
        position.lat = latest.lat;
        position.lon = latest.lon;
        projectLocal(position, northMeters, eastMeters, position.lat, position.lon);
        position.ellipseHeight = static_cast<int16_t>(static_cast<float>(latest.ellipseHeight) + latest.verticalSpeed * dt);
        position.verticalSpeed = latest.verticalSpeed;
        position.groundSpeed = latest.groundSpeed;
        position.track = normalizeTrack(static_cast<float>(latest.track) + turnRateDegPerSec * dt);
        position.hTurnRate = turnRateDegPerSec;
        position.airborne = latest.groundSpeed > GATAS::GROUNDSPEED_CONSIDERING_AIRBORN;
        invalidateRelativeDistance(position);
        return true;
    }

    /**
     * Remove tracks whose newest sample has reached the maximum prediction age.
     *
     * @param nowUs Current timestamp used for age checks.
     */
    void maintenance(uint32_t nowUs)
    {
        for (auto it = tracks_.begin(); it != tracks_.end();)
        {
            if (it->second.count == 0 || isExpired(it->second.latest().timestamp, nowUs))
            {
                it = tracks_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void remove(GATAS::AircraftAddress address)
    {
        tracks_.erase(address);
    }

    bool contains(GATAS::AircraftAddress address) const
    {
        return tracks_.find(address) != tracks_.end();
    }

    bool full() const
    {
        return tracks_.full();
    }

    size_t size() const
    {
        return tracks_.size();
    }
};
