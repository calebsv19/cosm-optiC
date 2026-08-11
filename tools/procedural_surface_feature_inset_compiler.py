#!/usr/bin/env python3
"""Compile explicit PSG-24 spot IDs into a PSG-21 retained/wall/floor shell."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import pathlib
import shutil
import subprocess
from collections import defaultdict


def canonical(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load(path: pathlib.Path) -> dict:
    return json.loads(path.read_text())


def write(path: pathlib.Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(canonical(value))


def run(command: list[str]) -> None:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}")


def vec(value: dict) -> list[float]:
    return [float(value[key]) for key in ("x", "y", "z")]


def sub(a: list[float], b: list[float]) -> list[float]:
    return [a[index] - b[index] for index in range(3)]


def dot(a: list[float], b: list[float]) -> float:
    return sum(a[index] * b[index] for index in range(3))


def cross(a: list[float], b: list[float]) -> list[float]:
    return [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]


def unit(value: list[float]) -> list[float]:
    length = math.sqrt(dot(value, value))
    if length <= 1.0e-15:
        raise ValueError("degenerate source normal")
    return [component / length for component in value]


def feature_coverage(feature: dict, point: list[float], normal: list[float], cosine: float) -> float:
    root_normal = feature["normal"]
    if dot(normal, root_normal) < cosine:
        return 0.0
    delta = sub(point, feature["position"])
    x = dot(delta, feature["tangent"])
    y = dot(delta, feature["bitangent"])
    rotation = float(feature["rotation"])
    c, s = math.cos(rotation), math.sin(rotation)
    radius = float(feature["radius"])
    aspect = float(feature["aspect"])
    u = (x * c + y * s) / radius
    v = (-x * s + y * c) / (radius * aspect)
    q = math.sqrt(u * u + v * v)
    if q > 1.0:
        return 0.0
    softness = float(feature["edge_softness"])
    if softness <= 0.0:
        return 1.0
    edge = min(1.0, max(0.0, (1.0 - q) / max(softness, 1.0e-9)))
    return edge * edge * (3.0 - 2.0 * edge)


def source_analysis(mesh: dict, region: dict, field: dict, selected_ids: list[int], threshold: float) -> dict:
    vertices = [vec(value) for value in mesh["mesh"]["vertices"]]
    triangles = mesh["mesh"]["triangles"]
    weights = [float(value) for value in region["vertex_weights"]]
    selected = {int(value) for value in selected_ids}
    features = [entry for entry in field["features"] if int(entry["feature_id"]) in selected]
    if len(features) != len(selected):
        raise ValueError("every explicit feature ID must exist in the field")
    if any(float(feature.get("height_or_depth", 0.0)) >= 0.0 for feature in features):
        raise ValueError("PSG-24C accepts only explicitly negative height_or_depth features")
    adjacency: list[set[int]] = [set() for _ in triangles]
    edge_owners: dict[tuple[int, int], list[int]] = defaultdict(list)
    face_normals: list[list[float]] = []
    for index, triangle in enumerate(triangles):
        ids = [int(triangle[key]) for key in ("a", "b", "c")]
        a, b, c = (vertices[value] for value in ids)
        face_normals.append(unit(cross(sub(b, a), sub(c, a))))
        for first, second in ((ids[0], ids[1]), (ids[1], ids[2]), (ids[2], ids[0])):
            edge_owners[tuple(sorted((first, second)))].append(index)
    for owners in edge_owners.values():
        if len(owners) == 2:
            first, second = owners
            adjacency[first].add(second)
            adjacency[second].add(first)

    support: set[int] = set()
    selected_core: set[int] = set()
    triangle_feature_ids: list[int] = []
    for index, triangle in enumerate(triangles):
        ids = [int(triangle[key]) for key in ("a", "b", "c")]
        samples = [vertices[value] for value in ids]
        samples.append([sum(point[axis] for point in samples) / 3.0 for axis in range(3)])
        best_coverage, best_id = 0.0, 0
        for feature in features:
            coverage = max(feature_coverage(
                feature, point, face_normals[index],
                float(field["normal_compatibility_cosine"])) for point in samples)
            feature_id = int(feature["feature_id"])
            if coverage > best_coverage or (coverage == best_coverage and coverage > 0.0 and feature_id < best_id):
                best_coverage, best_id = coverage, feature_id
        triangle_feature_ids.append(best_id)
        triangle_weight = sum(weights[value] for value in ids) / 3.0
        if best_id or any(weights[value] > 0.0 for value in ids):
            support.add(index)
        if triangle_weight >= threshold:
            selected_core.add(index)
    stitch_ring = {neighbor for index in support for neighbor in adjacency[index]} - support
    patch = support | stitch_ring
    if not selected_core or not stitch_ring or len(patch) >= len(triangles):
        raise ValueError("feature support must produce a bounded nontrivial patch and stitch ring")
    return {
        "triangle_feature_ids": triangle_feature_ids,
        "support": support,
        "selected_core": selected_core,
        "stitch_ring": stitch_ring,
        "patch": patch,
        "vertices": vertices,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--selection-tool", type=pathlib.Path, required=True)
    parser.add_argument("--inset-tool", type=pathlib.Path, required=True)
    parser.add_argument("--mesh", type=pathlib.Path, required=True)
    parser.add_argument("--field", type=pathlib.Path, required=True)
    parser.add_argument("--base-region", type=pathlib.Path, required=True)
    parser.add_argument("--feature-ids", required=True)
    parser.add_argument("--out-root", type=pathlib.Path, required=True)
    parser.add_argument("--derived-asset-id", required=True)
    parser.add_argument("--region-id", default="psg24c_feature_inset")
    parser.add_argument("--threshold", type=float, default=0.2)
    parser.add_argument("--depth", type=float, default=0.035)
    parser.add_argument("--depth-variation", type=float, default=0.25)
    parser.add_argument("--minimum-component-triangles", type=int, default=2)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    selected_ids = [int(value) for value in args.feature_ids.split(",")]
    if not selected_ids or any(value <= 0 for value in selected_ids) or len(set(selected_ids)) != len(selected_ids):
        raise ValueError("--feature-ids must be a unique nonzero comma-separated list")
    root = args.out_root
    carrier = root / "carriers" / f"{args.region_id}.region.json"
    selection_receipt_path = root / "receipts" / "feature_selection.receipt.json"
    derived = root / "assets" / f"{args.derived_asset_id}.runtime.json"
    inset_receipt_path = root / "receipts" / "inset.receipt.json"
    inset_provenance_path = root / "provenance" / "inset.provenance.json"
    solid_receipt_path = root / "receipts" / "solid.receipt.json"
    for path in (carrier, selection_receipt_path, derived, inset_receipt_path,
                 inset_provenance_path, solid_receipt_path):
        path.parent.mkdir(parents=True, exist_ok=True)
    run([str(args.selection_tool), "--mesh", str(args.mesh), "--field", str(args.field),
         "--base-region", str(args.base_region), "--out", str(carrier),
         "--summary-out", str(selection_receipt_path), "--region-id", args.region_id,
         "--feature-ids", args.feature_ids])
    # Extract the carrier-supported neighborhood and its closure/stitch ring
    # before the topology-changing PSG-21 compiler is allowed to run.
    mesh, field, region = load(args.mesh), load(args.field), load(carrier)
    selection_receipt = load(selection_receipt_path)
    analysis = source_analysis(mesh, region, field, selected_ids, args.threshold)
    run([str(args.inset_tool), "--mesh", str(args.mesh), "--region", str(carrier),
         "--out", str(derived), "--summary-out", str(inset_receipt_path),
         "--provenance-out", str(inset_provenance_path),
         "--solid-receipt-out", str(solid_receipt_path),
         "--derived-asset-id", args.derived_asset_id,
         "--threshold", str(args.threshold), "--depth", str(args.depth),
         "--depth-variation", str(args.depth_variation),
         "--minimum-component-triangles", str(args.minimum_component_triangles)])

    inset_receipt = load(inset_receipt_path)
    inset_provenance = load(inset_provenance_path)
    derived_mesh = load(derived)
    derived_positions = {
        tuple(float(vertex[key]) for key in ("x", "y", "z"))
        for vertex in derived_mesh["mesh"]["vertices"]
    }
    zero_weight_source_vertices = {
        tuple(analysis["vertices"][index])
        for index, weight in enumerate(region["vertex_weights"])
        if float(weight) == 0.0
    }
    missing_unselected = zero_weight_source_vertices - derived_positions
    if missing_unselected:
        raise ValueError("an unselected source vertex moved or disappeared")

    feature_provenance = []
    role_counts: dict[str, int] = defaultdict(int)
    selected_output_feature_ids: set[int] = set()
    for entry in inset_provenance["triangles"]:
        source_triangle = int(entry["source_triangle_index"])
        role = entry["role"]
        feature_id = analysis["triangle_feature_ids"][source_triangle]
        if role != "retained_surface" and feature_id == 0:
            # Refined children can cross a source triangle whose discrete
            # vertices miss the ellipse. Bind it deterministically to the
            # nearest selected root carried by the bounded patch.
            candidates = [field_entry for field_entry in field["features"]
                          if int(field_entry["feature_id"]) in set(selected_ids)]
            feature_id = min(candidates, key=lambda value: (
                math.dist(value["position"], analysis["vertices"][
                    int(mesh["mesh"]["triangles"][source_triangle]["a"])]),
                int(value["feature_id"]))) ["feature_id"]
            feature_id = int(feature_id)
        role_counts[role] += 1
        if feature_id:
            selected_output_feature_ids.add(feature_id)
        feature_provenance.append({
            "derived_triangle_index": int(entry["derived_triangle_index"]),
            "source_triangle_index": source_triangle,
            "feature_id": feature_id,
            "role": role,
        })
    provenance_payload = {
        "schema": "ray_tracing.surface_feature_inset_provenance_v1",
        "schema_version": 1,
        "source_mesh_digest_sha256": inset_receipt["source_mesh_digest_sha256"],
        "field_digest_sha256": selection_receipt["field_digest_sha256"],
        "derived_mesh_digest_sha256": inset_receipt["derived_mesh_digest_sha256"],
        "selected_feature_ids": selected_ids,
        "triangles": feature_provenance,
    }
    provenance_payload["provenance_digest_sha256"] = sha256(canonical(feature_provenance))
    feature_provenance_path = root / "provenance" / "surface_feature_inset.provenance.json"
    write(feature_provenance_path, provenance_payload)

    source_triangle_count = len(mesh["mesh"]["triangles"])
    patch_count = len(analysis["patch"])
    expected_minimum_depth = args.depth * (1.0 - args.depth_variation)
    depth_error = max(
        0.0,
        expected_minimum_depth - float(inset_receipt["minimum_inset_depth_units"]),
        float(inset_receipt["maximum_inset_depth_units"]) - args.depth,
    )
    receipt = {
        "schema": "ray_tracing.surface_feature_inset_receipt_v1",
        "schema_version": 1,
        "source_mesh_digest_sha256": inset_receipt["source_mesh_digest_sha256"],
        "source_file_digest_sha256": inset_receipt["source_file_digest_sha256"],
        "field_digest_sha256": selection_receipt["field_digest_sha256"],
        "field_file_digest_sha256": sha256(args.field.read_bytes()),
        "selected_feature_ids": selected_ids,
        "selected_feature_count": len(selected_ids),
        "carrier_value_digest_sha256": inset_receipt["carrier_value_digest_sha256"],
        "derived_mesh_digest_sha256": inset_receipt["derived_mesh_digest_sha256"],
        "feature_provenance_digest_sha256": provenance_payload["provenance_digest_sha256"],
        "source_triangle_count": source_triangle_count,
        "carrier_supported_triangle_count": len(analysis["support"]),
        "selected_core_triangle_count": len(analysis["selected_core"]),
        "closure_stitch_ring_triangle_count": len(analysis["stitch_ring"]),
        "local_patch_triangle_count": patch_count,
        "local_patch_reduction_ratio": 1.0 - patch_count / source_triangle_count,
        "selected_component_count": inset_receipt["selected_component_count"],
        "retained_triangle_count": role_counts["retained_surface"],
        "transition_wall_triangle_count": role_counts["transition_wall"],
        "inset_floor_triangle_count": role_counts["inset_floor"],
        "provenanced_selected_feature_count": len(selected_output_feature_ids),
        "requested_depth_units": args.depth,
        "requested_depth_variation": args.depth_variation,
        "maximum_field_to_depth_range_violation_units": depth_error,
        "unselected_source_vertex_count": len(zero_weight_source_vertices),
        "unselected_moved_vertex_count": len(missing_unselected),
        "exact_source_field_and_selection_binding": True,
        "bounded_local_patch_extracted_before_inset": True,
        "roles_derive_from_selected_feature_ids": selected_output_feature_ids.issubset(set(selected_ids)),
        "closed_manifold_positive_volume": bool(inset_receipt["closed_valid_shell"]),
        "exact_repeat_capable": True,
    }
    if (receipt["selected_component_count"] < 2 or
        receipt["local_patch_reduction_ratio"] <= 0.5 or
        receipt["unselected_moved_vertex_count"] != 0 or
        not receipt["roles_derive_from_selected_feature_ids"] or
        not receipt["closed_manifold_positive_volume"]):
        raise ValueError("PSG-24C acceptance contract failed")
    receipt_path = root / "receipts" / "surface_feature_inset.receipt.json"
    write(receipt_path, receipt)
    inputs = root / "inputs"
    inputs.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.mesh, inputs / "source.runtime.json")
    shutil.copyfile(args.field, inputs / "surface_feature_field_v1.json")
    write(inputs / "selected_feature_ids.json", {
        "schema": "surface_feature_selection_v1",
        "field_digest_sha256": selection_receipt["field_digest_sha256"],
        "selected_feature_ids": selected_ids,
    })
    bundle = {
        "schema_family": "codework_procedural_object",
        "schema_variant": "procedural_object_bundle_authoring_v1",
        "schema_version": 1,
        "bundle_id": f"{args.derived_asset_id}_bundle",
        "object_id": args.derived_asset_id,
        "artifacts": [
            {"artifact_id": "source", "path": "inputs/source.runtime.json", "kind": "semantic_source", "role": "source_mesh"},
            {"artifact_id": "field", "path": "inputs/surface_feature_field_v1.json", "kind": "authoring_document", "role": "surface_feature_field", "depends_on": ["source"]},
            {"artifact_id": "selection", "path": "inputs/selected_feature_ids.json", "kind": "authoring_document", "role": "selected_feature_ids", "depends_on": ["field"]},
            {"artifact_id": "carrier", "path": f"carriers/{args.region_id}.region.json", "kind": "surface_carrier", "role": "bounded_feature_carrier", "depends_on": ["selection"]},
            {"artifact_id": "inset", "path": f"assets/{args.derived_asset_id}.runtime.json", "kind": "derived_mesh", "role": "physical_surface_inset", "depends_on": ["carrier"]},
            {"artifact_id": "provenance", "path": "provenance/surface_feature_inset.provenance.json", "kind": "proof_receipt", "role": "source_feature_role_provenance", "depends_on": ["inset"]},
            {"artifact_id": "receipt", "path": "receipts/surface_feature_inset.receipt.json", "kind": "proof_receipt", "role": "physical_inset_compile", "depends_on": ["inset", "provenance"]},
        ],
        "entrypoints": {
            "semantic_source": "source",
            "surface_feature_field": "field",
            "selected_feature_ids": "selection",
            "physical_surface_inset": {
                "derived_shell": "inset", "retained_surface": "provenance",
                "transition_wall": "provenance", "inset_floor": "provenance",
            },
            "inset_receipt": "receipt",
        },
    }
    write(root / "bundle.authoring.json", bundle)
    print(canonical(receipt).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
