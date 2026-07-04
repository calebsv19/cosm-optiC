#!/usr/bin/env python3
"""Run the visual glass-sphere caustic/mist proof matrix."""

from __future__ import annotations

import argparse
import json
import math
import platform
import shutil
import struct
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def workspace_root() -> Path:
    return repo_root().parent


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = root / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless"
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def default_review_root() -> Path:
    return workspace_root() / "_private_workspace_artifacts" / "agent_runs" / "ray_tracing" / "caustic_visual_sphere_mist_matrix"


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--review-root", type=Path, default=default_review_root())
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--keep-going", action="store_true")
    return parser.parse_args()


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_soft_mist_vf3d(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    w = h = d = 32
    cell_count = w * h * d
    magic = (ord("V") << 24) | (ord("F") << 16) | (ord("3") << 8) | ord("D")
    header = struct.pack(
        "@IIIIIdQdfffffffIIII",
        magic,
        1,
        w,
        h,
        d,
        0.0,
        0,
        1.0 / 24.0,
        -1.8,
        -1.8,
        0.0,
        3.6 / float(w),
        0.0,
        0.0,
        1.0,
        0,
        0,
        0,
        0,
    )
    if struct.calcsize("@IIIIIdQdfffffffIIII") != 92:
        raise RuntimeError("unexpected native vf3d header packing")
    density = []
    for z in range(d):
        nz = (float(z) + 0.5) / float(d)
        for y in range(h):
            ny = (float(y) + 0.5) / float(h)
            for x in range(w):
                nx = (float(x) + 0.5) / float(w)
                edge_distance = min(nx, ny, nz, 1.0 - nx, 1.0 - ny, 1.0 - nz)
                edge_feather = max(0.0, min(1.0, edge_distance / 0.22))
                center_dx = (nx - 0.5) / 0.50
                center_dy = (ny - 0.5) / 0.50
                center_dz = (nz - 0.32) / 0.42
                local_radius2 = center_dx * center_dx + center_dy * center_dy + center_dz * center_dz
                center_falloff = math.exp(-1.55 * local_radius2)
                floor_lift = max(0.0, min(1.0, (nz - 0.04) / 0.20))
                density.append(0.075 * edge_feather * center_falloff * floor_lift)
    zero_float = [0.0] * cell_count
    solid = bytes(cell_count)
    with path.open("wb") as f:
        f.write(header)
        f.write(b"\0\0\0\0")
        f.write(struct.pack(f"@{cell_count}f", *density))
        f.write(struct.pack(f"@{cell_count}f", *zero_float))
        f.write(struct.pack(f"@{cell_count}f", *zero_float))
        f.write(struct.pack(f"@{cell_count}f", *zero_float))
        f.write(struct.pack(f"@{cell_count}f", *zero_float))
        f.write(solid)


def rgb_u24(r: int, g: int, b: int) -> int:
    return ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff)


