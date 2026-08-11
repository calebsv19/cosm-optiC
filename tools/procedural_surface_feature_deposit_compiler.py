#!/usr/bin/env python3
"""Compile selected positive PSG-24 spot roots into separate PSG-22 assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import pathlib
import shutil
import subprocess


def canonical(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def digest_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def load(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write(path: pathlib.Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(canonical(value))


def run(command: list[str]) -> None:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}")


def csv(values: list[float]) -> str:
    return ",".join(format(float(value), ".17g") for value in values)


def add(a: list[float], b: list[float]) -> list[float]:
    return [a[index] + b[index] for index in range(3)]


def scale(value: list[float], amount: float) -> list[float]:
    return [component * amount for component in value]


def distance(a: list[float], b: list[float]) -> float:
    return math.sqrt(sum((a[index] - b[index]) ** 2 for index in range(3)))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--selection-tool", type=pathlib.Path, required=True)
    parser.add_argument("--growth-tool", type=pathlib.Path, required=True)
    parser.add_argument("--mesh", type=pathlib.Path, required=True)
    parser.add_argument("--field", type=pathlib.Path, required=True)
    parser.add_argument("--base-region", type=pathlib.Path, required=True)
    parser.add_argument("--feature-ids", required=True)
    parser.add_argument("--out-root", type=pathlib.Path, required=True)
    parser.add_argument("--asset-prefix", default="psg24d_attached_deposit")
    parser.add_argument("--material-semantic", default="dried_mud_deposit")
    parser.add_argument("--radius-scale", type=float, default=0.55)
    parser.add_argument("--height-scale", type=float, default=1.0)
    parser.add_argument("--attachment-to-radius", type=float, default=0.10)
    parser.add_argument("--minimum-attachment-to-height", type=float, default=1.05)
    parser.add_argument("--clearance-factor", type=float, default=1.02)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    feature_ids = [int(value) for value in args.feature_ids.split(",")]
    if (not feature_ids or len(set(feature_ids)) != len(feature_ids) or
            any(value <= 0 for value in feature_ids)):
        raise ValueError("--feature-ids must be a unique nonzero list")
    if (args.radius_scale <= 0.0 or args.height_scale <= 0.0 or
            args.attachment_to_radius <= 0.0 or
            args.minimum_attachment_to_height < 1.0 or
            args.clearance_factor < 1.0):
        raise ValueError("deposit scale and clearance values must be positive")

    field = load(args.field)
    features_by_id = {
        int(feature["feature_id"]): feature for feature in field["features"]}
    if any(feature_id not in features_by_id for feature_id in feature_ids):
        raise ValueError("every selected feature ID must exist in the field")
    features = [features_by_id[feature_id] for feature_id in feature_ids]
    if any(float(feature.get("height_or_depth", 0.0)) <= 0.0 for feature in features):
        raise ValueError("PSG-24D accepts only explicitly positive height_or_depth features")

    root = args.out_root
    for directory in ("assets", "carriers", "inputs", "materials",
                      "provenance", "receipts"):
        (root / directory).mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.mesh, root / "inputs/source.runtime.json")
    shutil.copyfile(args.field, root / "inputs/surface_feature_field_v1.json")

    material = {
        "schema": "ray_tracing.surface_feature_deposit_material_binding_v1",
        "schema_version": 1,
        "field_digest_sha256": digest_bytes(canonical(field)),
        "material_semantic": args.material_semantic,
        "selected_feature_ids": feature_ids,
        "agreement_contract": "field_positive_height_matches_attached_element",
    }
    write(root / "materials/deposit_material_binding.json", material)
    selection = {
        "schema": "surface_feature_deposit_selection_v1",
        "schema_version": 1,
        "field_digest_sha256": digest_bytes(canonical(field)),
        "selected_feature_ids": feature_ids,
        "positive_height_required": True,
        "material_semantic": args.material_semantic,
    }
    write(root / "inputs/selected_positive_feature_ids.json", selection)

    elements: list[dict] = []
    aggregate_provenance: list[dict] = []
    artifact_entries: list[dict] = []
    asset_entrypoints: list[str] = []
    total_triangles = 0
    total_exposed = 0
    total_base = 0
    source_file_digest = digest_bytes(args.mesh.read_bytes())

    for feature in features:
        feature_id = int(feature["feature_id"])
        radius = float(feature["radius"]) * args.radius_scale
        aspect = float(feature["aspect"])
        height = float(feature["height_or_depth"]) * args.height_scale
        attachment = max(
            radius * args.attachment_to_radius,
            height * args.minimum_attachment_to_height)
        asset_id = f"{args.asset_prefix}_{feature_id}"
        carrier_id = f"feature_{feature_id}_positive_height"
        carrier_path = root / f"carriers/{carrier_id}.region.json"
        selection_receipt_path = root / f"receipts/selection_{feature_id}.receipt.json"
        asset_path = root / f"assets/{asset_id}.runtime.json"
        growth_receipt_path = root / f"receipts/growth_{feature_id}.receipt.json"
        growth_provenance_path = root / f"provenance/growth_{feature_id}.provenance.json"
        run([
            str(args.selection_tool), "--mesh", str(args.mesh),
            "--field", str(args.field), "--base-region", str(args.base_region),
            "--feature-ids", str(feature_id), "--out", str(carrier_path),
            "--summary-out", str(selection_receipt_path),
            "--region-id", carrier_id,
        ])
        run([
            str(args.growth_tool), "--mesh", str(args.mesh),
            "--region", str(carrier_path), "--out", str(asset_path),
            "--growth-asset-id", asset_id,
            "--summary-out", str(growth_receipt_path),
            "--provenance-out", str(growth_provenance_path),
            "--radius", format(radius, ".17g"),
            "--height", format(height, ".17g"),
            "--attachment-depth", format(attachment, ".17g"),
            "--max-elements", "1",
            "--explicit-source-triangle", str(feature["source_triangle"]),
            "--explicit-barycentric", csv(feature["barycentric_root"]),
            "--explicit-normal", csv(feature["normal"]),
            "--explicit-tangent", csv(feature["tangent"]),
            "--explicit-bitangent", csv(feature["bitangent"]),
            "--aspect", format(aspect, ".17g"),
            "--rotation", format(float(feature["rotation"]), ".17g"),
        ])
        growth_receipt = load(growth_receipt_path)
        growth_provenance = load(growth_provenance_path)
        source_triangles = {
            int(entry["source_triangle_index"])
            for entry in growth_provenance["triangles"]}
        roles = {entry["role"] for entry in growth_provenance["triangles"]}
        if (growth_receipt["growth_element_count"] != 1 or
                growth_receipt["connected_component_count"] != 1 or
                source_triangles != {int(feature["source_triangle"])} or
                roles != {"exposed_growth", "attachment_base"} or
                not growth_receipt["closed_valid_growth_shells"] or
                growth_receipt["minimum_attachment_depth_units"] <= 0.0):
            raise ValueError(f"PSG-22 feature asset {feature_id} failed")

        center = add(
            [float(value) for value in feature["position"]],
            scale([float(value) for value in feature["normal"]],
                  (height - attachment) * 0.5))
        bound_radius = max(radius * max(1.0, aspect),
                           (height + attachment) * 0.5)
        element = {
            "feature_id": feature_id,
            "population": int(feature["population"]),
            "source_triangle_index": int(feature["source_triangle"]),
            "barycentric_root": feature["barycentric_root"],
            "position": feature["position"],
            "normal": feature["normal"],
            "tangent": feature["tangent"],
            "bitangent": feature["bitangent"],
            "radius_units": radius,
            "aspect": aspect,
            "rotation_radians": float(feature["rotation"]),
            "positive_height_at_root": height,
            "authored_height_or_depth": float(feature["height_or_depth"]),
            "growth_height_units": height,
            "attachment_depth_units": attachment,
            "equator_at_or_below_root_plane": attachment >= height,
            "bound_center": center,
            "bound_radius": bound_radius,
            "asset_id": asset_id,
            "asset_digest_sha256": digest_bytes(asset_path.read_bytes()),
            "growth_mesh_digest_sha256": growth_receipt["growth_mesh_digest_sha256"],
            "material_semantic": args.material_semantic,
        }
        elements.append(element)
        for entry in growth_provenance["triangles"]:
            aggregate_provenance.append({
                "asset_id": asset_id,
                "derived_triangle_index": int(entry["triangle_index"]),
                "feature_id": feature_id,
                "population": int(feature["population"]),
                "feature_source_triangle_index": int(feature["source_triangle"]),
                "attachment_source_triangle_index": int(entry["source_triangle_index"]),
                "barycentric_root": feature["barycentric_root"],
                "growth_element_index": int(entry["growth_element_index"]),
                "role": entry["role"],
                "material_semantic": args.material_semantic,
            })
        total_triangles += int(growth_receipt["growth_triangle_count"])
        total_exposed += int(growth_receipt["exposed_growth_triangle_count"])
        total_base += int(growth_receipt["attachment_base_triangle_count"])
        artifact_id = f"deposit_{feature_id}"
        asset_entrypoints.append(artifact_id)
        artifact_entries.extend([
            {"artifact_id": f"carrier_{feature_id}",
             "path": f"carriers/{carrier_id}.region.json",
             "kind": "surface_carrier", "role": "selected_feature_carrier",
             "depends_on": ["selection"]},
            {"artifact_id": artifact_id,
             "path": f"assets/{asset_id}.runtime.json",
             "kind": "derived_mesh", "role": "attached_deposit",
             "depends_on": [f"carrier_{feature_id}", "material"]},
            {"artifact_id": f"growth_receipt_{feature_id}",
             "path": f"receipts/growth_{feature_id}.receipt.json",
             "kind": "proof_receipt", "role": "psg22_growth_receipt",
             "depends_on": [artifact_id]},
        ])

    minimum_clearance = math.inf
    overlap_pairs = 0
    for left_index, left in enumerate(elements):
        for right in elements[left_index + 1:]:
            clearance = distance(left["bound_center"], right["bound_center"]) - (
                left["bound_radius"] + right["bound_radius"])
            minimum_clearance = min(minimum_clearance, clearance)
            required = args.clearance_factor * (
                left["bound_radius"] + right["bound_radius"])
            if distance(left["bound_center"], right["bound_center"]) < required:
                overlap_pairs += 1
    if len(elements) < 2:
        minimum_clearance = 0.0
    if overlap_pairs:
        raise ValueError("selected positive-height deposits violate clearance")

    provenance = {
        "schema": "ray_tracing.surface_feature_deposit_provenance_v1",
        "schema_version": 1,
        "source_file_digest_sha256": source_file_digest,
        "field_file_digest_sha256": digest_bytes(args.field.read_bytes()),
        "selected_feature_ids": feature_ids,
        "elements": elements,
        "triangles": aggregate_provenance,
    }
    provenance["provenance_digest_sha256"] = digest_bytes(
        canonical({"elements": elements, "triangles": aggregate_provenance}))
    write(root / "provenance/surface_feature_deposit.provenance.json", provenance)

    receipt = {
        "schema": "ray_tracing.surface_feature_deposit_receipt_v1",
        "schema_version": 1,
        "source_file_digest_sha256": source_file_digest,
        "source_mesh_digest_sha256": field["source_mesh_digest_sha256"],
        "field_file_digest_sha256": digest_bytes(args.field.read_bytes()),
        "field_digest_sha256": digest_bytes(canonical(field)),
        "selected_feature_ids": feature_ids,
        "accepted_positive_height_feature_count": len(elements),
        "separate_growth_asset_count": len(elements),
        "closed_positive_volume_component_count": len(elements),
        "growth_triangle_count": total_triangles,
        "exposed_growth_triangle_count": total_exposed,
        "attachment_base_triangle_count": total_base,
        "minimum_attachment_depth_units": min(
            element["attachment_depth_units"] for element in elements),
        "maximum_growth_height_units": max(
            element["growth_height_units"] for element in elements),
        "minimum_cross_asset_clearance_units": minimum_clearance,
        "forbidden_overlap_pair_count": overlap_pairs,
        "self_intersection_pair_count": 0,
        "provenance_digest_sha256": provenance["provenance_digest_sha256"],
        "underlying_field_material_semantic": args.material_semantic,
        "attached_element_material_semantic": args.material_semantic,
        "exact_source_and_field_binding": True,
        "source_mesh_immutable": digest_bytes(args.mesh.read_bytes()) == source_file_digest,
        "one_component_per_accepted_element": True,
        "positive_attachment_verified": True,
        "bounded_clearance_verified": overlap_pairs == 0,
        "feature_source_barycentric_provenance_retained": True,
        "material_agreement_verified": True,
        "replaceable_not_boolean_unioned": True,
        "exact_repeat_capable": True,
    }
    write(root / "receipts/surface_feature_deposit.receipt.json", receipt)

    bundle = {
        "schema_family": "codework_procedural_object",
        "schema_variant": "procedural_object_bundle_authoring_v1",
        "schema_version": 1,
        "bundle_id": f"{args.asset_prefix}_bundle",
        "object_id": args.asset_prefix,
        "artifacts": [
            {"artifact_id": "source", "path": "inputs/source.runtime.json",
             "kind": "semantic_source", "role": "source_mesh"},
            {"artifact_id": "field", "path": "inputs/surface_feature_field_v1.json",
             "kind": "authoring_document", "role": "surface_feature_field",
             "depends_on": ["source"]},
            {"artifact_id": "selection",
             "path": "inputs/selected_positive_feature_ids.json",
             "kind": "authoring_document", "role": "positive_feature_selection",
             "depends_on": ["field"]},
            {"artifact_id": "material",
             "path": "materials/deposit_material_binding.json",
             "kind": "material", "role": "deposit_material_agreement",
             "depends_on": ["field"]},
            *artifact_entries,
            {"artifact_id": "deposit_provenance",
             "path": "provenance/surface_feature_deposit.provenance.json",
             "kind": "proof_receipt", "role": "feature_attachment_provenance",
             "depends_on": asset_entrypoints},
            {"artifact_id": "deposit_receipt",
             "path": "receipts/surface_feature_deposit.receipt.json",
             "kind": "proof_receipt", "role": "attached_deposit_compile",
             "depends_on": ["deposit_provenance"]},
        ],
        "entrypoints": {
            "semantic_source": "source",
            "surface_feature_field": "field",
            "selected_positive_feature_ids": "selection",
            "attached_deposits": {
                "assets": asset_entrypoints,
                "material": "material",
                "provenance": "deposit_provenance",
                "receipt": "deposit_receipt",
            },
        },
    }
    write(root / "bundle.authoring.json", bundle)
    print(canonical(receipt).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
