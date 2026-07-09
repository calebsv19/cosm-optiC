#!/usr/bin/env python3
"""Resolve scene-local atmosphere declarations into VF3D preset specs."""

from __future__ import annotations

import argparse
import copy
import json
import math
import re
from pathlib import Path
from typing import Any

from vf3d_initial_state_preset_tool import PresetError, generate_preset, spec_for_name, write_json


DEFAULT_OUTPUT_ROOT = (
    Path(__file__).resolve().parents[2]
    / "_private_workspace_artifacts"
    / "ambient_air_probe"
    / "vf3d_scene_atmosphere_resolver"
)

CORE_MODES = ("small_patch", "scene_fit", "scene_overscan")

MODE_CONFIGS = {
    "small_patch": {
        "size_scale": [0.42, 0.42, 0.36],
        "center_offset": [0.0, -0.08, -0.10],
        "density_scale": 0.78,
        "support_divisor": 1.44,
        "rotation_degrees": [0.0, 0.0, 0.0],
    },
    "scene_fit": {
        "size_scale": [1.0, 1.0, 1.0],
        "center_offset": [0.0, 0.0, 0.0],
        "density_scale": 0.64,
        "support_divisor": 1.56,
        "rotation_degrees": [0.0, 0.0, 0.0],
    },
    "scene_overscan": {
        "size_scale": [1.38, 1.38, 1.14],
        "center_offset": [0.0, 0.0, 0.04],
        "density_scale": 0.54,
        "support_divisor": 1.62,
        "rotation_degrees": [0.0, 0.0, 0.0],
    },
    "scene_fit_shift_left": {
        "size_scale": [1.0, 1.0, 1.0],
        "center_offset": [-0.22, 0.0, 0.0],
        "density_scale": 0.64,
        "support_divisor": 1.56,
        "rotation_degrees": [0.0, 0.0, 0.0],
    },
    "scene_fit_shift_right": {
        "size_scale": [1.0, 1.0, 1.0],
        "center_offset": [0.22, 0.0, 0.0],
        "density_scale": 0.64,
        "support_divisor": 1.56,
        "rotation_degrees": [0.0, 0.0, 0.0],
    },
    "scene_fit_shift_up": {
        "size_scale": [1.0, 1.0, 1.0],
        "center_offset": [0.0, 0.0, 0.16],
        "density_scale": 0.64,
        "support_divisor": 1.56,
        "rotation_degrees": [0.0, 0.0, 0.0],
    },
    "scene_fit_rotated_z25": {
        "size_scale": [1.0, 1.0, 1.0],
        "center_offset": [0.0, 0.0, 0.0],
        "density_scale": 0.64,
        "support_divisor": 1.56,
        "rotation_degrees": [0.0, 0.0, 25.0],
    },
}


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text())


def slugify(value: str) -> str:
    slug = re.sub(r"[^A-Za-z0-9]+", "_", value.strip()).strip("_").lower()
    return slug or "scene"


def vec3_from_mapping(value: Any, *, default: tuple[float, float, float] | None = None) -> tuple[float, float, float]:
    if isinstance(value, dict):
        return (
            float(value.get("x", 0.0)),
            float(value.get("y", 0.0)),
            float(value.get("z", 0.0)),
        )
    if isinstance(value, list) and len(value) == 3:
        return (float(value[0]), float(value[1]), float(value[2]))
    if default is not None:
        return default
    raise ValueError("expected vec3 object or array")


