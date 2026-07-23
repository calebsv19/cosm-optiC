#!/usr/bin/env python3
"""Render and diagnose deterministic high-resolution water photon caustics."""

from __future__ import annotations

import argparse
import json
import math
import platform
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
import generate_ray_tracing_denoise_review_artifacts as images  # noqa: E402

GRID = 96
FRAMES = 8
EXTENT_X = 4.0
EXTENT_Y = 2.0
BASE_HEIGHT = 0.72
TRIANGLES = 2 * (GRID - 1) * (GRID - 1)
CAPILLARY_GRID = 144
CAPILLARY_MODES = (
    (0.80, 0.0060, 0.13, 0.7),
    (0.66, 0.0052, 0.81, 2.1),
    (0.55, 0.0046, 1.47, 4.2),
    (0.46, 0.0040, 2.18, 1.2),
    (0.39, 0.0035, 2.79, 5.1),
    (0.33, 0.0031, 0.49, 3.3),
    (0.28, 0.0027, 1.16, 0.2),
    (0.24, 0.0024, 1.91, 2.8),
    (0.21, 0.0021, 2.55, 4.7),
    (0.18, 0.0018, 0.31, 5.8),
    (0.155, 0.0015, 1.68, 1.7),
    (0.135, 0.0012, 2.39, 3.9),
)


