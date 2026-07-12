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
 * - protocol-provided or derived horizontal constant-turn motion
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
    static constexpr int32_t MIN_OUTPUT_PREDICTION_AGE_US = 500'000;
    static constexpr float MAX_TURN_RATE_DEG_PER_SEC = 15.0f;
    static constexpr float MIN_TURN_RATE_FOR_ARC_DEG_PER_SEC = 0.1f;
    static constexpr float MIN_GROUNDSPEED_FOR_TURNRATE_MS = 3.0f;
    static constexpr uint32_t MIN_TURNRATE_DT_US = 200'000;
    static constexpr uint32_t MAX_TURNRATE_DT_US = 5'000'000;

    static int16_t normalizeTrack(float trackDegrees)
    {
        return static_cast<int16_t>(CoreUtils::toBearing(trackDegrees + 0.5f));
    }

    struct HistorySample
    {
        uint32_t timestamp = 0;
        float groundSpeed = 0.0f;
        float hTurnRate = 0.0f;
        int16_t track = 0;
    };

    struct TrackState
    {
        struct CachedPrediction
        {
            float lat = 0.0f;
            float lon = 0.0f;
            int32_t ellipseHeight = 0;
            int16_t track = 0;
        };

        etl::array<HistorySample, HISTORY_SIZE> history = {};
        mutable CachedPrediction cachedPrediction = {};
        float estimatedTurnRateDegPerSec = 0.0f;
        uint32_t priorityDistance = UINT32_MAX;
        mutable uint32_t cachedPredictionTimestamp = 0;
        uint8_t historyCount = 0;
        uint8_t nextHistoryIndex = 0;
        bool estimatedTurnRateValid = false;
        mutable bool cachedPredictionValid = false;

        void push(const GATAS::AircraftPositionInfo &sample)
        {
            history[nextHistoryIndex] = toHistorySample(sample);
            nextHistoryIndex = static_cast<uint8_t>((nextHistoryIndex + 1) % HISTORY_SIZE);
            if (historyCount < HISTORY_SIZE)
            {
                historyCount += 1;
            }
            invalidateCache();
        }

        void replaceLatest(const GATAS::AircraftPositionInfo &sample)
        {
            history[(nextHistoryIndex + HISTORY_SIZE - 1) % HISTORY_SIZE] = toHistorySample(sample);
            invalidateCache();
        }

        const HistorySample &latest() const
        {
            return history[(nextHistoryIndex + HISTORY_SIZE - 1) % HISTORY_SIZE];
        }

        const HistorySample *previous(size_t stepsBack) const
        {
            if (stepsBack >= historyCount)
            {
                return nullptr;
            }

            return &history[(nextHistoryIndex + HISTORY_SIZE - 1 - stepsBack) % HISTORY_SIZE];
        }

        void invalidateCache()
        {
            cachedPredictionValid = false;
        }

        static HistorySample toHistorySample(const GATAS::AircraftPositionInfo &position)
        {
            HistorySample sample;
            sample.timestamp = position.timestamp;
            sample.groundSpeed = position.groundSpeed;
            sample.hTurnRate = position.hTurnRate;
            sample.track = normalizeTrack(static_cast<float>(position.track));
            return sample;
        }
    };

    using TrackMap = etl::unordered_map<GATAS::AircraftAddress, TrackState, SIZE>;

    TrackMap tracks_;
    bool enabledFlag = true;

    static float clampTurnRate(float turnRateDegPerSec)
    {
        return etl::clamp(turnRateDegPerSec, -MAX_TURN_RATE_DEG_PER_SEC, MAX_TURN_RATE_DEG_PER_SEC);
    }

    static float dtSeconds(uint32_t fromUs, uint32_t toUs)
    {
        return static_cast<float>(static_cast<int32_t>(toUs - fromUs)) * 1e-6f;
    }

    static void invalidateRelativeDistance(GATAS::AircraftPositionInfo &position)
    {
        position.distanceFromOwn = static_cast<uint32_t>(INT32_MIN);
    }

    static void applyPredictedState(uint32_t timeStampUs,
                                    const typename TrackState::CachedPrediction &prediction,
                                    float hTurnRate,
                                    GATAS::AircraftPositionInfo &position)
    {
        // Timestamp must match the predicted location, not the original sample.
        position.timestamp = timeStampUs;
        position.lat = prediction.lat;
        position.lon = prediction.lon;
        position.ellipseHeight = prediction.ellipseHeight;
        position.hTurnRate = hTurnRate;
        position.track = prediction.track;
        invalidateRelativeDistance(position);
    }

    static bool isExpired(uint32_t lastTimestampUs, uint32_t nowUs)
    {
        return CoreUtils::isUsReached(lastTimestampUs + MAX_PREDICTION_AGE_US, nowUs);
    }

    typename TrackMap::iterator findActiveTrack(GATAS::AircraftAddress address, uint32_t nowUs)
    {
        auto it = tracks_.find(address);
        if (it != tracks_.end())
        {
            if (it->second.historyCount == 0 || isExpired(it->second.latest().timestamp, nowUs))
            {
                tracks_.erase(it);
                return tracks_.end();
            }
        }
        return it;
    }

    void reclaimExpiredIfFull(uint32_t nowUs)
    {
        if (tracks_.full())
        {
            maintenance(nowUs);
        }
    }

    static bool turnRateFromSegment(const HistorySample &from, const HistorySample &to, float &turnRateDegPerSec)
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
     * A finite turn rate supplied by the protocol takes priority. By contract,
     * zero means straight flight. Only a non-finite value means unavailable and
     * enables derivation from recent heading segments.
     *
     * @param state Track history and estimated motion state for one aircraft.
     */
    static void refreshTurnRate(TrackState &state)
    {
        if (state.historyCount == 0)
        {
            state.estimatedTurnRateDegPerSec = 0.0f;
            state.estimatedTurnRateValid = false;
            return;
        }

        const HistorySample &latest = state.latest();
        if (isfinite(latest.hTurnRate))
        {
            state.estimatedTurnRateDegPerSec = clampTurnRate(latest.hTurnRate);
            state.estimatedTurnRateValid = true;
            return;
        }

        float recentRate = 0.0f;
        float olderRate = 0.0f;
        float weightedSum = 0.0f;
        float totalWeight = 0.0f;

        if (const HistorySample *previous = state.previous(1))
        {
            if (turnRateFromSegment(*previous, latest, recentRate))
            {
                weightedSum += recentRate * 2.0f;
                totalWeight += 2.0f;
            }

            if (const HistorySample *older = state.previous(2))
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
    static void projectLocal(float originLat, float originLon, float northMeters, float eastMeters, float &lat, float &lon)
    {
        lat = originLat + northMeters / 111139.0f;

        float cosLat = cosf(originLat * DEG_TO_RADS);
        if (fabsf(cosLat) < 0.01f)
        {
            cosLat = cosLat >= 0.0f ? 0.01f : -0.01f;
        }

        lon = CoreUtils::wrapLonDelta(originLon + eastMeters / (111321.0f * cosLat));
    }

    bool removeFarthestIfCloser(uint32_t candidateDistance)
    {
        if (!tracks_.full())
        {
            return true;
        }

        auto farthest = tracks_.end();
        for (auto it = tracks_.begin(); it != tracks_.end(); ++it)
        {
            if (farthest == tracks_.end() || farthest->second.priorityDistance < it->second.priorityDistance)
            {
                farthest = it;
            }
        }

        if (farthest == tracks_.end() || candidateDistance >= farthest->second.priorityDistance)
        {
            return false;
        }

        tracks_.erase(farthest);
        return true;
    }

public:
    void enabled(bool enabled)
    {
        enabledFlag = enabled;
    }

    bool enabled() const
    {
        return enabledFlag;
    }

    /**
     * Insert or replace a track sample from AircraftPositionInfo.
     *
     * Only the kinematic fields used by the predictor are retained.
     *
     * @param position Complete aircraft sample to store.
     * @return true when the sample was accepted, otherwise false.
     */
    bool update(const GATAS::AircraftPositionInfo &position)
    {
        auto it = findActiveTrack(position.address, position.timestamp);
        if (it == tracks_.end())
        {
            reclaimExpiredIfFull(position.timestamp);
            if (!removeFarthestIfCloser(position.distanceFromOwn))
            {
                return false;
            }

            TrackState state;
            state.priorityDistance = position.distanceFromOwn;
            state.push(position);
            refreshTurnRate(state);
            tracks_.insert({position.address, state});
            return true;
        }

        TrackState &state = it->second;
        if (state.historyCount > 0)
        {
            int32_t orderUs = static_cast<int32_t>(position.timestamp - state.latest().timestamp);
            if (orderUs < 0)
            {
                return false;
            }

            if (orderUs == 0)
            {
                state.replaceLatest(position);
            }
            else
            {
                state.push(position);
            }
        }
        else
        {
            state.push(position);
        }

        state.priorityDistance = position.distanceFromOwn;
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
     * Because this standalone predictor has no ownship context, a predicted
     * position preserves caller-provided metadata but invalidates distanceFromOwn.
     * If no prediction can be produced, the input position is returned unchanged.
     *
     * @param timeStampUs Requested prediction timestamp.
     * @param position Latest measured aircraft state.
     * @return Predicted copy when possible, otherwise the original position.
     */
    GATAS::AircraftPositionInfo extrapolatedPos(uint32_t timeStampUs, const GATAS::AircraftPositionInfo &position) const
    {
        if (!enabledFlag)
        {
            // Prediction is disabled, so the measured position is the best available position.
            return position;
        }

        GATAS::AircraftAddress address = position.address;
        auto it = tracks_.find(address);
        if (it == tracks_.end())
        {
            // No predictor history exists for this aircraft, so keep the measured position.
            return position;
        }

        const TrackState &state = it->second;
        if (state.historyCount == 0)
        {
            // The track has no usable samples, so there is nothing to extrapolate from.
            return position;
        }

        const HistorySample &latest = state.latest();
        int32_t ageUs = static_cast<int32_t>(timeStampUs - latest.timestamp);
        if (ageUs < 0)
        {
            // The requested time is before the newest sample, so extrapolation would go backward.
            return position;
        }

        if (ageUs < MIN_OUTPUT_PREDICTION_AGE_US)
        {
            // The sample is recent enough that the measured position is still preferable.
            return position;
        }

        if (isExpired(latest.timestamp, timeStampUs))
        {
            // The sample is too old for a trustworthy short-term prediction.
            return position;
        }

        float turnRateDegPerSec = state.estimatedTurnRateValid ? state.estimatedTurnRateDegPerSec : 0.0f;

        if (state.cachedPredictionValid)
        {
            if (state.cachedPredictionTimestamp == timeStampUs)
            {
                GATAS::AircraftPositionInfo result = position;
                applyPredictedState(timeStampUs, state.cachedPrediction, turnRateDegPerSec, result);
                return result;
            }
        }

        float dt = dtSeconds(latest.timestamp, timeStampUs);
        float turnRateRadPerSec = turnRateDegPerSec * DEG_TO_RADS;
        float headingRad = static_cast<float>(latest.track) * DEG_TO_RADS;
        float groundSpeed = position.groundSpeed;
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

        typename TrackState::CachedPrediction prediction;
        prediction.lat = position.lat;
        prediction.lon = position.lon;
        projectLocal(position.lat, position.lon, northMeters, eastMeters, prediction.lat, prediction.lon);
        prediction.ellipseHeight = static_cast<int32_t>(static_cast<float>(position.ellipseHeight) + position.verticalSpeed * dt);
        prediction.track = normalizeTrack(static_cast<float>(latest.track) + turnRateDegPerSec * dt);
        state.cachedPrediction = prediction;
        state.cachedPredictionTimestamp = timeStampUs;
        state.cachedPredictionValid = true;

        GATAS::AircraftPositionInfo result = position;
        applyPredictedState(timeStampUs, prediction, turnRateDegPerSec, result);
        return result;
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
            if (it->second.historyCount == 0 || isExpired(it->second.latest().timestamp, nowUs))
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
