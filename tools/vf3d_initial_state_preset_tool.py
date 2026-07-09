#!/usr/bin/env python3
"""Generate local VF3D initial-state presets and RayTracing proof requests."""

from __future__ import annotations

import argparse
import json
import math
import struct
import subprocess
import zlib
from array import array
from pathlib import Path
from typing import Any


SCRIPT_PATH = Path(__file__).resolve()
RAY_ROOT = SCRIPT_PATH.parents[1]
WORKSPACE_ROOT = RAY_ROOT.parent
DEFAULT_OUTPUT_ROOT = (
    WORKSPACE_ROOT
    / "_private_workspace_artifacts"
    / "ambient_air_probe"
    / "vf3d_initial_state_presets"
)
REFERENCE_ROOT = (
    WORKSPACE_ROOT
    / "_private_workspace_artifacts"
    / "ambient_air_probe"
    / "high_complex_static_vf3d_v1"
)
DEFAULT_SCENE_PATH = REFERENCE_ROOT / "scene_runtime_dark_backdrop.json"
RUNTIME_SCENE_FILENAME = "scene_runtime_dark_backdrop.json"
RENDER_CLI_CANDIDATES = (
    RAY_ROOT / "build" / "toolchains" / "clang" / "arm64" / "tools" / "cli" / "ray_tracing_render_headless",
    RAY_ROOT / "build" / "arm64" / "tools" / "cli" / "ray_tracing_render_headless",
    RAY_ROOT / "build" / "tools" / "cli" / "ray_tracing_render_headless",
)
VF3D_MAGIC = (ord("V") << 24) | (ord("F") << 16) | (ord("3") << 8) | ord("D")
VF3D_VERSION = 1
HEADER_STRUCT = struct.Struct("<IIIII4xdQdfffffffIIII4x")


class PresetError(RuntimeError):
    pass


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise PresetError(f"failed to read JSON: {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise PresetError(f"failed to parse JSON: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise PresetError(f"JSON root must be an object: {path}")
    return value


def fallback_dark_backdrop_scene() -> dict[str, Any]:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "vf3d_initial_state_dark_backdrop_v1",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            {
                "object_id": "ambient_floor",
                "object_type": "plane_primitive",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": {"x": 0.0, "y": 0.0, "z": -0.65},
                    "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
                },
                "primitive": {
                    "kind": "plane_primitive",
                    "width": 5.2,
                    "height": 4.2,
                    "frame": {
                        "origin": {"x": 0.0, "y": 0.0, "z": -0.65},
                        "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
                        "axis_v": {"x": 0.0, "y": 1.0, "z": 0.0},
                        "normal": {"x": 0.0, "y": 0.0, "z": 1.0},
                    },
                },
            },
            {
                "object_id": "ambient_back_wall",
                "object_type": "plane_primitive",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": {"x": 0.0, "y": 1.65, "z": 0.35},
                    "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
                },
                "primitive": {
                    "kind": "plane_primitive",
                    "width": 5.2,
                    "height": 2.2,
                    "frame": {
                        "origin": {"x": 0.0, "y": 1.65, "z": 0.35},
                        "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
                        "axis_v": {"x": 0.0, "y": 0.0, "z": 1.0},
                        "normal": {"x": 0.0, "y": -1.0, "z": 0.0},
                    },
                },
            },
            {
                "object_id": "ambient_left_wall",
                "object_type": "plane_primitive",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": {"x": -2.2, "y": -0.1, "z": 0.35},
                    "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
                },
                "primitive": {
                    "kind": "plane_primitive",
                    "width": 4.2,
                    "height": 2.2,
                    "frame": {
                        "origin": {"x": -2.2, "y": -0.1, "z": 0.35},
                        "axis_u": {"x": 0.0, "y": 1.0, "z": 0.0},
                        "axis_v": {"x": 0.0, "y": 0.0, "z": 1.0},
                        "normal": {"x": 1.0, "y": 0.0, "z": 0.0},
                    },
                },
            },
        ],
        "materials": [],
        "lights": [{"position": {"x": 0.8, "y": 1.18, "z": 1.28}}],
        "cameras": [],
        "constraints": [],
        "hierarchy": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "light_settings": {"intensity": 7.8, "radius": 0.22},
                    "environment": {
                        "light_mode": 2,
                        "ambient_strength": 0.1,
                        "top_fill_strength": 0.3,
                    },
                    "object_materials": [
                        {
                            "object_id": "ambient_floor",
                            "material_id": 0,
                            "object_color": 5264988,
                            "roughness": 0.78,
                            "reflectivity": 0.025,
                        },
                        {
                            "object_id": "ambient_back_wall",
                            "material_id": 0,
                            "object_color": 3093560,
                            "roughness": 0.82,
                            "reflectivity": 0.015,
                        },
                        {
                            "object_id": "ambient_left_wall",
                            "material_id": 0,
                            "object_color": 8088402,
                            "roughness": 0.8,
                            "reflectivity": 0.015,
                        },
                    ],
                }
            }
        },
    }


def clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def rotate_xyz_degrees(
    point: tuple[float, float, float],
    degrees: tuple[float, float, float],
) -> tuple[float, float, float]:
    x, y, z = point
    rx, ry, rz = [math.radians(v) for v in degrees]
    cx, sx = math.cos(rx), math.sin(rx)
    cy, sy = math.cos(ry), math.sin(ry)
    cz, sz = math.cos(rz), math.sin(rz)

    y, z = y * cx - z * sx, y * sx + z * cx
    x, z = x * cy + z * sy, -x * sy + z * cy
    x, y = x * cz - y * sz, x * sz + y * cz
    return (x, y, z)


def smoothstep(edge0: float, edge1: float, x: float) -> float:
    if edge0 == edge1:
        return 0.0 if x < edge0 else 1.0
    t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def hash_noise(x: float, y: float, z: float, seed: int) -> float:
    value = math.sin(
        x * 12.9898
        + y * 78.233
        + z * 37.719
        + seed * 19.191
    ) * 43758.5453123
    return value - math.floor(value)


def wave_noise(x: float, y: float, z: float, seed: int) -> float:
    a = math.sin(3.1 * x + 4.3 * y + 1.7 * z + seed * 0.37)
    b = math.sin(5.7 * x - 2.1 * y + 3.6 * z + seed * 0.73)
    c = math.sin(-2.4 * x + 6.2 * y + 4.8 * z + seed * 1.17)
    return (a + b + c) / 3.0


def fbm(x: float, y: float, z: float, seed: int) -> float:
    total = 0.0
    amp = 0.55
    freq = 1.0
    norm = 0.0
    for octave in range(4):
        total += amp * wave_noise(x * freq, y * freq, z * freq, seed + octave * 17)
        total += amp * 0.35 * (hash_noise(x * freq, y * freq, z * freq, seed + octave * 31) * 2.0 - 1.0)
        norm += amp * 1.35
        amp *= 0.5
        freq *= 2.05
    return total / norm if norm > 0.0 else 0.0


def domain_warp(x: float, y: float, z: float, seed: int, strength: float) -> tuple[float, float, float]:
    if strength <= 0.0:
        return x, y, z
    wx = fbm(x * 1.7 + 11.0, y * 1.7 - 3.0, z * 1.7 + 5.0, seed + 101)
    wy = fbm(x * 1.9 - 7.0, y * 1.9 + 13.0, z * 1.9 - 2.0, seed + 211)
    wz = fbm(x * 1.5 + 2.0, y * 1.5 + 8.0, z * 1.5 + 17.0, seed + 307)
    return x + wx * strength, y + wy * strength, z + wz * strength


def ridge_noise(x: float, y: float, z: float, seed: int) -> float:
    value = fbm(x, y, z, seed)
    return 1.0 - abs(value * 2.0)


def index_3d(x: int, y: int, z: int, w: int, h: int) -> int:
    return (z * h + y) * w + x


def preset_label(run_id: str) -> str:
    return run_id.replace("_", " ").title()