def dump(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def wave(x: float, y: float, frame: int) -> float:
    p = frame * 2.0 * math.pi / FRAMES
    return BASE_HEIGHT + (
        0.036 * math.sin(2.0 * math.pi * x / 1.18 + p)
        + 0.024 * math.sin(2.0 * math.pi * (0.42 * x + y) / 0.83 - 0.7 * p)
        + 0.014 * math.cos(2.0 * math.pi * (x - 0.63 * y) / 0.51 + 1.3 * p)
    )


def water_frame(frame: int) -> dict:
    spacing_x = EXTENT_X / (GRID - 1)
    spacing_y = EXTENT_Y / (GRID - 1)
    heights = [
        wave(x * spacing_x, y * spacing_y, frame)
        for y in range(GRID)
        for x in range(GRID)
    ]
    normals: list[float] = []
    max_slope = 0.0
    for y in range(GRID):
        for x in range(GRID):
            xl, xr = max(0, x - 1), min(GRID - 1, x + 1)
            yd, yu = max(0, y - 1), min(GRID - 1, y + 1)
            dx, dy = (xr - xl) * spacing_x, (yu - yd) * spacing_y
            sx = (heights[y * GRID + xr] - heights[y * GRID + xl]) / dx
            sy = (heights[yu * GRID + x] - heights[yd * GRID + x]) / dy
            length = math.sqrt(sx * sx + 1.0 + sy * sy)
            normals.extend((-sx / length, 1.0 / length, -sy / length))
            max_slope = max(max_slope, math.hypot(sx, sy))
    return {
        "schema": "physics_sim_water_surface_heightfield_v1",
        "version": 1,
        "frame_index": frame,
        "time_seconds": frame / 24.0,
        "dt_seconds": 1.0 / 24.0,
        "surface_representation": "heightfield",
        "layout": "row_major_z_x",
        "surface_axis": "y",
        "height_units": "meters",
        "grid_w": GRID,
        "grid_d": GRID,
        "sample_count": GRID * GRID,
        "volume_grid_w": GRID,
        "volume_grid_h": 16,
        "volume_grid_d": GRID,
        "origin_x": 0.0,
        "origin_y": 0.0,
        "origin_z": 0.0,
        "sample_origin_x": 0.0,
        "sample_origin_z": 0.0,
        "sample_spacing_x": spacing_x,
        "sample_spacing_z": spacing_y,
        "density_threshold": 0.5,
        "summary": {
            "wet_columns": GRID * GRID,
            "dry_columns": 0,
            "solid_columns": 0,
            "water_cells": GRID * GRID,
            "surface_min_y": min(heights),
            "surface_max_y": max(heights),
            "surface_avg_y": sum(heights) / len(heights),
            "max_slope": max_slope,
            "review_ripples_applied": True,
            "finite_normals": True,
        },
        "heights_y": heights,
        "normals_xyz": normals,
    }


def bilinear_source_height(source: dict, u: float, v: float) -> float:
    grid_w = int(source["grid_w"])
    grid_d = int(source["grid_d"])
    heights = source["heights_y"]
    sx = 1.0 + u * (grid_w - 3)
    sy = 1.0 + v * (grid_d - 3)
    x0 = min(grid_w - 2, int(sx))
    y0 = min(grid_d - 2, int(sy))
    x1 = min(grid_w - 2, x0 + 1)
    y1 = min(grid_d - 2, y0 + 1)
    tx, ty = sx - x0, sy - y0
    a = float(heights[y0 * grid_w + x0])
    b = float(heights[y0 * grid_w + x1])
    c = float(heights[y1 * grid_w + x0])
    d = float(heights[y1 * grid_w + x1])
    return (
        a * (1.0 - tx) * (1.0 - ty)
        + b * tx * (1.0 - ty)
        + c * (1.0 - tx) * ty
        + d * tx * ty
    )


def capillary_review_source(
    source: dict,
    frame: int,
    capillary_grid: int = CAPILLARY_GRID,
) -> dict:
    positive = [float(value) for value in source["heights_y"] if value > 0.0]
    source_mean = sum(positive) / len(positive)
    review_time_seconds = frame / 12.0
    heights: list[float] = []
    for y in range(capillary_grid):
        world_y = y / (capillary_grid - 1) * EXTENT_Y
        for x in range(capillary_grid):
            world_x = x / (capillary_grid - 1) * EXTENT_X
            macro = bilinear_source_height(
                source,
                x / (capillary_grid - 1),
                y / (capillary_grid - 1),
            )
            height = BASE_HEIGHT + 0.38 * (macro - source_mean)
            for index, (wavelength, amplitude, angle, phase) in enumerate(
                CAPILLARY_MODES
            ):
                projected = world_x * math.cos(angle) + world_y * math.sin(angle)
                wave_number = 2.0 * math.pi / wavelength
                angular_frequency = math.sqrt(
                    9.81 * wave_number + 0.000074 * wave_number**3
                )
                motion = angular_frequency * review_time_seconds * 0.32
                height += amplitude * math.sin(
                    wave_number * projected - motion + phase
                )
            heights.append(height)
    return {
        **source,
        "grid_w": capillary_grid,
        "grid_d": capillary_grid,
        "sample_count": capillary_grid * capillary_grid,
        "volume_grid_w": capillary_grid,
        "volume_grid_d": capillary_grid,
        "heights_y": heights,
        "summary": {
            **source.get("summary", {}),
            "wet_columns": capillary_grid * capillary_grid,
            "dry_columns": 0,
            "solid_columns": 0,
            "photon_fixture_capillary_review": True,
            "photon_fixture_capillary_mode_count": len(CAPILLARY_MODES),
            "photon_fixture_capillary_grid": capillary_grid,
            "photon_fixture_macro_amplitude_scale": 0.38,
            "photon_fixture_capillary_time_seconds": review_time_seconds,
            "photon_fixture_capillary_speed_scale": 0.32,
        },
    }


def retarget_water_frame(
    source: dict,
    frame: int,
    capillary_review: bool = False,
    capillary_grid: int = CAPILLARY_GRID,
) -> dict:
    source_frame_index = int(source.get("frame_index", frame))
    source_time_seconds = float(source.get("time_seconds", frame / 12.0))
    if capillary_review:
        source = capillary_review_source(source, frame, capillary_grid)
    grid_w = int(source["grid_w"])
    grid_d = int(source["grid_d"])
    heights = [float(value) for value in source["heights_y"]]
    positive = [value for value in heights if value > 0.0]
    source_mean = sum(positive) / len(positive)
    heights = [
        0.0 if value <= 0.0 else BASE_HEIGHT + value - source_mean
        for value in heights
    ]
    spacing_x = EXTENT_X / (grid_w - 1)
    spacing_y = EXTENT_Y / (grid_d - 1)
    normals: list[float] = []
    max_slope = 0.0
    for y in range(grid_d):
        for x in range(grid_w):
            if heights[y * grid_w + x] <= 0.0:
                normals.extend((0.0, 1.0, 0.0))
                continue
            xl, xr = max(0, x - 1), min(grid_w - 1, x + 1)
            yd, yu = max(0, y - 1), min(grid_d - 1, y + 1)
            left = heights[y * grid_w + xl] or heights[y * grid_w + x]
            right = heights[y * grid_w + xr] or heights[y * grid_w + x]
            down = heights[yd * grid_w + x] or heights[y * grid_w + x]
            up = heights[yu * grid_w + x] or heights[y * grid_w + x]
            sx = (right - left) / max(spacing_x, (xr - xl) * spacing_x)
            sy = (up - down) / max(spacing_y, (yu - yd) * spacing_y)
            length = math.sqrt(sx * sx + 1.0 + sy * sy)
            normals.extend((-sx / length, 1.0 / length, -sy / length))
            max_slope = max(max_slope, math.hypot(sx, sy))
    return {
        **source,
        "frame_index": frame,
        "time_seconds": frame / 12.0,
        "dt_seconds": 1.0 / 12.0,
        "source_frame_index": source_frame_index,
        "source_time_seconds": source_time_seconds,
        "origin_x": 0.0,
        "origin_y": 0.0,
        "origin_z": 0.0,
        "sample_origin_x": 0.0,
        "sample_origin_z": 0.0,
        "sample_spacing_x": spacing_x,
        "sample_spacing_z": spacing_y,
        "summary": {
            **source.get("summary", {}),
            "surface_min_y": min(positive) - source_mean + BASE_HEIGHT,
            "surface_max_y": max(positive) - source_mean + BASE_HEIGHT,
            "surface_avg_y": BASE_HEIGHT,
            "max_slope": max_slope,
            "photon_fixture_retargeted": True,
            "photon_fixture_source_frame_index": source_frame_index,
            "photon_fixture_source_time_seconds": source_time_seconds,
        },
        "heights_y": heights,
        "normals_xyz": normals,
    }


def water_bundle(
    root: Path,
    source_manifest: Path | None = None,
    capillary_review: bool = False,
    capillary_grid: int = CAPILLARY_GRID,
) -> tuple[Path, dict]:
    frames = []
    source_frames: list[dict] = []
    source_metadata: dict = {"kind": "procedural_fixture"}
    if source_manifest is not None:
        manifest = json.loads(source_manifest.read_text())
        entries = manifest["frames"]
        if len(entries) < FRAMES:
            raise RuntimeError(f"water manifest needs at least {FRAMES} frames")
        selected = (
            entries[:FRAMES]
            if capillary_review
            else [
                entries[round(index * (len(entries) - 1) / (FRAMES - 1))]
                for index in range(FRAMES)
            ]
        )
        source_frames = [
            retarget_water_frame(
                json.loads((source_manifest.parent / entry["path"]).read_text()),
                index,
                capillary_review,
                capillary_grid,
            )
            for index, entry in enumerate(selected)
        ]
        source_metadata = {
            "kind": "physics_sim_aquarium_cache",
            "manifest": str(source_manifest),
            "source_frame_indices": [entry["frame_index"] for entry in selected],
            "source_grid": [source_frames[0]["grid_w"], source_frames[0]["grid_d"]],
            "capillary_grid": capillary_grid if capillary_review else None,
            "surface_profile": (
                "physics_sim_macro_plus_capillary_review"
                if capillary_review
                else "physics_sim_retargeted"
            ),
        }
    else:
        source_frames = [water_frame(frame) for frame in range(FRAMES)]
    for frame, payload in enumerate(source_frames):
        name = f"water_surface_{frame:06d}.json"
        dump(root / name, payload)
        frames.append(
            {
                "frame_index": frame,
                "path": name,
                "frame_contract": "water_surface_heightfield_v1",
            }
        )
    dump(
        root / "water_manifest_v1.json",
        {
            "schema": "physics_sim_water_manifest_v1",
            "version": 1,
            "mode": "water",
            "surface_representation": "heightfield",
            "surface_axis": "y",
            "height_units": "meters",
            "frame_contract": "water_surface_heightfield_v1",
            "grid_w": source_frames[0]["grid_w"],
            "grid_d": source_frames[0]["grid_d"],
            "volume_grid_w": source_frames[0]["grid_w"],
            "volume_grid_h": source_frames[0].get("volume_grid_h", 16),
            "volume_grid_d": source_frames[0]["grid_d"],
            "density_threshold": 0.5,
            "material": {
                "ior": 1.333,
                "absorption_distance_m": 5.0,
                "absorption_rgb": [0.06, 0.025, 0.012],
                "reflectivity": 0.035,
                "roughness": 0.008,
            },
            "frames": frames,
            "photon_fixture_source": source_metadata,
        },
    )
    bundle = root / "scene_bundle.json"
    dump(
        bundle,
        {
            "bundle_type": "physics_scene_bundle_v1",
            "bundle_version": 1,
            "profile": "physics",
            "water_source": {
                "kind": "water_manifest",
                "path": "water_manifest_v1.json",
                "contract": "water_manifest_v1",
                "surface_representation": "heightfield",
            },
        },
    )
    return bundle, source_metadata


def scene(path: Path, pool_review: bool = False) -> None:
    dump(
        path,
        {
            "schema_family": "codework_scene",
            "schema_variant": "scene_runtime_v1",
            "schema_version": 1,
            "scene_id": "animated_water_photon_caustic_acceptance",
            "space_mode_default": "3d",
            "unit_system": "meters",
            "world_scale": 1.0,
            "objects": [
                {
                    "object_id": "receiver_floor",
                    "object_type": "plane_primitive",
                    "space_mode_intent": "3d",
                    "dimensional_mode": "full_3d",
                    "transform": {
                        "position": {"x": 2.0, "y": 1.0, "z": 0.02},
                        "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                        "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
                    },
                    "primitive": {
                        "kind": "plane_primitive",
                        "width": 4.5,
                        "height": 2.5,
                        "frame": {
                            "origin": {"x": 2.0, "y": 1.0, "z": 0.02},
                            "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
                            "axis_v": {"x": 0.0, "y": 1.0, "z": 0.0},
                            "normal": {"x": 0.0, "y": 0.0, "z": 1.0},
                        },
                    },
                }
            ],
            "materials": [
                {
                    "material_id": "mat_receiver_floor",
                    "kind": "lambert",
                    "albedo": [0.22, 0.24, 0.25] if pool_review else [0.48, 0.51, 0.54],
                }
            ],
            "lights": [
                ({
                    "light_id": "distant_point_sun_proxy",
                    "kind": "point",
                    "position": {"x": 2.35, "y": 0.78, "z": 20.0},
                    "intensity": 2.0,
                    "falloff_distance": 30.0,
                    "falloff_mode": "quadratic",
                    "color": {"r": 1.0, "g": 0.96, "b": 0.86},
                    "enabled": True,
                    "moving": False,
                } if pool_review else {
                    "light_id": "authored_sun_proxy",
                    "kind": "rect",
                    "position": {"x": 2.45, "y": 0.72, "z": 3.25},
                    "width": 3.6,
                    "height": 1.55,
                    "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
                    "axis_v": {"x": 0.0, "y": -1.0, "z": 0.0},
                    "normal": {"x": -0.14, "y": 0.09, "z": -0.986},
                    "emission_profile": "one_sided",
                    "radiometric_mode": "lambertian_radiance",
                    "radiance": 12.0,
                    "intensity": 12.0,
                    "falloff_distance": 8.0,
                    "color": {"r": 1.0, "g": 0.96, "b": 0.86},
                    "enabled": True,
                    "moving": False,
                })
            ],
            "cameras": [],
            "constraints": [],
            "hierarchy": [],
            "extensions": {
                "ray_tracing": {
                    "authoring": {
                        "environment": {
                            "light_mode": 1,
                            "ambient_strength": 0.025,
                            "top_fill_strength": 0.0,
                        },
                        "object_materials": [
                            {
                                "object_id": "receiver_floor",
                                "material_id": 0,
                                "object_color": 5199194 if pool_review else 8422796,
                                "roughness": 0.86,
                                "reflectivity": 0.015,
                                "alpha": 1.0,
                            }
                        ],
                    }
                }
            },
        },
    )


def render_request(
    scene_path: Path,
    bundle: Path,
    out: Path,
    count: int,
    enabled: bool,
    photon_budget: int,
    pool_review: bool = False,
) -> dict:
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": out.name,
        "scene": {"runtime_scene_path": str(scene_path)},
        "volume": {
            "enabled": False,
            "visible": False,
            "source_kind": "scene_bundle",
            "source_path": str(bundle),
            "affects_lighting": False,
            "debug_overlay": False,
        },
        "render": {
            "start_frame": 0,
            "frame_count": count,
            "width": 400,
            "height": 240,
            "temporal_frames": 1,
            "integrator_3d": "disney_v2",
        },
        "inspection": {
            "camera_zoom": 1.0,
            "camera_position": {"x": 2.0, "y": 0.97, "z": 2.55},
            "camera_look_at": {"x": 2.0, "y": 1.0, "z": 0.25},
            "environment_light_mode": "ambient",
            "ambient_strength": 0.002 if pool_review else 0.025,
            "top_fill_strength": 0.0,
            "background_brightness": 0.001 if pool_review else 0.008,
            "caustic_product_mode": "production",
            "caustic_transport_engine": "photon_map",
            "caustic_photon_consumer": "direct_map",
            "caustic_surface_query_enabled": True,
            "caustic_volume_query_enabled": False,
            "caustic_photon_render_contribution_enabled": enabled,
            "caustic_photon_render_prep_population_enabled": True,
            "caustic_photon_surface_diagnostics_enabled": True,
            "caustic_photon_sample_budget": photon_budget,
            "caustic_photon_emission_seed": 1592594996,
            "caustic_photon_max_path_depth": 6,
            "caustic_photon_surface_query_radius": 0.045 if pool_review else 0.16,
            "caustic_photon_surface_gather_max_radius": 0.075 if pool_review else 0.28,
            "caustic_photon_surface_gather_neighbors": 8 if pool_review else 16,
            "caustic_photon_surface_allow_active_medium_receiver": True,
            "caustic_photon_surface_radiance_scale": 5000.0 if pool_review else 5.0,
            "caustic_sidecar_enabled": False,
            "trace_route": "flattened_bvh",
        },
        "output": {"root": str(out), "overwrite": True},
        "progress": {
            "summary_path": str(out / "render_summary.json"),
            "progress_path": str(out / "render_progress.json"),
        },
    }


