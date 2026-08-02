#!/usr/bin/env python3
"""Author deterministic carrier-aware guide/clump curve assets for PSG-23E."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import os
import pathlib
import sys
import tempfile
from typing import Any

SCHEMA = "ray_tracing.procedural_carrier_curve_groom_authoring_v1"
RECEIPT_SCHEMA = "ray_tracing.procedural_carrier_curve_groom_receipt"
RUNTIME_FAMILY = "codework_curve_asset"
RUNTIME_VARIANT = "curve_asset_runtime_v1"

EDITABLE = {
    "groom.selection_threshold",
    "groom.strand_count",
    "groom.guide_count",
    "groom.points_per_strand",
    "groom.length",
    "groom.length_variation",
    "groom.root_radius",
    "groom.tip_radius",
    "groom.root_penetration",
    "groom.lift",
    "groom.comb_direction",
    "groom.comb_strength",
    "groom.part_axis",
    "groom.part_strength",
    "groom.bend",
    "groom.curl",
    "groom.clump_strength",
    "groom.clump_tip_spread",
    "groom.seed",
}


def canonical_bytes(document: dict) -> bytes:
    return (
        json.dumps(document, indent=2, sort_keys=True, allow_nan=False) + "\n"
    ).encode("utf-8")


def sha_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha_file(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_atomic(path: pathlib.Path, document: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    data = canonical_bytes(document)
    descriptor, temporary = tempfile.mkstemp(
        prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
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


def length(a: list[float]) -> float:
    return math.sqrt(dot(a, a))


def normalized(a: list[float], field: str) -> list[float]:
    magnitude = length(a)
    if not math.isfinite(magnitude) or magnitude <= 1.0e-12:
        raise ValueError(f"{field}: vector must be nonzero")
    return mul(a, 1.0 / magnitude)


def projected_tangent(
    direction: list[float], normal: list[float],
) -> list[float]:
    projected = sub(direction, mul(normal, dot(direction, normal)))
    if length(projected) <= 1.0e-10:
        reference = [0.0, 1.0, 0.0]
        if abs(dot(reference, normal)) > 0.94:
            reference = [1.0, 0.0, 0.0]
        projected = cross(reference, normal)
    return normalized(projected, "projected tangent")


def validate_authoring(document: dict) -> None:
    if document.get("schema") != SCHEMA or document.get("schema_version") != 1:
        raise ValueError("authoring schema/version mismatch")
    asset_id = document.get("asset_id")
    if not isinstance(asset_id, str) or not asset_id or len(asset_id) >= 64:
        raise ValueError("asset_id must be a nonempty string shorter than 64")
    binding = document.get("binding")
    groom = document.get("groom")
    if not isinstance(binding, dict) or not isinstance(groom, dict):
        raise ValueError("binding and groom objects are required")
    for key in (
        "source_asset_id", "source_file_sha256", "source_mesh_digest_sha256",
        "region_id", "carrier_file_sha256", "carrier_value_digest_sha256",
    ):
        if not isinstance(binding.get(key), str) or not binding[key]:
            raise ValueError(f"binding.{key} is required")
    for key in (
        "source_file_sha256", "source_mesh_digest_sha256",
        "carrier_file_sha256", "carrier_value_digest_sha256",
    ):
        if len(binding[key]) != 64:
            raise ValueError(f"binding.{key} must be SHA-256")
    integer_ranges = {
        "strand_count": (4, 4096),
        "guide_count": (1, 256),
        "points_per_strand": (4, 64),
        "seed": (0, 2**31 - 1),
    }
    for key, limits in integer_ranges.items():
        value = groom.get(key)
        if (
            not isinstance(value, int) or isinstance(value, bool)
            or value < limits[0] or value > limits[1]
        ):
            raise ValueError(f"groom.{key} outside {limits}")
    if groom["guide_count"] > groom["strand_count"]:
        raise ValueError("guide_count cannot exceed strand_count")
    numeric_ranges = {
        "selection_threshold": (0.0, 1.0),
        "length": (0.01, 3.0),
        "length_variation": (0.0, 0.9),
        "root_radius": (1.0e-5, 0.25),
        "tip_radius": (1.0e-5, 0.25),
        "root_penetration": (0.0, 0.25),
        "lift": (0.05, 4.0),
        "comb_strength": (0.0, 4.0),
        "part_strength": (0.0, 4.0),
        "bend": (-2.0, 2.0),
        "curl": (0.0, 2.0),
        "clump_strength": (0.0, 1.0),
        "clump_tip_spread": (0.0, 1.0),
    }
    for key, limits in numeric_ranges.items():
        value = groom.get(key)
        if not finite_number(value) or not limits[0] <= float(value) <= limits[1]:
            raise ValueError(f"groom.{key} outside {limits}")
    if groom["tip_radius"] > groom["root_radius"]:
        raise ValueError("tip_radius cannot exceed root_radius")
    vec3(groom.get("comb_direction"), "groom.comb_direction")
    vec3(groom.get("part_axis"), "groom.part_axis")
    normalized(vec3(groom["comb_direction"], "groom.comb_direction"),
               "groom.comb_direction")
    normalized(vec3(groom["part_axis"], "groom.part_axis"),
               "groom.part_axis")


def default_groom() -> dict:
    return {
        "selection_threshold": 0.56,
        "strand_count": 112,
        "guide_count": 14,
        "points_per_strand": 9,
        "length": 0.54,
        "length_variation": 0.18,
        "root_radius": 0.010,
        "tip_radius": 0.0025,
        "root_penetration": 0.008,
        "lift": 1.0,
        "comb_direction": [0.0, -1.0, 0.0],
        "comb_strength": 0.35,
        "part_axis": [1.0, 0.0, 0.0],
        "part_strength": 0.35,
        "bend": 0.16,
        "curl": 0.035,
        "clump_strength": 0.55,
        "clump_tip_spread": 0.035,
        "seed": 23005,
    }


def binding_from_files(mesh_path: pathlib.Path, region_path: pathlib.Path) -> dict:
    mesh = load(mesh_path)
    region = load(region_path)
    if (
        mesh.get("schema_family") != "codework_geometry"
        or mesh.get("schema_variant") != "mesh_asset_runtime_v1"
        or mesh.get("schema_version") != 1
    ):
        raise ValueError("source mesh must be mesh_asset_runtime_v1")
    if (
        region.get("schema")
        != "ray_tracing.procedural_imported_surface_region"
        or region.get("schema_version") != 1
        or not region.get("topology_unchanged")
        or not region.get("source_triangle_provenance_retained")
    ):
        raise ValueError("carrier must be a valid immutable PSG-19 region")
    mesh_id = mesh.get("asset_id")
    mesh_file_sha = sha_file(mesh_path)
    mesh_document = mesh.get("mesh", {})
    if (
        region.get("source_asset_id") != mesh_id
        or region.get("source_file_digest_sha256") != mesh_file_sha
        or region.get("vertex_count") != mesh_document.get("vertex_count")
        or region.get("triangle_count") != mesh_document.get("triangle_count")
    ):
        raise ValueError("carrier does not exactly bind the source mesh")
    return {
        "source_asset_id": mesh_id,
        "source_file_sha256": mesh_file_sha,
        "source_mesh_digest_sha256": region["source_mesh_digest_sha256"],
        "region_id": region["region_id"],
        "carrier_file_sha256": sha_file(region_path),
        "carrier_value_digest_sha256": region["value_digest_sha256"],
    }


def validate_binding(
    authoring: dict, mesh_path: pathlib.Path, region_path: pathlib.Path,
) -> tuple[dict, dict]:
    actual = binding_from_files(mesh_path, region_path)
    if authoring["binding"] != actual:
        raise ValueError("authoring binding does not match source/carrier")
    return load(mesh_path), load(region_path)


def triangle_candidate(
    mesh: dict, weights: list, triangle_index: int,
) -> dict:
    triangle = mesh["mesh"]["triangles"][triangle_index]
    indices = [triangle[key] for key in ("a", "b", "c")]
    positions = [
        [
            float(mesh["mesh"]["vertices"][index]["x"]),
            float(mesh["mesh"]["vertices"][index]["y"]),
            float(mesh["mesh"]["vertices"][index]["z"]),
        ]
        for index in indices
    ]
    normal = normalized(
        cross(sub(positions[1], positions[0]),
              sub(positions[2], positions[0])),
        f"triangle {triangle_index} normal",
    )
    return {
        "triangle_index": triangle_index,
        "weight": sum(float(weights[index]) for index in indices) / 3.0,
        "position": [
            sum(position[axis] for position in positions) / 3.0
            for axis in range(3)
        ],
        "normal": normal,
    }


def distance2(a: list[float], b: list[float]) -> float:
    return sum((a[index] - b[index]) ** 2 for index in range(3))


def farthest_select(candidates: list[dict], count: int) -> list[dict]:
    if count > len(candidates):
        raise ValueError(
            f"requested {count} strands but carrier provides "
            f"{len(candidates)} qualifying triangles")
    remaining = list(candidates)
    first = max(
        remaining,
        key=lambda item: (
            item["weight"], item["position"][2], -item["triangle_index"]))
    selected = [first]
    remaining.remove(first)
    while len(selected) < count:
        choice = max(
            remaining,
            key=lambda item: (
                min(distance2(item["position"], other["position"])
                    for other in selected) * (0.5 + item["weight"]),
                item["weight"],
                -item["triangle_index"],
            ),
        )
        selected.append(choice)
        remaining.remove(choice)
    return sorted(selected, key=lambda item: item["triangle_index"])


def deterministic_signed(seed: int, index: int, channel: int) -> float:
    digest = hashlib.sha256(
        f"{seed}:{index}:{channel}".encode("ascii")).digest()
    integer = int.from_bytes(digest[:8], "big")
    return (integer / float(2**64 - 1)) * 2.0 - 1.0


def direction_for_root(root: dict, groom: dict) -> tuple[list[float], list[float]]:
    normal = root["normal"]
    comb = projected_tangent(
        normalized(vec3(groom["comb_direction"], "comb_direction"),
                   "comb_direction"),
        normal,
    )
    part_axis = normalized(
        vec3(groom["part_axis"], "part_axis"), "part_axis")
    side = 1.0 if dot(root["position"], part_axis) >= 0.0 else -1.0
    part = projected_tangent(mul(part_axis, side), normal)
    direction = normalized(add(
        mul(normal, float(groom["lift"])),
        add(mul(comb, float(groom["comb_strength"])),
            mul(part, float(groom["part_strength"]))),
    ), "groom direction")
    return direction, comb


def curve_points(
    root: dict,
    guide: dict,
    strand_index: int,
    groom: dict,
) -> list[dict]:
    point_count = groom["points_per_strand"]
    seed = groom["seed"]
    variation = deterministic_signed(seed, strand_index, 0)
    strand_length = groom["length"] * (
        1.0 + groom["length_variation"] * variation)
    guide_variation = deterministic_signed(
        seed, guide["selection_index"], 0)
    guide_length = groom["length"] * (
        1.0 + groom["length_variation"] * guide_variation)
    direction, tangent = direction_for_root(root, groom)
    guide_direction, guide_tangent = direction_for_root(guide, groom)
    bitangent = normalized(cross(root["normal"], tangent), "bitangent")
    guide_bitangent = normalized(
        cross(guide["normal"], guide_tangent), "guide bitangent")
    penetration = float(groom["root_penetration"])
    root_embedded = sub(root["position"], mul(root["normal"], penetration))
    guide_embedded = sub(
        guide["position"], mul(guide["normal"], penetration))
    phase = deterministic_signed(seed, strand_index, 1) * math.pi
    spread_phase = deterministic_signed(seed, strand_index, 2) * math.pi
    points = []
    for point_index in range(point_count):
        t = point_index / (point_count - 1)
        smooth = t * t * (3.0 - 2.0 * t)
        release = min(1.0, t * (point_count - 1))
        curl_wave = math.sin(math.pi * t)
        curl_a = groom["curl"] * curl_wave * math.sin(
            math.pi * 2.0 * t + phase)
        curl_b = groom["curl"] * curl_wave * math.cos(
            math.pi * 2.0 * t + phase)
        independent = add(
            root_embedded,
            add(
                mul(root["normal"], penetration * release),
                add(
                    mul(direction, strand_length * t),
                    add(
                        mul(tangent, groom["bend"] * strand_length
                            * smooth * smooth),
                        add(mul(tangent, curl_a),
                            mul(bitangent, curl_b)),
                    ),
                ),
            ),
        )
        guide_curve = add(
            guide_embedded,
            add(
                mul(guide["normal"], penetration * release),
                add(
                    mul(guide_direction, guide_length * t),
                    mul(guide_tangent, groom["bend"] * guide_length
                        * smooth * smooth),
                ),
            ),
        )
        root_offset = sub(root_embedded, guide_embedded)
        spread_direction = add(
            mul(guide_tangent, math.cos(spread_phase)),
            mul(guide_bitangent, math.sin(spread_phase)),
        )
        target = add(
            guide_curve,
            add(
                mul(root_offset, 1.0 - smooth),
                mul(spread_direction,
                    groom["clump_tip_spread"] * smooth),
            ),
        )
        blend = groom["clump_strength"] * smooth
        position = add(mul(independent, 1.0 - blend), mul(target, blend))
        radius = (
            groom["root_radius"] * (1.0 - t)
            + groom["tip_radius"] * t
        )
        points.append({
            "position": {
                axis: round(position[index], 12)
                for index, axis in enumerate(("x", "y", "z"))
            },
            "radius": round(radius, 12),
        })
    return points


def compile_asset(
    authoring: dict, mesh: dict, region: dict,
) -> tuple[dict, dict]:
    groom = authoring["groom"]
    weights = region.get("vertex_weights")
    triangles = mesh.get("mesh", {}).get("triangles")
    if (
        not isinstance(weights, list)
        or not isinstance(triangles, list)
        or len(weights) != mesh["mesh"].get("vertex_count")
        or len(triangles) != mesh["mesh"].get("triangle_count")
    ):
        raise ValueError("source/carrier topology arrays are invalid")
    candidates = []
    for triangle_index in range(len(triangles)):
        candidate = triangle_candidate(mesh, weights, triangle_index)
        if candidate["weight"] >= groom["selection_threshold"]:
            candidates.append(candidate)
    roots = farthest_select(candidates, groom["strand_count"])
    for selection_index, root in enumerate(roots):
        root["selection_index"] = selection_index
    guides = farthest_select(roots, groom["guide_count"])
    guide_lookup = {
        guide["triangle_index"]: index for index, guide in enumerate(guides)
    }
    for guide in guides:
        guide["guide_index"] = guide_lookup[guide["triangle_index"]]
    strands = []
    histogram = [0 for _ in guides]
    for strand_index, root in enumerate(roots):
        guide = min(
            guides,
            key=lambda item: (
                distance2(root["position"], item["position"]),
                item["triangle_index"],
            ),
        )
        guide_index = guide_lookup[guide["triangle_index"]]
        histogram[guide_index] += 1
        points = curve_points(root, guide, strand_index, groom)
        strands.append({
            "strand_index": strand_index,
            "guide_index": guide_index,
            "source_triangle_index": root["triangle_index"],
            "root_barycentrics": [
                0.333333333333, 0.333333333333, 0.333333333334],
            "carrier_weight": round(root["weight"], 12),
            "root_normal": [round(value, 12) for value in root["normal"]],
            "points": points,
        })
    runtime = {
        "schema_family": RUNTIME_FAMILY,
        "schema_variant": RUNTIME_VARIANT,
        "schema_version": 1,
        "asset_id": authoring["asset_id"],
        "source_binding": copy.deepcopy(authoring["binding"]),
        "groom_summary": {
            "guide_count": len(guides),
            "clump_histogram": histogram,
        },
        "strands": strands,
    }
    receipt = {
        "schema": RECEIPT_SCHEMA,
        "schema_version": 1,
        "asset_id": authoring["asset_id"],
        **copy.deepcopy(authoring["binding"]),
        "candidate_triangle_count": len(candidates),
        "strand_count": len(strands),
        "guide_count": len(guides),
        "points_per_strand": groom["points_per_strand"],
        "control_point_count": len(strands) * groom["points_per_strand"],
        "clump_histogram": histogram,
        "minimum_carrier_weight": min(
            strand["carrier_weight"] for strand in strands),
        "maximum_carrier_weight": max(
            strand["carrier_weight"] for strand in strands),
        "exact_source_and_carrier_binding": True,
        "root_triangle_mapping_retained": True,
        "root_barycentrics_valid": True,
        "finite_positive_curve_asset": True,
        "guide_assignment_complete": sum(histogram) == len(strands),
        "replaceable_serialized_curve_asset": True,
        "hair_bsdf_added": False,
    }
    return runtime, receipt


def parse_set(text: str) -> tuple[str, Any]:
    if "=" not in text:
        raise ValueError("--set requires dotted.path=JSON")
    path, raw = text.split("=", 1)
    if path not in EDITABLE:
        raise ValueError(f"{path}: field is not editable")
    return path, json.loads(raw)


def set_path(document: dict, path: str, value: Any) -> None:
    parts = path.split(".")
    target = document
    for part in parts[:-1]:
        target = target[part]
    target[parts[-1]] = value


def command_init(args: argparse.Namespace) -> int:
    document = {
        "schema": SCHEMA,
        "schema_version": 1,
        "asset_id": args.asset_id,
        "binding": binding_from_files(args.mesh.resolve(),
                                      args.region.resolve()),
        "groom": default_groom(),
    }
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
        "schema": document["schema"],
        "schema_version": 1,
        "asset_id": document["asset_id"],
        "authoring_sha256": sha_file(args.input.resolve()),
        "binding": document["binding"],
        "groom": document["groom"],
        "editable_fields": sorted(EDITABLE),
    }, indent=2, sort_keys=True))
    return 0


def command_compile(args: argparse.Namespace) -> int:
    authoring_path = args.authoring.resolve()
    mesh_path = args.mesh.resolve()
    region_path = args.region.resolve()
    document = load(authoring_path)
    validate_authoring(document)
    mesh, region = validate_binding(document, mesh_path, region_path)
    runtime, receipt = compile_asset(document, mesh, region)
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
    init.add_argument("--mesh", type=pathlib.Path, required=True)
    init.add_argument("--region", type=pathlib.Path, required=True)
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
    compile_parser.add_argument("--mesh", type=pathlib.Path, required=True)
    compile_parser.add_argument("--region", type=pathlib.Path, required=True)
    compile_parser.add_argument("--output", type=pathlib.Path, required=True)
    compile_parser.add_argument("--receipt", type=pathlib.Path, required=True)
    compile_parser.set_defaults(function=command_compile)
    return root


def main() -> int:
    try:
        args = parser().parse_args()
        return args.function(args)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