def default_spec(run_id: str, preset_type: str, seed: int, output_root: Path) -> dict[str, Any]:
    parameters: dict[str, Any]
    if preset_type == "single_turbulent_cloud":
        parameters = {
            "center": [0.05, -0.05, 0.48],
            "extent_xyz": [1.25, 0.82, 0.68],
            "edge_softness": 0.36,
            "surface_breakup": 0.42,
            "density_scale": 3.2,
            "curl_strength": 0.28,
            "drift": [0.025, 0.015, 0.0],
        }
        if run_id.endswith("_v2"):
            parameters.update(
                {
                    "edge_softness": 0.48,
                    "surface_breakup": 0.78,
                    "density_scale": 3.05,
                    "domain_warp_strength": 0.28,
                    "boundary_noise_strength": 0.30,
                    "wispy_falloff": 0.36,
                    "wisp_density_scale": 0.42,
                    "edge_erosion_strength": 0.42,
                    "tendril_strength": 0.28,
                }
            )
    elif preset_type == "atmospheric_layer":
        parameters = {
            "height": 0.32,
            "thickness": 0.46,
            "falloff": 0.34,
            "density_scale": 1.9,
            "turbulence_strength": 0.36,
            "horizontal_wind": [0.18, 0.035, 0.0],
            "layer_extent": [2.35, 1.72],
        }
        if run_id.endswith("_v2"):
            parameters.update(
                {
                    "thickness": 0.40,
                    "falloff": 0.58,
                    "density_scale": 1.65,
                    "turbulence_strength": 0.62,
                    "domain_warp_strength": 0.24,
                    "height_warp_strength": 0.20,
                    "edge_erosion_strength": 0.34,
                    "wisp_density_scale": 0.22,
                    "layer_extent": [2.1, 1.58],
                }
            )
        if run_id == "room_haze_v1":
            parameters.update(
                {
                    "height": 0.38,
                    "thickness": 0.88,
                    "falloff": 0.82,
                    "density_scale": 0.46,
                    "turbulence_strength": 0.24,
                    "domain_warp_strength": 0.16,
                    "height_warp_strength": 0.08,
                    "edge_erosion_strength": 0.10,
                    "wisp_density_scale": 0.08,
                    "horizontal_wind": [0.045, 0.012, 0.0],
                    "layer_extent": [2.45, 1.82],
                }
            )
    elif preset_type == "cloud_bank":
        parameters = {
            "lobes": [
                {"center": [-0.78, -0.08, 0.52], "extent_xyz": [0.88, 0.58, 0.48], "weight": 0.84},
                {"center": [-0.18, 0.02, 0.64], "extent_xyz": [1.02, 0.68, 0.54], "weight": 0.74},
                {"center": [0.54, -0.04, 0.56], "extent_xyz": [0.92, 0.55, 0.48], "weight": 0.68},
                {"center": [0.03, 0.20, 0.42], "extent_xyz": [1.16, 0.42, 0.34], "weight": 0.48},
            ],
            "density_scale": 4.20,
            "edge_softness": 0.72,
            "surface_breakup": 0.82,
            "domain_warp_strength": 0.22,
            "boundary_noise_strength": 0.34,
            "wisp_density_scale": 0.62,
            "edge_erosion_strength": 0.20,
            "wind": [0.12, 0.035, 0.012],
            "curl_strength": 0.18,
        }
    elif preset_type == "rolling_fog_layer":
        parameters = {
            "height": 0.24,
            "thickness": 0.54,
            "falloff": 0.82,
            "density_scale": 2.05,
            "layer_extent": [2.25, 1.62],
            "ridge_strength": 0.68,
            "clump_strength": 0.52,
            "domain_warp_strength": 0.30,
            "height_warp_strength": 0.24,
            "edge_erosion_strength": 0.26,
            "wind": [0.18, 0.055, 0.0],
            "roll_strength": 0.16,
        }
        if run_id == "low_floor_fog_v1":
            parameters.update(
                {
                    "height": 0.18,
                    "thickness": 0.42,
                    "falloff": 0.95,
                    "density_scale": 3.10,
                    "layer_extent": [2.38, 1.70],
                    "ridge_strength": 0.48,
                    "clump_strength": 0.42,
                    "domain_warp_strength": 0.18,
                    "height_warp_strength": 0.10,
                    "edge_erosion_strength": 0.18,
                    "wind": [0.075, 0.018, 0.0],
                    "roll_strength": 0.06,
                }
            )
    elif preset_type == "mist_patch":
        parameters = {
            "center": [0.12, -0.08, 0.30],
            "extent_xyz": [1.08, 0.72, 0.42],
            "rotation_degrees": [0.0, 0.0, 0.0],
            "density_scale": 0.92,
            "edge_softness": 0.66,
            "surface_breakup": 0.48,
            "domain_warp_strength": 0.18,
            "boundary_noise_strength": 0.22,
            "wisp_density_scale": 0.13,
            "sparse_threshold": 0.56,
            "drift": [0.045, 0.018, 0.0],
            "curl_strength": 0.08,
        }
    elif preset_type == "plume_column":
        parameters = {
            "source_position": [0.02, -0.08, -0.50],
            "rise_height": 1.95,
            "base_radius": 0.18,
            "top_radius": 0.78,
            "core_density_scale": 3.4,
            "wisp_density_scale": 0.62,
            "side_wind": [0.20, 0.04, 0.0],
            "curl_strength": 0.38,
            "domain_warp_strength": 0.24,
            "boundary_noise_strength": 0.24,
            "edge_erosion_strength": 0.38,
            "tendril_strength": 0.46,
            "source_brightness": 0.55,
        }
        if run_id.endswith("_v2"):
            parameters.update(
                {
                    "base_radius": 0.22,
                    "top_radius": 0.84,
                    "core_density_scale": 2.65,
                    "wisp_density_scale": 0.86,
                    "side_wind": [0.16, 0.035, 0.0],
                    "curl_strength": 0.32,
                    "domain_warp_strength": 0.30,
                    "boundary_noise_strength": 0.32,
                    "edge_erosion_strength": 0.20,
                    "tendril_strength": 0.34,
                    "source_brightness": 0.24,
                    "core_inner_radius": 0.10,
                    "core_outer_radius": 0.86,
                    "envelope_inner_radius": 0.48,
                    "envelope_outer_radius": 1.92,
                    "diffuse_shell_density": 0.34,
                    "mid_column_soften": 0.32,
                    "top_fade_start": 0.46,
                    "top_fade_end": 0.88,
                    "top_breakup_strength": 0.20,
                }
            )
        if run_id.endswith("_v3"):
            parameters.update(
                {
                    "base_radius": 0.25,
                    "top_radius": 0.70,
                    "core_density_scale": 2.38,
                    "wisp_density_scale": 1.05,
                    "side_wind": [0.20, 0.055, 0.0],
                    "curl_strength": 0.42,
                    "domain_warp_strength": 0.38,
                    "boundary_noise_strength": 0.40,
                    "edge_erosion_strength": 0.28,
                    "tendril_strength": 0.58,
                    "source_brightness": 0.14,
                    "core_inner_radius": 0.18,
                    "core_outer_radius": 0.76,
                    "envelope_inner_radius": 0.42,
                    "envelope_outer_radius": 1.72,
                    "diffuse_shell_density": 0.42,
                    "mid_column_soften": 0.48,
                    "top_fade_start": 0.32,
                    "top_fade_end": 0.76,
                    "top_breakup_strength": 0.42,
                    "base_breakup_strength": 0.56,
                    "side_lobe_density": 0.38,
                    "detached_tendril_density": 0.32,
                    "layered_clump_strength": 0.42,
                    "asymmetric_shear": 0.22,
                }
            )
    else:
        raise PresetError(f"unsupported preset type: {preset_type}")

    grid_origin = [-2.3, -1.725, -0.65]
    if run_id in {
        "cloud_bank_v1",
        "rolling_fog_layer_v1",
        "room_haze_v1",
        "low_floor_fog_v1",
    }:
        grid_origin[2] = 0.0

    return {
        "schema_version": "vf3d_initial_state_preset_v1",
        "run_id": run_id,
        "grid": {
            "w": 128,
            "h": 96,
            "d": 64,
            "origin": grid_origin,
            "voxel_size": 0.0359375,
            "scene_up": [0.0, 0.0, 1.0],
            "author_window_w": 772,
            "author_window_h": 772,
            "import_fit": 0.25,
        },
        "preset": {
            "type": preset_type,
            "seed": seed,
            "parameters": parameters,
        },
        "render_profile": {
            "scene_path": str(DEFAULT_SCENE_PATH),
            "production_profile": "production_visibility_tuned",
            "debug_profile": "density_debug",
        },
        "output": {"root": str(output_root / run_id)},
    }


def validate_spec(spec: dict[str, Any]) -> None:
    if spec.get("schema_version") != "vf3d_initial_state_preset_v1":
        raise PresetError("schema_version must be vf3d_initial_state_preset_v1")
    if not isinstance(spec.get("run_id"), str) or not spec["run_id"]:
        raise PresetError("run_id is required")
    grid = spec.get("grid")
    if not isinstance(grid, dict):
        raise PresetError("grid object is required")
    for key in ("w", "h", "d"):
        if int(grid.get(key, 0)) <= 0:
            raise PresetError(f"grid.{key} must be positive")
    if len(grid.get("origin", [])) != 3 or len(grid.get("scene_up", [])) != 3:
        raise PresetError("grid.origin and grid.scene_up must have three values")
    if float(grid.get("voxel_size", 0.0)) <= 0.0:
        raise PresetError("grid.voxel_size must be positive")
    preset = spec.get("preset")
    if not isinstance(preset, dict) or not isinstance(preset.get("type"), str):
        raise PresetError("preset.type is required")
    render_profile = spec.get("render_profile")
    if not isinstance(render_profile, dict) or not render_profile.get("scene_path"):
        raise PresetError("render_profile.scene_path is required")
    output = spec.get("output")
    if not isinstance(output, dict) or not output.get("root"):
        raise PresetError("output.root is required")
    warmup = spec.get("warmup_preview")
    if warmup is not None:
        if not isinstance(warmup, dict):
            raise PresetError("warmup_preview must be an object when present")
        if int(warmup.get("frames", 0)) < 0:
            raise PresetError("warmup_preview.frames must be non-negative")


def sample_single_turbulent_cloud(
    x: float,
    y: float,
    z: float,
    params: dict[str, Any],
    seed: int,
) -> tuple[float, tuple[float, float, float]]:
    cx, cy, cz = [float(v) for v in params["center"]]
    ex, ey, ez = [float(v) for v in params["extent_xyz"]]
    rotation = tuple(float(v) for v in params.get("rotation_degrees", (0.0, 0.0, 0.0)))
    raw_dx, raw_dy, raw_dz = rotate_xyz_degrees(((x - cx) / ex, (y - cy) / ey, (z - cz) / ez), rotation)
    dx, dy, dz = domain_warp(
        raw_dx,
        raw_dy,
        raw_dz,
        seed,
        float(params.get("domain_warp_strength", 0.0)),
    )
    boundary_noise = fbm(dx * 2.1, dy * 2.1, dz * 2.1, seed + 71)
    boundary_noise += 0.45 * ridge_noise(dx * 5.4, dy * 5.4, dz * 5.4, seed + 73)
    boundary_noise *= float(params.get("boundary_noise_strength", 0.0))
    r = math.sqrt(dx * dx + dy * dy + dz * dz) - boundary_noise
    edge_softness = float(params["edge_softness"])
    shell = 1.0 - smoothstep(1.0 - edge_softness, 1.0, r)
    wisp_shell = 1.0 - smoothstep(1.0, 1.0 + float(params.get("wispy_falloff", 0.0)), r)
    if shell <= 0.0 and wisp_shell <= 0.0:
        return 0.0, tuple(float(v) for v in params["drift"])

    noise = fbm(dx * 3.3 + 0.5 * dz, dy * 3.3 - 0.35 * dx, dz * 3.2, seed)
    band = 0.5 + 0.5 * math.sin(10.5 * r + 2.6 * math.atan2(dy, dx) + noise * 3.1)
    breakup = float(params["surface_breakup"])
    density = float(params["density_scale"]) * shell * (0.62 + breakup * noise + 0.28 * band)
    density *= smoothstep(0.02, 0.22, 1.0 - r + 0.22 * noise)
    erosion = smoothstep(
        0.35,
        0.95,
        fbm(dx * 8.0 + dz * 1.7, dy * 8.0 - dx * 1.1, dz * 8.0, seed + 313) * 0.5 + 0.5,
    )
    edge_band = smoothstep(0.48, 1.12, r)
    density *= 1.0 - float(params.get("edge_erosion_strength", 0.0)) * erosion * edge_band
    tendril = ridge_noise(dx * 6.0 + dy * 1.8, dy * 4.2 - dz * 2.0, dz * 5.5, seed + 409)
    tendril_gate = smoothstep(0.56, 1.24, r) * wisp_shell
    density += (
        float(params.get("wisp_density_scale", 0.0))
        * float(params["density_scale"])
        * max(0.0, tendril)
        * tendril_gate
        * (0.35 + float(params.get("tendril_strength", 0.0)))
    )
    density = max(0.0, density)

    curl = float(params["curl_strength"])
    drift_x, drift_y, drift_z = [float(v) for v in params["drift"]]
    vx = drift_x - dy * curl + 0.045 * noise
    vy = drift_y + dx * curl + 0.035 * wave_noise(dx * 2.0, dy * 2.0, dz * 2.0, seed + 5)
    vz = drift_z + 0.08 * math.sin(dx * 3.2 + dy * 1.7 + seed * 0.1) * max(shell, wisp_shell)
    return density, (vx, vy, vz)