def plane_object(object_id: str,
                 frame: dict,
                 width: float,
                 height: float,
                 material_id: str) -> dict:
    return {
        "object_id": object_id,
        "object_type": "plane_primitive",
        "space_mode_intent": "2d",
        "dimensional_mode": "plane_locked",
        "locked_plane": "custom",
        "transform": {
            "position": {"x": 0.0, "y": 0.0, "z": 0.0},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "primitive": {
            "kind": "plane_primitive",
            "width": width,
            "height": height,
            "frame": frame,
        },
        "material_id": material_id,
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def write_visual_scene(review_root: Path) -> Path:
    scene_dir = review_root / "generated_scene"
    mesh_dir = scene_dir / "assets" / "mesh_assets"
    mesh_dir.mkdir(parents=True, exist_ok=True)
    source_mesh = (
        repo_root() /
        "tests" /
        "fixtures" /
        "mesh_asset_runtime_spheres" /
        "assets" /
        "mesh_assets" /
        "asset_sphere_256x128.runtime.json"
    )
    shutil.copy2(source_mesh, mesh_dir / "asset_sphere_256x128.runtime.json")
    objects = [
        plane_object(
            "matte_receiver_floor",
            {
                "origin": {"x": 0.0, "y": 0.0, "z": 0.0},
                "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
                "axis_v": {"x": 0.0, "y": 1.0, "z": 0.0},
                "normal": {"x": 0.0, "y": 0.0, "z": 1.0},
            },
            6.0,
            5.2,
            "mat_warm_floor",
        ),
        plane_object(
            "back_color_wall",
            {
                "origin": {"x": 0.0, "y": 2.35, "z": 1.45},
                "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
                "axis_v": {"x": 0.0, "y": 0.0, "z": 1.0},
                "normal": {"x": 0.0, "y": -1.0, "z": 0.0},
            },
            6.0,
            2.9,
            "mat_back_wall",
        ),
        plane_object(
            "left_color_wall",
            {
                "origin": {"x": -2.95, "y": 0.0, "z": 1.45},
                "axis_u": {"x": 0.0, "y": 1.0, "z": 0.0},
                "axis_v": {"x": 0.0, "y": 0.0, "z": 1.0},
                "normal": {"x": 1.0, "y": 0.0, "z": 0.0},
            },
            5.2,
            2.9,
            "mat_left_wall",
        ),
        plane_object(
            "right_color_wall",
            {
                "origin": {"x": 2.95, "y": 0.0, "z": 1.45},
                "axis_u": {"x": 0.0, "y": 1.0, "z": 0.0},
                "axis_v": {"x": 0.0, "y": 0.0, "z": 1.0},
                "normal": {"x": -1.0, "y": 0.0, "z": 0.0},
            },
            5.2,
            2.9,
            "mat_right_wall",
        ),
        {
            "object_id": "high_quality_glass_sphere",
            "object_type": "mesh_asset_instance",
            "space_mode_intent": "3d",
            "dimensional_mode": "full_3d",
            "transform": {
                "position": {"x": 0.0, "y": 0.0, "z": 0.72},
                "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                "scale": {"x": 0.72, "y": 0.72, "z": 0.72},
            },
            "geometry_ref": {
                "kind": "mesh_asset",
                "id": "asset_sphere_256x128",
                "variant": "runtime_default",
            },
            "extensions": {
                "line_drawing": {
                    "runtime_mesh_path": "assets/mesh_assets/asset_sphere_256x128.runtime.json",
                }
            },
            "material_id": "mat_glass_sphere",
            "flags": {"visible": True, "locked": False, "selectable": True},
        },
    ]
    scene = {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "caustic_visual_sphere_mist_room_v1",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": objects,
        "materials": [
            {"material_id": "mat_warm_floor", "kind": "lambert", "albedo": [0.62, 0.54, 0.45]},
            {"material_id": "mat_back_wall", "kind": "lambert", "albedo": [0.34, 0.46, 0.58]},
            {"material_id": "mat_left_wall", "kind": "lambert", "albedo": [0.58, 0.32, 0.30]},
            {"material_id": "mat_right_wall", "kind": "lambert", "albedo": [0.28, 0.53, 0.49]},
            {"material_id": "mat_glass_sphere", "kind": "dielectric", "albedo": [0.92, 0.97, 1.0]},
        ],
        "lights": [
            {
                "light_id": "overhead_focus_light",
                "kind": "sphere",
                "position": {"x": 0.0, "y": 0.0, "z": 3.45},
                "radius": 0.08,
                "intensity": 8.8,
                "falloff_distance": 8.0,
                "color": {"r": 1.0, "g": 0.96, "b": 0.88},
                "enabled": True,
                "moving": False,
            }
        ],
        "cameras": [
            {
                "camera_id": "caustic_visual_camera",
                "kind": "perspective",
                "position": {"x": 0.0, "y": -4.65, "z": 1.65},
                "target": {"x": 0.0, "y": 0.0, "z": 0.58},
                "yaw": 0.0,
                "look_pitch": -0.18,
            }
        ],
        "constraints": [],
        "hierarchy": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": {"x": 0.0, "y": 0.0, "z": 0.58},
                    "environment": {
                        "light_mode": 1,
                        "ambient_strength": 0.08,
                        "top_fill_strength": 0.16,
                    },
                    "object_materials": [
                        {
                            "object_id": "matte_receiver_floor",
                            "material_id": 0,
                            "object_color": rgb_u24(158, 138, 115),
                            "roughness": 0.86,
                            "reflectivity": 0.02,
                            "alpha": 1.0,
                        },
                        {
                            "object_id": "back_color_wall",
                            "material_id": 0,
                            "object_color": rgb_u24(87, 117, 148),
                            "roughness": 0.88,
                            "reflectivity": 0.01,
                            "alpha": 1.0,
                        },
                        {
                            "object_id": "left_color_wall",
                            "material_id": 0,
                            "object_color": rgb_u24(148, 82, 77),
                            "roughness": 0.88,
                            "reflectivity": 0.01,
                            "alpha": 1.0,
                        },
                        {
                            "object_id": "right_color_wall",
                            "material_id": 0,
                            "object_color": rgb_u24(71, 135, 125),
                            "roughness": 0.88,
                            "reflectivity": 0.01,
                            "alpha": 1.0,
                        },
                        {
                            "object_id": "high_quality_glass_sphere",
                            "material_id": 5,
                            "object_color": rgb_u24(231, 247, 255),
                            "alpha": 0.42,
                            "roughness": 0.015,
                            "reflectivity": 0.04,
                        },
                    ],
                }
            }
        },
    }
    scene_path = scene_dir / "scene_runtime.json"
    write_json(scene_path, scene)
    return scene_path


