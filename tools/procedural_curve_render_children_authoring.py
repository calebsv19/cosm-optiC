#!/usr/bin/env python3
"""Expand PSG-23E guide curves into deterministic thin PSG-23F render children."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import os
import pathlib
import tempfile
from typing import Any

SCHEMA = "ray_tracing.procedural_curve_render_children_authoring_v1"
RECEIPT_SCHEMA = "ray_tracing.procedural_curve_render_children_receipt"
RUNTIME_FAMILY = "codework_curve_asset"
RUNTIME_VARIANT = "curve_asset_runtime_v1"
LOD_NAMES = ("preview", "interactive", "final")

EDITABLE = {
    "lod.preview_children_per_parent",
    "lod.interactive_children_per_parent",
    "lod.final_children_per_parent",
    "children.root_barycentric_spread",
    "children.length_variation",
    "children.shape_variation",
    "children.root_radius_scale",
    "children.tip_radius_scale",
    "children.seed",
}


def canonical_bytes(document: dict) -> bytes:
    return (
        json.dumps(document, indent=2, sort_keys=True, allow_nan=False) + "\n"
    ).encode("utf-8")


def sha_file(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_atomic(path: pathlib.Path, document: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(canonical_bytes(document))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def load(path: pathlib.Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected JSON object")
    return value


def finite_number(value: Any) -> bool:
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(float(value))
    )


def vec3(value: Any, field: str) -> list[float]:
    if (
        not isinstance(value, list)
        or len(value) != 3
        or not all(finite_number(component) for component in value)
    ):
        raise ValueError(f"{field}: expected finite three-number array")
    return [float(component) for component in value]


def point_vec(point: dict, field: str) -> list[float]:
    position = point.get("position")
    if not isinstance(position, dict):
        raise ValueError(f"{field}: position object required")
    values = [position.get(axis) for axis in ("x", "y", "z")]
    if not all(finite_number(value) for value in values):
        raise ValueError(f"{field}: finite position required")
    return [float(value) for value in values]


def add(a: list[float], b: list[float]) -> list[float]:
    return [a[index] + b[index] for index in range(3)]


def sub(a: list[float], b: list[float]) -> list[float]:
    return [a[index] - b[index] for index in range(3)]


def mul(a: list[float], scalar: float) -> list[float]:
    return [component * scalar for component in a]


def dot(a: list[float], b: list[float]) -> float:
    return sum(a[index] * b[index] for index in range(3))


def cross(a: list[float], b: list[float]) -> list[float]:
    return [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]


def magnitude(a: list[float]) -> float:
    return math.sqrt(dot(a, a))


def normalized(a: list[float], field: str) -> list[float]:
    length = magnitude(a)
    if not math.isfinite(length) or length <= 1.0e-12:
        raise ValueError(f"{field}: nonzero vector required")
    return mul(a, 1.0 / length)


def deterministic_signed(seed: int, parent: int, child: int, channel: int) -> float:
    digest = hashlib.sha256(
        f"{seed}:{parent}:{child}:{channel}".encode("ascii")).digest()
    integer = int.from_bytes(digest[:8], "big")
    return integer / float(2**64 - 1) * 2.0 - 1.0


def validate_guide_asset(document: dict) -> None:
    if (
        document.get("schema_family") != RUNTIME_FAMILY
        or document.get("schema_variant") != RUNTIME_VARIANT
        or document.get("schema_version") != 1
    ):
        raise ValueError("guide asset schema/version mismatch")
    if not isinstance(document.get("source_binding"), dict):
        raise ValueError("guide asset must retain PSG-23E source binding")
    strands = document.get("strands")
    if not isinstance(strands, list) or not strands or len(strands) > 4096:
        raise ValueError("guide asset strand count invalid")
    point_count = None
    for index, strand in enumerate(strands):
        points = strand.get("points") if isinstance(strand, dict) else None
        if (
            strand.get("strand_index") != index
            or not isinstance(strand.get("guide_index"), int)
            or not isinstance(strand.get("source_triangle_index"), int)
            or not isinstance(points, list)
            or len(points) < 4
            or len(points) > 64
        ):
            raise ValueError("guide strand structure/order invalid")
        if point_count is not None and len(points) != point_count:
            raise ValueError("guide strand control counts must match")
        point_count = len(points)
        for point_index, point in enumerate(points):
            point_vec(point, f"strand {index} point {point_index}")
            radius = point.get("radius")
            if not finite_number(radius) or float(radius) <= 0.0:
                raise ValueError("guide point radius must be positive")


def mesh_identity(mesh: dict, path: pathlib.Path) -> dict:
    if (
        mesh.get("schema_family") != "codework_geometry"
        or mesh.get("schema_variant") != "mesh_asset_runtime_v1"
        or mesh.get("schema_version") != 1
    ):
        raise ValueError("source mesh must be mesh_asset_runtime_v1")
    mesh_body = mesh.get("mesh")
    if (
        not isinstance(mesh_body, dict)
        or not isinstance(mesh_body.get("vertices"), list)
        or not isinstance(mesh_body.get("triangles"), list)
        or mesh_body.get("vertex_count") != len(mesh_body["vertices"])
        or mesh_body.get("triangle_count") != len(mesh_body["triangles"])
    ):
        raise ValueError("source mesh topology arrays invalid")
    return {
        "source_asset_id": mesh.get("asset_id"),
        "source_file_sha256": sha_file(path),
        "source_vertex_count": mesh_body["vertex_count"],
        "source_triangle_count": mesh_body["triangle_count"],
    }


def binding_from_files(
    guide_path: pathlib.Path, mesh_path: pathlib.Path,
) -> dict:
    guide = load(guide_path)
    mesh = load(mesh_path)
    validate_guide_asset(guide)
    identity = mesh_identity(mesh, mesh_path)
    source_binding = guide["source_binding"]
    if (
        source_binding.get("source_asset_id") != identity["source_asset_id"]
        or source_binding.get("source_file_sha256")
        != identity["source_file_sha256"]
    ):
        raise ValueError("guide asset does not exactly bind the source mesh")
    return {
        "guide_asset_id": guide.get("asset_id"),
        "guide_asset_sha256": sha_file(guide_path),
        **identity,
        "carrier_file_sha256": source_binding.get("carrier_file_sha256"),
        "carrier_value_digest_sha256":
            source_binding.get("carrier_value_digest_sha256"),
    }


def default_authoring(asset_id: str, binding: dict) -> dict:
    return {
        "schema": SCHEMA,
        "schema_version": 1,
        "asset_id": asset_id,
        "binding": binding,
        "lod": {
            "preview_children_per_parent": 4,
            "interactive_children_per_parent": 16,
            "final_children_per_parent": 48,
        },
        "children": {
            "root_barycentric_spread": 0.22,
            "length_variation": 0.08,
            "shape_variation": 0.022,
            "root_radius_scale": 0.22,
            "tip_radius_scale": 0.12,
            "seed": 23006,
        },
    }


def validate_authoring(document: dict) -> None:
    if document.get("schema") != SCHEMA or document.get("schema_version") != 1:
        raise ValueError("render-child authoring schema/version mismatch")
    if (
        not isinstance(document.get("asset_id"), str)
        or not document["asset_id"]
        or len(document["asset_id"]) >= 64
    ):
        raise ValueError("asset_id must be a nonempty string shorter than 64")
    binding = document.get("binding")
    lod = document.get("lod")
    children = document.get("children")
    if not all(isinstance(value, dict) for value in (binding, lod, children)):
        raise ValueError("binding, lod, and children objects are required")
    for key in (
        "guide_asset_id", "guide_asset_sha256", "source_asset_id",
        "source_file_sha256", "carrier_file_sha256",
        "carrier_value_digest_sha256",
    ):
        if not isinstance(binding.get(key), str) or not binding[key]:
            raise ValueError(f"binding.{key} is required")
    for key in (
        "guide_asset_sha256", "source_file_sha256", "carrier_file_sha256",
        "carrier_value_digest_sha256",
    ):
        if len(binding[key]) != 64:
            raise ValueError(f"binding.{key} must be SHA-256")
    counts = [
        lod.get(f"{name}_children_per_parent") for name in LOD_NAMES]
    if (
        not all(isinstance(value, int) and not isinstance(value, bool)
                for value in counts)
        or not 1 <= counts[0] <= counts[1] <= counts[2] <= 64
    ):
        raise ValueError("LOD child counts must be monotonic within [1,64]")
    ranges = {
        "root_barycentric_spread": (0.0, 0.30),
        "length_variation": (0.0, 0.30),
        "shape_variation": (0.0, 0.20),
        "root_radius_scale": (0.02, 1.0),
        "tip_radius_scale": (0.01, 1.0),
    }
    for key, limits in ranges.items():
        value = children.get(key)
        if not finite_number(value) or not limits[0] <= float(value) <= limits[1]:
            raise ValueError(f"children.{key} outside {limits}")
    if children["tip_radius_scale"] > children["root_radius_scale"]:
        raise ValueError("tip radius scale cannot exceed root radius scale")
    seed = children.get("seed")
    if (
        not isinstance(seed, int) or isinstance(seed, bool)
        or not 0 <= seed <= 2**31 - 1
    ):
        raise ValueError("children.seed outside valid range")


def parse_set(text: str) -> tuple[str, Any]:
    if "=" not in text:
        raise ValueError("--set requires dotted.path=JSON")
    path, raw = text.split("=", 1)
    if path not in EDITABLE:
        raise ValueError(f"{path}: field is not editable")
    return path, json.loads(raw)


def set_path(document: dict, path: str, value: Any) -> None:
    first, second = path.split(".")
    document[first][second] = value


def triangle_vertices(mesh: dict, triangle_index: int) -> list[list[float]]:
    triangles = mesh["mesh"]["triangles"]
    vertices = mesh["mesh"]["vertices"]
    if not 0 <= triangle_index < len(triangles):
        raise ValueError("guide source triangle outside source topology")
    triangle = triangles[triangle_index]
    indices = [triangle.get(key) for key in ("a", "b", "c")]
    if not all(isinstance(index, int) and 0 <= index < len(vertices)
               for index in indices):
        raise ValueError("source triangle indices invalid")
    return [
        [float(vertices[index][axis]) for axis in ("x", "y", "z")]
        for index in indices
    ]


def child_barycentrics(
    seed: int, parent_index: int, child_index: int, spread: float,
) -> list[float]:
    a = deterministic_signed(seed, parent_index, child_index, 0)
    b = deterministic_signed(seed, parent_index, child_index, 1)
    raw = [a, b, -a - b]
    maximum = max(abs(value) for value in raw)
    radius = 0.35 + 0.65 * (
        deterministic_signed(seed, parent_index, child_index, 2) + 1.0) * 0.5
    scale = 0.0 if maximum <= 1.0e-12 else spread * radius / maximum
    result = [1.0 / 3.0 + value * scale for value in raw]
    result[2] = 1.0 - result[0] - result[1]
    if min(result) <= 0.0 or max(result) >= 1.0:
        raise ValueError("generated child barycentrics invalid")
    return result


def barycentric_position(
    vertices: list[list[float]], barycentrics: list[float],
) -> list[float]:
    return [
        sum(vertices[corner][axis] * barycentrics[corner]
            for corner in range(3))
        for axis in range(3)
    ]


def child_strand(
    parent: dict,
    parent_index: int,
    child_index: int,
    stable_child_id: int,
    mesh: dict,
    settings: dict,
    lod_name: str,
) -> dict:
    seed = settings["seed"]
    vertices = triangle_vertices(mesh, parent["source_triangle_index"])
    parent_barycentrics = vec3(
        parent.get("root_barycentrics"), "parent root_barycentrics")
    if abs(sum(parent_barycentrics) - 1.0) > 1.0e-8:
        raise ValueError("parent root barycentrics do not sum to one")
    barycentrics = child_barycentrics(
        seed, parent_index, child_index,
        float(settings["root_barycentric_spread"]))
    parent_surface = barycentric_position(vertices, parent_barycentrics)
    child_surface = barycentric_position(vertices, barycentrics)
    normal = normalized(
        vec3(parent.get("root_normal"), "parent root_normal"),
        "parent root_normal")
    tangent = normalized(sub(vertices[1], vertices[0]), "triangle tangent")
    bitangent = normalized(cross(normal, tangent), "triangle bitangent")
    parent_points = parent["points"]
    parent_root = point_vec(parent_points[0], "parent root")
    penetration = max(0.0, dot(sub(parent_surface, parent_root), normal))
    child_root = sub(child_surface, mul(normal, penetration))
    length_scale = 1.0 + float(settings["length_variation"]) * (
        deterministic_signed(seed, parent_index, child_index, 3))
    phase = (
        deterministic_signed(seed, parent_index, child_index, 4) * math.pi)
    shape_scale = (
        0.4 + 0.6 * (
            deterministic_signed(seed, parent_index, child_index, 5) + 1.0)
        * 0.5)
    points = []
    for point_index, parent_point in enumerate(parent_points):
        t = point_index / (len(parent_points) - 1)
        parent_position = point_vec(
            parent_point, f"parent point {point_index}")
        displacement = mul(sub(parent_position, parent_root), length_scale)
        envelope = math.sin(math.pi * t)
        drift = float(settings["shape_variation"]) * shape_scale * envelope
        shape_offset = add(
            mul(tangent, drift * math.sin(2.0 * math.pi * t + phase)),
            mul(bitangent, drift * math.cos(2.0 * math.pi * t + phase)),
        )
        position = add(child_root, add(displacement, shape_offset))
        radius_scale = (
            float(settings["root_radius_scale"]) * (1.0 - t)
            + float(settings["tip_radius_scale"]) * t)
        radius_variation = (
            0.88 + 0.12 * (
                deterministic_signed(seed, parent_index, child_index, 6)
                + 1.0) * 0.5)
        radius = float(parent_point["radius"]) * radius_scale * radius_variation
        if radius <= 0.0:
            raise ValueError("generated child radius is not positive")
        points.append({
            "position": {
                axis: round(position[index], 12)
                for index, axis in enumerate(("x", "y", "z"))
            },
            "radius": round(radius, 12),
        })
    return {
        "strand_index": -1,
        "render_child_id": stable_child_id,
        "parent_strand_index": parent_index,
        "child_index": child_index,
        "guide_index": parent["guide_index"],
        "source_triangle_index": parent["source_triangle_index"],
        "root_barycentrics": [round(value, 12) for value in barycentrics],
        "carrier_weight": parent.get("carrier_weight"),
        "root_normal": [round(value, 12) for value in normal],
        "lod_level": lod_name,
        "points": points,
    }


def compile_children(
    authoring: dict, guide: dict, mesh: dict, lod_name: str,
) -> tuple[dict, dict]:
    children_per_parent = authoring["lod"][
        f"{lod_name}_children_per_parent"]
    final_children = authoring["lod"]["final_children_per_parent"]
    strands = []
    parent_histogram = []
    for parent_index, parent in enumerate(guide["strands"]):
        parent_histogram.append(children_per_parent)
        for child_index in range(children_per_parent):
            strand = child_strand(
                parent,
                parent_index,
                child_index,
                parent_index * final_children + child_index,
                mesh,
                authoring["children"],
                lod_name,
            )
            strand["strand_index"] = len(strands)
            strands.append(strand)
    if not strands or len(strands) > 16384:
        raise ValueError("render-child strand count exceeds runtime limit")
    points_per_strand = len(strands[0]["points"])
    primitive_count = len(strands) * (points_per_strand - 1)
    if primitive_count > 1048576:
        raise ValueError("render-child primitive count exceeds runtime limit")
    guide_summary = guide.get("groom_summary", {})
    runtime = {
        "schema_family": RUNTIME_FAMILY,
        "schema_variant": RUNTIME_VARIANT,
        "schema_version": 1,
        "asset_id": authoring["asset_id"],
        "source_binding": copy.deepcopy(guide["source_binding"]),
        "render_children_summary": {
            "lod_level": lod_name,
            "parent_asset_id": guide["asset_id"],
            "parent_strand_count": len(guide["strands"]),
            "children_per_parent": children_per_parent,
            "render_child_count": len(strands),
            "guide_count": guide_summary.get("guide_count"),
            "parents_included_as_render_curves": False,
        },
        "strands": strands,
    }
    radii = [
        point["radius"] for strand in strands for point in strand["points"]]
    receipt = {
        "schema": RECEIPT_SCHEMA,
        "schema_version": 1,
        "asset_id": authoring["asset_id"],
        **copy.deepcopy(authoring["binding"]),
        "lod_level": lod_name,
        "parent_strand_count": len(guide["strands"]),
        "children_per_parent": children_per_parent,
        "render_child_count": len(strands),
        "points_per_strand": points_per_strand,
        "control_point_count": len(strands) * points_per_strand,
        "primitive_count": primitive_count,
        "minimum_radius": min(radii),
        "maximum_radius": max(radii),
        "parent_histogram": parent_histogram,
        "exact_guide_and_source_binding": True,
        "source_triangle_mapping_retained": True,
        "root_barycentrics_valid": True,
        "deterministic_lod_subset_by_child_id": True,
        "parents_included_as_render_curves": False,
        "replaceable_serialized_curve_asset": True,
        "hair_bsdf_added": False,
    }
    return runtime, receipt


def command_init(args: argparse.Namespace) -> int:
    document = default_authoring(
        args.asset_id,
        binding_from_files(args.guide_asset.resolve(), args.mesh.resolve()))
    for assignment in args.set:
        path, value = parse_set(assignment)
        set_path(document, path, value)
    validate_authoring(document)
    write_atomic(args.output.resolve(), document)
    print(json.dumps({
        "status": "ok",
        "authoring": str(args.output.resolve()),
        "authoring_sha256": sha_file(args.output.resolve()),
    }, sort_keys=True))
    return 0


def command_edit(args: argparse.Namespace) -> int:
    source = args.input.resolve()
    if args.expect_sha256 and sha_file(source) != args.expect_sha256:
        raise ValueError("stale authoring SHA-256")
    document = load(source)
    validate_authoring(document)
    for assignment in args.set:
        path, value = parse_set(assignment)
        set_path(document, path, value)
    validate_authoring(document)
    write_atomic(args.output.resolve(), document)
    print(json.dumps({
        "status": "ok",
        "authoring": str(args.output.resolve()),
        "authoring_sha256": sha_file(args.output.resolve()),
    }, sort_keys=True))
    return 0


def command_inspect(args: argparse.Namespace) -> int:
    document = load(args.input.resolve())
    validate_authoring(document)
    print(json.dumps({
        **document,
        "authoring_sha256": sha_file(args.input.resolve()),
        "editable_fields": sorted(EDITABLE),
    }, indent=2, sort_keys=True))
    return 0


def command_compile(args: argparse.Namespace) -> int:
    authoring_path = args.authoring.resolve()
    guide_path = args.guide_asset.resolve()
    mesh_path = args.mesh.resolve()
    authoring = load(authoring_path)
    guide = load(guide_path)
    mesh = load(mesh_path)
    validate_authoring(authoring)
    validate_guide_asset(guide)
    if authoring["binding"] != binding_from_files(guide_path, mesh_path):
        raise ValueError("authoring binding does not match guide/source")
    runtime, receipt = compile_children(
        authoring, guide, mesh, args.lod)
    write_atomic(args.output.resolve(), runtime)
    receipt["authoring_sha256"] = sha_file(authoring_path)
    receipt["runtime_asset_sha256"] = sha_file(args.output.resolve())
    write_atomic(args.receipt.resolve(), receipt)
    print(json.dumps(receipt, sort_keys=True))
    return 0


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__)
    commands = root.add_subparsers(dest="command", required=True)
    init = commands.add_parser("init")
    init.add_argument("--guide-asset", type=pathlib.Path, required=True)
    init.add_argument("--mesh", type=pathlib.Path, required=True)
    init.add_argument("--asset-id", required=True)
    init.add_argument("--output", type=pathlib.Path, required=True)
    init.add_argument("--set", action="append", default=[])
    init.set_defaults(function=command_init)
    edit = commands.add_parser("edit")
    edit.add_argument("--input", type=pathlib.Path, required=True)
    edit.add_argument("--output", type=pathlib.Path, required=True)
    edit.add_argument("--expect-sha256")
    edit.add_argument("--set", action="append", default=[], required=True)
    edit.set_defaults(function=command_edit)
    inspect = commands.add_parser("inspect")
    inspect.add_argument("--input", type=pathlib.Path, required=True)
    inspect.set_defaults(function=command_inspect)
    compile_parser = commands.add_parser("compile")
    compile_parser.add_argument(
        "--authoring", type=pathlib.Path, required=True)
    compile_parser.add_argument(
        "--guide-asset", type=pathlib.Path, required=True)
    compile_parser.add_argument("--mesh", type=pathlib.Path, required=True)
    compile_parser.add_argument("--lod", choices=LOD_NAMES, required=True)
    compile_parser.add_argument("--output", type=pathlib.Path, required=True)
    compile_parser.add_argument("--receipt", type=pathlib.Path, required=True)
    compile_parser.set_defaults(function=command_compile)
    return root


def main() -> int:
    try:
        args = parser().parse_args()
        return args.function(args)
    except (
        OSError, ValueError, KeyError, TypeError, json.JSONDecodeError,
    ) as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