def sample_atmospheric_layer(
    x: float,
    y: float,
    z: float,
    params: dict[str, Any],
    seed: int,
) -> tuple[float, tuple[float, float, float]]:
    height = float(params["height"])
    thickness = float(params["thickness"])
    falloff = float(params["falloff"])
    nx = x / float(params["layer_extent"][0])
    ny = y / float(params["layer_extent"][1])
    nx, ny, wz = domain_warp(
        nx,
        ny,
        z,
        seed,
        float(params.get("domain_warp_strength", 0.0)),
    )
    height_warp = float(params.get("height_warp_strength", 0.0)) * fbm(nx * 3.2, ny * 3.2, wz * 2.5, seed + 503)
    vertical = abs(z - height - height_warp) / max(thickness, 1e-6)
    layer = 1.0 - smoothstep(0.25, 1.0 + falloff, vertical)
    wisp_layer = 1.0 - smoothstep(1.0, 1.0 + falloff * 1.4, vertical)
    if layer <= 0.0 and wisp_layer <= 0.0:
        wind_x, wind_y, wind_z = [float(v) for v in params["horizontal_wind"]]
        return 0.0, (wind_x, wind_y, wind_z)

    noise = fbm(nx * 4.4, ny * 4.4, z * 2.5, seed)
    roll = math.sin(nx * 13.0 + noise * 2.4) * math.cos(ny * 7.0 - noise)
    density = float(params["density_scale"]) * layer
    density *= 0.56 + float(params["turbulence_strength"]) * noise + 0.20 * roll
    horizontal_gate = smoothstep(-0.62, -0.18, nx) * (1.0 - smoothstep(0.80, 1.03, nx))
    edge_erosion = smoothstep(
        0.36,
        0.88,
        fbm(nx * 8.0, ny * 7.0, z * 5.0, seed + 601) * 0.5 + 0.5,
    )
    density *= horizontal_gate
    density *= 1.0 - float(params.get("edge_erosion_strength", 0.0)) * edge_erosion * (1.0 - layer)
    density += (
        float(params.get("wisp_density_scale", 0.0))
        * float(params["density_scale"])
        * max(0.0, ridge_noise(nx * 6.0, ny * 6.0, z * 4.5, seed + 607))
        * wisp_layer
        * horizontal_gate
    )
    density = max(0.0, density)

    wind_x, wind_y, wind_z = [float(v) for v in params["horizontal_wind"]]
    vx = wind_x + 0.045 * math.sin(ny * 8.0 + z * 2.0 + seed)
    vy = wind_y + 0.035 * math.cos(nx * 7.5 - z * 1.5 + seed * 0.3)
    vz = wind_z + 0.055 * roll * layer
    return density, (vx, vy, vz)


def sample_cloud_bank(
    x: float,
    y: float,
    z: float,
    params: dict[str, Any],
    seed: int,
) -> tuple[float, tuple[float, float, float]]:
    density = 0.0
    weighted_dx = 0.0
    weighted_dy = 0.0
    total_weight = 0.0
    edge_softness = float(params["edge_softness"])
    for lobe_i, lobe in enumerate(params["lobes"]):
        cx, cy, cz = [float(v) for v in lobe["center"]]
        ex, ey, ez = [float(v) for v in lobe["extent_xyz"]]
        weight = float(lobe.get("weight", 1.0))
        raw_dx = (x - cx) / ex
        raw_dy = (y - cy) / ey
        raw_dz = (z - cz) / ez
        dx, dy, dz = domain_warp(
            raw_dx,
            raw_dy,
            raw_dz,
            seed + 83 * lobe_i,
            float(params["domain_warp_strength"]),
        )
        boundary = fbm(dx * 2.2, dy * 2.2, dz * 2.0, seed + 811 + lobe_i)
        boundary += 0.45 * ridge_noise(dx * 5.5, dy * 5.0, dz * 4.6, seed + 821 + lobe_i)
        r = math.sqrt(dx * dx + dy * dy + dz * dz) - float(params["boundary_noise_strength"]) * boundary
        shell = 1.0 - smoothstep(1.0 - edge_softness, 1.0, r)
        wisp_shell = 1.0 - smoothstep(1.0, 1.45, r)
        if shell <= 0.0 and wisp_shell <= 0.0:
            continue
        noise = fbm(dx * 3.4 + dz * 0.7, dy * 3.0 - dx * 0.5, dz * 3.2, seed + 829 + lobe_i)
        breakup = 0.58 + float(params["surface_breakup"]) * noise
        erosion = smoothstep(0.30, 0.92, fbm(dx * 8.0, dy * 7.0, dz * 5.8, seed + 839 + lobe_i) * 0.5 + 0.5)
        edge_band = smoothstep(0.42, 1.12, r)
        lobe_density = weight * float(params["density_scale"]) * shell * max(0.0, breakup)
        lobe_density *= 1.0 - float(params["edge_erosion_strength"]) * erosion * edge_band
        wisp = max(0.0, ridge_noise(dx * 6.2 + dy, dy * 5.4 - dz, dz * 5.0, seed + 853 + lobe_i))
        lobe_density += weight * float(params["density_scale"]) * float(params["wisp_density_scale"]) * wisp * wisp_shell * edge_band
        density += max(0.0, lobe_density)
        weighted_dx += dx * max(lobe_density, 0.0)
        weighted_dy += dy * max(lobe_density, 0.0)
        total_weight += max(lobe_density, 0.0)

    wind_x, wind_y, wind_z = [float(v) for v in params["wind"]]
    if density <= 0.0:
        return 0.0, (wind_x, wind_y, wind_z)
    avg_dx = weighted_dx / max(total_weight, 1e-6)
    avg_dy = weighted_dy / max(total_weight, 1e-6)
    curl = float(params["curl_strength"])
    vx = wind_x - avg_dy * curl + 0.025 * fbm(x * 2.0, y * 2.0, z * 2.0, seed + 859)
    vy = wind_y + avg_dx * curl + 0.025 * wave_noise(x * 2.4, y * 2.0, z * 2.2, seed + 863)
    vz = wind_z + 0.035 * math.sin((x + y) * 2.2 + seed * 0.1)
    return density, (vx, vy, vz)


def sample_rolling_fog_layer(
    x: float,
    y: float,
    z: float,
    params: dict[str, Any],
    seed: int,
) -> tuple[float, tuple[float, float, float]]:
    nx = x / float(params["layer_extent"][0])
    ny = y / float(params["layer_extent"][1])
    nx, ny, wz = domain_warp(nx, ny, z, seed + 907, float(params["domain_warp_strength"]))
    height_warp = float(params["height_warp_strength"]) * fbm(nx * 3.0, ny * 2.7, wz * 2.0, seed + 911)
    vertical = abs(z - float(params["height"]) - height_warp) / max(float(params["thickness"]), 1e-6)
    base = 1.0 - smoothstep(0.20, 1.0 + float(params["falloff"]), vertical)
    if base <= 0.0:
        return 0.0, tuple(float(v) for v in params["wind"])
    ridge = 0.5 + 0.5 * math.sin(nx * 17.0 + 2.4 * fbm(nx * 3.4, ny * 3.1, z * 2.2, seed + 919))
    ridge *= 0.5 + 0.5 * math.cos(ny * 9.0 - z * 3.4 + seed * 0.2)
    clump = max(0.0, ridge_noise(nx * 5.4, ny * 4.6, z * 3.6, seed + 929))
    horizontal_gate = smoothstep(-1.05, -0.72, nx) * (1.0 - smoothstep(0.84, 1.08, nx))
    horizontal_gate *= smoothstep(-1.00, -0.74, ny) * (1.0 - smoothstep(0.78, 1.05, ny))
    erosion = smoothstep(0.28, 0.88, fbm(nx * 8.0, ny * 7.0, z * 5.5, seed + 937) * 0.5 + 0.5)
    density = float(params["density_scale"]) * base * horizontal_gate
    density *= 0.42 + float(params["ridge_strength"]) * ridge + float(params["clump_strength"]) * clump
    density *= 1.0 - float(params["edge_erosion_strength"]) * erosion * (1.0 - base)
    density = max(0.0, density)
    wind_x, wind_y, wind_z = [float(v) for v in params["wind"]]
    roll = float(params["roll_strength"]) * math.sin(nx * 9.0 + ny * 3.0 + seed * 0.1)
    vx = wind_x + 0.05 * math.sin(ny * 8.0 + z * 4.0)
    vy = wind_y + 0.04 * math.cos(nx * 7.0 - z * 3.0)
    vz = wind_z + roll * base
    return density, (vx, vy, vz)


