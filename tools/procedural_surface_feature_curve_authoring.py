#!/usr/bin/env python3
"""Compile deterministic PSG-24B surface scratch fields and bundle records."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

from procedural_surface_feature_curve_compiler import compile_curve_field
from procedural_surface_feature_spot_compiler import canonical, digest_bytes


def write(path: Path, value: object) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    data = canonical(value) + b"\n"
    path.write_bytes(data)
    return digest_bytes(data)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--authoring", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--mesh", type=Path, required=True)
    parser.add_argument("--solid-receipt", type=Path)
    parser.add_argument("--mesh-digest-tool", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    specification = json.loads(args.authoring.read_text())
    mesh = json.loads(args.mesh.read_text())
    if args.solid_receipt:
        mesh_digest = json.loads(
            args.solid_receipt.read_text())["mesh_digest_sha256"]
    elif args.mesh_digest_tool:
        mesh_digest = subprocess.check_output(
            [str(args.mesh_digest_tool), str(args.mesh)], text=True).strip()
    else:
        raise SystemExit("--solid-receipt or --mesh-digest-tool is required")
    field, receipt = compile_curve_field(specification, mesh, mesh_digest)
    repeated_field, repeated_receipt = compile_curve_field(
        specification, mesh, mesh_digest)
    if (canonical(field) != canonical(repeated_field) or
            canonical(receipt) != canonical(repeated_receipt)):
        raise RuntimeError(
            "deterministic repeat changed curve field or receipt bytes")
    root = args.output_root
    asset = root / "assets" / "surface_feature_curve_field_v1.json"
    asset_digest = write(asset, field)
    receipt["field_digest_sha256"] = asset_digest
    receipt["source_mesh_digest_sha256"] = mesh_digest
    receipt_path = root / "receipts" / "surface_feature_curve_field.receipt.json"
    write(receipt_path, receipt)
    source = root / "assets" / "curved_plaster_closed_v1.json"
    write(source, {
        "schema": "closed_curved_plaster_source_v1",
        "mesh_digest_sha256": mesh_digest,
        "convex_ridge": True,
        "concave_trough": True,
        "opposing_folds": True,
    })
    bundle = {
        "schema_family": "codework_procedural_object",
        "schema_variant": "procedural_object_bundle_authoring_v1",
        "schema_version": 1,
        "bundle_id": "curved_plaster_scratch_fields_psg24b",
        "object_id": "curved_plaster_scratches",
        "artifacts": [
            {"artifact_id": "source", "path": "assets/curved_plaster_closed_v1.json",
             "kind": "semantic_source", "role": "curved_plaster"},
            {"artifact_id": "curve_field",
             "path": "assets/surface_feature_curve_field_v1.json",
             "kind": "authoring_document", "role": "surface_feature_curve_field",
             "depends_on": ["source"]},
            {"artifact_id": "receipt",
             "path": "receipts/surface_feature_curve_field.receipt.json",
             "kind": "proof_receipt", "role": "curve_field_compile",
             "depends_on": ["curve_field"]},
        ],
        "entrypoints": {
            "semantic_source": "source",
            "surface_features": {"scratches": {
                "coverage": "curve_field",
                "interior": "curve_field",
                "rim": "curve_field",
                "depth": "curve_field",
                "curve_id": "curve_field",
                "segment_id": "curve_field",
                "tangent_direction": "curve_field",
                "macro_envelope": "receipt",
            }},
            "field_receipt": "receipt",
        },
    }
    bundle_path = root / "bundle.authoring.json"
    write(bundle_path, bundle)
    print(json.dumps({
        "field": str(asset),
        "receipt": str(receipt_path),
        "bundle_authoring": str(bundle_path),
        "curve_count": receipt["curve_count"],
        "segment_count": receipt["segment_count"],
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
