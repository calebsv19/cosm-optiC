#!/usr/bin/env python3
"""Render the PSG-23D serialized curve variation matrix at review quality."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import platform
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_DIR = ROOT / "tests" / "integration"
sys.path.insert(0, str(INTEGRATION_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
from procedural_surface_visual_proof import (  # noqa: E402
    image_metrics,
    object_audit,
    render_request,
    run_render_cli,
    write_json,
    write_labeled_contact_sheet,
)


def default_render_cli() -> Path:
    return (
        ROOT / "build/toolchains/clang" / platform.machine()
        / "tools/cli/ray_tracing_render_headless"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--render-cli", type=Path, default=default_render_cli())
    parser.add_argument(
        "--authoring-tool", type=Path,
        default=ROOT / "tools/procedural_curve_asset_authoring.py")
    parser.add_argument(
        "--baseline", type=Path,
        default=(
            ROOT / "tests/fixtures/procedural_curve_assets_psg23d"
            / "baseline.curve_authoring.json"))
    parser.add_argument("--output-root", type=Path)
    return parser.parse_args()


def run(command: list[str]) -> str:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(
            f"command failed: {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}")
    return result.stdout


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def curve_object(name: str, asset: Path, asset_sha: str) -> dict:
    return {
        "object_id": f"{name}_curves",
        "object_type": "curve_asset_instance",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": {"x": 0.0, "y": 0.0, "z": 0.025},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "geometry_ref": {
            "kind": "curve_asset",
            "id": name,
            "runtime_path": asset.name,
            "sha256": asset_sha,
        },
        "material_ref": {"id": "fiber_material"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def floor_object() -> dict:
    return {
        "object_id": "curve_field_floor",
        "object_type": "plane_primitive",
        "dimensional_mode": "plane_locked",
        "locked_plane": "xy",
        "transform": {
            "position": {"x": 0.0, "y": 0.0, "z": 0.0},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "primitive": {
            "kind": "plane_primitive",
            "width": 3.5,
            "height": 3.5,
            "frame": {
                "origin": {"x": 0.0, "y": 0.0, "z": 0.0},
                "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
                "axis_v": {"x": 0.0, "y": 1.0, "z": 0.0},
                "normal": {"x": 0.0, "y": 0.0, "z": 1.0},
            },
        },
        "material_ref": {"id": "floor_material"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def scene(name: str, asset: Path, asset_sha: str) -> dict:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"psg23d_{name}",
        "source_scene_id": f"psg23d_{name}",
        "compile_meta": {
            "compiler_version": "psg23d_curve_asset_visual_proof",
            "compiled_at_ns": 0,
            "normalization": "serialized_curve_asset_runtime_ingestion",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [floor_object(), curve_object(name, asset, asset_sha)],
        "materials": [
            {
                "id": "floor_material",
                "name": "Neutral curve carrier",
                "base_color": {"r": 0.23, "g": 0.25, "b": 0.28},
                "roughness": 0.92,
                "metallic": 0.0,
            },
            {
                "id": "fiber_material",
                "name": "Warm curve diagnostic",
                "base_color": {"r": 0.72, "g": 0.24, "b": 0.075},
                "roughness": 0.62,
                "metallic": 0.0,
            },
        ],
        "lights": [],
        "extensions": {},
    }


VARIANTS = [
    ("sparse_short", "SPARSE / SHORT", {
        "layout": {"rows": 6, "columns": 6,
                   "spacing_x": 0.24, "spacing_y": 0.24},
        "strand": {"length": 0.48, "length_variation": 0.04,
                   "direction_x": 0.0, "direction_y": 0.0,
                   "bend": 0.025, "curl": 0.006},
    }),
    ("dense_short", "DENSE / SHORT", {
        "layout": {"rows": 15, "columns": 15,
                   "spacing_x": 0.09, "spacing_y": 0.09},
        "strand": {"length": 0.48, "length_variation": 0.04,
                   "direction_x": 0.0, "direction_y": 0.0,
                   "bend": 0.025, "curl": 0.006},
    }),
    ("long_upright", "LONG / UPRIGHT", {
        "layout": {"rows": 10, "columns": 10,
                   "spacing_x": 0.14, "spacing_y": 0.14},
        "strand": {"length": 1.08, "length_variation": 0.14,
                   "direction_x": 0.0, "direction_y": 0.0,
                   "bend": 0.035, "curl": 0.008},
    }),
    ("swept_right", "SWEPT RIGHT", {
        "layout": {"rows": 10, "columns": 10,
                   "spacing_x": 0.14, "spacing_y": 0.14},
        "strand": {"length": 0.82, "length_variation": 0.10,
                   "direction_x": 0.52, "direction_y": 0.06,
                   "bend": 0.12, "curl": 0.008},
    }),
    ("swept_left", "SWEPT LEFT", {
        "layout": {"rows": 10, "columns": 10,
                   "spacing_x": 0.14, "spacing_y": 0.14},
        "strand": {"length": 0.82, "length_variation": 0.10,
                   "direction_x": -0.52, "direction_y": -0.04,
                   "bend": -0.12, "curl": 0.008},
    }),
    ("curled_tufts", "CURLED / TUFTED", {
        "layout": {"rows": 11, "columns": 11,
                   "spacing_x": 0.125, "spacing_y": 0.125},
        "strand": {"length": 0.78, "length_variation": 0.18,
                   "direction_x": 0.10, "direction_y": 0.04,
                   "bend": 0.18, "curl": 0.075},
    }),
]


def apply_variant(baseline: dict, name: str, changes: dict) -> dict:
    document = copy.deepcopy(baseline)
    document["asset_id"] = name
    for section, values in changes.items():
        document[section].update(values)
    return document


def changed_pixels(left: list, right: list) -> int:
    return sum(
        a != b
        for left_row, right_row in zip(left, right)
        for a, b in zip(left_row, right_row)
    )


def main() -> int:
    args = parse_args()
    output = (args.output_root or (
        ROOT / "build/agent_runs/ray_tracing/procedural_solid"
        / "psg23d_serialized_curve_assets_v1")).resolve()
    generated = output / "generated"
    review = output / "review"
    raw_root = output / "raw"
    for directory in (generated, review, raw_root, generated / "requests"):
        directory.mkdir(parents=True, exist_ok=True)
    baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
    render_contract = {
        "render": {
            "width": 900,
            "height": 700,
            "temporal_frames": 8,
            "integrator_3d": "disney_v2",
            "camera_zoom": 1.0,
        },
        "lighting": {
            "light_mode": 2,
            "ambient_strength": 0.24,
            "key_intensity": 1.40,
            "top_fill_strength": 0.75,
        },
    }
    cells = []
    evidence = []
    pixel_sets = {}
    for name, label, changes in VARIANTS:
        authoring = generated / f"{name}.curve_authoring.json"
        runtime_asset = generated / f"{name}.curve_runtime.json"
        scene_path = generated / f"{name}.scene.json"
        request_path = generated / "requests" / f"{name}.request.json"
        raw = raw_root / name
        document = apply_variant(baseline, name, changes)
        write_json(authoring, document)
        receipt = json.loads(run([
            sys.executable, str(args.authoring_tool.resolve()), "generate",
            "--authoring", str(authoring),
            "--output", str(runtime_asset),
        ]).splitlines()[0])
        asset_sha = sha(runtime_asset)
        if receipt["runtime_sha256"] != asset_sha:
            raise RuntimeError(f"{name}: runtime digest receipt mismatch")
        write_json(scene_path, scene(name, runtime_asset, asset_sha))
        view = {
            "id": name,
            "camera_position": {"x": 2.45, "y": -3.05, "z": 2.12},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 0.48},
        }
        write_json(request_path, render_request(
            "psg23d_serialized_curve_assets",
            view, scene_path, request_path, raw, render_contract))
        summary_path = raw / "render_summary.json"
        run_render_cli(args.render_cli.resolve(), request_path, summary_path)
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
        audit = object_audit(summary, f"{name}_curves")
        frame = raw / "frames/frame_0000.bmp"
        width, height, pixels = review_artifacts.read_bmp_rgb(frame)
        review_artifacts.write_png_rgb(
            review / f"{name}.png", width, height, pixels)
        metrics = image_metrics(pixels)
        if audit["primary_hit_pixels"] < 2000:
            raise RuntimeError(f"{name}: insufficient visible curve hits")
        if metrics["luma_standard_deviation"] < 7.0:
            raise RuntimeError(f"{name}: render is visually flat")
        cells.append((label, pixels))
        pixel_sets[name] = pixels
        evidence.append({
            "name": name,
            "label": label,
            "asset_sha256": asset_sha,
            "authoring_sha256": receipt["authoring_sha256"],
            "strand_count": receipt["strand_count"],
            "points_per_strand": receipt["points_per_strand"],
            "primary_hit_pixels": audit["primary_hit_pixels"],
            "image_metrics": metrics,
        })
    pairs = {
        f"{VARIANTS[index - 1][0]}__{VARIANTS[index][0]}":
            changed_pixels(
                pixel_sets[VARIANTS[index - 1][0]],
                pixel_sets[VARIANTS[index][0]])
        for index in range(1, len(VARIANTS))
    }
    if any(value < 5000 for value in pairs.values()):
        raise RuntimeError("adjacent curve variants are not visually distinct")
    matrix = review / "psg23d_curve_variation_high_quality_matrix.png"
    write_labeled_contact_sheet(matrix, cells, columns=3)
    repeat_asset = generated / "repeat.curve_runtime.json"
    run([
        sys.executable, str(args.authoring_tool.resolve()), "generate",
        "--authoring", str(generated / "curled_tufts.curve_authoring.json"),
        "--output", str(repeat_asset),
    ])
    if sha(repeat_asset) != sha(generated / "curled_tufts.curve_runtime.json"):
        raise RuntimeError("deterministic repeat curve asset differs")
    summary = {
        "schema": "ray_tracing.procedural_curve_asset_visual_proof",
        "schema_version": 1,
        "passed": True,
        "render_resolution_per_cell": [900, 700],
        "matrix_resolution": [2716, 1456],
        "matrix": str(matrix.relative_to(output)),
        "variants": evidence,
        "adjacent_changed_pixels": pairs,
        "deterministic_repeat_asset_sha256": sha(repeat_asset),
        "editable_handles": [
            "density", "spacing", "length", "length variation",
            "direction", "bend", "curl", "point count", "radius profile",
        ],
        "authority": {
            "serialized_curve_asset_ingested": True,
            "scene_curve_instance_ingested": True,
            "native_curve_blas_rendered": True,
            "purpose_fit_objects_generated_per_variant": True,
            "triangle_tube_fallback_used": False,
            "hair_bsdf_added": False,
            "grooming_dynamics_added": False,
            "motion_blur_added": False,
            "package_or_release_mutated": False,
        },
    }
    write_json(output / "proof_summary.json", summary)
    print(output / "proof_summary.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