def sample_mist_patch(
    x: float,
    y: float,
    z: float,
    params: dict[str, Any],
    seed: int,
) -> tuple[float, tuple[float, float, float]]:
    cx, cy, cz = [float(v) for v in params["center"]]
    ex, ey, ez = [float(v) for v in params["extent_xyz"]]
    raw_dx = (x - cx) / ex
    raw_dy = (y - cy) / ey
    raw_dz = (z - cz) / ez
    dx, dy, dz = domain_warp(raw_dx, raw_dy, raw_dz, seed + 967, float(params["domain_warp_strength"]))
    boundary = fbm(dx * 2.0, dy * 2.0, dz * 2.0, seed + 971) * float(params["boundary_noise_strength"])
    r = math.sqrt(dx * dx + dy * dy + dz * dz) - boundary
    shell = 1.0 - smoothstep(1.0 - float(params["edge_softness"]), 1.0, r)
    wisp_shell = 1.0 - smoothstep(1.0, 1.72, r)
    if shell <= 0.0 and wisp_shell <= 0.0:
        return 0.0, tuple(float(v) for v in params["drift"])
    noise = fbm(dx * 3.2, dy * 3.0, dz * 2.8, seed + 977)
    sparse = smoothstep(float(params["sparse_threshold"]), 1.0, noise)
    density = float(params["density_scale"]) * shell * (0.36 + float(params["surface_breakup"]) * noise) * (0.45 + 0.55 * sparse)
    wisps = max(0.0, ridge_noise(dx * 5.0 + dy, dy * 4.6 - dz, dz * 4.4, seed + 983))
    density += float(params["density_scale"]) * float(params["wisp_density_scale"]) * wisps * wisp_shell
    density = max(0.0, density)
    drift_x, drift_y, drift_z = [float(v) for v in params["drift"]]
    curl = float(params["curl_strength"])
    vx = drift_x - dy * curl + 0.018 * noise
    vy = drift_y + dx * curl + 0.014 * wave_noise(dx * 2.0, dy * 2.0, dz * 2.0, seed + 991)
    vz = drift_z + 0.025 * math.sin(dx * 2.6 + dy * 1.9 + seed * 0.1) * max(shell, wisp_shell)
    return density, (vx, vy, vz)


def sample_plume_column(
    x: float,
    y: float,
    z: float,
    params: dict[str, Any],
    seed: int,
) -> tuple[float, tuple[float, float, float]]:
    sx, sy, sz = [float(v) for v in params["source_position"]]
    rise_height = float(params["rise_height"])
    t = (z - sz) / max(rise_height, 1e-6)
    if t < -0.10 or t > 1.22:
        return 0.0, tuple(float(v) for v in params["side_wind"])

    t_clamped = clamp(t, 0.0, 1.0)
    theta = t_clamped * 5.8 + seed * 0.17
    center_wander = 0.11 * t_clamped * math.sin(theta * 1.4)
    shear = float(params.get("asymmetric_shear", 0.0))
    cx = sx + center_wander + 0.18 * t_clamped * t_clamped + shear * t_clamped * math.sin(theta * 0.7)
    cy = sy + 0.09 * t_clamped * math.cos(theta * 1.1) + 0.45 * shear * t_clamped * t_clamped
    radius = float(params["base_radius"]) + (float(params["top_radius"]) - float(params["base_radius"])) * smoothstep(0.0, 1.0, t_clamped)
    radius *= 1.0 + 0.16 * fbm(t_clamped * 2.0, 0.0, 0.0, seed + 701)

    raw_dx = (x - cx) / max(radius, 1e-6)
    raw_dy = (y - cy) / max(radius * 0.82, 1e-6)
    raw_dz = t_clamped * 2.0 - 1.0
    dx, dy, dz = domain_warp(
        raw_dx,
        raw_dy,
        raw_dz,
        seed + 709,
        float(params["domain_warp_strength"]) * (0.45 + 0.85 * t_clamped),
    )
    radial = math.sqrt(dx * dx + dy * dy)
    height_gate = smoothstep(-0.04, 0.08, t) * (1.0 - smoothstep(1.0, 1.18, t))
    boundary_noise = fbm(dx * 2.4, dy * 2.4, dz * 1.2, seed + 719)
    boundary_noise += 0.40 * ridge_noise(dx * 6.5, dy * 6.5, dz * 2.0, seed + 727)
    top_breakup = float(params.get("top_breakup_strength", 0.0)) * fbm(dx * 3.2, dy * 3.2, dz * 2.4, seed + 721)
    top_fade_start = float(params.get("top_fade_start", 1.0)) + top_breakup
    top_fade_end = float(params.get("top_fade_end", 1.18)) + top_breakup
    height_gate *= 1.0 - smoothstep(top_fade_start, top_fade_end, t)
    radial_warped = radial - float(params["boundary_noise_strength"]) * boundary_noise
    core_inner = float(params.get("core_inner_radius", 0.18))
    core_outer = float(params.get("core_outer_radius", 0.92))
    envelope_inner = float(params.get("envelope_inner_radius", 0.72))
    envelope_outer = float(params.get("envelope_outer_radius", 1.38))
    core = 1.0 - smoothstep(core_inner, core_outer, radial_warped)
    envelope = 1.0 - smoothstep(envelope_inner, envelope_outer, radial_warped)
    if envelope <= 0.0 or height_gate <= 0.0:
        return 0.0, tuple(float(v) for v in params["side_wind"])

    curl_band = 0.5 + 0.5 * math.sin(
        9.5 * radial
        + 8.0 * t_clamped
        + 2.8 * math.atan2(dy, dx)
        + 2.2 * fbm(dx * 2.5, dy * 2.5, dz * 2.0, seed + 733)
    )
    erosion = smoothstep(
        0.35,
        0.90,
        fbm(dx * 8.0, dy * 8.0, dz * 4.2, seed + 739) * 0.5 + 0.5,
    )
    edge_band = smoothstep(0.46, envelope_outer * 0.92, radial_warped)
    mid_soften = float(params.get("mid_column_soften", 0.0)) * smoothstep(0.18, 0.62, t_clamped) * (
        1.0 - smoothstep(0.74, 1.0, t_clamped)
    )
    density = float(params["core_density_scale"]) * height_gate * envelope
    density *= 0.30 + 0.42 * core + 0.34 * curl_band
    clump_strength = float(params.get("layered_clump_strength", 0.0))
    if clump_strength > 0.0:
        clump = 0.5 + 0.5 * math.sin(28.0 * t_clamped + 3.4 * fbm(dx * 2.0, dy * 2.0, dz * 2.0, seed + 735))
        density *= 1.0 - clump_strength * 0.42 + clump_strength * 0.72 * clump
    density *= 1.0 - mid_soften * core
    base_breakup = float(params.get("base_breakup_strength", 0.0))
    if base_breakup > 0.0:
        base_holes = smoothstep(0.32, 0.86, fbm(dx * 7.0, dy * 7.0, t_clamped * 4.5, seed + 737) * 0.5 + 0.5)
        density *= 1.0 - base_breakup * base_holes * (1.0 - smoothstep(0.12, 0.48, t_clamped)) * core
    density *= 1.0 - float(params["edge_erosion_strength"]) * erosion * edge_band
    tendril = ridge_noise(dx * 6.0 + dz * 1.6, dy * 6.0 - dz * 1.1, t_clamped * 4.0, seed + 743)
    tendril_gate = smoothstep(0.38, envelope_outer * 0.86, radial_warped) * envelope
    density += (
        float(params["wisp_density_scale"])
        * float(params["core_density_scale"])
        * tendril
        * tendril_gate
        * height_gate
        * (0.45 + float(params["tendril_strength"]) * t_clamped)
    )
    diffuse_shell = 1.0 - smoothstep(0.92, envelope_outer, radial_warped)
    diffuse_shell *= smoothstep(0.30, envelope_outer * 0.90, radial_warped)
    diffuse_noise = 0.45 + 0.55 * fbm(dx * 3.5, dy * 3.5, dz * 2.1, seed + 747)
    density += (
        float(params.get("diffuse_shell_density", 0.0))
        * float(params["core_density_scale"])
        * diffuse_shell
        * diffuse_noise
        * height_gate
    )
    side_lobe_density = float(params.get("side_lobe_density", 0.0))
    if side_lobe_density > 0.0:
        side_lobes = max(0.0, ridge_noise(dx * 4.2 + t_clamped * 5.0, dy * 4.2 - t_clamped * 3.0, dz * 2.0, seed + 761))
        side_gate = smoothstep(0.58, envelope_outer * 0.78, radial_warped) * (1.0 - smoothstep(envelope_outer * 0.72, envelope_outer, radial_warped))
        density += side_lobe_density * float(params["core_density_scale"]) * side_lobes * side_gate * height_gate
    detached_density = float(params.get("detached_tendril_density", 0.0))
    if detached_density > 0.0:
        detached = max(0.0, ridge_noise(dx * 7.0 + dz * 2.0, dy * 5.5 - dz * 1.5, t_clamped * 6.0, seed + 769))
        detached_gate = smoothstep(0.70, envelope_outer * 0.86, radial_warped) * smoothstep(0.18, 0.70, t_clamped)
        density += detached_density * float(params["core_density_scale"]) * detached * detached_gate * height_gate
    source_core = math.exp(-((x - sx) ** 2 + (y - sy) ** 2) / max(float(params["base_radius"]) ** 2, 1e-6))
    density += float(params["source_brightness"]) * float(params["core_density_scale"]) * source_core * smoothstep(0.14, 0.0, abs(t))
    density = max(0.0, density)

    wind_x, wind_y, wind_z = [float(v) for v in params["side_wind"]]
    curl = float(params["curl_strength"]) * (0.35 + 0.95 * t_clamped)
    vx = wind_x * t_clamped - dy * curl * 0.16 + 0.035 * fbm(dx * 3.0, dy * 3.0, dz * 2.0, seed + 751)
    vy = wind_y * t_clamped + dx * curl * 0.16 + 0.035 * wave_noise(dx * 2.0, dy * 2.0, dz * 2.0, seed + 757)
    vz = 0.24 + 0.40 * (1.0 - t_clamped) + 0.08 * curl_band
    return density, (vx, vy, wind_z + vz)


def empty_stats() -> dict[str, float | int]:
    return {
        "min": 0.0,
        "max": 0.0,
        "sum": 0.0,
        "mean": 0.0,
    }


def compute_stats(values: array) -> dict[str, float | int]:
    if not values:
        return empty_stats()
    total = 0.0
    min_value = float("inf")
    max_value = float("-inf")
    non_zero = 0
    for raw in values:
        value = float(raw)
        total += value
        min_value = min(min_value, value)
        max_value = max(max_value, value)
        if abs(value) > 0.0:
            non_zero += 1
    return {
        "min": min_value,
        "max": max_value,
        "sum": total,
        "mean": total / len(values),
        "non_zero_count": non_zero,
    }


