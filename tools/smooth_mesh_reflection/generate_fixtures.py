#!/usr/bin/env python3
"""Generate deterministic sphere and organic reflection stress meshes."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

Vec3 = tuple[float, float, float]
Face = tuple[int, int, int]

TIER_SUBDIVISIONS = {
    "unit": 3,
    "high": 5,
    "very_high": 7,
    "ultra": 8,
}


def normalize(v: Vec3) -> Vec3:
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    if length <= 0.0:
        raise ValueError("cannot normalize zero vector")
    return (v[0] / length, v[1] / length, v[2] / length)


def cross(a: Vec3, b: Vec3) -> Vec3:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def sub(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def dot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def base_icosahedron() -> tuple[list[Vec3], list[Face]]:
    phi = (1.0 + math.sqrt(5.0)) * 0.5
    vertices = [
        (-1.0, phi, 0.0), (1.0, phi, 0.0), (-1.0, -phi, 0.0),
        (1.0, -phi, 0.0), (0.0, -1.0, phi), (0.0, 1.0, phi),
        (0.0, -1.0, -phi), (0.0, 1.0, -phi), (phi, 0.0, -1.0),
        (phi, 0.0, 1.0), (-phi, 0.0, -1.0), (-phi, 0.0, 1.0),
    ]
    faces = [
        (0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
        (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
        (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
        (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1),
    ]
    return [normalize(v) for v in vertices], faces


def subdivide(vertices: list[Vec3], faces: list[Face]) -> tuple[list[Vec3], list[Face]]:
    midpoint_cache: dict[tuple[int, int], int] = {}

    def midpoint(a: int, b: int) -> int:
        key = (a, b) if a < b else (b, a)
        cached = midpoint_cache.get(key)
        if cached is not None:
            return cached
        va, vb = vertices[a], vertices[b]
        index = len(vertices)
        vertices.append(normalize(((va[0] + vb[0]) * 0.5,
                                   (va[1] + vb[1]) * 0.5,
                                   (va[2] + vb[2]) * 0.5)))
        midpoint_cache[key] = index
        return index

    refined: list[Face] = []
    for a, b, c in faces:
        ab, bc, ca = midpoint(a, b), midpoint(b, c), midpoint(c, a)
        refined.extend(((a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)))
    return vertices, refined


def organic_radius(direction: Vec3, radius: float, amplitude: float,
                   frequency: int, seed: int) -> float:
    x, y, z = direction
    phase = float(seed) * 0.6180339887498948
    signal = (
        0.46 * math.sin(frequency * x + phase) * math.cos((frequency - 1) * y - phase)
        + 0.31 * math.sin((frequency + 1) * z + 0.7 * phase)
        + 0.23 * math.cos((frequency - 2) * (x + y - z) - 1.3 * phase)
    )
    return radius * (1.0 + amplitude * signal)


def build_geodesic(family: str, subdivisions: int, radius: float,
                   amplitude: float, frequency: int, seed: int) -> tuple[list[Vec3], list[Face]]:
    if subdivisions < 0 or subdivisions > 8:
        raise ValueError("subdivisions must be between 0 and 8")
    if radius <= 0.0 or amplitude < 0.0 or amplitude >= 0.5 or frequency < 3:
        raise ValueError("invalid geodesic fixture parameters")
    vertices, faces = base_icosahedron()
    for _ in range(subdivisions):
        vertices, faces = subdivide(vertices, faces)
    if family == "organic_blob":
        vertices = [
            tuple(component * organic_radius(v, radius, amplitude, frequency, seed)
                  for component in v)
            for v in vertices
        ]
    else:
        vertices = [tuple(component * radius for component in v) for v in vertices]
    return vertices, faces


def build_crease_fixture() -> tuple[list[Vec3], list[Face]]:
    vertices = [
        (-1.0, -1.0, -1.0), (1.0, -1.0, -1.0), (1.0, 1.0, -1.0),
        (-1.0, 1.0, -1.0), (-1.0, -1.0, 1.0), (1.0, -1.0, 1.0),
        (1.0, 1.0, 1.0), (-1.0, 1.0, 1.0),
    ]
    faces = [
        (0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7),
        (0, 1, 5), (0, 5, 4), (1, 2, 6), (1, 6, 5),
        (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7),
    ]
    return vertices, faces


def validate(vertices: list[Vec3], faces: list[Face]) -> dict[str, object]:
    if not vertices or not faces:
        raise ValueError("fixture is empty")
    minimum = [math.inf, math.inf, math.inf]
    maximum = [-math.inf, -math.inf, -math.inf]
    for vertex in vertices:
        if not all(math.isfinite(component) for component in vertex):
            raise ValueError("fixture contains non-finite vertex")
        for axis in range(3):
            minimum[axis] = min(minimum[axis], vertex[axis])
            maximum[axis] = max(maximum[axis], vertex[axis])
    for index, (ia, ib, ic) in enumerate(faces):
        a, b, c = vertices[ia], vertices[ib], vertices[ic]
        normal = cross(sub(b, a), sub(c, a))
        if dot(normal, normal) <= 1e-24:
            raise ValueError(f"degenerate triangle {index}")
        center = ((a[0] + b[0] + c[0]) / 3.0,
                  (a[1] + b[1] + c[1]) / 3.0,
                  (a[2] + b[2] + c[2]) / 3.0)
        if dot(normal, center) <= 0.0:
            raise ValueError(f"inward triangle {index}")
    return {"vertex_count": len(vertices), "triangle_count": len(faces),
            "bounds": {"min": minimum, "max": maximum}}


def write_binary_stl(path: Path, vertices: list[Vec3], faces: list[Face]) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    with path.open("wb") as stream:
        header = b"CodeWork deterministic smooth reflection fixture".ljust(80, b"\0")
        stream.write(header)
        stream.write(struct.pack("<I", len(faces)))
        for ia, ib, ic in faces:
            a, b, c = vertices[ia], vertices[ib], vertices[ic]
            normal = normalize(cross(sub(b, a), sub(c, a)))
            record = struct.pack("<12fH", *(normal + a + b + c), 0)
            stream.write(record)
            digest.update(record)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--family", choices=("analytic_sphere", "icosphere", "organic_blob", "crease"), required=True)
    parser.add_argument("--tier", choices=tuple(TIER_SUBDIVISIONS), default="unit")
    parser.add_argument("--subdivisions", type=int)
    parser.add_argument("--radius", type=float, default=1.0)
    parser.add_argument("--amplitude", type=float, default=0.14)
    parser.add_argument("--frequency", type=int, default=5)
    parser.add_argument("--seed", type=int, default=17)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary", type=Path)
    parser.add_argument("--authoring-output", type=Path)
    args = parser.parse_args()

    subdivisions = args.subdivisions if args.subdivisions is not None else TIER_SUBDIVISIONS[args.tier]
    if args.family == "crease":
        vertices, faces = build_crease_fixture()
    else:
        vertices, faces = build_geodesic(args.family, subdivisions, args.radius,
                                         args.amplitude, args.frequency, args.seed)
    summary = validate(vertices, faces)
    summary.update({"schema": "smooth_mesh_reflection_fixture_v1", "family": args.family,
                    "tier": args.tier, "subdivisions": subdivisions,
                    "seed": args.seed, "amplitude": args.amplitude,
                    "frequency": args.frequency})
    summary["triangle_payload_sha256"] = write_binary_stl(args.output, vertices, faces)
    summary["stl_bytes"] = args.output.stat().st_size
    summary_path = args.summary or args.output.with_suffix(".summary.json")
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if args.authoring_output:
        authoring = {
            "schema_family": "codework_geometry", "schema_variant": "mesh_asset_authoring_v1",
            "schema_version": 1, "asset_id": f"smooth_reflection_{args.family}_{args.tier}",
            "unit_system": "meter", "world_scale": 1.0, "asset_type": "solid_mesh",
            "pivot": {"origin": {"x": 0.0, "y": 0.0, "z": 0.0},
                      "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
                      "axis_v": {"x": 0.0, "y": 1.0, "z": 0.0},
                      "normal": {"x": 0.0, "y": 0.0, "z": 1.0}},
            "authoring": {"source_mode": "imported_mesh", "imported_mesh": {
                "import_id": f"smooth_reflection_{args.family}_{args.tier}_stl",
                "source_format": "stl", "source_uri": str(args.output.resolve()),
                "source_unit_system": "meter", "source_to_asset_scale": 1.0,
                "orientation_policy": "source_axes", "default_surface_group_id": "smooth_surface",
                "weld_vertices": True, "weld_tolerance": 0.0000001,
                "preserve_source_normals": False,
                "normal_mode": "crease_aware" if args.family == "crease" else "smooth",
                "crease_angle_degrees": 30.0 if args.family == "crease" else 180.0,
                "topology_closed_volume_observed": True, "topology_manifold_observed": True}},
            "surface_groups": [],
            "compile_hints": {"expect_closed_volume": True, "expect_manifold": True},
            "extensions": {"smooth_mesh_reflection_fixture": summary},
        }
        args.authoring_output.parent.mkdir(parents=True, exist_ok=True)
        args.authoring_output.write_text(json.dumps(authoring, indent=2, sort_keys=True) + "\n",
                                         encoding="utf-8")
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