def run(cli: Path, request_path: Path, out: Path) -> dict:
    result = subprocess.run(
        [str(cli), "--request", str(request_path), "--render"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    (out / "stdout_summary.json").write_text(result.stdout)
    (out / "stderr.txt").write_text(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{request_path.name}: {result.returncode}: {result.stderr}")
    return json.loads((out / "render_summary.json").read_text())


def jsonl(path: Path) -> list[dict]:
    return [json.loads(line) for line in path.read_text().splitlines() if line]


def heatmap(
    samples: list[dict],
    key: str,
    path: Path,
    pixel_sink: list[list[list[tuple[int, int, int]]]] | None = None,
) -> dict:
    size = 512
    bins = [[0.0] * size for _ in range(size)]
    lattice_size = int(round(math.sqrt(len(samples))))
    if key == "physical_flux" and lattice_size * lattice_size == len(samples):
        values = [
            max(0.0, sum(sample[key]) / 3.0)
            for sample in samples
        ]
        min_x = samples[0]["position"][0]
        max_x = samples[lattice_size - 1]["position"][0]
        min_y = samples[0]["position"][1]
        max_y = samples[-1]["position"][1]
        for py in range(size):
            world_y = (size - 1 - py) / (size - 1) * EXTENT_Y
            if world_y < min_y or world_y > max_y:
                continue
            gy = (world_y - min_y) / (max_y - min_y) * (lattice_size - 1)
            y0 = min(lattice_size - 1, int(gy))
            y1 = min(lattice_size - 1, y0 + 1)
            ty = gy - y0
            for px in range(size):
                world_x = px / (size - 1) * EXTENT_X
                if world_x < min_x or world_x > max_x:
                    continue
                gx = (world_x - min_x) / (max_x - min_x) * (lattice_size - 1)
                x0 = min(lattice_size - 1, int(gx))
                x1 = min(lattice_size - 1, x0 + 1)
                tx = gx - x0
                a = values[y0 * lattice_size + x0]
                b = values[y0 * lattice_size + x1]
                c = values[y1 * lattice_size + x0]
                d = values[y1 * lattice_size + x1]
                bins[py][px] = (
                    a * (1.0 - tx) * (1.0 - ty)
                    + b * tx * (1.0 - ty)
                    + c * (1.0 - tx) * ty
                    + d * tx * ty
                )
    else:
        for sample in samples:
            x, y, _ = sample["position"]
            px = min(size - 1, max(0, int(x / EXTENT_X * size)))
            py = min(size - 1, max(0, int(y / EXTENT_Y * size)))
            values = sample[key]
            bins[size - 1 - py][px] += max(0.0, sum(values) / 3.0)
    positive = sorted(v for row in bins for v in row if v > 0.0)
    scale = positive[max(0, int(len(positive) * 0.98) - 1)] if positive else 1.0
    pixels = []
    for row in bins:
        output_row = []
        for value in row:
            if key == "physical_flux":
                t = min(1.0, value / scale)
            else:
                t = min(1.0, math.log1p(12.0 * value / scale) / math.log(13.0))
            output_row.append(
                (
                    int(255 * min(1.0, 1.7 * t)),
                    int(255 * min(1.0, max(0.0, 1.7 * t - 0.35))),
                    int(255 * min(1.0, max(0.0, 2.2 * t - 1.15))),
                )
            )
        pixels.append(output_row)
    images.write_png_rgb(path, size, size, pixels)
    if pixel_sink is not None:
        pixel_sink.append(
            [
                [pixels[y][x] for x in range(0, size, 4)]
                for y in range(0, size, 4)
            ]
        )
    return {
        "sample_count": len(samples),
        "positive_bin_count": len(positive),
        "max_bin": max(positive, default=0.0),
        "p98_bin": scale,
    }


def spatial_bins(samples: list[dict], key: str, size: int = 64) -> list[float]:
    bins = [0.0] * (size * size)
    for sample in samples:
        x, y, _ = sample["position"]
        px = min(size - 1, max(0, int(x / EXTENT_X * size)))
        py = min(size - 1, max(0, int(y / EXTENT_Y * size)))
        bins[py * size + px] += max(0.0, sum(sample[key]) / 3.0)
    total = sum(bins)
    return [value / total for value in bins] if total > 0.0 else bins


def spatial_l1(lhs: list[float], rhs: list[float]) -> float:
    return sum(abs(a - b) for a, b in zip(lhs, rhs)) / 2.0


def spatial_cosine(lhs: list[float], rhs: list[float]) -> float:
    dot = sum(a * b for a, b in zip(lhs, rhs))
    length = math.sqrt(sum(a * a for a in lhs) * sum(b * b for b in rhs))
    return dot / length if length > 0.0 else 0.0


def bmp_png(source: Path, destination: Path) -> tuple[int, int, list[list[tuple[int, int, int]]]]:
    width, height, pixels = images.read_bmp_rgb(source)
    images.write_png_rgb(destination, width, height, pixels)
    return width, height, pixels


def contact_sheet(
    frames: list[list[list[tuple[int, int, int]]]],
    path: Path,
    columns: int = 4,
) -> None:
    if not frames:
        return
    tile_height, tile_width = len(frames[0]), len(frames[0][0])
    rows = (len(frames) + columns - 1) // columns
    black = (0, 0, 0)
    output = [[black] * (tile_width * columns) for _ in range(tile_height * rows)]
    for index, frame in enumerate(frames):
        ox = (index % columns) * tile_width
        oy = (index // columns) * tile_height
        for y, row in enumerate(frame):
            output[oy + y][ox : ox + tile_width] = row
    images.write_png_rgb(path, tile_width * columns, tile_height * rows, output)


def bmp_frame_delta(
    lhs: list[list[tuple[int, int, int]]],
    rhs: list[list[tuple[int, int, int]]],
) -> float:
    total = 0.0
    count = 0
    for lhs_row, rhs_row in zip(lhs, rhs):
        for a, b in zip(lhs_row, rhs_row):
            total += sum(abs(b[channel] - a[channel]) for channel in range(3)) / 3.0
            count += 1
    return total / count if count else 0.0


def difference(
    before: Path,
    after: Path,
    path: Path,
    pixel_sink: list[list[list[tuple[int, int, int]]]] | None = None,
) -> dict:
    width, height, a = images.read_bmp_rgb(before)
    width_b, height_b, b = images.read_bmp_rgb(after)
    if (width, height) != (width_b, height_b):
        raise RuntimeError("beauty control dimensions differ")
    output, positive, total = [], 0, 0.0
    for y in range(height):
        row = []
        for x in range(width):
            delta = tuple(max(0, b[y][x][c] - a[y][x][c]) for c in range(3))
            positive += max(delta) > 0
            total += sum(delta) / 3.0
            row.append(tuple(min(255, channel * 8) for channel in delta))
        output.append(row)
    images.write_png_rgb(path, width, height, output)
    if pixel_sink is not None:
        pixel_sink.append(output)
    return {
        "positive_pixel_count": positive,
        "mean_positive_rgb_delta": total / (width * height),
    }


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--cli",
        type=Path,
        default=root / "build/toolchains/clang" / platform.machine() / "tools/cli/ray_tracing_render_headless",
    )
    parser.add_argument(
        "--review-root",
        type=Path,
        default=root / "build/agent_runs/ray_tracing/animated_water_photon_caustics",
    )
    parser.add_argument("--static-only", action="store_true")
    parser.add_argument("--water-manifest", type=Path)
    parser.add_argument("--capillary-review", action="store_true")
    parser.add_argument("--photon-budget", type=int, default=16384)
    args = parser.parse_args()
    args.cli = args.cli.resolve()
    args.review_root = args.review_root.resolve()
    if args.review_root.exists():
        shutil.rmtree(args.review_root)
    generated = args.review_root / "generated"
    scene_path = generated / "scene_runtime.json"
    source_manifest = args.water_manifest.resolve() if args.water_manifest else None
    bundle, source_metadata = water_bundle(
        generated / "water",
        source_manifest,
        args.capillary_review,
    )
    scene(scene_path)
    specs = [("static_population_only", 1, False), ("static_photon_contribution", 1, True)]
    if not args.static_only:
        specs.extend(
            (
                ("animated_population_only", FRAMES, False),
                ("animated_photon_contribution", FRAMES, True),
            )
        )
    summaries = {}
    for run_id, count, enabled in specs:
        out = args.review_root / "runs" / run_id
        out.mkdir(parents=True)
        request_path = args.review_root / "requests" / f"{run_id}.json"
        dump(
            request_path,
            render_request(
                scene_path,
                bundle,
                out,
                count,
                enabled,
                args.photon_budget,
            ),
        )
        summaries[run_id] = run(args.cli, request_path, out)

    summary = summaries["static_photon_contribution"]
    inspection = summary.get("inspection", {})
    water = summary.get("water_surface", {})
    population = (
        summary.get("inspection", {})
        .get("caustic_state", {})
        .get("photon_callsite", {})
        .get("map_population", {})
    )
    if int(water.get("triangle_count", 0)) < 9000:
        raise RuntimeError(f"expected at least 9000 water triangles: {water}")
    if not population.get("prepared_scene_mesh_dielectric_succeeded"):
        raise RuntimeError(f"real water mesh harvest failed: {population}")
    if population.get("fixture_mesh_dielectric_fallback_used"):
        raise RuntimeError("fixture mesh dielectric fallback was used")
    if int(population.get("surface_map_record_count", 0)) <= 0:
        raise RuntimeError(f"surface photon map is empty: {population}")
    if not inspection.get("caustic_photon_surface_allow_active_medium_receiver"):
        raise RuntimeError("active-medium receiver request was not applied")
    if not inspection.get("caustic_photon_surface_diagnostics_enabled"):
        raise RuntimeError("surface diagnostics request was not applied")

    diagnostics = args.review_root / "diagnostics"
    diagnostics.mkdir()
    photon_out = args.review_root / "runs/static_photon_contribution"
    raw_metrics = heatmap(
        jsonl(photon_out / "photon_surface_records_0000.jsonl"),
        "flux",
        diagnostics / "raw_photon_landings.png",
    )
    gather_metrics = heatmap(
        jsonl(photon_out / "photon_surface_queries_0000.jsonl"),
        "physical_flux",
        diagnostics / "reconstructed_surface_gather.png",
    )
    raw_static = jsonl(photon_out / "photon_surface_records_0000.jsonl")
    gather_static = jsonl(photon_out / "photon_surface_queries_0000.jsonl")
    raw_gather_cosine = spatial_cosine(
        spatial_bins(raw_static, "flux"),
        spatial_bins(gather_static, "physical_flux"),
    )
    delta_metrics = difference(
        args.review_root / "runs/static_population_only/frames/frame_0000.bmp",
        photon_out / "frames/frame_0000.bmp",
        diagnostics / "beauty_photon_difference.png",
    )
    temporal_metrics = None
    if not args.static_only:
        animated = args.review_root / "runs/animated_photon_contribution"
        animation_diagnostics = diagnostics / "animation"
        animation_diagnostics.mkdir()
        raw_bins: list[list[float]] = []
        gather_bins: list[list[float]] = []
        raw_pixels: list[list[list[tuple[int, int, int]]]] = []
        gather_pixels: list[list[list[tuple[int, int, int]]]] = []
        beauty_pixels: list[list[list[tuple[int, int, int]]]] = []
        caustic_difference_pixels: list[list[list[tuple[int, int, int]]]] = []
        raw_counts: list[int] = []
        gather_counts: list[int] = []
        for frame in range(FRAMES):
            raw_samples = jsonl(animated / f"photon_surface_records_{frame:04d}.jsonl")
            gather_samples = jsonl(animated / f"photon_surface_queries_{frame:04d}.jsonl")
            raw_counts.append(len(raw_samples))
            gather_counts.append(len(gather_samples))
            raw_bins.append(spatial_bins(raw_samples, "flux"))
            gather_bins.append(spatial_bins(gather_samples, "physical_flux"))
            heatmap(
                raw_samples,
                "flux",
                animation_diagnostics / f"raw_photon_landings_{frame:04d}.png",
                raw_pixels,
            )
            heatmap(
                gather_samples,
                "physical_flux",
                animation_diagnostics / f"reconstructed_surface_gather_{frame:04d}.png",
                gather_pixels,
            )
            _, _, pixels = bmp_png(
                animated / "frames" / f"frame_{frame:04d}.bmp",
                animation_diagnostics / f"beauty_{frame:04d}.png",
            )
            beauty_pixels.append(pixels)
            difference(
                args.review_root
                / "runs/animated_population_only/frames"
                / f"frame_{frame:04d}.bmp",
                animated / "frames" / f"frame_{frame:04d}.bmp",
                animation_diagnostics / f"beauty_photon_difference_{frame:04d}.png",
                caustic_difference_pixels,
            )
        contact_sheet(
            raw_pixels,
            diagnostics / "animated_raw_photon_landings_contact_sheet.png",
        )
        contact_sheet(
            gather_pixels,
            diagnostics / "animated_reconstructed_surface_gather_contact_sheet.png",
        )
        contact_sheet(
            beauty_pixels,
            diagnostics / "animated_beauty_contact_sheet.png",
        )
        contact_sheet(
            caustic_difference_pixels,
            diagnostics / "animated_beauty_photon_difference_contact_sheet.png",
        )
        raw_l1 = [
            spatial_l1(raw_bins[index - 1], raw_bins[index])
            for index in range(1, FRAMES)
        ]
        gather_l1 = [
            spatial_l1(gather_bins[index - 1], gather_bins[index])
            for index in range(1, FRAMES)
        ]
        beauty_delta = [
            bmp_frame_delta(beauty_pixels[index - 1], beauty_pixels[index])
            for index in range(1, FRAMES)
        ]
        caustic_difference_delta = [
            bmp_frame_delta(
                caustic_difference_pixels[index - 1],
                caustic_difference_pixels[index],
            )
            for index in range(1, FRAMES)
        ]
        temporal_metrics = {
            "raw_record_counts": raw_counts,
            "gather_query_counts": gather_counts,
            "raw_pairwise_spatial_l1": raw_l1,
            "gather_pairwise_spatial_l1": gather_l1,
            "beauty_pairwise_mean_rgb_delta": beauty_delta,
            "beauty_photon_difference_pairwise_mean_rgb_delta":
                caustic_difference_delta,
            "all_raw_frames_changed": all(value > 0.01 for value in raw_l1),
            "all_gather_frames_changed": all(value > 0.01 for value in gather_l1),
            "all_beauty_frames_changed": all(value > 0.01 for value in beauty_delta),
            "all_beauty_photon_difference_frames_changed": all(
                value > 0.01 for value in caustic_difference_delta
            ),
        }
        if min(raw_counts, default=0) <= 0 or not all(
            (
                temporal_metrics["all_raw_frames_changed"],
                temporal_metrics["all_gather_frames_changed"],
                temporal_metrics["all_beauty_frames_changed"],
                temporal_metrics["all_beauty_photon_difference_frames_changed"],
            )
        ):
            raise RuntimeError(f"animated photon-caustic evidence did not evolve: {temporal_metrics}")
    report = {
        "status": "pass",
        "grid": [water.get("grid_w"), water.get("grid_d")],
        "water_source": source_metadata,
        "water_triangle_count": water.get("triangle_count"),
        "water_triangle_count_nominal": TRIANGLES,
        "frame_count": 1 if args.static_only else FRAMES,
        "raw_landing_metrics": raw_metrics,
        "reconstruction_metrics": gather_metrics,
        "raw_gather_spatial_cosine": raw_gather_cosine,
        "beauty_difference_metrics": delta_metrics,
        "temporal_metrics": temporal_metrics,
        "population": population,
        "summary_paths": {
            key: str(args.review_root / "runs" / key / "render_summary.json")
            for key in summaries
        },
    }
    dump(args.review_root / "acceptance_report.json", report)
    print(args.review_root / "acceptance_report.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
