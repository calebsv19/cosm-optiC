#!/usr/bin/env python3
"""Compile deterministic PSG-24A spot fields and their proof/bundle records."""

from __future__ import annotations

import argparse
import json
import math
import subprocess
from pathlib import Path

from procedural_surface_feature_spot_compiler import compile_field, digest_bytes, canonical


def write(path: Path, value: object) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    data = canonical(value) + b"\n"
    path.write_bytes(data)
    return digest_bytes(data)


def ppm(path: Path, mode: str, field: dict, grazing: bool = False) -> None:
    width, height = 1200, 900
    base = 0 if mode in {"coverage", "interior", "rim", "feature_id", "repeat_difference", "envelope"} else 185
    pixels = bytearray([base, base, base] * width * height)
    if mode == "repeat_difference":
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(f"P6\n{width} {height}\n255\n".encode() + pixels)
        return
    for feature in field["features"]:
        center_x = int((feature["position"][0] + 1.0) * 0.5 * (width - 1))
        center_y = int((feature["position"][1] + 0.8) / 1.6 * (height - 1))
        radius_x = max(1, int(feature["radius"] * 0.5 * width))
        radius_y = max(1, int(feature["radius"] * feature["aspect"] / 1.6 * height))
        for pixel_y in range(max(0, center_y - radius_y), min(height, center_y + radius_y + 1)):
            for pixel_x in range(max(0, center_x - radius_x), min(width, center_x + radius_x + 1)):
                distance = math.hypot((pixel_x - center_x) / radius_x, (pixel_y - center_y) / radius_y)
                if distance > 1.0:
                    continue
                interior = distance < 1.0 - feature["rim_width"]
                if mode == "coverage":
                    color = [int((1.0 - distance) * 255)] * 3
                elif mode == "interior":
                    color = [255 if interior else 0] * 3
                elif mode == "rim":
                    color = [0 if interior else 255] * 3
                elif mode == "feature_id":
                    color = [(feature["feature_id"] * factor) % 256 for factor in (67, 131, 197)]
                elif mode == "envelope":
                    population = feature["population"]
                    color = [60 + 55 * population, 42 + 34 * population, 28 + 22 * population]
                elif mode in {"geometric_normal", "shading_normal", "normal"}:
                    normal = feature["normal"]
                    color = [int((normal[index] * 0.5 + 0.5) * 255) for index in range(3)]
                else:
                    color = [125 if not interior else 92, 88 if not interior else 61, 53 if not interior else 37]
                if grazing:
                    color = [int(value * 0.78) for value in color]
                offset = (pixel_y * width + pixel_x) * 3
                pixels[offset:offset + 3] = bytes(color)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode() + pixels)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--authoring", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--mesh", type=Path, required=True)
    parser.add_argument("--solid-receipt", type=Path,
                        help="Receipt containing the canonical mesh_digest_sha256.")
    parser.add_argument("--mesh-digest-tool", type=Path,
                        help="Legacy helper that prints the canonical mesh digest.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    specification = json.loads(args.authoring.read_text())
    mesh = json.loads(args.mesh.read_text())
    if args.solid_receipt:
        mesh_digest = json.loads(args.solid_receipt.read_text())["mesh_digest_sha256"]
    elif args.mesh_digest_tool:
        mesh_digest = subprocess.check_output(
            [str(args.mesh_digest_tool), str(args.mesh)], text=True).strip()
    else:
        raise SystemExit("--solid-receipt or --mesh-digest-tool is required")
    field, receipt = compile_field(specification, mesh, mesh_digest)
    repeated_field, repeated_receipt = compile_field(specification, mesh, mesh_digest)
    if canonical(field) != canonical(repeated_field) or canonical(receipt) != canonical(repeated_receipt):
        raise RuntimeError("deterministic repeat compile changed field or receipt bytes")
    root = args.output_root
    asset = root / "assets" / "surface_feature_field_v1.json"
    asset_digest = write(asset, field)
    receipt["field_digest_sha256"] = asset_digest
    receipt["source_mesh_digest_sha256"] = mesh_digest
    receipt_path = root / "receipts" / "surface_feature_field.receipt.json"
    write(receipt_path, receipt)
    source = root / "assets" / "curved_plaster_closed_v1.json"
    write(source, {
        "schema": "closed_curved_plaster_source_v1",
        "mesh_digest_sha256": mesh_digest,
        "convex_ridge": True,
        "concave_trough": True,
        "opposing_folds": True,
    })
    for mode in ("control", "beauty", "coverage", "interior", "rim", "feature_id", "envelope", "geometric_normal", "shading_normal", "repeat_difference"):
        ppm(root / "proof" / f"{mode}.ppm", "beauty" if mode == "control" else mode, field)
        if mode in ("control", "beauty", "geometric_normal", "shading_normal"):
            ppm(root / "proof" / f"{mode}_grazing.ppm", "beauty" if mode == "control" else mode, field, True)
    bundle = {
        "schema_family": "codework_procedural_object",
        "schema_variant": "procedural_object_bundle_authoring_v1",
        "schema_version": 1,
        "bundle_id": "curved_plaster_spot_fields_psg24a",
        "object_id": "curved_plaster_spots",
        "artifacts": [
            {"artifact_id": "source", "path": "assets/curved_plaster_closed_v1.json", "kind": "semantic_source", "role": "curved_plaster"},
            {"artifact_id": "field", "path": "assets/surface_feature_field_v1.json", "kind": "authoring_document", "role": "surface_feature_field", "depends_on": ["source"]},
            {"artifact_id": "receipt", "path": "receipts/surface_feature_field.receipt.json", "kind": "proof_receipt", "role": "field_compile", "depends_on": ["field"]},
        ],
        "entrypoints": {
            "semantic_source": "source",
            "surface_features": {"dirt": {
                "coverage": "field", "interior": "field", "rim": "field",
                "height_or_depth": "field", "feature_id": "field",
                "tangent_direction": "field", "macro_envelope": "receipt",
            }},
            "field_receipt": "receipt",
        },
    }
    write(root / "bundle.authoring.json", bundle)
    print(json.dumps({
        "field": str(asset), "receipt": str(receipt_path),
        "bundle_authoring": str(root / "bundle.authoring.json"),
        "feature_count": len(field["features"]),
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