def refresh_field_stats(fields: dict[str, Any]) -> None:
    velocity_magnitude = array("f", [0.0]) * len(fields["density"])
    for idx in range(len(velocity_magnitude)):
        vx = float(fields["velx"][idx])
        vy = float(fields["vely"][idx])
        vz = float(fields["velz"][idx])
        velocity_magnitude[idx] = math.sqrt(vx * vx + vy * vy + vz * vz)
    fields["density_stats"] = compute_stats(fields["density"])
    fields["velocity_stats"] = compute_stats(velocity_magnitude)
    fields["active_density_cells"] = int(fields["density_stats"]["non_zero_count"])


def build_fields(spec: dict[str, Any]) -> dict[str, Any]:
    grid = spec["grid"]
    w = int(grid["w"])
    h = int(grid["h"])
    d = int(grid["d"])
    origin = [float(v) for v in grid["origin"]]
    voxel_size = float(grid["voxel_size"])
    preset = spec["preset"]
    preset_type = str(preset["type"])
    seed = int(preset.get("seed", 1))
    params = dict(preset.get("parameters", {}))
    cell_count = w * h * d

    density = array("f", [0.0]) * cell_count
    velx = array("f", [0.0]) * cell_count
    vely = array("f", [0.0]) * cell_count
    velz = array("f", [0.0]) * cell_count
    pressure = array("f", [0.0]) * cell_count
    solid = bytearray(cell_count)
    velocity_magnitude = array("f", [0.0]) * cell_count

    sampler = sample_single_turbulent_cloud
    if preset_type == "atmospheric_layer":
        sampler = sample_atmospheric_layer
    elif preset_type == "cloud_bank":
        sampler = sample_cloud_bank
    elif preset_type == "rolling_fog_layer":
        sampler = sample_rolling_fog_layer
    elif preset_type == "mist_patch":
        sampler = sample_mist_patch
    elif preset_type == "plume_column":
        sampler = sample_plume_column
    elif preset_type != "single_turbulent_cloud":
        raise PresetError(f"unsupported preset type: {preset_type}")

    for z_i in range(d):
        z = origin[2] + (z_i + 0.5) * voxel_size
        for y_i in range(h):
            y = origin[1] + (y_i + 0.5) * voxel_size
            for x_i in range(w):
                x = origin[0] + (x_i + 0.5) * voxel_size
                idx = index_3d(x_i, y_i, z_i, w, h)
                rho, vel = sampler(x, y, z, params, seed)
                density[idx] = float(rho)
                velx[idx] = float(vel[0])
                vely[idx] = float(vel[1])
                velz[idx] = float(vel[2])
                velocity_magnitude[idx] = math.sqrt(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2])

    density_stats = compute_stats(density)
    velocity_stats = compute_stats(velocity_magnitude)
    active_density_cells = int(density_stats["non_zero_count"])
    return {
        "density": density,
        "velx": velx,
        "vely": vely,
        "velz": velz,
        "pressure": pressure,
        "solid": solid,
        "density_stats": density_stats,
        "velocity_stats": velocity_stats,
        "active_density_cells": active_density_cells,
    }


def sample_array_trilinear(values: array, x: float, y: float, z: float, w: int, h: int, d: int) -> float:
    if x < 0.0 or y < 0.0 or z < 0.0 or x > w - 1 or y > h - 1 or z > d - 1:
        return 0.0
    x0 = int(math.floor(x))
    y0 = int(math.floor(y))
    z0 = int(math.floor(z))
    x1 = min(w - 1, x0 + 1)
    y1 = min(h - 1, y0 + 1)
    z1 = min(d - 1, z0 + 1)
    tx = x - x0
    ty = y - y0
    tz = z - z0

    def at(ix: int, iy: int, iz: int) -> float:
        return float(values[index_3d(ix, iy, iz, w, h)])

    c000 = at(x0, y0, z0)
    c100 = at(x1, y0, z0)
    c010 = at(x0, y1, z0)
    c110 = at(x1, y1, z0)
    c001 = at(x0, y0, z1)
    c101 = at(x1, y0, z1)
    c011 = at(x0, y1, z1)
    c111 = at(x1, y1, z1)
    c00 = c000 * (1.0 - tx) + c100 * tx
    c10 = c010 * (1.0 - tx) + c110 * tx
    c01 = c001 * (1.0 - tx) + c101 * tx
    c11 = c011 * (1.0 - tx) + c111 * tx
    c0 = c00 * (1.0 - ty) + c10 * ty
    c1 = c01 * (1.0 - ty) + c11 * ty
    return c0 * (1.0 - tz) + c1 * tz