def base_request(run_id: str,
                 scene_path: Path,
                 output_root: Path,
                 summary_path: Path,
                 volume_path: Path) -> dict:
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": run_id,
        "scene": {
            "runtime_scene_path": str(scene_path),
        },
        "volume": {
            "enabled": True,
            "source_kind": "raw_vf3d",
            "source_path": str(volume_path),
            "affects_lighting": True,
            "debug_overlay": False,
        },
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": 192,
            "height": 136,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "integrator_3d": "disney_v2",
            "denoise_enabled": True,
        },
        "inspection": {
            "preset": "glass_review",
            "camera_position": {"x": 0.0, "y": -4.65, "z": 1.65},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 0.58},
            "camera_zoom": 0.96,
            "environment_light_mode": "ambient",
            "ambient_strength": 0.06,
            "top_fill_strength": 0.04,
            "light_intensity": 8.8,
            "light_radius": 0.08,
            "secondary_diffuse_samples_3d": 8,
            "transmission_samples_3d": 8,
            "volume_density_scale": 0.65,
            "volume_density_gamma": 1.0,
            "volume_scatter_gain": 2.4,
            "volume_absorption_gain": 0.06,
            "volume_opacity_clamp": 0.55,
            "volume_step_scale": 0.9,
            "volume_tint": {"r": 1.0, "g": 0.98, "b": 0.92},
            "caustic_debug_summary": True,
            "object_audit_enabled": True,
        },
        "output": {
            "root": str(output_root),
            "overwrite": True,
        },
        "progress": {
            "summary_path": str(summary_path),
            "progress_path": str(output_root / "render_progress.json"),
        },
    }


def matrix_requests(review_root: Path, scene_path: Path, volume_path: Path) -> list[dict]:
    run_root = review_root / "runs"
    specs = [
        {
            "cell_id": "mist_no_caustic",
            "run_id": "caustic_visual_sphere_mist_no_caustic",
            "inspection": {
                "caustic_mode": "off",
                "caustic_volume_enabled": False,
                "caustic_surface_enabled": False,
                "caustic_sidecar_enabled": False,
                "caustic_sample_budget": 0,
            },
        },
        {
            "cell_id": "surface_caustic_only",
            "run_id": "caustic_visual_sphere_surface_caustic_only",
            "inspection": {
                "caustic_mode": "transport",
                "caustic_volume_enabled": False,
                "caustic_surface_enabled": True,
                "caustic_sidecar_enabled": False,
                "caustic_sample_budget": 3072,
                "caustic_max_path_depth": 2,
                "caustic_surface_energy_scale": 8.0,
                "caustic_surface_footprint_scale": 16.0,
                "caustic_surface_receiver_fallback_enabled": False,
            },
        },
        {
            "cell_id": "surface_and_volume_caustic",
            "run_id": "caustic_visual_sphere_surface_and_volume_caustic",
            "inspection": {
                "caustic_mode": "transport",
                "caustic_volume_enabled": True,
                "caustic_surface_enabled": True,
                "caustic_sidecar_enabled": False,
                "caustic_sample_budget": 3072,
                "caustic_max_path_depth": 2,
                "caustic_surface_energy_scale": 8.0,
                "caustic_surface_footprint_scale": 16.0,
                "caustic_surface_receiver_fallback_enabled": False,
            },
        },
    ]
    requests = []
    for spec in specs:
        cell_id = spec["cell_id"]
        output_root = run_root / cell_id
        summary_path = output_root / "render_summary.json"
        request = base_request(spec["run_id"], scene_path, output_root, summary_path, volume_path)
        request["inspection"].update(spec["inspection"])
        request_path = review_root / "generated_requests" / f"request_{cell_id}.json"
        write_json(request_path, request)
        requests.append({"cell_id": cell_id, "request_path": request_path, "summary_path": summary_path})
    return requests


