#!/usr/bin/env python3
"""Focused deterministic tests for the PSG-24A mesh-aware spot compiler."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from procedural_surface_feature_spot_compiler import canonical, compile_field  # noqa: E402


def source_mesh() -> dict:
    # Two consistently wound faces meet in a concave fold. Boundary edges are
    # intentional: this fixture isolates signed shared-edge classification.
    return {
        "mesh": {
            "vertices": [
                {"x": 0.0, "y": 0.0, "z": 0.0},
                {"x": 1.0, "y": 0.0, "z": 0.0},
                {"x": 0.0, "y": 1.0, "z": 0.0},
                {"x": 1.0, "y": 1.0, "z": 0.5},
            ],
            "triangles": [
                {"a": 0, "b": 1, "c": 2},
                {"a": 2, "b": 1, "c": 3},
            ],
        }
    }


def authoring() -> dict:
    return {
        "schema": "surface_feature_field_authoring_v1",
        "field_id": "focused_concave_fold",
        "normal_compatibility_cosine": 0.5,
        "populations": [{
            "id": "counted", "count": 3, "radius": [0.02, 0.04],
            "aspect": [0.8, 1.2], "edge_softness": 0.1,
            "rim_width": 0.2, "cluster": 0.5, "jitter": 0.7,
            "cluster_hops": 1, "seed": 24,
        }, {
            "id": "density", "density_per_square_unit": 2.0,
            "radius": [0.05, 0.08], "aspect": [0.9, 1.1],
            "edge_softness": 0.15, "rim_width": 0.25,
            "cluster": 0.0, "seed": 25,
        }],
        "macro_envelope": {
            "enabled": True, "minimum_weight": 0.0,
            "receiver_direction": [0.0, 0.0, 1.0],
            "minimum_facing_cosine": 0.0,
            "height_range": [-1.0, 1.0],
            "concavity": {
                "enabled": True,
                "minimum_signed_dihedral_degrees": 2.0,
                "surface_falloff_units": 2.0,
            },
        },
    }


def main() -> int:
    digest = "a" * 64
    first, first_receipt = compile_field(authoring(), source_mesh(), digest)
    second, second_receipt = compile_field(authoring(), source_mesh(), digest)
    assert canonical(first) == canonical(second)
    assert canonical(first_receipt) == canonical(second_receipt)
    assert len(first["features"]) == sum(
        population["accepted_count"] for population in first_receipt["populations"])
    assert {population["placement_mode"] for population in first_receipt["populations"]} == {
        "count", "density_per_square_unit"}
    assert first_receipt["macro_envelope"]["concavity_seed_triangle_count"] == 2
    assert first_receipt["macro_envelope"]["maximum_observed_concave_dihedral_degrees"] > 2.0
    assert first_receipt["provenance"]["all_source_triangles_in_range"]
    assert first_receipt["provenance"]["maximum_frame_orthonormal_error"] <= 1.0e-8
    assert first_receipt["candidate_search"]["capacity_respected"]
    assert first_receipt["normal_compatibility"]["opposing_fold_incompatible_assignments"] == 0
    print(json.dumps({
        "status": "passed",
        "feature_count": len(first["features"]),
        "receipt": first_receipt,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