def apply_warmup_preview(spec: dict[str, Any], fields: dict[str, Any]) -> dict[str, Any]:
    warmup = dict(spec.get("warmup_preview", {}))
    frames = int(warmup.get("frames", 0))
    if frames <= 0:
        diagnostics = density_distribution_diagnostics(spec, fields["density"])
        return {
            "enabled": False,
            "frames": 0,
            "input_density_stats": fields["density_stats"],
            "output_density_stats": fields["density_stats"],
            "input_density_distribution": diagnostics,
            "output_density_distribution": diagnostics,
        }

    grid = spec["grid"]
    w = int(grid["w"])
    h = int(grid["h"])
    d = int(grid["d"])
    voxel_size = float(grid["voxel_size"])
    dt_seconds = float(warmup.get("dt_seconds", 0.012))
    diffusion = clamp(float(warmup.get("diffusion", 0.075)), 0.0, 0.45)
    dissipation = clamp(float(warmup.get("dissipation", 0.996)), 0.90, 1.0)
    buoyancy_gain = float(warmup.get("buoyancy_gain", 0.10))
    lateral_noise = float(warmup.get("lateral_noise", 0.0))
    sample_mode = str(warmup.get("sample_mode", "nearest"))
    seed = int(spec["preset"].get("seed", 1))
    cell_count = w * h * d
    density = fields["density"]
    input_distribution = density_distribution_diagnostics(spec, density)
    scratch = array("f", [0.0]) * cell_count
    next_density = array("f", [0.0]) * cell_count
    input_density_stats = dict(fields["density_stats"])
    active_indices = [idx for idx, value in enumerate(density) if float(value) > 1e-5]
    if active_indices:
        xs = [idx % w for idx in active_indices]
        ys = [(idx // w) % h for idx in active_indices]
        zs = [idx // (w * h) for idx in active_indices]
        pad = int(warmup.get("active_region_pad", max(8, frames)))
        x_min = max(0, min(xs) - pad)
        x_max = min(w - 1, max(xs) + pad)
        y_min = max(0, min(ys) - pad)
        y_max = min(h - 1, max(ys) + pad)
        z_min = max(0, min(zs) - pad)
        z_max = min(d - 1, max(zs) + pad)
    else:
        x_min, x_max = 0, w - 1
        y_min, y_max = 0, h - 1
        z_min, z_max = 0, d - 1

    for frame in range(frames):
        phase = frame * 0.071
        for z_i in range(z_min, z_max + 1):
            z_norm = z_i / max(d - 1, 1)
            for y_i in range(y_min, y_max + 1):
                y_norm = y_i / max(h - 1, 1)
                for x_i in range(x_min, x_max + 1):
                    x_norm = x_i / max(w - 1, 1)
                    idx = index_3d(x_i, y_i, z_i, w, h)
                    local_density = float(density[idx])
                    if lateral_noise != 0.0:
                        swirl_x = lateral_noise * math.sin(
                            x_norm * 17.0 + y_norm * 7.0 + z_norm * 11.0 + phase + seed * 0.13
                        )
                        swirl_y = lateral_noise * math.cos(
                            x_norm * 9.0 - y_norm * 15.0 + z_norm * 6.0 + phase * 1.7 + seed * 0.19
                        )
                    else:
                        swirl_x = 0.0
                        swirl_y = 0.0
                    vx = float(fields["velx"][idx]) + swirl_x
                    vy = float(fields["vely"][idx]) + swirl_y
                    vz = float(fields["velz"][idx]) + buoyancy_gain * local_density / (1.0 + local_density)
                    back_x = x_i - (vx * dt_seconds / voxel_size)
                    back_y = y_i - (vy * dt_seconds / voxel_size)
                    back_z = z_i - (vz * dt_seconds / voxel_size)
                    if sample_mode == "trilinear":
                        scratch[idx] = sample_array_trilinear(density, back_x, back_y, back_z, w, h, d)
                    else:
                        sx_i = int(round(back_x))
                        sy_i = int(round(back_y))
                        sz_i = int(round(back_z))
                        if 0 <= sx_i < w and 0 <= sy_i < h and 0 <= sz_i < d:
                            scratch[idx] = density[index_3d(sx_i, sy_i, sz_i, w, h)]
                        else:
                            scratch[idx] = 0.0

        for z_i in range(z_min, z_max + 1):
            for y_i in range(y_min, y_max + 1):
                for x_i in range(x_min, x_max + 1):
                    idx = index_3d(x_i, y_i, z_i, w, h)
                    center = float(scratch[idx])
                    neighbor_sum = 0.0
                    neighbor_count = 0
                    for ox, oy, oz in (
                        (-1, 0, 0),
                        (1, 0, 0),
                        (0, -1, 0),
                        (0, 1, 0),
                        (0, 0, -1),
                        (0, 0, 1),
                    ):
                        nx = x_i + ox
                        ny = y_i + oy
                        nz = z_i + oz
                        if 0 <= nx < w and 0 <= ny < h and 0 <= nz < d:
                            neighbor_sum += float(scratch[index_3d(nx, ny, nz, w, h)])
                            neighbor_count += 1
                    blurred = neighbor_sum / neighbor_count if neighbor_count else center
                    next_density[idx] = max(0.0, (center * (1.0 - diffusion) + blurred * diffusion) * dissipation)

        density, next_density = next_density, density

    fields["density"] = density
    refresh_field_stats(fields)
    output_distribution = density_distribution_diagnostics(spec, density)
    input_sum = float(input_distribution["density_sum"])
    output_sum = float(output_distribution["density_sum"])
    input_com = input_distribution["center_of_mass_world"]
    output_com = output_distribution["center_of_mass_world"]
    return {
        "enabled": True,
        "frames": frames,
        "variant_label": str(warmup.get("variant_label", "")),
        "dt_seconds": dt_seconds,
        "simulated_seconds": frames * dt_seconds,
        "diffusion": diffusion,
        "dissipation": dissipation,
        "buoyancy_gain": buoyancy_gain,
        "lateral_noise": lateral_noise,
        "sample_mode": sample_mode,
        "active_region": {
            "x_min": x_min,
            "x_max": x_max,
            "y_min": y_min,
            "y_max": y_max,
            "z_min": z_min,
            "z_max": z_max,
        },
        "input_density_stats": input_density_stats,
        "output_density_stats": fields["density_stats"],
        "input_density_distribution": input_distribution,
        "output_density_distribution": output_distribution,
        "diagnostic_delta": {
            "density_sum_delta": output_sum - input_sum,
            "density_sum_ratio": output_sum / input_sum if input_sum > 0.0 else 0.0,
            "center_of_mass_world_delta": {
                "x": float(output_com["x"]) - float(input_com["x"]),
                "y": float(output_com["y"]) - float(input_com["y"]),
                "z": float(output_com["z"]) - float(input_com["z"]),
            },
            "bottom_fraction_delta": (
                float(output_distribution["z_distribution"]["bottom_fraction"])
                - float(input_distribution["z_distribution"]["bottom_fraction"])
            ),
            "top_fraction_delta": (
                float(output_distribution["z_distribution"]["top_fraction"])
                - float(input_distribution["z_distribution"]["top_fraction"])
            ),
        },
    }


def write_vf3d(path: Path, spec: dict[str, Any], fields: dict[str, Any]) -> None:
    grid = spec["grid"]
    w = int(grid["w"])
    h = int(grid["h"])
    d = int(grid["d"])
    origin = [float(v) for v in grid["origin"]]
    scene_up = [float(v) for v in grid["scene_up"]]
    path.parent.mkdir(parents=True, exist_ok=True)
    header = HEADER_STRUCT.pack(
        VF3D_MAGIC,
        VF3D_VERSION,
        w,
        h,
        d,
        0.0,
        0,
        0.0,
        origin[0],
        origin[1],
        origin[2],
        float(grid["voxel_size"]),
        scene_up[0],
        scene_up[1],
        scene_up[2],
        0,
        0,
        0,
        0,
    )
    with path.open("wb") as handle:
        handle.write(header)
        for key in ("density", "velx", "vely", "velz", "pressure"):
            values = fields[key]
            if values.itemsize != 4:
                raise PresetError(f"{key} array must be float32")
            handle.write(values.tobytes())
        handle.write(fields["solid"])


def space_contract(spec: dict[str, Any]) -> dict[str, Any]:
    grid = spec["grid"]
    origin = [float(v) for v in grid["origin"]]
    scene_up = [float(v) for v in grid["scene_up"]]
    return {
        "version": 2,
        "space_mode": "3d",
        "axis_authority": "xyz",
        "grid_w": int(grid["w"]),
        "grid_h": int(grid["h"]),
        "grid_d": int(grid["d"]),
        "origin_x": origin[0],
        "origin_y": origin[1],
        "origin_z": origin[2],
        "voxel_size": float(grid["voxel_size"]),
        "scene_up_x": scene_up[0],
        "scene_up_y": scene_up[1],
        "scene_up_z": scene_up[2],
        "author_window_w": int(grid.get("author_window_w", 772)),
        "author_window_h": int(grid.get("author_window_h", 772)),
        "import_fit": float(grid.get("import_fit", 0.25)),
    }


def emit_sidecars(run_root: Path, label: str, spec: dict[str, Any]) -> tuple[Path, Path]:
    volume_root = run_root / "volume_frames" / label
    contract = space_contract(spec)
    manifest = {
        "preset": label,
        "frames": [
            {
                "frame_index": 0,
                "time_seconds": 0.0,
                "dt_seconds": 0.0,
                "path": "frame_000000.vf3d",
                "frame_contract": "vf3d",
            }
        ],
        "space_contract": contract,
        "manifest_version": 2,
        "run_name": spec["run_id"],
        "frame_contract": "vf3d",
        "space_mode": "3d",
        **contract,
        "solid_mask_crc32": 0,
    }
    scene_bundle = {
        "bundle_type": "physics_scene_bundle_v1",
        "bundle_version": 1,
        "profile": "physics",
        "fluid_source": {
            "kind": "manifest",
            "path": "manifest.json",
            "contract": "vf3d",
        },
        "scene_metadata": {
            "asset_mapping_profile": "physics_to_ray_v1",
            "space_contract": contract,
        },
        "notes": f"Single-frame authored VF3D initial-state preset: {spec['run_id']}.",
    }
    manifest_path = volume_root / "manifest.json"
    scene_bundle_path = volume_root / "scene_bundle.json"
    write_json(manifest_path, manifest)
    write_json(scene_bundle_path, scene_bundle)
    return manifest_path, scene_bundle_path


def emit_runtime_scene(run_root: Path, spec: dict[str, Any]) -> Path:
    scene_path = Path(spec["render_profile"]["scene_path"])
    if scene_path.is_file():
        scene_doc = read_json(scene_path)
    else:
        scene_doc = fallback_dark_backdrop_scene()
    out = run_root / "runtime_scenes" / RUNTIME_SCENE_FILENAME
    write_json(out, scene_doc)
    return out


def base_render_request(
    spec: dict[str, Any],
    scene_bundle_path: Path,
    runtime_scene_path: Path,
    run_root: Path,
    slug: str,
    *,
    debug_overlay: bool,
) -> dict[str, Any]:
    output_root = run_root / slug
    if debug_overlay:
        volume_settings = {
            "volume_density_scale": 1.15,
            "volume_density_gamma": 0.55,
            "volume_scatter_gain": 3.6,
            "volume_absorption_gain": 0.0,
            "volume_opacity_clamp": 2.0,
            "volume_albedo": {"r": 0.92, "g": 0.98, "b": 1.0},
        }
    else:
        volume_settings = {
            "volume_density_scale": 1.55,
            "volume_density_gamma": 0.48,
            "volume_scatter_gain": 8.8,
            "volume_absorption_gain": 0.055,
            "volume_opacity_clamp": 2.2,
            "volume_albedo": {"r": 0.96, "g": 0.93, "b": 0.88},
        }
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": f"{spec['run_id']}_{slug.removeprefix('ray_')}",
        "scene": {
            "runtime_scene_path": str(runtime_scene_path.resolve())
        },
        "volume": {
            "enabled": True,
            "source_kind": "scene_bundle",
            "source_path": str(scene_bundle_path.resolve()),
            "visible": True,
            "affects_lighting": True,
            "debug_overlay": debug_overlay,
        },
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": 480,
            "height": 300,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "integrator_3d": "direct_light",
        },
        "inspection": {
            "camera_zoom": 1.08,
            "camera_position": {"x": -0.35, "y": -4.15, "z": 1.22},
            "camera_look_at": {"x": 0.03, "y": -0.02, "z": 0.43},
            "environment_brightness": 0.0,
            "ambient_strength": 0.045,
            "environment_light_mode": "ambient",
            "background_brightness": 0.035,
            "background_color": {"r": 0.09, "g": 0.075, "b": 0.065},
            "top_fill_strength": 0.2,
            "light_intensity": 8.2,
            "light_radius": 0.2,
            "forward_decay": 128.0,
            "volume_step_scale": 0.75,
            "object_audit_enabled": True,
            "object_audit_max_dimension": 160,
            **volume_settings,
        },
        "output": {"root": str(output_root.resolve()), "overwrite": True},
        "progress": {
            "summary_path": str((output_root / "render_summary.json").resolve()),
            "progress_path": str((output_root / "render_progress.json").resolve()),
        },
    }


def png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(chunk_type)
    crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + chunk_type + payload + struct.pack(">I", crc)


def write_rgb_png(path: Path, width: int, height: int, pixels: list[tuple[int, int, int]]) -> None:
    if len(pixels) != width * height:
        raise PresetError("PNG pixel count mismatch")
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        row = pixels[y * width : (y + 1) * width]
        for r, g, b in row:
            raw.extend((clamp(int(r), 0, 255), clamp(int(g), 0, 255), clamp(int(b), 0, 255)))
    data = b"\x89PNG\r\n\x1a\n"
    data += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    data += png_chunk(b"IDAT", zlib.compress(bytes(raw), level=6))
    data += png_chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def write_gray_png(path: Path, width: int, height: int, gray: list[int]) -> None:
    pixels = [(v, v, v) for v in gray]
    write_rgb_png(path, width, height, pixels)


def projection_images(run_root: Path, spec: dict[str, Any], density: array) -> dict[str, str]:
    grid = spec["grid"]
    w = int(grid["w"])
    h = int(grid["h"])
    d = int(grid["d"])
    max_density = max((float(v) for v in density), default=0.0)
    scale = 255.0 / max(max_density, 1e-6)
    outputs: dict[str, str] = {}

    xy = [0.0] * (w * h)
    xz = [0.0] * (w * d)
    yz = [0.0] * (h * d)
    for z_i in range(d):
        for y_i in range(h):
            for x_i in range(w):
                value = float(density[index_3d(x_i, y_i, z_i, w, h)])
                xy[y_i * w + x_i] = max(xy[y_i * w + x_i], value)
                xz[z_i * w + x_i] = max(xz[z_i * w + x_i], value)
                yz[z_i * h + y_i] = max(yz[z_i * h + y_i], value)

    for name, width, height, values in (
        ("density_projection_xy.png", w, h, xy),
        ("density_projection_xz.png", w, d, xz),
        ("density_projection_yz.png", h, d, yz),
    ):
        gray = [int(clamp(math.sqrt(v * scale / 255.0) * 255.0, 0, 255)) for v in values]
        out = run_root / name
        write_gray_png(out, width, height, gray)
        outputs[name.removesuffix(".png")] = str(out)
    return outputs


def density_distribution_diagnostics(spec: dict[str, Any], density: array) -> dict[str, Any]:
    grid = spec["grid"]
    w = int(grid["w"])
    h = int(grid["h"])
    d = int(grid["d"])
    origin = [float(v) for v in grid["origin"]]
    voxel_size = float(grid["voxel_size"])
    total = 0.0
    weighted_x = 0.0
    weighted_y = 0.0
    weighted_z = 0.0
    max_density = 0.0
    max_index = 0
    bottom_sum = 0.0
    middle_sum = 0.0
    top_sum = 0.0
    z_bins = [0.0] * d
    y_bins = [0.0] * h
    threshold_01 = 0
    threshold_05 = 0
    threshold_10 = 0
    for z_i in range(d):
        for y_i in range(h):
            for x_i in range(w):
                idx = index_3d(x_i, y_i, z_i, w, h)
                value = float(density[idx])
                if value <= 0.0:
                    continue
                total += value
                weighted_x += value * x_i
                weighted_y += value * y_i
                weighted_z += value * z_i
                z_bins[z_i] += value
                y_bins[y_i] += value
                z_norm = z_i / max(d - 1, 1)
                if z_norm < 1.0 / 3.0:
                    bottom_sum += value
                elif z_norm < 2.0 / 3.0:
                    middle_sum += value
                else:
                    top_sum += value
                if value > max_density:
                    max_density = value
                    max_index = idx
                if value >= 0.1:
                    threshold_01 += 1
                if value >= 0.5:
                    threshold_05 += 1
                if value >= 1.0:
                    threshold_10 += 1

    if total > 0.0:
        cx = weighted_x / total
        cy = weighted_y / total
        cz = weighted_z / total
    else:
        cx = cy = cz = 0.0
    max_x = max_index % w
    max_y = (max_index // w) % h
    max_z = max_index // (w * h)
    max_z_sum = max(z_bins, default=0.0)
    peak_z = z_bins.index(max_z_sum) if max_z_sum > 0.0 else 0
    max_y_sum = max(y_bins, default=0.0)
    peak_y = y_bins.index(max_y_sum) if max_y_sum > 0.0 else 0

    def world(ix: float, iy: float, iz: float) -> dict[str, float]:
        return {
            "x": origin[0] + ix * voxel_size,
            "y": origin[1] + iy * voxel_size,
            "z": origin[2] + iz * voxel_size,
        }

    return {
        "density_sum": total,
        "center_of_mass_voxel": {"x": cx, "y": cy, "z": cz},
        "center_of_mass_world": world(cx, cy, cz),
        "max_density": max_density,
        "max_density_voxel": {"x": max_x, "y": max_y, "z": max_z},
        "max_density_world": world(max_x, max_y, max_z),
        "z_distribution": {
            "bottom_third_sum": bottom_sum,
            "middle_third_sum": middle_sum,
            "top_third_sum": top_sum,
            "bottom_fraction": bottom_sum / total if total > 0.0 else 0.0,
            "middle_fraction": middle_sum / total if total > 0.0 else 0.0,
            "top_fraction": top_sum / total if total > 0.0 else 0.0,
            "peak_z_slice": peak_z,
            "peak_z_slice_fraction": peak_z / max(d - 1, 1),
        },
        "y_distribution": {
            "peak_y_slice": peak_y,
            "peak_y_slice_fraction": peak_y / max(h - 1, 1),
        },
        "threshold_cell_counts": {
            "density_gte_0_1": threshold_01,
            "density_gte_0_5": threshold_05,
            "density_gte_1_0": threshold_10,
        },
    }


FONT: dict[str, list[str]] = {
    " ": ["000", "000", "000", "000", "000"],
    "-": ["000", "000", "111", "000", "000"],
    "_": ["000", "000", "000", "000", "111"],
    ".": ["000", "000", "000", "000", "010"],
    "/": ["001", "001", "010", "100", "100"],
    ":": ["000", "010", "000", "010", "000"],
}


def glyph(ch: str) -> list[str]:
    ch = ch.upper()
    if ch in FONT:
        return FONT[ch]
    if "A" <= ch <= "Z":
        patterns = {
            "A": ["010", "101", "111", "101", "101"],
            "B": ["110", "101", "110", "101", "110"],
            "C": ["011", "100", "100", "100", "011"],
            "D": ["110", "101", "101", "101", "110"],
            "E": ["111", "100", "110", "100", "111"],
            "F": ["111", "100", "110", "100", "100"],
            "G": ["011", "100", "101", "101", "011"],
            "H": ["101", "101", "111", "101", "101"],
            "I": ["111", "010", "010", "010", "111"],
            "J": ["001", "001", "001", "101", "010"],
            "K": ["101", "101", "110", "101", "101"],
            "L": ["100", "100", "100", "100", "111"],
            "M": ["101", "111", "111", "101", "101"],
            "N": ["101", "111", "111", "111", "101"],
            "O": ["010", "101", "101", "101", "010"],
            "P": ["110", "101", "110", "100", "100"],
            "Q": ["010", "101", "101", "111", "011"],
            "R": ["110", "101", "110", "101", "101"],
            "S": ["011", "100", "010", "001", "110"],
            "T": ["111", "010", "010", "010", "010"],
            "U": ["101", "101", "101", "101", "111"],
            "V": ["101", "101", "101", "101", "010"],
            "W": ["101", "101", "111", "111", "101"],
            "X": ["101", "101", "010", "101", "101"],
            "Y": ["101", "101", "010", "010", "010"],
            "Z": ["111", "001", "010", "100", "111"],
        }
        return patterns[ch]
    if "0" <= ch <= "9":
        patterns = {
            "0": ["111", "101", "101", "101", "111"],
            "1": ["010", "110", "010", "010", "111"],
            "2": ["111", "001", "111", "100", "111"],
            "3": ["111", "001", "111", "001", "111"],
            "4": ["101", "101", "111", "001", "001"],
            "5": ["111", "100", "111", "001", "111"],
            "6": ["111", "100", "111", "101", "111"],
            "7": ["111", "001", "001", "001", "001"],
            "8": ["111", "101", "111", "101", "111"],
            "9": ["111", "101", "111", "001", "111"],
        }
        return patterns[ch]
    return ["111", "001", "010", "000", "010"]


def draw_text(
    pixels: list[tuple[int, int, int]],
    width: int,
    height: int,
    x: int,
    y: int,
    text: str,
    color: tuple[int, int, int] = (238, 238, 238),
) -> None:
    cx = x
    for ch in text:
        pattern = glyph(ch)
        for row_i, row in enumerate(pattern):
            for col_i, value in enumerate(row):
                if value != "1":
                    continue
                px = cx + col_i
                py = y + row_i
                if 0 <= px < width and 0 <= py < height:
                    pixels[py * width + px] = color
        cx += 4


def read_bmp(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise PresetError(f"unsupported BMP file: {path}")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height_raw = struct.unpack_from("<i", data, 22)[0]
    planes = struct.unpack_from("<H", data, 26)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if planes != 1 or not ((bpp == 24 and compression == 0) or (bpp == 32 and compression in (0, 3))):
        raise PresetError(f"BMP must be uncompressed 24-bit or 32-bit bitfield: {path}")
    top_down = height_raw < 0
    height = abs(height_raw)
    bytes_per_pixel = bpp // 8
    row_stride = ((width * bytes_per_pixel + 3) // 4) * 4
    pixels: list[tuple[int, int, int]] = [(0, 0, 0)] * (width * height)
    for row in range(height):
        src_y = row if top_down else height - 1 - row
        base = offset + src_y * row_stride
        for x in range(width):
            pixel_base = base + x * bytes_per_pixel
            b, g, r = data[pixel_base : pixel_base + 3]
            pixels[row * width + x] = (r, g, b)
    return width, height, pixels


def scaled_image(
    width: int,
    height: int,
    pixels: list[tuple[int, int, int]],
    target_w: int,
    target_h: int,
) -> list[tuple[int, int, int]]:
    out: list[tuple[int, int, int]] = []
    for y in range(target_h):
        sy = min(height - 1, int(y * height / target_h))
        for x in range(target_w):
            sx = min(width - 1, int(x * width / target_w))
            out.append(pixels[sy * width + sx])
    return out


def paste(
    canvas: list[tuple[int, int, int]],
    canvas_w: int,
    canvas_h: int,
    x0: int,
    y0: int,
    width: int,
    height: int,
    pixels: list[tuple[int, int, int]],
) -> None:
    for y in range(height):
        dst_y = y0 + y
        if not 0 <= dst_y < canvas_h:
            continue
        for x in range(width):
            dst_x = x0 + x
            if 0 <= dst_x < canvas_w:
                canvas[dst_y * canvas_w + dst_x] = pixels[y * width + x]


def make_contact_sheet(run_root: Path) -> Path:
    cells = [
        ("density debug render", run_root / "ray_density_debug" / "frames" / "frame_0000.bmp"),
        ("production visibility tuned", run_root / "ray_production_visibility_tuned" / "frames" / "frame_0000.bmp"),
        ("density projection xy", run_root / "density_projection_xy.png"),
        ("density projection xz", run_root / "density_projection_xz.png"),
    ]
    cell_w = 480
    cell_h = 300
    label_h = 22
    margin = 12
    sheet_w = margin * 3 + cell_w * 2
    sheet_h = margin * 3 + (cell_h + label_h) * 2
    canvas = [(18, 18, 18)] * (sheet_w * sheet_h)
    for idx, (label, path) in enumerate(cells):
        row = idx // 2
        col = idx % 2
        x0 = margin + col * (cell_w + margin)
        y0 = margin + row * (cell_h + label_h + margin)
        if path.suffix.lower() == ".bmp":
            width, height, pixels = read_bmp(path)
        else:
            # The generated projection PNGs are also available from source data,
            # but keeping contact-sheet input as file paths makes missing
            # artifacts fail loudly. Decode only our simple RGB PNG form.
            width, height, pixels = read_simple_png(path)
        scaled = scaled_image(width, height, pixels, cell_w, cell_h)
        paste(canvas, sheet_w, sheet_h, x0, y0 + label_h, cell_w, cell_h, scaled)
        draw_text(canvas, sheet_w, sheet_h, x0, y0 + 6, label)
    out = run_root / "vf3d_initial_state_contact_labeled.png"
    write_rgb_png(out, sheet_w, sheet_h, canvas)
    return out


def read_simple_png(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise PresetError(f"unsupported PNG file: {path}")
    pos = 8
    width = height = 0
    color_type = -1
    payload = bytearray()
    while pos < len(data):
        size = struct.unpack_from(">I", data, pos)[0]
        chunk_type = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + size]
        pos += 12 + size
        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, _, _, _ = struct.unpack(">IIBBBBB", chunk)
            if bit_depth != 8 or color_type != 2:
                raise PresetError(f"unsupported PNG format: {path}")
        elif chunk_type == b"IDAT":
            payload.extend(chunk)
        elif chunk_type == b"IEND":
            break
    raw = zlib.decompress(bytes(payload))
    stride = width * 3
    pixels: list[tuple[int, int, int]] = []
    p = 0
    for _ in range(height):
        filter_type = raw[p]
        p += 1
        if filter_type != 0:
            raise PresetError(f"unsupported PNG filter: {path}")
        row = raw[p : p + stride]
        p += stride
        for x in range(width):
            r, g, b = row[x * 3 : x * 3 + 3]
            pixels.append((r, g, b))
    return width, height, pixels


def render_request(request_path: Path) -> None:
    render_cli = next((path for path in RENDER_CLI_CANDIDATES if path.is_file()), None)
    if not render_cli:
        candidates = ", ".join(str(path) for path in RENDER_CLI_CANDIDATES)
        raise PresetError(f"RayTracing render CLI missing; checked: {candidates}")
    command = [str(render_cli), "--request", str(request_path), "--render"]
    subprocess.run(command, cwd=str(WORKSPACE_ROOT), check=True)


def convert_render_bmp(run_root: Path, slug: str) -> Path:
    bmp = run_root / slug / "frames" / "frame_0000.bmp"
    width, height, pixels = read_bmp(bmp)
    out = run_root / slug / "frame_0000.png"
    write_rgb_png(out, width, height, pixels)
    return out


def generate_preset(spec: dict[str, Any], *, render: bool) -> dict[str, Any]:
    validate_spec(spec)
    run_id = spec["run_id"]
    label = preset_label(run_id)
    run_root = Path(spec["output"]["root"]).resolve()
    volume_root = run_root / "volume_frames" / label
    vf3d_path = volume_root / "frame_000000.vf3d"

    fields = build_fields(spec)
    warmup_summary = apply_warmup_preview(spec, fields)
    write_vf3d(vf3d_path, spec, fields)
    manifest_path, scene_bundle_path = emit_sidecars(run_root, label, spec)
    runtime_scene_path = emit_runtime_scene(run_root, spec)
    projection_paths = projection_images(run_root, spec, fields["density"])

    debug_request = base_render_request(
        spec,
        scene_bundle_path,
        runtime_scene_path,
        run_root,
        "ray_density_debug",
        debug_overlay=True,
    )
    production_request = base_render_request(
        spec,
        scene_bundle_path,
        runtime_scene_path,
        run_root,
        "ray_production_visibility_tuned",
        debug_overlay=False,
    )
    debug_request_path = run_root / "ray_density_debug_request.json"
    production_request_path = run_root / "ray_production_visibility_tuned_request.json"
    write_json(debug_request_path, debug_request)
    write_json(production_request_path, production_request)

    summary = {
        "schema_version": "vf3d_initial_state_generation_summary_v1",
        "run_id": run_id,
        "preset_label": label,
        "preset": spec["preset"],
        "grid": spec["grid"],
        "cell_count": int(spec["grid"]["w"]) * int(spec["grid"]["h"]) * int(spec["grid"]["d"]),
        "active_density_cells": fields["active_density_cells"],
        "active_density_fraction": fields["active_density_cells"]
        / (int(spec["grid"]["w"]) * int(spec["grid"]["h"]) * int(spec["grid"]["d"])),
        "density_stats": fields["density_stats"],
        "velocity_magnitude_stats": fields["velocity_stats"],
        "warmup_preview": warmup_summary,
        "vf3d_size_bytes": vf3d_path.stat().st_size,
        "vf3d_path": str(vf3d_path),
        "manifest_path": str(manifest_path),
        "scene_bundle_path": str(scene_bundle_path),
        "runtime_scene_path": str(runtime_scene_path),
        "ray_density_debug_request_path": str(debug_request_path),
        "ray_production_visibility_tuned_request_path": str(production_request_path),
        "projection_paths": projection_paths,
        "stable_iteration_policy": "Regenerate this same frame_000000.vf3d in place while tuning the preset root.",
    }

    if render:
        render_request(debug_request_path)
        render_request(production_request_path)
        summary["ray_density_debug_png"] = str(convert_render_bmp(run_root, "ray_density_debug"))
        summary["ray_production_visibility_tuned_png"] = str(
            convert_render_bmp(run_root, "ray_production_visibility_tuned")
        )
        summary["contact_sheet_path"] = str(make_contact_sheet(run_root))

    write_json(run_root / "preset_spec.json", spec)
    write_json(run_root / "generation_summary.json", summary)
    return summary


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--preset",
        choices=(
            "single_turbulent_cloud_v1",
            "atmospheric_layer_v1",
            "all-first-two",
            "single_turbulent_cloud_v2",
            "atmospheric_layer_v2",
            "all-v2",
            "cloud_bank_v1",
            "rolling_fog_layer_v1",
            "room_haze_v1",
            "low_floor_fog_v1",
            "mist_patch_v1",
            "plume_column_v1",
            "plume_column_v2",
            "plume_column_v3",
            "all-family-expansion-v1",
        ),
        required=True,
    )
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--spec", type=Path, help="Optional preset spec JSON to run directly.")
    parser.add_argument("--render", action="store_true", help="Run RayTracing renders and contact sheet generation.")
    parser.add_argument("--warmup-frames", type=int, default=0, help="Run cheap local warm-up preview frames before writing the final VF3D.")
    parser.add_argument("--warmup-label", default="", help="Optional suffix for warm-up variant run ids.")
    parser.add_argument("--warmup-dt", type=float, default=0.012, help="Warm-up preview timestep in seconds.")
    parser.add_argument("--warmup-diffusion", type=float, default=0.075, help="Warm-up density diffusion blend per frame.")
    parser.add_argument("--warmup-dissipation", type=float, default=0.996, help="Warm-up density dissipation per frame.")
    parser.add_argument("--warmup-buoyancy", type=float, default=0.10, help="Warm-up density-driven upward velocity gain.")
    parser.add_argument("--warmup-lateral-noise", type=float, default=0.0, help="Warm-up procedural lateral drift strength.")
    parser.add_argument("--warmup-active-pad", type=int, default=None, help="Voxel padding around active density bounds for warm-up preview.")
    parser.add_argument("--warmup-sample-mode", choices=("nearest", "trilinear"), default="nearest", help="Warm-up advection sampler; nearest is the cheap local preview default.")
    return parser


def apply_cli_warmup(spec: dict[str, Any], args: argparse.Namespace) -> dict[str, Any]:
    frames = int(args.warmup_frames)
    if frames <= 0:
        return spec
    base_run_id = str(spec["run_id"])
    variant_label = str(args.warmup_label or "").strip().replace(" ", "_")
    suffix = f"_{variant_label}" if variant_label else ""
    run_id = f"{base_run_id}_warmup_{frames:04d}{suffix}"
    warmed = json.loads(json.dumps(spec))
    warmed["run_id"] = run_id
    warmed["output"]["root"] = str(Path(args.output_root) / run_id)
    warmed["warmup_preview"] = {
        "schema_version": "vf3d_warmup_preview_v1",
        "source_run_id": base_run_id,
        "variant_label": variant_label,
        "frames": frames,
        "dt_seconds": float(args.warmup_dt),
        "diffusion": float(args.warmup_diffusion),
        "dissipation": float(args.warmup_dissipation),
        "buoyancy_gain": float(args.warmup_buoyancy),
        "lateral_noise": float(args.warmup_lateral_noise),
        "sample_mode": str(args.warmup_sample_mode),
        "mode": "cheap_local_semi_lagrangian_density_preview",
        "output_policy": "write final warmed state only as frame_000000.vf3d",
    }
    if args.warmup_active_pad is not None:
        warmed["warmup_preview"]["active_region_pad"] = int(args.warmup_active_pad)
    return warmed


def spec_for_name(name: str, seed: int, output_root: Path) -> dict[str, Any]:
    if name in ("single_turbulent_cloud_v1", "single_turbulent_cloud_v2"):
        return default_spec(name, "single_turbulent_cloud", seed, output_root)
    if name in ("atmospheric_layer_v1", "atmospheric_layer_v2"):
        return default_spec(name, "atmospheric_layer", seed, output_root)
    if name == "cloud_bank_v1":
        return default_spec(name, "cloud_bank", seed, output_root)
    if name == "rolling_fog_layer_v1":
        return default_spec(name, "rolling_fog_layer", seed, output_root)
    if name == "room_haze_v1":
        return default_spec(name, "atmospheric_layer", seed, output_root)
    if name == "low_floor_fog_v1":
        return default_spec(name, "rolling_fog_layer", seed, output_root)
    if name == "mist_patch_v1":
        return default_spec(name, "mist_patch", seed, output_root)
    if name in ("plume_column_v1", "plume_column_v2", "plume_column_v3"):
        return default_spec(name, "plume_column", seed, output_root)
    raise PresetError(f"unsupported preset name: {name}")


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()
    try:
        summaries = []
        if args.spec:
            summaries.append(generate_preset(read_json(args.spec), render=args.render))
        elif args.preset == "all-first-two":
            for name in ("single_turbulent_cloud_v1", "atmospheric_layer_v1"):
                summaries.append(generate_preset(apply_cli_warmup(spec_for_name(name, args.seed, args.output_root), args), render=args.render))
        elif args.preset == "all-v2":
            for name in ("single_turbulent_cloud_v2", "atmospheric_layer_v2"):
                summaries.append(generate_preset(apply_cli_warmup(spec_for_name(name, args.seed, args.output_root), args), render=args.render))
        elif args.preset == "all-family-expansion-v1":
            for name in (
                "cloud_bank_v1",
                "rolling_fog_layer_v1",
                "room_haze_v1",
                "low_floor_fog_v1",
                "mist_patch_v1",
                "plume_column_v3",
            ):
                summaries.append(generate_preset(apply_cli_warmup(spec_for_name(name, args.seed, args.output_root), args), render=args.render))
        else:
            summaries.append(generate_preset(apply_cli_warmup(spec_for_name(args.preset, args.seed, args.output_root), args), render=args.render))
    except (PresetError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"vf3d_initial_state_preset_tool: error: {exc}\n")

    for summary in summaries:
        print(json.dumps({
            "run_id": summary["run_id"],
            "root": str(Path(summary["vf3d_path"]).parents[2]),
            "active_density_cells": summary["active_density_cells"],
            "density_max": summary["density_stats"]["max"],
            "velocity_max": summary["velocity_magnitude_stats"]["max"],
        }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