def render_request(cli: Path, request_path: Path, summary_path: Path, skip_render: bool) -> float | None:
    if skip_render:
        return None
    stdout_path = summary_path.parent / "stdout_summary.json"
    stderr_path = summary_path.parent / "stderr.txt"
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    with stdout_path.open("w", encoding="utf-8") as stdout:
        result = subprocess.run(
            [str(cli), "--request", str(request_path), "--render", "--summary", str(summary_path)],
            stdout=stdout,
            stderr=subprocess.PIPE,
            text=True,
        )
    elapsed = time.perf_counter() - start
    stderr_path.write_text(result.stderr or "", encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(f"render failed for {request_path} with exit {result.returncode}; stderr: {stderr_path}")
    return elapsed


def first_frame_path(summary: dict) -> Path:
    frame_path = Path(summary.get("outputs", {}).get("first_frame_path", ""))
    if frame_path.exists():
        return frame_path
    return Path(summary.get("output_root", "")) / "frames" / "frame_0000.bmp"


def copy_frame_png(summary: dict, review_root: Path, cell_id: str) -> tuple[Path, Path]:
    frame_path = first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{cell_id}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return frame_path, png_path


def luma(pixel: tuple[int, int, int]) -> float:
    r, g, b = pixel
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def frame_delta_metrics(runs: dict[str, dict]) -> dict:
    baseline_id = "mist_no_caustic"
    if baseline_id not in runs:
        return {}
    baseline_path = Path(runs[baseline_id]["frame_path"])
    width, height, baseline_pixels = review_artifacts.read_bmp_rgb(baseline_path)
    result: dict[str, dict] = {}
    for cell_id, run in runs.items():
        frame_path = Path(run["frame_path"])
        frame_width, frame_height, pixels = review_artifacts.read_bmp_rgb(frame_path)
        if (frame_width, frame_height) != (width, height):
            raise ValueError(f"{cell_id}: frame dimensions do not match no-caustic baseline")
        changed_count = 0
        positive_count = 0
        total_delta = 0.0
        max_delta = 0.0
        positive_deltas: list[float] = []
        for y in range(height):
            for x in range(width):
                delta = luma(pixels[y][x]) - luma(baseline_pixels[y][x])
                total_delta += delta
                if abs(delta) > 0.5:
                    changed_count += 1
                if delta > 0.5:
                    positive_count += 1
                    positive_deltas.append(delta)
                    if delta > max_delta:
                        max_delta = delta
        positive_deltas.sort()
        p95 = 0.0
        if positive_deltas:
            p95 = positive_deltas[min(len(positive_deltas) - 1,
                                      int(math.floor(0.95 * float(len(positive_deltas) - 1))))]
        result[cell_id] = {
            "baseline_cell_id": baseline_id,
            "changed_pixel_count": changed_count,
            "positive_pixel_count": positive_count,
            "positive_area_ratio": positive_count / float(width * height),
            "mean_luma_delta": total_delta / float(width * height),
            "max_luma_delta": max_delta,
            "positive_luma_delta_p95": p95,
        }
    return result


def rgb_sum(value: dict) -> float:
    return float(value.get("r", 0.0)) + float(value.get("g", 0.0)) + float(value.get("b", 0.0))


def safe_ratio(numerator: float, denominator: float) -> float:
    return numerator / denominator if denominator > 0.0 else 0.0


def caustic_digest(summary: dict) -> dict:
    inspection = summary.get("inspection", {})
    state = inspection.get("caustic_state", {})
    stats = summary.get("render_stats", {})
    sampled_surface = state.get("surface_caustic_sampled_radiance", {})
    scatter_radiance = state.get("volume_scatter_caustic_radiance", {})
    direct_scatter = state.get("volume_scatter_direct_radiance", {})
    cache_radiance = state.get("volume_cache_total_radiance", {})
    cache_cells = int(state.get("volume_cache_cell_count", 0))
    cache_nonzero = int(state.get("volume_cache_nonzero_cell_count", 0))
    cache_lookups = int(state.get("volume_cache_sample_lookup_count", 0))
    cache_hits = int(state.get("volume_cache_sample_contributing_count", 0))
    cache_sum = float(state.get("volume_cache_total_radiance_sum", rgb_sum(cache_radiance)))
    scatter_sum = float(state.get("volume_scatter_caustic_radiance_sum", rgb_sum(scatter_radiance)))
    direct_sum = float(state.get("volume_scatter_direct_radiance_sum", rgb_sum(direct_scatter)))
    footprint_input_sum = float(state.get("volume_cache_footprint_input_radiance_sum", 0.0))
    footprint_deposited_sum = float(state.get("volume_cache_footprint_deposited_radiance_sum", 0.0))
    return {
        "mode": state.get("mode", "unknown"),
        "analytic_sidecar_requested": bool(state.get("analytic_sidecar_requested", False)),
        "temporary_bootstrap_active": bool(state.get("temporary_analytic_bridge", False)),
        "transport_active": bool(state.get("transport_path_emission_active", False)),
        "transport_evaluated_paths": int(state.get("transport_evaluated_path_count", 0)),
        "transport_emitted_paths": int(state.get("transport_emitted_path_count", 0)),
        "transport_volume_segments": int(state.get("transport_volume_segment_count", 0)),
        "volume_cache_requested": bool(state.get("volume_cache_requested", False)),
        "volume_cache_bound": bool(state.get("volume_cache_bound", False)),
        "volume_cache_allocated": bool(state.get("volume_cache_allocated", False)),
        "volume_cache_cell_count": cache_cells,
        "volume_cache_nonzero_cells": cache_nonzero,
        "volume_cache_nonzero_cell_ratio": float(
            state.get("volume_cache_nonzero_cell_ratio", safe_ratio(cache_nonzero, cache_cells))
        ),
        "volume_cache_deposit_attempts": int(state.get("volume_cache_deposit_attempt_count", 0)),
        "volume_cache_deposit_accepted": int(state.get("volume_cache_deposit_accepted_count", 0)),
        "volume_cache_sample_lookups": cache_lookups,
        "volume_cache_sample_hits": cache_hits,
        "volume_cache_sample_hit_ratio": float(
            state.get("volume_cache_sample_hit_ratio", safe_ratio(cache_hits, cache_lookups))
        ),
        "volume_cache_total_radiance": cache_radiance,
        "volume_cache_total_radiance_sum": cache_sum,
        "volume_cache_max_radiance": float(state.get("volume_cache_max_cell_radiance", 0.0)),
        "volume_cache_footprint_deposit_count": int(state.get("volume_cache_footprint_deposit_count", 0)),
        "volume_cache_footprint_cell_contribution_count": int(
            state.get("volume_cache_footprint_cell_contribution_count", 0)
        ),
        "volume_cache_average_footprint_radius_voxels": float(
            state.get("volume_cache_average_footprint_radius_voxels", 0.0)
        ),
        "volume_cache_footprint_input_radiance_sum": footprint_input_sum,
        "volume_cache_footprint_deposited_radiance_sum": footprint_deposited_sum,
        "volume_cache_footprint_deposited_to_input_ratio": float(
            state.get("volume_cache_footprint_deposited_to_input_ratio",
                      safe_ratio(footprint_deposited_sum, footprint_input_sum))
        ),
        "volume_cache_radiance_centroid": state.get("volume_cache_radiance_centroid", {}),
        "volume_cache_nonzero_bounds_min": state.get("volume_cache_nonzero_bounds_min", {}),
        "volume_cache_nonzero_bounds_max": state.get("volume_cache_nonzero_bounds_max", {}),
        "volume_scatter_bound": bool(state.get("volume_scatter_caustic_sampling_bound", False)),
        "volume_scatter_direct_samples": int(state.get("volume_scatter_direct_sample_count", 0)),
        "volume_scatter_direct_radiance": direct_scatter,
        "volume_scatter_direct_radiance_sum": direct_sum,
        "volume_scatter_samples": int(state.get("volume_scatter_caustic_sample_count", 0)),
        "volume_scatter_contributing_samples": int(state.get("volume_scatter_caustic_contributing_sample_count", 0)),
        "volume_scatter_radiance": scatter_radiance,
        "volume_scatter_radiance_sum": scatter_sum,
        "volume_scatter_to_cache_radiance_ratio": float(
            state.get("volume_scatter_caustic_to_cache_radiance_ratio",
                      safe_ratio(scatter_sum, cache_sum))
        ),
        "volume_scatter_to_direct_radiance_ratio": float(
            state.get("volume_scatter_caustic_to_direct_radiance_ratio",
                      safe_ratio(scatter_sum, direct_sum))
        ),
        "surface_cache_requested": bool(state.get("surface_cache_requested", False)),
        "surface_receiver_fallback_enabled": bool(state.get("surface_receiver_fallback_enabled", True)),
        "surface_receiver_fallback_count": int(state.get("transport_surface_receiver_fallback_count", 0)),
        "surface_cache_record_count": int(state.get("surface_cache_record_count", 0)),
        "surface_cache_deposit_accepted": int(state.get("surface_cache_deposit_accepted_count", 0)),
        "surface_cache_sample_contributing_count": int(state.get("surface_cache_sample_contributing_count", 0)),
        "surface_caustic_sampled_radiance": sampled_surface,
        "surface_caustic_sampled_radiance_sum": rgb_sum(sampled_surface),
        "caustic_sidecar_enabled": int(stats.get("caustic_sidecar_enabled", 0)) > 0,
    }


def validate_cell(cell_id: str, digest: dict) -> list[str]:
    failures: list[str] = []
    if cell_id == "mist_no_caustic":
        if digest["transport_active"] or digest["caustic_sidecar_enabled"]:
            failures.append("no-caustic baseline unexpectedly activated caustic transport or sidecar")
        if digest["surface_cache_record_count"] != 0 or digest["volume_cache_nonzero_cells"] != 0:
            failures.append("no-caustic baseline produced caustic cache data")
    elif cell_id == "surface_caustic_only":
        if not digest["transport_active"]:
            failures.append("surface-only cell did not activate physical transport")
        if digest["temporary_bootstrap_active"] or digest["caustic_sidecar_enabled"]:
            failures.append("surface-only cell used bootstrap or analytic sidecar")
        if digest["surface_receiver_fallback_enabled"] or digest["surface_receiver_fallback_count"] != 0:
            failures.append("surface-only cell used tangent receiver fallback")
        if digest["surface_cache_record_count"] <= 0 or digest["surface_cache_deposit_accepted"] <= 0:
            failures.append("surface-only cell did not populate receiver records")
    elif cell_id == "surface_and_volume_caustic":
        if not digest["transport_active"]:
            failures.append("combined cell did not activate physical transport")
        if digest["temporary_bootstrap_active"] or digest["caustic_sidecar_enabled"]:
            failures.append("combined cell used bootstrap or analytic sidecar")
        if digest["surface_receiver_fallback_enabled"] or digest["surface_receiver_fallback_count"] != 0:
            failures.append("combined cell used tangent receiver fallback")
        if digest["surface_cache_record_count"] <= 0 or digest["surface_cache_deposit_accepted"] <= 0:
            failures.append("combined cell did not populate receiver records")
        if digest["volume_cache_nonzero_cells"] <= 0 or digest["volume_cache_deposit_accepted"] <= 0:
            failures.append("combined cell did not populate volume cache")
        if digest["volume_scatter_contributing_samples"] <= 0 or digest["volume_scatter_radiance_sum"] <= 0.0:
            failures.append("combined cell did not scatter caustic volume radiance")
        if digest["volume_cache_sample_lookups"] <= 0:
            failures.append("combined cell did not report caustic volume cache lookups")
        if digest["volume_cache_sample_hit_ratio"] <= 0.0:
            failures.append("combined cell did not report caustic volume cache hit ratio")
        if digest["volume_cache_nonzero_cell_ratio"] <= 0.0:
            failures.append("combined cell did not report caustic volume cache occupancy ratio")
        if digest["volume_cache_total_radiance_sum"] <= 0.0:
            failures.append("combined cell did not report caustic volume cache radiance sum")
        if digest["volume_scatter_to_cache_radiance_ratio"] <= 0.0:
            failures.append("combined cell did not report cache-to-scatter conversion ratio")
        if digest["volume_cache_footprint_deposit_count"] <= 0:
            failures.append("combined cell did not report volume-cache footprint deposits")
        if digest["volume_cache_footprint_cell_contribution_count"] <= digest["volume_cache_footprint_deposit_count"]:
            failures.append("combined cell did not expand volume-cache deposits across footprint cells")
        if not 0.99 <= digest["volume_cache_footprint_deposited_to_input_ratio"] <= 1.01:
            failures.append("combined cell footprint deposit energy is not normalized")
    return failures


def write_contact_sheet(path: Path, runs: list[dict]) -> None:
    images = []
    for run in runs:
        width, height, pixels = review_artifacts.read_bmp_rgb(Path(run["frame_path"]))
        images.append((run["cell_id"], width, height, pixels))
    if not images:
        return
    cell_width = images[0][1]
    cell_height = images[0][2]
    separator = 8
    label_height = 0
    sheet_width = cell_width * len(images) + separator * (len(images) - 1)
    sheet_height = cell_height + label_height
    rows = []
    sep = [(36, 36, 36)] * separator
    for y in range(sheet_height):
        row = []
        for idx, (_, _, _, pixels) in enumerate(images):
            if idx:
                row.extend(sep)
            row.extend(pixels[y])
        rows.append(row)
    review_artifacts.write_png_rgb(path, sheet_width, sheet_height, rows)


def write_delta_artifacts(review_root: Path, runs_by_cell: dict[str, dict]) -> dict:
    baseline = runs_by_cell.get("mist_no_caustic")
    volume = runs_by_cell.get("surface_and_volume_caustic")
    if not baseline or not volume:
        return {}
    before_w, before_h, before = review_artifacts.read_bmp_rgb(Path(baseline["frame_path"]))
    after_w, after_h, after = review_artifacts.read_bmp_rgb(Path(volume["frame_path"]))
    if (before_w, before_h) != (after_w, after_h):
        raise ValueError("delta frames must have matching dimensions")
    out_dir = review_root / "diffs"
    out_dir.mkdir(parents=True, exist_ok=True)
    diff4_path = out_dir / "surface_and_volume_vs_mist_diff4x.png"
    diff16_path = out_dir / "surface_and_volume_vs_mist_diff16x.png"
    side_path = out_dir / "side_by_side_mist_volume_diff16x.png"
    diff4 = review_artifacts.abs_diff_pixels(before, after, 4)
    diff16 = review_artifacts.abs_diff_pixels(before, after, 16)
    side_w, side_h, side = review_artifacts.side_by_side(before, after, diff16)
    review_artifacts.write_png_rgb(diff4_path, before_w, before_h, diff4)
    review_artifacts.write_png_rgb(diff16_path, before_w, before_h, diff16)
    review_artifacts.write_png_rgb(side_path, side_w, side_h, side)
    return {
        "diff4_path": str(diff4_path),
        "diff16_path": str(diff16_path),
        "side_by_side_diff16_path": str(side_path),
    }


def write_index(path: Path, report: dict) -> None:
    root = path.parent.resolve()
    lines = [
        "# Visual Sphere Mist Caustic Matrix",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- scene: `{report['scene_path']}`",
        f"- vf3d: `{report['vf3d_path']}`",
        f"- contact sheet: `{Path(report['contact_sheet_path']).name}`",
        f"- amplified delta: `{Path(report['delta_artifacts'].get('side_by_side_diff16_path', '')).name}`",
        "",
        "## Runs",
        "",
    ]
    for run in report["runs"]:
        digest = run["caustic"]
        png = Path(run["png_path"]).resolve()
        try:
            png_text = png.relative_to(root).as_posix()
        except ValueError:
            png_text = str(png)
        lines.append(
            f"- `{run['cell_id']}`: mode `{digest['mode']}`, "
            f"transport `{digest['transport_active']}`, sidecar `{digest['caustic_sidecar_enabled']}`, "
            f"surface records `{digest['surface_cache_record_count']}`, "
            f"surface contrib `{digest['surface_cache_sample_contributing_count']}`, "
            f"surface radiance `{digest['surface_caustic_sampled_radiance_sum']:.4f}`, "
            f"volume cells `{digest['volume_cache_nonzero_cells']}`, "
            f"volume occupancy `{digest['volume_cache_nonzero_cell_ratio']:.6f}`, "
            f"volume hit ratio `{digest['volume_cache_sample_hit_ratio']:.6f}`, "
            f"footprints `{digest['volume_cache_footprint_deposit_count']}`, "
            f"footprint cells `{digest['volume_cache_footprint_cell_contribution_count']}`, "
            f"footprint radius `{digest['volume_cache_average_footprint_radius_voxels']:.3f} vox`, "
            f"footprint energy ratio `{digest['volume_cache_footprint_deposited_to_input_ratio']:.6f}`, "
            f"volume scatter contrib `{digest['volume_scatter_contributing_samples']}`, "
            f"volume radiance `{digest['volume_scatter_radiance_sum']:.4f}`, "
            f"scatter/cache `{digest['volume_scatter_to_cache_radiance_ratio']:.9f}`, "
            f"fallback `{digest['surface_receiver_fallback_enabled']}`/"
            f"`{digest['surface_receiver_fallback_count']}`, PNG `{png_text}`"
        )
    lines.extend(["", "## Frame Deltas Vs Mist No Caustic", ""])
    for cell_id, deltas in report["frame_deltas_vs_off"].items():
        lines.append(
            f"- `{cell_id}`: changed `{deltas['changed_pixel_count']}`, "
            f"positive `{deltas['positive_pixel_count']}`, "
            f"area `{deltas['positive_area_ratio']:.5f}`, "
            f"mean `{deltas['mean_luma_delta']:.4f}`, "
            f"max `{deltas['max_luma_delta']:.4f}`, "
            f"p95+ `{deltas['positive_luma_delta_p95']:.4f}`"
        )
    if report["failures"]:
        lines.extend(["", "## Failures", ""])
        for failure in report["failures"]:
            lines.append(f"- {failure}")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    cli = args.cli.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2

    review_root.mkdir(parents=True, exist_ok=True)
    scene_path = write_visual_scene(review_root)
    vf3d_path = review_root / "generated_volume" / "low_density_uniform_mist.vf3d"
    write_soft_mist_vf3d(vf3d_path)
    requests = matrix_requests(review_root, scene_path, vf3d_path)

    runs = []
    runs_by_cell: dict[str, dict] = {}
    failures: list[str] = []
    for item in requests:
        cell_id = item["cell_id"]
        try:
            elapsed = render_request(cli, item["request_path"], item["summary_path"], args.skip_render)
            summary = load_json(item["summary_path"])
            frame_path, png_path = copy_frame_png(summary, review_root, cell_id)
            summary_copy = review_root / "summaries" / f"summary_{cell_id}.json"
            summary_copy.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item["summary_path"], summary_copy)
            digest = caustic_digest(summary)
            cell_failures = validate_cell(cell_id, digest)
            failures.extend([f"{cell_id}: {failure}" for failure in cell_failures])
            run = {
                "cell_id": cell_id,
                "request_path": str(item["request_path"]),
                "summary_path": str(item["summary_path"]),
                "summary_copy": str(summary_copy),
                "frame_path": str(frame_path),
                "png_path": str(png_path),
                "elapsed_seconds": elapsed,
                "caustic": digest,
                "failures": cell_failures,
            }
            runs.append(run)
            runs_by_cell[cell_id] = run
        except Exception as exc:
            failures.append(f"{cell_id}: {exc}")
            if not args.keep_going:
                break

    frame_deltas = frame_delta_metrics(runs_by_cell) if runs_by_cell else {}
    volume_cell_id = "surface_and_volume_caustic"
    if runs_by_cell.get(volume_cell_id):
        volume_delta = frame_deltas.get(volume_cell_id, {})
        volume_digest = runs_by_cell[volume_cell_id]["caustic"]
        if volume_delta.get("positive_pixel_count", 0) < 64:
            failures.append(
                f"{volume_cell_id}: rendered frame did not produce a reviewable caustic-mist area "
                f"versus no-caustic baseline"
            )
        if volume_delta.get("max_luma_delta", 0.0) < 4.0:
            failures.append(
                f"{volume_cell_id}: rendered frame caustic-mist delta is below visual review threshold"
            )
        if volume_digest.get("volume_scatter_radiance_sum", 0.0) < 0.05:
            failures.append(
                f"{volume_cell_id}: volume caustic scatter radiance is below visual proof threshold"
            )
        if volume_digest.get("volume_cache_sample_hit_ratio", 0.0) < 0.001:
            failures.append(
                f"{volume_cell_id}: caustic volume cache hit ratio is too sparse for visual proof"
            )
        if volume_digest.get("volume_cache_nonzero_cell_ratio", 0.0) < 0.005:
            failures.append(
                f"{volume_cell_id}: caustic volume cache occupancy is too sparse for visual proof"
            )
        if volume_digest.get("volume_scatter_to_cache_radiance_ratio", 0.0) < 0.0001:
            failures.append(
                f"{volume_cell_id}: caustic volume scatter/cache radiance conversion is too weak for visual proof"
            )
    contact_sheet_path = review_root / "visual_sphere_mist_contact_sheet.png"
    write_contact_sheet(contact_sheet_path, runs)
    delta_artifacts = write_delta_artifacts(review_root, runs_by_cell)
    report = {
        "schema_version": "ray_tracing_spatial_caustic_visual_sphere_mist_matrix_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "scene_path": str(scene_path),
        "vf3d_path": str(vf3d_path),
        "contact_sheet_path": str(contact_sheet_path),
        "delta_artifacts": delta_artifacts,
        "runs": runs,
        "frame_deltas_vs_off": frame_deltas,
        "failures": failures,
        "passed": len(failures) == 0 and len(runs) == len(requests),
    }
    write_json(review_root / "visual_sphere_mist_matrix_report.json", report)
    write_index(review_root / "visual_sphere_mist_matrix_index.md", report)
    print(review_root / "visual_sphere_mist_matrix_report.json")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
