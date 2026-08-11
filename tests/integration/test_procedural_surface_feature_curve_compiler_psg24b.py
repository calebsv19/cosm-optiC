#!/usr/bin/env python3
"""Focused deterministic contract for the PSG-24B projected curve compiler."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from procedural_surface_feature_curve_compiler import compile_curve_field  # noqa: E402
from procedural_surface_feature_spot_compiler import canonical  # noqa: E402


def strip_mesh() -> dict:
    vertices = []
    columns, rows = 7, 4
    for row in range(rows):
        for column in range(columns):
            vertices.append({
                "x": -1.2 + column * 0.4,
                "y": -0.45 + row * 0.3,
                "z": -0.08 * abs(column - 3) + 0.025 * row,
            })
    triangles = []
    for row in range(rows - 1):
        for column in range(columns - 1):
            a = row * columns + column
            b = a + 1
            c = a + columns
            d = c + 1
            triangles.extend([
                {"a": a, "b": b, "c": d, "surface_group_id": "plaster"},
                {"a": a, "b": d, "c": c, "surface_group_id": "plaster"},
            ])
    return {"mesh": {"vertices": vertices, "triangles": triangles}}


def authoring() -> dict:
    return {
        "schema": "surface_feature_curve_authoring_v1",
        "schema_version": 1,
        "field_id": "focused_strip_scratches",
        "seed": 24,
        "normal_compatibility_cosine": 0.5,
        "macro_envelope": {
            "enabled": False,
            "receiver_direction": [0, 0, 1],
            "minimum_facing_cosine": -1.0,
            "height_range": [-2, 2],
            "minimum_weight": 0.01,
            "trace_minimum_weight": 0.01,
            "concavity": {"enabled": False, "surface_falloff_units": 1.0},
        },
        "populations": [{
            "id": "focused",
            "count": 2,
            "segment_count": [4, 7],
            "width": [0.025, 0.04],
            "depth": [0.008, 0.015],
            "end_width_scale": [0.55, 0.75],
            "end_depth_scale": [0.55, 0.8],
            "edge_softness": 0.25,
            "rim_width": 0.18,
            "jitter": 0.2,
            "seed": 246,
            "branching": {
                "maximum_branches": 1,
                "probability": 1.0,
                "segment_count": [2, 3],
                "angle_degrees": [30, 45],
            },
        }],
    }


def main() -> int:
    mesh = strip_mesh()
    digest = "c" * 64
    first_field, first_receipt = compile_curve_field(authoring(), mesh, digest)
    second_field, second_receipt = compile_curve_field(authoring(), mesh, digest)
    assert canonical(first_field) == canonical(second_field)
    assert canonical(first_receipt) == canonical(second_receipt)
    assert first_receipt["segment_count"] > 0
    assert first_receipt["curve_count"] >= 2
    assert first_receipt["continuity"]["maximum_consecutive_endpoint_gap"] <= 1e-6
    assert first_receipt["profile_accuracy"]["maximum_depth_profile_error"] == 0.0
    assert first_receipt["provenance"]["all_source_triangles_in_range"]
    assert first_receipt["provenance"]["stable_segment_ids_unique"]
    population = first_receipt["populations"][0]
    assert population["realized_width_quantiles"]["minimum"] > 0.0
    assert population["realized_depth_quantiles"]["minimum"] > 0.0
    assert (population["realized_width_quantiles"]["maximum"] <=
            population["declared_width_range"][1])
    assert (population["realized_depth_quantiles"]["maximum"] <=
            population["declared_depth_range"][1])
    assert first_receipt["candidate_search"]["capacity_respected"]
    assert not first_receipt["candidate_search"]["full_scan_permitted"]
    assert (first_receipt["normal_compatibility"]
            ["opposing_fold_incompatible_assignments"] == 0)
    print(json.dumps({
        "status": "passed",
        "curve_count": first_receipt["curve_count"],
        "segment_count": first_receipt["segment_count"],
        "continuity": first_receipt["continuity"],
        "candidate_search": first_receipt["candidate_search"],
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