def add(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def mul(a: tuple[float, float, float], scalar: float) -> tuple[float, float, float]:
    return (a[0] * scalar, a[1] * scalar, a[2] * scalar)


def mul_components(
    a: tuple[float, float, float],
    b: tuple[float, float, float],
) -> tuple[float, float, float]:
    return (a[0] * b[0], a[1] * b[1], a[2] * b[2])


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


def include_point(bounds: dict[str, list[float]], point: tuple[float, float, float]) -> None:
    for i in range(3):
        bounds["min"][i] = min(bounds["min"][i], point[i])
        bounds["max"][i] = max(bounds["max"][i], point[i])


def plane_points(obj: dict[str, Any]) -> list[tuple[float, float, float]]:
    primitive = obj.get("primitive") if isinstance(obj.get("primitive"), dict) else {}
    frame = primitive.get("frame") if isinstance(primitive.get("frame"), dict) else {}
    transform = obj.get("transform") if isinstance(obj.get("transform"), dict) else {}
    scale = vec3_from_mapping(transform.get("scale"), default=(1.0, 1.0, 1.0))
    origin = vec3_from_mapping(frame.get("origin") or transform.get("position"), default=(0.0, 0.0, 0.0))
    axis_u = vec3_from_mapping(frame.get("axis_u"), default=(1.0, 0.0, 0.0))
    axis_v = vec3_from_mapping(frame.get("axis_v"), default=(0.0, 1.0, 0.0))
    half_u = 0.5 * float(primitive.get("width", 0.0)) * scale[0]
    half_v = 0.5 * float(primitive.get("height", 0.0)) * scale[1]
    if half_u <= 0.0 or half_v <= 0.0:
        return []
    points = []
    for su in (-1.0, 1.0):
        for sv in (-1.0, 1.0):
            points.append(add(add(origin, mul(axis_u, su * half_u)), mul(axis_v, sv * half_v)))
    return points


def rect_prism_points(obj: dict[str, Any]) -> list[tuple[float, float, float]]:
    primitive = obj.get("primitive") if isinstance(obj.get("primitive"), dict) else {}
    transform = obj.get("transform") if isinstance(obj.get("transform"), dict) else {}
    center = vec3_from_mapping(transform.get("position"), default=(0.0, 0.0, 0.0))
    scale = vec3_from_mapping(transform.get("scale"), default=(1.0, 1.0, 1.0))
    rotation = vec3_from_mapping(transform.get("rotation"), default=(0.0, 0.0, 0.0))
    half = (
        0.5 * float(primitive.get("width", 0.0)) * scale[0],
        0.5 * float(primitive.get("height", 0.0)) * scale[1],
        0.5 * float(primitive.get("depth", 0.0)) * scale[2],
    )
    if min(half) <= 0.0:
        return []
    points = []
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            for sz in (-1.0, 1.0):
                local = (sx * half[0], sy * half[1], sz * half[2])
                points.append(add(center, rotate_xyz_degrees(local, rotation)))
    return points


def mesh_points(obj: dict[str, Any], scene_dir: Path) -> list[tuple[float, float, float]]:
    extensions = obj.get("extensions") if isinstance(obj.get("extensions"), dict) else {}
    scene_iteration = extensions.get("scene_iteration") if isinstance(extensions.get("scene_iteration"), dict) else {}
    object_add_mesh = (
        scene_iteration.get("object_add_mesh") if isinstance(scene_iteration.get("object_add_mesh"), dict) else {}
    )
    relpath = object_add_mesh.get("payload_relpath")
    if not isinstance(relpath, str) or not relpath:
        return []
    payload_path = (scene_dir / relpath).resolve()
    if not payload_path.exists():
        return []
    runtime_doc = read_json(payload_path)
    local_bounds = runtime_doc.get("local_bounds") if isinstance(runtime_doc.get("local_bounds"), dict) else {}
    local_min = vec3_from_mapping(local_bounds.get("min"), default=(0.0, 0.0, 0.0))
    local_max = vec3_from_mapping(local_bounds.get("max"), default=(0.0, 0.0, 0.0))
    transform = obj.get("transform") if isinstance(obj.get("transform"), dict) else {}
    position = vec3_from_mapping(transform.get("position"), default=(0.0, 0.0, 0.0))
    scale = vec3_from_mapping(transform.get("scale"), default=(1.0, 1.0, 1.0))
    rotation = vec3_from_mapping(transform.get("rotation"), default=(0.0, 0.0, 0.0))
    pivot_policy = str(transform.get("pivot_policy") or "authored_origin")
    pivot = (0.0, 0.0, 0.0)
    if pivot_policy == "bounds_center":
        pivot = mul(add(local_min, local_max), 0.5)
    points = []
    for sx in (local_min[0], local_max[0]):
        for sy in (local_min[1], local_max[1]):
            for sz in (local_min[2], local_max[2]):
                local = mul_components(sub((sx, sy, sz), pivot), scale)
                points.append(add(position, rotate_xyz_degrees(local, rotation)))
    return points


def object_points(obj: dict[str, Any], scene_dir: Path) -> list[tuple[float, float, float]]:
    object_type = str(obj.get("object_type") or "")
    primitive = obj.get("primitive") if isinstance(obj.get("primitive"), dict) else {}
    primitive_kind = str(primitive.get("kind") or "")
    if object_type == "plane_primitive" or primitive_kind == "plane_primitive":
        return plane_points(obj)
    if object_type == "rect_prism_primitive" or primitive_kind in {"rect_prism_primitive", "box"}:
        return rect_prism_points(obj)
    if object_type == "mesh_asset_instance":
        return mesh_points(obj, scene_dir)
    return []


def scene_object_bounds(scene: dict[str, Any], scene_dir: Path) -> dict[str, Any]:
    objects = scene.get("objects")
    if not isinstance(objects, list):
        raise PresetError("runtime scene has no objects array")
    bounds = {"min": [math.inf, math.inf, math.inf], "max": [-math.inf, -math.inf, -math.inf]}
    included_objects = []
    for obj in objects:
        if not isinstance(obj, dict):
            continue
        points = object_points(obj, scene_dir)
        if not points:
            continue
        included_objects.append(str(obj.get("object_id") or ""))
        for point in points:
            include_point(bounds, point)
    if not included_objects:
        raise PresetError("could not resolve scene bounds from runtime scene objects")
    min_v = bounds["min"]
    max_v = bounds["max"]
    size = [max_v[i] - min_v[i] for i in range(3)]
    if min(size) <= 0.0:
        raise PresetError(f"resolved scene bounds are degenerate: {bounds}")
    return {
        "source": "runtime_scene_objects",
        "min": min_v,
        "max": max_v,
        "center": [(min_v[i] + max_v[i]) * 0.5 for i in range(3)],
        "size": size,
        "included_object_ids": included_objects,
    }


def resolved_mode_bounds(
    scene_bounds: dict[str, Any],
    mode: str,
    *,
    config_override: dict[str, Any] | None = None,
) -> dict[str, Any]:
    config = config_override or MODE_CONFIGS[mode]
    source_center = [float(v) for v in scene_bounds["center"]]
    source_size = [float(v) for v in scene_bounds["size"]]
    size = [source_size[i] * float(config["size_scale"][i]) for i in range(3)]
    offset = [source_size[i] * float(config["center_offset"][i]) for i in range(3)]
    center = [source_center[i] + offset[i] for i in range(3)]
    return {
        "mode": mode,
        "min": [center[i] - size[i] * 0.5 for i in range(3)],
        "max": [center[i] + size[i] * 0.5 for i in range(3)],
        "center": center,
        "size": size,
        "size_scale": list(config["size_scale"]),
        "center_offset": offset,
        "rotation_degrees": list(config["rotation_degrees"]),
    }


def apply_bounds_to_mist_spec(
    spec: dict[str, Any],
    *,
    mode: str,
    scene_path: Path,
    scene_id: str,
    scene_bounds: dict[str, Any],
    atmosphere_bounds: dict[str, Any],
    config_override: dict[str, Any] | None = None,
    declaration_override: dict[str, Any] | None = None,
) -> dict[str, Any]:
    config = config_override or MODE_CONFIGS[mode]
    out = copy.deepcopy(spec)
    size = [float(v) for v in atmosphere_bounds["size"]]
    center = [float(v) for v in atmosphere_bounds["center"]]
    support_divisor = float(config["support_divisor"])
    extent = [max(0.05, size[i] * 0.5 / support_divisor) for i in range(3)]

    params = out["preset"]["parameters"]
    params["center"] = center
    params["extent_xyz"] = extent
    params["density_scale"] = float(config["density_scale"])
    params["rotation_degrees"] = list(config["rotation_degrees"])
    if any(abs(float(value)) > 0.0 for value in config["rotation_degrees"]):
        params["drift"] = list(rotate_xyz_degrees(tuple(float(v) for v in params["drift"]), tuple(config["rotation_degrees"])))

    grid = out["grid"]
    w = int(grid["w"])
    h = int(grid["h"])
    d = int(grid["d"])
    grid_padding = 1.08
    voxel_size = max(
        size[0] * grid_padding / w,
        size[1] * grid_padding / h,
        size[2] * grid_padding / d,
        0.001,
    )
    grid["voxel_size"] = voxel_size
    grid["origin"] = [
        center[0] - w * voxel_size * 0.5,
        center[1] - h * voxel_size * 0.5,
        center[2] - d * voxel_size * 0.5,
    ]

    declaration = {
        "preset": "mist_patch_v1",
        "fit": "scene_bounds",
        "bounds_mode": mode,
        "density_scale": params["density_scale"],
        "rotation_degrees": params["rotation_degrees"],
        "cache_policy": "reuse_by_spec_hash",
    }
    if declaration_override:
        declaration.update(declaration_override)

    out["scene_atmosphere"] = {
        "schema_version": "scene_atmosphere_initial_state_resolver_v1",
        "scene_id": scene_id,
        "runtime_scene_path": str(scene_path),
        "declaration": declaration,
        "source_scene_bounds": scene_bounds,
        "resolved_bounds": atmosphere_bounds,
        "support_divisor": support_divisor,
    }
    return out


def resolve_specs(
    runtime_scene_path: Path,
    *,
    output_root: Path,
    seed: int,
    mode: str,
    preset: str,
) -> dict[str, Any]:
    scene = read_json(runtime_scene_path)
    scene_id = str(scene.get("scene_id") or runtime_scene_path.parent.name)
    scene_slug = slugify(scene_id)
    scene_bounds = scene_object_bounds(scene, runtime_scene_path.parent)
    modes = CORE_MODES if mode == "all" else (mode,)
    specs = []
    spec_dir = output_root / scene_slug / "resolved_specs"
    generated_root = output_root / scene_slug / "generated"
    for one_mode in modes:
        atmosphere_bounds = resolved_mode_bounds(scene_bounds, one_mode)
        run_id = f"{scene_slug}_{preset}_{one_mode}_seed{seed}"
        spec = spec_for_name(preset, seed, generated_root)
        spec["run_id"] = run_id
        spec["output"]["root"] = str(generated_root / run_id)
        resolved = apply_bounds_to_mist_spec(
            spec,
            mode=one_mode,
            scene_path=runtime_scene_path,
            scene_id=scene_id,
            scene_bounds=scene_bounds,
            atmosphere_bounds=atmosphere_bounds,
        )
        spec_path = spec_dir / f"{run_id}.json"
        write_json(spec_path, resolved)
        specs.append(
            {
                "mode": one_mode,
                "run_id": run_id,
                "spec_path": str(spec_path),
                "output_root": resolved["output"]["root"],
                "center": resolved["preset"]["parameters"]["center"],
                "extent_xyz": resolved["preset"]["parameters"]["extent_xyz"],
                "grid": resolved["grid"],
                "resolved_bounds": atmosphere_bounds,
            }
        )

    return {
        "schema_version": "vf3d_scene_atmosphere_resolver_summary_v1",
        "runtime_scene_path": str(runtime_scene_path),
        "scene_id": scene_id,
        "preset": preset,
        "seed": seed,
        "scene_bounds": scene_bounds,
        "specs": specs,
    }


def resolve_transform_spec(
    runtime_scene_path: Path,
    *,
    output_root: Path,
    seed: int,
    preset: str,
    base_mode: str,
    size_scale: list[float],
    center_offset: list[float],
    rotation_degrees: list[float],
    density_scale: float,
    run_suffix: str,
) -> dict[str, Any]:
    if base_mode not in MODE_CONFIGS:
        raise PresetError(f"unsupported atmosphere base mode: {base_mode}")
    if preset != "mist_patch_v1":
        raise PresetError("resolve_transform_spec currently supports mist_patch_v1 only")
    scene = read_json(runtime_scene_path)
    scene_id = str(scene.get("scene_id") or runtime_scene_path.parent.name)
    scene_slug = slugify(scene_id)
    scene_bounds = scene_object_bounds(scene, runtime_scene_path.parent)
    config = copy.deepcopy(MODE_CONFIGS[base_mode])
    config["size_scale"] = [float(value) for value in size_scale]
    config["center_offset"] = [float(value) for value in center_offset]
    config["rotation_degrees"] = [float(value) for value in rotation_degrees]
    config["density_scale"] = float(density_scale)

    atmosphere_bounds = resolved_mode_bounds(
        scene_bounds,
        base_mode,
        config_override=config,
    )
    run_id = f"{scene_slug}_{preset}_{base_mode}_{run_suffix}_seed{seed}"
    generated_root = output_root / scene_slug / "generated"
    spec = spec_for_name(preset, seed, generated_root)
    spec["run_id"] = run_id
    spec["output"]["root"] = str(generated_root / run_id)
    resolved = apply_bounds_to_mist_spec(
        spec,
        mode=base_mode,
        scene_path=runtime_scene_path,
        scene_id=scene_id,
        scene_bounds=scene_bounds,
        atmosphere_bounds=atmosphere_bounds,
        config_override=config,
        declaration_override={
            "transform": {
                "bounds_scale": list(config["size_scale"]),
                "center_offset": list(config["center_offset"]),
                "rotation_degrees": list(config["rotation_degrees"]),
            }
        },
    )
    spec_dir = output_root / scene_slug / "resolved_specs"
    spec_path = spec_dir / f"{run_id}.json"
    write_json(spec_path, resolved)
    return {
        "schema_version": "vf3d_scene_atmosphere_transform_resolver_v1",
        "runtime_scene_path": str(runtime_scene_path),
        "scene_id": scene_id,
        "preset": preset,
        "seed": seed,
        "base_mode": base_mode,
        "run_id": run_id,
        "spec_path": str(spec_path),
        "output_root": resolved["output"]["root"],
        "center": resolved["preset"]["parameters"]["center"],
        "extent_xyz": resolved["preset"]["parameters"]["extent_xyz"],
        "grid": resolved["grid"],
        "resolved_bounds": atmosphere_bounds,
        "transform": {
            "bounds_scale": list(config["size_scale"]),
            "center_offset": list(config["center_offset"]),
            "rotation_degrees": list(config["rotation_degrees"]),
            "density_scale": float(config["density_scale"]),
        },
    }


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-scene", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--seed", type=int, default=12)
    parser.add_argument("--preset", choices=("mist_patch_v1",), default="mist_patch_v1")
    parser.add_argument("--mode", choices=(*MODE_CONFIGS.keys(), "all"), default="all")
    parser.add_argument("--generate", action="store_true", help="Generate VF3D artifacts for the resolved specs.")
    return parser


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()
    try:
        runtime_scene_path = args.runtime_scene.resolve()
        if not runtime_scene_path.exists():
            raise PresetError(f"runtime scene not found: {runtime_scene_path}")
        summary = resolve_specs(
            runtime_scene_path,
            output_root=args.output_root,
            seed=args.seed,
            mode=args.mode,
            preset=args.preset,
        )
        if args.generate:
            for spec_entry in summary["specs"]:
                generated = generate_preset(read_json(Path(spec_entry["spec_path"])), render=False)
                spec_entry["generation_summary_path"] = str(Path(generated["vf3d_path"]).parents[2] / "generation_summary.json")
                spec_entry["vf3d_path"] = generated["vf3d_path"]
                spec_entry["active_density_cells"] = generated["active_density_cells"]
                spec_entry["density_max"] = generated["density_stats"]["max"]
                spec_entry["velocity_max"] = generated["velocity_magnitude_stats"]["max"]

        scene_slug = slugify(summary["scene_id"])
        summary_path = args.output_root / scene_slug / "resolver_summary.json"
        write_json(summary_path, summary)
        print(json.dumps({"summary_path": str(summary_path), **summary}, indent=2))
        return 0
    except PresetError as exc:
        parser.exit(1, f"vf3d_scene_atmosphere_resolver: error: {exc}\n")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
