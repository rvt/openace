#!/usr/bin/env python3

import argparse
import json
import math
from dataclasses import asdict, dataclass

DEG_TO_RAD = math.pi / 180.0
RAD_TO_DEG = 180.0 / math.pi
LAT_METERS_PER_DEG = 111_139.0
LON_METERS_PER_DEG_AT_EQUATOR = 111_321.0


@dataclass
class AircraftState:
    lat: float
    lon: float
    track_deg: float
    altitude_m: float
    horizontal_distance_traveled_m: float
    distance_traveled_3d_m: float


def normalize_track(track_deg: float) -> float:
    return track_deg % 360.0


def clamp_cos_lat(lat_deg: float) -> float:
    cos_lat = math.cos(lat_deg * DEG_TO_RAD)
    if abs(cos_lat) < 0.01:
        return 0.01 if cos_lat >= 0.0 else -0.01
    return cos_lat


def wrap_lon(lon_deg: float) -> float:
    return ((lon_deg + 180.0) % 360.0) - 180.0


def project_step(lat_deg: float, lon_deg: float, north_m: float, east_m: float) -> tuple[float, float]:
    next_lat = lat_deg + north_m / LAT_METERS_PER_DEG
    next_lon = lon_deg + east_m / (LON_METERS_PER_DEG_AT_EQUATOR * clamp_cos_lat(lat_deg))
    return next_lat, wrap_lon(next_lon)


def simulate(lat: float,
             lon: float,
             track_deg: float,
             altitude_m: float,
             speed_mps: float,
             turn_rate_deg_per_sec: float,
             vertical_rate_mps: float,
             simulation_time_ms: int,
             step_ms: int) -> AircraftState:
    if simulation_time_ms < 0:
        raise ValueError("simulation_time_ms must be >= 0")
    if step_ms <= 0:
        raise ValueError("step_ms must be > 0")

    state = AircraftState(
        lat=lat,
        lon=lon,
        track_deg=normalize_track(track_deg),
        altitude_m=altitude_m,
        horizontal_distance_traveled_m=0.0,
        distance_traveled_3d_m=0.0,
    )

    remaining_ms = simulation_time_ms
    while remaining_ms > 0:
        current_step_ms = min(step_ms, remaining_ms)
        dt_sec = current_step_ms / 1000.0

        # Use midpoint heading for this discrete step so turning motion converges
        # more quickly than a pure forward-Euler position update.
        midpoint_track_deg = normalize_track(state.track_deg + 0.5 * turn_rate_deg_per_sec * dt_sec)
        midpoint_track_rad = midpoint_track_deg * DEG_TO_RAD

        north_m = speed_mps * dt_sec * math.cos(midpoint_track_rad)
        east_m = speed_mps * dt_sec * math.sin(midpoint_track_rad)
        state.lat, state.lon = project_step(state.lat, state.lon, north_m, east_m)

        state.track_deg = normalize_track(state.track_deg + turn_rate_deg_per_sec * dt_sec)
        state.altitude_m += vertical_rate_mps * dt_sec
        horizontal_step_m = speed_mps * dt_sec
        vertical_step_m = vertical_rate_mps * dt_sec
        state.horizontal_distance_traveled_m += horizontal_step_m
        state.distance_traveled_3d_m += math.hypot(horizontal_step_m, vertical_step_m)
        remaining_ms -= current_step_ms

    return state


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Mini discrete flight simulator for validating aircraft path prediction."
    )
    parser.add_argument("--lat", type=float, required=True, help="Initial latitude in degrees")
    parser.add_argument("--lon", type=float, required=True, help="Initial longitude in degrees")
    parser.add_argument("--track", type=float, required=True, help="Initial track in degrees (0=north, 90=east)")
    parser.add_argument("--altitude", type=float, required=True, help="Initial altitude in meters")
    parser.add_argument("--speed", type=float, required=True, help="Ground speed in m/s")
    parser.add_argument("--turnrate", type=float, required=True, help="Turn rate in degrees/second")
    parser.add_argument("--verticalrate", type=float, required=True, help="Vertical rate in m/s")
    parser.add_argument("--time-ms", type=int, required=True, help="Total simulation time in milliseconds")
    parser.add_argument("--step-ms", type=int, required=True, help="Simulation integration step in milliseconds")
    parser.add_argument("--json", action="store_true", help="Output final state as JSON")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    result = simulate(
        lat=args.lat,
        lon=args.lon,
        track_deg=args.track,
        altitude_m=args.altitude,
        speed_mps=args.speed,
        turn_rate_deg_per_sec=args.turnrate,
        vertical_rate_mps=args.verticalrate,
        simulation_time_ms=args.time_ms,
        step_ms=args.step_ms,
    )

    if args.json:
        print(json.dumps(asdict(result), indent=2, sort_keys=True))
    else:
        print(f"lat={result.lat:.8f}")
        print(f"lon={result.lon:.8f}")
        print(f"track_deg={result.track_deg:.3f}")
        print(f"altitude_m={result.altitude_m:.3f}")
        print(f"horizontal_distance_traveled_m={result.horizontal_distance_traveled_m:.3f}")
        print(f"distance_traveled_3d_m={result.distance_traveled_3d_m:.3f}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
