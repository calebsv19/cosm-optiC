#!/usr/bin/env python3
"""Generate MRT0 mesh_asset_runtime_v1 sphere fixtures."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path


RAY_TRACING_ROOT = Path(__file__).resolve().parents[1]

SPHERE_SPECS = (
    ("asset_sphere_8x4", 8, 4, "low"),
    ("asset_sphere_16x8", 16, 8, "medium"),
    ("asset_sphere_32x16", 32, 16, "high"),
)

PRESSURE_SPHERE_SPECS = (
    ("asset_sphere_64x32", 64, 32, "pressure"),
)

MRT8_PRESSURE_SPHERE_SPECS = (
    ("asset_sphere_128x64", 128, 64, "pressure_mrt8"),
)

MRT10_PRESSURE_SPHERE_SPECS = (
    ("asset_sphere_256x128", 256, 128, "pressure_mrt10"),
)


def rounded(value: float) -> float:
    if abs(value) < 0.0000000005:
        return 0.0
    return round(value, 9)


def vertex(x: float, y: float, z: float) -> dict[str, float]:
    return {"x": rounded(x), "y": rounded(y), "z": rounded(z)}


def make_sphere(asset_id: str, segments: int, rings: int, fidelity: str) -> dict:
    if segments < 3:
        raise ValueError("segments must be >= 3")
    if rings < 2:
        raise ValueError("rings must be >= 2")

    vertices: list[dict[str, float]] = [vertex(0.0, 0.0, 1.0)]
    ring_indices: list[list[int]] = []

    for ring in range(1, rings):
        theta = math.pi * ring / rings
        z = math.cos(theta)
        radius = math.sin(theta)
        indices: list[int] = []
        for segment in range(segments):
            phi = 2.0 * math.pi * segment / segments
            indices.append(len(vertices))
            vertices.append(vertex(radius * math.cos(phi), radius * math.sin(phi), z))
        ring_indices.append(indices)

    south_pole = len(vertices)
    vertices.append(vertex(0.0, 0.0, -1.0))

    triangles: list[dict[str, int | str]] = []
    group_id = "sphere_shell"

    first_ring = ring_indices[0]
    for segment in range(segments):
        triangles.append(
            {
                "a": 0,
                "b": first_ring[segment],
                "c": first_ring[(segment + 1) % segments],
                "surface_group_id": group_id,
            }
        )

    for ring in range(len(ring_indices) - 1):
        upper = ring_indices[ring]
        lower = ring_indices[ring + 1]
        for segment in range(segments):
            u0 = upper[segment]
            u1 = upper[(segment + 1) % segments]
            l0 = lower[segment]
            l1 = lower[(segment + 1) % segments]
            triangles.append({"a": u0, "b": l0, "c": l1, "surface_group_id": group_id})
            triangles.append({"a": u0, "b": l1, "c": u1, "surface_group_id": group_id})

    last_ring = ring_indices[-1]
    for segment in range(segments):
        triangles.append(
            {
                "a": last_ring[segment],
                "b": south_pole,
                "c": last_ring[(segment + 1) % segments],
                "surface_group_id": group_id,
            }
        )

    expected_triangles = 2 * segments * (rings - 1)
    if len(triangles) != expected_triangles:
        raise AssertionError(f"{asset_id}: expected {expected_triangles} triangles, got {len(triangles)}")

    return {
        "schema_family": "codework_geometry",
        "schema_variant": "mesh_asset_runtime_v1",
        "schema_version": 1,
        "asset_id": asset_id,
        "source_asset_id": "generated_uv_sphere",
        "asset_type": "solid_mesh",
        "compile_meta": {
            "profile": "runtime_default",
            "generator": "ray_tracing/tools/generate_mesh_asset_sphere_fixtures.py",
            "shape": "uv_sphere",
            "segments": segments,
            "rings": rings,
            "fidelity": fidelity,
            "vertex_order": "north_pole_then_latitude_rings_then_south_pole",
            "triangle_order": "north_cap_then_body_bands_then_south_cap",
        },
        "local_bounds": {
            "min": {"x": -1.0, "y": -1.0, "z": -1.0},
            "max": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "mesh": {
            "vertex_count": len(vertices),
            "triangle_count": len(triangles),
            "vertices": vertices,
            "triangles": triangles,
        },
        "surface_groups": [
            {
                "group_id": group_id,
                "semantic": "continuous_sphere_shell",
                "triangle_span": {"start": 0, "count": len(triangles)},
            }
        ],
        "topology_flags": {
            "closed_volume": True,
            "manifold_expected": True,
        },
        "extensions": {},
    }


def primitive_plane(
    object_id: str,
    material_id: str,
    origin: tuple[float, float, float],
    axis_u: tuple[float, float, float],
    axis_v: tuple[float, float, float],
    normal: tuple[float, float, float],
    width: float,
    height: float,
) -> dict:
    return {
        "object_id": object_id,
        "object_type": "plane_primitive",
        "dimensional_mode": "plane_locked",
        "locked_plane": "xy",
        "transform": {
            "position": {"x": origin[0], "y": origin[1], "z": origin[2]},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "geometry_ref": {"kind": "shape_asset", "id": f"shape_{object_id}"},
        "material_ref": {"id": material_id},
        "flags": {"visible": True, "locked": False, "selectable": True},
        "primitive": {
            "kind": "plane_primitive",
            "width": width,
            "height": height,
            "frame": {
                "origin": {"x": origin[0], "y": origin[1], "z": origin[2]},
                "axis_u": {"x": axis_u[0], "y": axis_u[1], "z": axis_u[2]},
                "axis_v": {"x": axis_v[0], "y": axis_v[1], "z": axis_v[2]},
                "normal": {"x": normal[0], "y": normal[1], "z": normal[2]},
            },
            "lock_to_construction_plane": False,
            "lock_to_bounds": False,
        },
    }


def mesh_instance(object_id: str, asset_id: str, material_id: str, x: float, y: float, z: float) -> dict:
    return {
        "object_id": object_id,
        "object_type": "mesh_asset_instance",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": {"x": x, "y": y, "z": z},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 0.65, "y": 0.65, "z": 0.65},
        },
        "geometry_ref": {
            "kind": "mesh_asset",
            "id": asset_id,
            "variant": "runtime_default",
        },
        "material_ref": {"id": material_id},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def make_scene() -> dict:
    objects = [
        primitive_plane(
            "obj_floor",
            "mat_floor",
            (0.0, 0.0, 0.0),
            (1.0, 0.0, 0.0),
            (0.0, 1.0, 0.0),
            (0.0, 0.0, 1.0),
            8.0,
            6.0,
        ),
        primitive_plane(
            "obj_back_wall",
            "mat_wall",
            (0.0, 3.0, 2.0),
            (1.0, 0.0, 0.0),
            (0.0, 0.0, 1.0),
            (0.0, -1.0, 0.0),
            8.0,
            4.0,
        ),
        primitive_plane(
            "obj_left_wall",
            "mat_wall",
            (-4.0, 0.0, 2.0),
            (0.0, 1.0, 0.0),
            (0.0, 0.0, 1.0),
            (1.0, 0.0, 0.0),
            6.0,
            4.0,
        ),
        mesh_instance("obj_sphere_low", "asset_sphere_8x4", "mat_sphere_low", -2.0, 0.3, 0.75),
        mesh_instance("obj_sphere_medium", "asset_sphere_16x8", "mat_sphere_medium", 0.0, 0.3, 0.75),
        mesh_instance("obj_sphere_high", "asset_sphere_32x16", "mat_sphere_high", 2.0, 0.3, 0.75),
    ]
    object_materials = [
        {"object_id": "obj_floor", "material_id": 0, "object_color": 0x6F746F, "roughness": 0.8},
        {"object_id": "obj_back_wall", "material_id": 0, "object_color": 0x8A8781, "roughness": 0.75},
        {"object_id": "obj_left_wall", "material_id": 0, "object_color": 0x777B83, "roughness": 0.75},
        {"object_id": "obj_sphere_low", "material_id": 0, "object_color": 0xC55A4B, "roughness": 0.45},
        {"object_id": "obj_sphere_medium", "material_id": 0, "object_color": 0x4F8CC9, "roughness": 0.38},
        {"object_id": "obj_sphere_high", "material_id": 0, "object_color": 0xD5A84A, "roughness": 0.32},
    ]
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "scene_mesh_asset_sphere_room_mrt0",
        "source_scene_id": "scene_mesh_asset_sphere_room_mrt0",
        "compile_meta": {
            "compiler_version": "mrt0_fixture",
            "compiled_at_ns": 0,
            "normalization": "manual_mrt0_fixture",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": objects,
        "hierarchy": [],
        "materials": [
            {"material_id": "mat_floor", "kind": "lambert", "albedo": [0.45, 0.45, 0.42]},
            {"material_id": "mat_wall", "kind": "lambert", "albedo": [0.58, 0.56, 0.52]},
            {"material_id": "mat_sphere_low", "kind": "lambert", "albedo": [0.77, 0.35, 0.29]},
            {"material_id": "mat_sphere_medium", "kind": "lambert", "albedo": [0.31, 0.55, 0.79]},
            {"material_id": "mat_sphere_high", "kind": "lambert", "albedo": [0.84, 0.66, 0.29]},
        ],
        "lights": [
            {
                "light_id": "light_key",
                "kind": "point",
                "position": {"x": -2.5, "y": -2.8, "z": 4.2},
                "intensity": 2.8,
                "radius": 0.12,
            }
        ],
        "cameras": [
            {
                "camera_id": "cam_main",
                "kind": "perspective",
                "position": {"x": 0.0, "y": -6.2, "z": 2.2},
                "target": {"x": 0.0, "y": 0.4, "z": 0.85},
                "yaw": 0.0,
                "look_pitch": -0.08,
            }
        ],
        "constraints": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": {"x": 0.0, "y": 0.35, "z": 0.9},
                    "object_materials": object_materials,
                }
            }
        },
    }


def make_pressure_scene() -> dict:
    objects = [
        primitive_plane(
            "obj_floor",
            "mat_floor",
            (0.0, 0.0, 0.0),
            (1.0, 0.0, 0.0),
            (0.0, 1.0, 0.0),
            (0.0, 0.0, 1.0),
            8.0,
            6.0,
        ),
        primitive_plane(
            "obj_back_wall",
            "mat_wall",
            (0.0, 3.0, 2.0),
            (1.0, 0.0, 0.0),
            (0.0, 0.0, 1.0),
            (0.0, -1.0, 0.0),
            8.0,
            4.0,
        ),
        primitive_plane(
            "obj_left_wall",
            "mat_wall",
            (-4.0, 0.0, 2.0),
            (0.0, 1.0, 0.0),
            (0.0, 0.0, 1.0),
            (1.0, 0.0, 0.0),
            6.0,
            4.0,
        ),
        mesh_instance("obj_sphere_pressure", "asset_sphere_64x32", "mat_sphere_pressure", 0.0, 0.3, 0.75),
    ]
    object_materials = [
        {"object_id": "obj_floor", "material_id": 0, "object_color": 0x6F746F, "roughness": 0.8},
        {"object_id": "obj_back_wall", "material_id": 0, "object_color": 0x8A8781, "roughness": 0.75},
        {"object_id": "obj_left_wall", "material_id": 0, "object_color": 0x777B83, "roughness": 0.75},
        {"object_id": "obj_sphere_pressure", "material_id": 0, "object_color": 0xD5A84A, "roughness": 0.28},
    ]
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "scene_mesh_asset_sphere_pressure_mrt6",
        "source_scene_id": "scene_mesh_asset_sphere_pressure_mrt6",
        "compile_meta": {
            "compiler_version": "mrt6_fixture",
            "compiled_at_ns": 0,
            "normalization": "manual_mrt6_fixture",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": objects,
        "hierarchy": [],
        "materials": [
            {"material_id": "mat_floor", "kind": "lambert", "albedo": [0.45, 0.45, 0.42]},
            {"material_id": "mat_wall", "kind": "lambert", "albedo": [0.58, 0.56, 0.52]},
            {"material_id": "mat_sphere_pressure", "kind": "lambert", "albedo": [0.84, 0.66, 0.29]},
        ],
        "lights": [
            {
                "light_id": "light_key",
                "kind": "point",
                "position": {"x": -2.5, "y": -2.8, "z": 4.2},
                "intensity": 2.8,
                "radius": 0.12,
            }
        ],
        "cameras": [
            {
                "camera_id": "cam_main",
                "kind": "perspective",
                "position": {"x": 0.0, "y": -6.2, "z": 2.2},
                "target": {"x": 0.0, "y": 0.35, "z": 0.85},
                "yaw": 0.0,
                "look_pitch": -0.08,
            }
        ],
        "constraints": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": {"x": 0.0, "y": 0.35, "z": 0.9},
                    "object_materials": object_materials,
                }
            }
        },
    }


def make_mrt8_pressure_scene() -> dict:
    objects = [
        primitive_plane(
            "obj_floor",
            "mat_floor",
            (0.0, 0.0, 0.0),
            (1.0, 0.0, 0.0),
            (0.0, 1.0, 0.0),
            (0.0, 0.0, 1.0),
            8.0,
            6.0,
        ),
        primitive_plane(
            "obj_back_wall",
            "mat_wall",
            (0.0, 3.0, 2.0),
            (1.0, 0.0, 0.0),
            (0.0, 0.0, 1.0),
            (0.0, -1.0, 0.0),
            8.0,
            4.0,
        ),
        primitive_plane(
            "obj_left_wall",
            "mat_wall",
            (-4.0, 0.0, 2.0),
            (0.0, 1.0, 0.0),
            (0.0, 0.0, 1.0),
            (1.0, 0.0, 0.0),
            6.0,
            4.0,
        ),
        mesh_instance("obj_sphere_pressure_mrt8", "asset_sphere_128x64", "mat_sphere_pressure", 0.0, 0.3, 0.75),
    ]
    object_materials = [
        {"object_id": "obj_floor", "material_id": 0, "object_color": 0x6F746F, "roughness": 0.8},
        {"object_id": "obj_back_wall", "material_id": 0, "object_color": 0x8A8781, "roughness": 0.75},
        {"object_id": "obj_left_wall", "material_id": 0, "object_color": 0x777B83, "roughness": 0.75},
        {"object_id": "obj_sphere_pressure_mrt8", "material_id": 0, "object_color": 0xD5A84A, "roughness": 0.24},
    ]
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "scene_mesh_asset_sphere_pressure_mrt8",
        "source_scene_id": "scene_mesh_asset_sphere_pressure_mrt8",
        "compile_meta": {
            "compiler_version": "mrt8_fixture",
            "compiled_at_ns": 0,
            "normalization": "manual_mrt8_fixture",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": objects,
        "hierarchy": [],
        "materials": [
            {"material_id": "mat_floor", "kind": "lambert", "albedo": [0.45, 0.45, 0.42]},
            {"material_id": "mat_wall", "kind": "lambert", "albedo": [0.58, 0.56, 0.52]},
            {"material_id": "mat_sphere_pressure", "kind": "lambert", "albedo": [0.84, 0.66, 0.29]},
        ],
        "lights": [
            {
                "light_id": "light_key",
                "kind": "point",
                "position": {"x": -2.5, "y": -2.8, "z": 4.2},
                "intensity": 2.8,
                "radius": 0.12,
            }
        ],
        "cameras": [
            {
                "camera_id": "cam_main",
                "kind": "perspective",
                "position": {"x": 0.0, "y": -6.2, "z": 2.2},
                "target": {"x": 0.0, "y": 0.35, "z": 0.85},
                "yaw": 0.0,
                "look_pitch": -0.08,
            }
        ],
        "constraints": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": {"x": 0.0, "y": 0.35, "z": 0.9},
                    "object_materials": object_materials,
                }
            }
        },
    }


def make_mrt10_pressure_scene() -> dict:
    objects = [
        primitive_plane(
            "obj_floor",
            "mat_floor",
            (0.0, 0.0, 0.0),
            (1.0, 0.0, 0.0),
            (0.0, 1.0, 0.0),
            (0.0, 0.0, 1.0),
            8.0,
            6.0,
        ),
        primitive_plane(
            "obj_back_wall",
            "mat_wall",
            (0.0, 3.0, 2.0),
            (1.0, 0.0, 0.0),
            (0.0, 0.0, 1.0),
            (0.0, -1.0, 0.0),
            8.0,
            4.0,
        ),
        primitive_plane(
            "obj_left_wall",
            "mat_wall",
            (-4.0, 0.0, 2.0),
            (0.0, 1.0, 0.0),
            (0.0, 0.0, 1.0),
            (1.0, 0.0, 0.0),
            6.0,
            4.0,
        ),
        mesh_instance("obj_sphere_pressure_mrt10", "asset_sphere_256x128", "mat_sphere_pressure", 0.0, 0.3, 0.75),
    ]
    object_materials = [
        {"object_id": "obj_floor", "material_id": 0, "object_color": 0x6F746F, "roughness": 0.8},
        {"object_id": "obj_back_wall", "material_id": 0, "object_color": 0x8A8781, "roughness": 0.75},
        {"object_id": "obj_left_wall", "material_id": 0, "object_color": 0x777B83, "roughness": 0.75},
        {"object_id": "obj_sphere_pressure_mrt10", "material_id": 0, "object_color": 0xD5A84A, "roughness": 0.22},
    ]
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "scene_mesh_asset_sphere_pressure_mrt10",
        "source_scene_id": "scene_mesh_asset_sphere_pressure_mrt10",
        "compile_meta": {
            "compiler_version": "mrt10_fixture",
            "compiled_at_ns": 0,
            "normalization": "manual_mrt10_fixture",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": objects,
        "hierarchy": [],
        "materials": [
            {"material_id": "mat_floor", "kind": "lambert", "albedo": [0.45, 0.45, 0.42]},
            {"material_id": "mat_wall", "kind": "lambert", "albedo": [0.58, 0.56, 0.52]},
            {"material_id": "mat_sphere_pressure", "kind": "lambert", "albedo": [0.84, 0.66, 0.29]},
        ],
        "lights": [
            {
                "light_id": "light_key",
                "kind": "point",
                "position": {"x": -2.5, "y": -2.8, "z": 4.2},
                "intensity": 2.8,
                "radius": 0.12,
            }
        ],
        "cameras": [
            {
                "camera_id": "cam_main",
                "kind": "perspective",
                "position": {"x": 0.0, "y": -6.2, "z": 2.2},
                "target": {"x": 0.0, "y": 0.35, "z": 0.85},
                "yaw": 0.0,
                "look_pitch": -0.08,
            }
        ],
        "constraints": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": {"x": 0.0, "y": 0.35, "z": 0.9},
                    "object_materials": object_materials,
                }
            }
        },
    }


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=False) + "\n", encoding="utf-8")


def triangle_orientation_ok(vertices: list[dict], triangle: dict) -> bool:
    a = vertices[triangle["a"]]
    b = vertices[triangle["b"]]
    c = vertices[triangle["c"]]
    ux = b["x"] - a["x"]
    uy = b["y"] - a["y"]
    uz = b["z"] - a["z"]
    vx = c["x"] - a["x"]
    vy = c["y"] - a["y"]
    vz = c["z"] - a["z"]
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    area = math.sqrt(nx * nx + ny * ny + nz * nz)
    if area <= 1e-9:
        return False
    mx = (a["x"] + b["x"] + c["x"]) / 3.0
    my = (a["y"] + b["y"] + c["y"]) / 3.0
    mz = (a["z"] + b["z"] + c["z"]) / 3.0
    return nx * mx + ny * my + nz * mz > 0.0


def validate_asset(path: Path, segments: int, rings: int) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    mesh = data["mesh"]
    vertices = mesh["vertices"]
    triangles = mesh["triangles"]
    expected_vertices = 2 + (rings - 1) * segments
    expected_triangles = 2 * segments * (rings - 1)

    if mesh["vertex_count"] != expected_vertices or len(vertices) != expected_vertices:
        raise AssertionError(f"{path}: vertex count mismatch")
    if mesh["triangle_count"] != expected_triangles or len(triangles) != expected_triangles:
        raise AssertionError(f"{path}: triangle count mismatch")

    for triangle in triangles:
        for key in ("a", "b", "c"):
            index = triangle[key]
            if not isinstance(index, int) or index < 0 or index >= len(vertices):
                raise AssertionError(f"{path}: triangle index out of range")
        if not triangle_orientation_ok(vertices, triangle):
            raise AssertionError(f"{path}: triangle winding or area invalid")

    group = data["surface_groups"][0]
    span = group["triangle_span"]
    if span["start"] != 0 or span["count"] != expected_triangles:
        raise AssertionError(f"{path}: surface group span mismatch")

    return {
        "asset_id": data["asset_id"],
        "vertex_count": len(vertices),
        "triangle_count": len(triangles),
    }


def validate_output(out_dir: Path) -> dict:
    scene_path = out_dir / "scene_runtime.json"
    pressure_scene_path = out_dir / "scene_runtime_pressure.json"
    mrt8_pressure_scene_path = out_dir / "scene_runtime_pressure_mrt8.json"
    mrt10_pressure_scene_path = out_dir / "scene_runtime_pressure_mrt10.json"
    summary_path = out_dir / "fixture_summary.json"
    if not scene_path.exists():
        raise AssertionError(f"missing {scene_path}")
    if not pressure_scene_path.exists():
        raise AssertionError(f"missing {pressure_scene_path}")
    if not mrt8_pressure_scene_path.exists():
        raise AssertionError(f"missing {mrt8_pressure_scene_path}")
    if not mrt10_pressure_scene_path.exists():
        raise AssertionError(f"missing {mrt10_pressure_scene_path}")
    if not summary_path.exists():
        raise AssertionError(f"missing {summary_path}")

    scene = json.loads(scene_path.read_text(encoding="utf-8"))
    asset_refs = {
        obj["geometry_ref"]["id"]
        for obj in scene["objects"]
        if obj.get("object_type") == "mesh_asset_instance"
    }
    results = []
    for asset_id, segments, rings, _fidelity in SPHERE_SPECS:
        path = out_dir / "assets" / "mesh_assets" / f"{asset_id}.runtime.json"
        if not path.exists():
            raise AssertionError(f"missing {path}")
        if asset_id not in asset_refs:
            raise AssertionError(f"scene does not reference {asset_id}")
        results.append(validate_asset(path, segments, rings))

    pressure_scene = json.loads(pressure_scene_path.read_text(encoding="utf-8"))
    pressure_refs = {
        obj["geometry_ref"]["id"]
        for obj in pressure_scene["objects"]
        if obj.get("object_type") == "mesh_asset_instance"
    }
    for asset_id, segments, rings, _fidelity in PRESSURE_SPHERE_SPECS:
        path = out_dir / "assets" / "mesh_assets" / f"{asset_id}.runtime.json"
        if not path.exists():
            raise AssertionError(f"missing {path}")
        if asset_id not in pressure_refs:
            raise AssertionError(f"pressure scene does not reference {asset_id}")
        results.append(validate_asset(path, segments, rings))

    mrt8_pressure_scene = json.loads(mrt8_pressure_scene_path.read_text(encoding="utf-8"))
    mrt8_pressure_refs = {
        obj["geometry_ref"]["id"]
        for obj in mrt8_pressure_scene["objects"]
        if obj.get("object_type") == "mesh_asset_instance"
    }
    for asset_id, segments, rings, _fidelity in MRT8_PRESSURE_SPHERE_SPECS:
        path = out_dir / "assets" / "mesh_assets" / f"{asset_id}.runtime.json"
        if not path.exists():
            raise AssertionError(f"missing {path}")
        if asset_id not in mrt8_pressure_refs:
            raise AssertionError(f"MRT8 pressure scene does not reference {asset_id}")
        results.append(validate_asset(path, segments, rings))

    mrt10_pressure_scene = json.loads(mrt10_pressure_scene_path.read_text(encoding="utf-8"))
    mrt10_pressure_refs = {
        obj["geometry_ref"]["id"]
        for obj in mrt10_pressure_scene["objects"]
        if obj.get("object_type") == "mesh_asset_instance"
    }
    for asset_id, segments, rings, _fidelity in MRT10_PRESSURE_SPHERE_SPECS:
        path = out_dir / "assets" / "mesh_assets" / f"{asset_id}.runtime.json"
        if not path.exists():
            raise AssertionError(f"missing {path}")
        if asset_id not in mrt10_pressure_refs:
            raise AssertionError(f"MRT10 pressure scene does not reference {asset_id}")
        results.append(validate_asset(path, segments, rings))
    return {"ok": True, "asset_count": len(results), "assets": results}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out",
        type=Path,
        default=RAY_TRACING_ROOT / "tests" / "fixtures" / "mesh_asset_runtime_spheres",
        help="output fixture directory",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="validate generated fixtures without rewriting them",
    )
    args = parser.parse_args()

    out_dir = args.out
    if args.check:
        print(json.dumps(validate_output(out_dir), indent=2))
        return 0

    mesh_dir = out_dir / "assets" / "mesh_assets"
    summary = {
        "fixture": "ray_tracing_mesh_asset_runtime_spheres_mrt0",
        "scene_runtime": "scene_runtime.json",
        "assets": [],
    }

    pressure_specs = PRESSURE_SPHERE_SPECS + MRT8_PRESSURE_SPHERE_SPECS + MRT10_PRESSURE_SPHERE_SPECS
    for asset_id, segments, rings, fidelity in SPHERE_SPECS + pressure_specs:
        sphere = make_sphere(asset_id, segments, rings, fidelity)
        filename = f"{asset_id}.runtime.json"
        write_json(mesh_dir / filename, sphere)
        summary["assets"].append(
            {
                "asset_id": asset_id,
                "path": f"assets/mesh_assets/{filename}",
                "segments": segments,
                "rings": rings,
                "vertex_count": sphere["mesh"]["vertex_count"],
                "triangle_count": sphere["mesh"]["triangle_count"],
                "fidelity": fidelity,
            }
        )

    write_json(out_dir / "scene_runtime.json", make_scene())
    write_json(out_dir / "scene_runtime_pressure.json", make_pressure_scene())
    write_json(out_dir / "scene_runtime_pressure_mrt8.json", make_mrt8_pressure_scene())
    write_json(out_dir / "scene_runtime_pressure_mrt10.json", make_mrt10_pressure_scene())
    write_json(out_dir / "fixture_summary.json", summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
