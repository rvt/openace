# Aircraft Tracker Test Helpers

## `mini_flight_sim.py`

Small discrete flight simulator used to sanity-check the path predictor tests.

It advances the aircraft state step-by-step instead of using the closed-form
math from `AircraftPathPredictor`, so it is useful as an independent reference.

### Inputs

Required command-line arguments:

- `--lat` initial latitude in degrees
- `--lon` initial longitude in degrees
- `--track` initial track in degrees (`0 = north`, `90 = east`)
- `--altitude` initial altitude in meters
- `--speed` horizontal ground speed in m/s
- `--turnrate` horizontal turn rate in degrees/second
- `--verticalrate` vertical speed in m/s
- `--time-ms` total simulation time in milliseconds
- `--step-ms` integration step size in milliseconds

Optional:

- `--json` output the final state as JSON

### Outputs

The simulator prints the final:

- `lat`
- `lon`
- `track_deg`
- `altitude_m`
- `horizontal_distance_traveled_m`
- `distance_traveled_3d_m`

### Important model assumptions

- `speed` is **horizontal** ground speed, not 3D airspeed.
- `track` is the **horizontal** direction of travel.
- `verticalrate` only affects altitude and 3D path length.
- For predictor validation, `horizontal_distance_traveled_m` is usually the
  relevant comparison, because the predictor handles horizontal and vertical
  motion separately.

### Example

```bash
python3 mini_flight_sim.py \
  --lat 52.0 \
  --lon 4.0 \
  --track 90 \
  --altitude 1000 \
  --speed 50 \
  --turnrate 3 \
  --verticalrate -2 \
  --time-ms 180000 \
  --step-ms 100 \
  --json
```

Example output:

```json
{
  "altitude_m": 639.9999999999181,
  "distance_traveled_3d_m": 9007.197122301302,
  "horizontal_distance_traveled_m": 9000.0,
  "lat": 51.98281555980561,
  "lon": 4.0000125964279505,
  "track_deg": 270.0000000000129
}
```

### Notes for comparing with predictor tests

- Use a small `--step-ms` such as `10` or `1` when validating curved motion.
- Antimeridian crossings need care: longitude wraps into `[-180, 180)`, so
  naive longitude subtraction can look wrong even when the result is correct.
