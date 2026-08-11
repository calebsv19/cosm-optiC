#!/usr/bin/env python3
"""Compile an editable formed-concrete preset into a signed PSG-24A field.

The coverage control is a *nominal non-overlap area target*.  The emitted
receipt records the deterministic measured union coverage; callers must use
that readback, not the nominal target, for a realized-coverage claim.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

from procedural_surface_feature_spot_compiler import (
    canonical, compile_field, digest_bytes, dot, mesh_analysis, normalized, sub,
)


SCHEMA = "ray_tracing.formed_concrete_preset_v1"


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write(path: Path, value: object) -> str:
    data = canonical(value) + b"\n"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return digest_bytes(data)


def mean(pair: list[float]) -> float:
    return (float(pair[0]) + float(pair[1])) * 0.5


def count_for(target_fraction: float, area: float, radius: list[float], aspect: list[float], share: float) -> int:
    # This is deliberately an initial calibration, not an exact union-area
    # solver: overlap, edge softness, and discrete population counts change
    # realized coverage and are receipt-visible after compilation.
    nominal_feature_area = 3.141592653589793 * mean(radius) ** 2 * mean(aspect)
    return max(1, int(round(target_fraction * area * share / nominal_feature_area)))


def expand(preset: dict, mesh: dict) -> tuple[dict, dict]:
    if preset.get("schema") != SCHEMA or preset.get("schema_version") != 1:
        raise ValueError("expected ray_tracing.formed_concrete_preset_v1 schema version 1")
    coverage = preset["coverage"]
    requested = float(coverage["nominal_target_fraction"])
    if not 0.0 < requested < 1.0:
        raise ValueError("coverage.nominal_target_fraction must be in (0, 1)")
    shape = preset["feature_shape"]
    proportions = preset["proportions"]
    pore_fraction = float(proportions["pore_fraction"])
    if not 0.0 < pore_fraction < 1.0:
        raise ValueError("proportions.pore_fraction must be in (0, 1)")
    radius = shape["radius_units"]
    aspect = shape["aspect_ratio"]
    if len(radius) != 2 or len(aspect) != 2 or min(radius) <= 0.0 or min(aspect) <= 0.0:
        raise ValueError("feature_shape radius_units and aspect_ratio require positive ordered pairs")

    # Mesh area is intentionally only used to bind a readable pre-compile
    # count estimate. The field compiler remains the authority for eligible
    # area and realized coverage.
    vertices = mesh["mesh"]["vertices"]
    triangles = mesh["mesh"]["triangles"]
    def point(i: int) -> tuple[float, float, float]:
        value = vertices[i]
        return float(value["x"]), float(value["y"]), float(value["z"])
    def tri_area(triangle: dict) -> float:
        a, b, c = (point(triangle[key]) for key in ("a", "b", "c"))
        cross = ((b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]),
                 (b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]),
                 (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))
        return 0.5 * sum(component * component for component in cross) ** 0.5
    mesh_area = sum(tri_area(triangle) for triangle in triangles)
    variation = preset["variation"]
    material = preset["material_response"]
    base_seed = int(preset["seed"])
    field_authoring = {
        "schema": "surface_feature_field_authoring_v1",
        "field_id": f"{preset['preset_id']}_field",
        "preset_identity": {"preset_id": preset["preset_id"], "preset_digest_sha256": digest_bytes(canonical(preset))},
        "normal_compatibility_cosine": float(preset["normal_compatibility_cosine"]),
        "seed": base_seed,
        "populations": [
            {"id": "inward_pores", "count": count_for(requested, mesh_area, radius, aspect, pore_fraction),
             "radius": radius, "aspect": aspect, "edge_softness": float(shape["edge_softness"]),
             "rim_width": float(shape["rim_width"]), "height_or_depth": preset["signed_relief"]["inward_depth_units"],
             "cluster": float(variation["cluster"]), "jitter": float(variation["jitter"]),
             "cluster_hops": int(variation["cluster_hops"]), "seed": base_seed + 1},
            {"id": "outward_aggregate", "count": count_for(requested, mesh_area, radius, aspect, 1.0 - pore_fraction),
             "radius": radius, "aspect": aspect, "edge_softness": float(shape["edge_softness"]),
             "rim_width": float(shape["rim_width"]), "height_or_depth": preset["signed_relief"]["outward_height_units"],
             "cluster": float(variation["cluster"]), "jitter": float(variation["jitter"]),
             "cluster_hops": int(variation["cluster_hops"]), "seed": base_seed + 2},
        ],
        "macro_envelope": preset["macro_envelope"],
        "material_response": material,
    }
    calibration = {
        "method": "nominal_nonoverlap_ellipse_area_v1",
        "requested_nominal_coverage_fraction": requested,
        "mesh_area_before_envelope": mesh_area,
        "realized_coverage_authority": "surface_feature_field_receipt_v1.coverage.eligible_measured",
        "not_an_exact_coverage_solver": True,
    }
    return field_authoring, calibration


def realized_coverage(field: dict, mesh: dict, envelope: dict) -> dict:
    """Measure the field union densely enough for the 0.1% preset tier.

    The legacy PSG-24A receipt retains its compact 13-sample readback. This
    adapter adds a deterministic 48-subdivision barycentric measurement for
    preset calibration without changing that established compiler contract.
    """
    analysis = mesh_analysis(mesh, envelope)
    eligible = [index for index, weight in enumerate(analysis["weights"])
                if weight >= float(envelope.get("minimum_weight", 0.0))]
    eligible_set = set(eligible)
    # Cap total sample work for a refined PSG-18 source while retaining the
    # full 48-subdivision calibration for small authoring fixtures.
    subdivisions = min(
        48,
        max(1, int((100000 / max(1, len(analysis["triangles"]) * len(field["features"]))) ** 0.5)),
    )
    weighted_total = weighted_covered = 0.0
    for triangle_index, triangle in enumerate(analysis["triangles"]):
        vertices = [analysis["vertices"][triangle[key]] for key in ("a", "b", "c")]
        normals = [analysis["smooth_normals"][triangle[key]] for key in ("a", "b", "c")]
        for i in range(subdivisions + 1):
            for j in range(subdivisions + 1 - i):
                bary = (i / subdivisions, j / subdivisions, (subdivisions - i - j) / subdivisions)
                position = [sum(bary[k] * vertices[k][axis] for k in range(3)) for axis in range(3)]
                normal = normalized([sum(bary[k] * normals[k][axis] for k in range(3)) for axis in range(3)])
                coverage = 0.0
                for feature in field["features"]:
                    if dot(normal, feature["normal"]) < field["normal_compatibility_cosine"]:
                        continue
                    delta = sub(position, feature["position"])
                    x, y = dot(delta, feature["tangent"]), dot(delta, feature["bitangent"])
                    cosine, sine = math.cos(feature["rotation"]), math.sin(feature["rotation"])
                    q = math.hypot((x * cosine + y * sine) / feature["radius"],
                                   (-x * sine + y * cosine) / (feature["radius"] * feature["aspect"]))
                    if q <= 1.0:
                        edge = min(1.0, max(0.0, (1.0 - q) / max(feature["edge_softness"], 1.0e-12)))
                        coverage = max(coverage, edge * edge * (3.0 - 2.0 * edge))
                if triangle_index in eligible_set:
                    # Each regular lattice sample receives equal in-triangle
                    # weight; triangle area supplies the inter-triangle weight.
                    weight = analysis["areas"][triangle_index]
                    weighted_total += weight
                    weighted_covered += weight * coverage
    return {"method": "deterministic_barycentric_lattice_union_v1",
            "subdivisions_per_triangle_edge": subdivisions,
            "eligible_fraction": round(weighted_covered / max(weighted_total, 1.0e-12), 9)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", type=Path, required=True)
    parser.add_argument("--mesh", type=Path, required=True)
    parser.add_argument("--source-mesh-digest", required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    preset, mesh = load(args.preset), load(args.mesh)
    preset_digest = digest_bytes(canonical(preset))
    authoring, calibration = expand(preset, mesh)
    field, field_receipt = compile_field(authoring, mesh, args.source_mesh_digest)
    dense_coverage = realized_coverage(field, mesh, authoring["macro_envelope"])
    root = args.output_root
    authoring_digest = write(root / "authoring" / "surface_feature_field.authoring.json", authoring)
    field_digest = write(root / "assets" / "surface_feature_field_v1.json", field)
    field_receipt["field_digest_sha256"] = field_digest
    field_receipt_digest = write(root / "receipts" / "surface_feature_field.receipt.json", field_receipt)
    receipt = {
        "schema": "ray_tracing.formed_concrete_preset_receipt_v1", "schema_version": 1,
        "preset_id": preset["preset_id"], "preset_digest_sha256": preset_digest,
        "field_authoring_digest_sha256": authoring_digest, "field_digest_sha256": field_digest,
        "field_receipt_digest_sha256": field_receipt_digest,
        "source_mesh_digest_sha256": args.source_mesh_digest,
        "coverage": {**calibration, "realized_eligible_fraction": dense_coverage["eligible_fraction"],
                     "realized_measurement": dense_coverage,
                     "legacy_compact_sample_eligible_fraction": field_receipt["coverage"]["eligible_measured"],
                     "realized_total_fraction": field_receipt["coverage"]["total_measured"]},
        "radius_quantiles": {row["id"]: row["radius_quantiles"] for row in field_receipt["populations"]},
        "signed_displacement_ranges": {row["id"]: row["height_or_depth_quantiles"] for row in field_receipt["populations"]},
        "candidate_bounds": field_receipt["candidate_search"],
        "macro_envelope": field_receipt["macro_envelope"],
        "geometry_claim": "signed PSG-24A field; physical relief requires a separately receipt-bound PSG-18 compile",
        "material_claim": {"aligned_field_identity": field_digest, "configured_response": preset["material_response"]},
    }
    receipt_digest = write(root / "receipts" / "formed_concrete_preset.receipt.json", receipt)
    print(json.dumps({"preset_receipt": str(root / "receipts" / "formed_concrete_preset.receipt.json"),
                      "preset_receipt_digest_sha256": receipt_digest, "field": str(root / "assets" / "surface_feature_field_v1.json")}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
