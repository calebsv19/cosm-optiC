#!/usr/bin/env python3
"""Render PSG-23E carrier-aware scalp guide/clump variations."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import platform
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
INTEGRATION = ROOT / "tests" / "integration"
sys.path.insert(0, str(INTEGRATION))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
from procedural_surface_visual_proof import (  # noqa: E402
    image_metrics,
    object_audit,
    render_request,
    run_render_cli,
    write_json,
    write_labeled_contact_sheet,
)


def default_binary(name: str) -> pathlib.Path:
    return (
        ROOT / "build/toolchains/clang" / platform.machine()
        / "tools/cli" / name
    )


def parse_args() -> argparse.Namespace:
    fixture = (
        ROOT / "tests/fixtures/procedural_imported_surface_strands_psg23a")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--groom-tool", type=pathlib.Path,
        default=ROOT / "tools/procedural_carrier_curve_groom_authoring.py")
    parser.add_argument(
        "--region-tool", type=pathlib.Path,
        default=default_binary("procedural_imported_surface_region_tool"))
    parser.add_argument(
        "--render-cli", type=pathlib.Path,
        default=default_binary("ray_tracing_render_headless"))
    parser.add_argument(
        "--stl-tool", type=pathlib.Path,
        default=(
            ROOT.parents[1] / "tools/procedural_object_authoring"
            / "procedural_stl_tool.py"))
    parser.add_argument(
        "--import-harness", type=pathlib.Path,
        default=(
            ROOT.parent / "line_drawing/build/toolchains/clang/bin"
            / "imported_mesh_harness"))
    parser.add_argument(
        "--recipe", type=pathlib.Path,
        default=fixture / "scalp_bust.recipe.json")
    parser.add_argument(
        "--region-recipe", type=pathlib.Path,
        default=fixture / "scalp_hair.region_recipe.json")
    parser.add_argument("--output-root", type=pathlib.Path)
    return parser.parse_args()


def run(command: list[str]) -> str:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(
            f"command failed: {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}")
    return result.stdout


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def mesh_object(name: str) -> dict:
    return {
        "object_id": f"{name}_scalp",
        "object_type": "mesh_asset_instance",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": {"x": 0.0, "y": 0.0, "z": 0.0},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "geometry_ref": {
            "kind": "mesh_asset",
            "id": "psg23a_scalp_bust",
        },
        "material_ref": {"id": "scalp_material"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def curve_object(
    name: str, asset_id: str, runtime: pathlib.Path,
) -> dict:
    return {
        "object_id": f"{name}_curves",
        "object_type": "curve_asset_instance",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": {"x": 0.0, "y": 0.0, "z": 0.0},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "geometry_ref": {
            "kind": "curve_asset",
            "id": asset_id,
            "runtime_path": runtime.name,
            "sha256": sha(runtime),
        },
        "material_ref": {"id": "hair_material"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def scene(
    name: str,
    asset_id: str,
    runtime: pathlib.Path,
    hair_color: tuple[float, float, float],
) -> dict:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"psg23e_{name}",
        "source_scene_id": f"psg23e_{name}",
        "compile_meta": {
            "compiler_version": "psg23e_groom_visual_proof",
            "compiled_at_ns": 0,
            "normalization": "fresh_curved_scalp_guide_clump_groom",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            mesh_object(name),
            curve_object(name, asset_id, runtime),
        ],
        "materials": [
            {
                "id": "scalp_material",
                "name": "Cool blue sculpted scalp",
                "base_color": {"r": 0.12, "g": 0.21, "b": 0.36},
                "roughness": 0.88,
                "metallic": 0.0,
            },
            {
                "id": "hair_material",
                "name": "Bright diagnostic hair",
                "base_color": {
                    "r": hair_color[0],
                    "g": hair_color[1],
                    "b": hair_color[2],
                },
                "roughness": 0.38,
                "metallic": 0.0,
            },
        ],
        "lights": [],
        "extensions": {},
    }


VARIANTS = [
    ("loose_natural", "LOOSE / NATURAL", [
        "groom.strand_count=96", "groom.guide_count=16",
        "groom.length=0.46", "groom.clump_strength=0.05",
        "groom.clump_tip_spread=0.05", "groom.part_strength=0.12",
        "groom.comb_strength=0.18", "groom.bend=0.08",
        "groom.curl=0.008",
    ], (0.98, 0.60, 0.12)),
    ("soft_clumps", "SOFT CLUMPS", [
        "groom.strand_count=112", "groom.guide_count=14",
        "groom.length=0.54", "groom.clump_strength=0.55",
        "groom.clump_tip_spread=0.028", "groom.part_strength=0.30",
        "groom.comb_strength=0.35", "groom.bend=0.16",
        "groom.curl=0.028",
    ], (0.96, 0.43, 0.10)),
    ("strong_locks", "STRONG LOCKS", [
        "groom.strand_count=112", "groom.guide_count=8",
        "groom.length=0.62", "groom.clump_strength=0.94",
        "groom.clump_tip_spread=0.006", "groom.part_strength=0.24",
        "groom.comb_strength=0.42", "groom.bend=0.20",
        "groom.curl=0.045",
    ], (0.93, 0.20, 0.14)),
    ("center_part", "CENTER PART", [
        "groom.strand_count=120", "groom.guide_count=18",
        "groom.length=0.58", "groom.clump_strength=0.34",
        "groom.clump_tip_spread=0.035", "groom.part_strength=1.35",
        "groom.comb_direction=[0.0,-1.0,0.0]",
        "groom.comb_strength=0.28", "groom.bend=0.14",
        "groom.curl=0.018",
    ], (0.24, 0.82, 0.74)),
    ("side_sweep", "SIDE SWEEP", [
        "groom.strand_count=112", "groom.guide_count=16",
        "groom.length=0.66", "groom.clump_strength=0.38",
        "groom.clump_tip_spread=0.032", "groom.part_strength=0.08",
        "groom.comb_direction=[1.0,-0.25,0.0]",
        "groom.comb_strength=1.55", "groom.bend=0.34",
        "groom.curl=0.015",
    ], (0.34, 0.60, 0.98)),
    ("curled_clumps", "CURLED CLUMPS", [
        "groom.strand_count=120", "groom.guide_count=10",
        "groom.length=0.70", "groom.clump_strength=0.82",
        "groom.clump_tip_spread=0.014", "groom.part_strength=0.30",
        "groom.comb_direction=[0.2,-1.0,0.0]",
        "groom.comb_strength=0.45", "groom.bend=0.24",
        "groom.curl=0.18",
    ], (0.86, 0.30, 0.94)),
]


def changed_pixels(left: list, right: list) -> int:
    return sum(
        a != b
        for left_row, right_row in zip(left, right)
        for a, b in zip(left_row, right_row)
    )


def draw_debug(runtime: dict, width: int, height: int) -> list:
    pixels = [[(12, 18, 28) for _ in range(width)] for _ in range(height)]
    palette = [
        (249, 196, 87), (93, 214, 189), (240, 111, 126),
        (125, 171, 247), (202, 124, 236), (129, 220, 110),
        (245, 153, 79), (104, 203, 235),
    ]
    points = []
    for strand in runtime["strands"]:
        root = strand["points"][0]["position"]
        points.append((root["x"], root["y"], strand["guide_index"]))
    minimum_x = min(item[0] for item in points)
    maximum_x = max(item[0] for item in points)
    minimum_y = min(item[1] for item in points)
    maximum_y = max(item[1] for item in points)

    def project(x: float, y: float) -> tuple[int, int]:
        px = int(40 + (x - minimum_x) / (maximum_x - minimum_x) * (width - 80))
        py = int(40 + (maximum_y - y) / (maximum_y - minimum_y) * (height - 80))
        return px, py

    for x, y, guide in points:
        px, py = project(x, y)
        color = palette[guide % len(palette)]
        for offset_y in range(-4, 5):
            for offset_x in range(-4, 5):
                if offset_x * offset_x + offset_y * offset_y <= 16:
                    x2, y2 = px + offset_x, py + offset_y
                    if 0 <= x2 < width and 0 <= y2 < height:
                        pixels[y2][x2] = color
    return pixels


def main() -> int:
    args = parse_args()
    output = (args.output_root or (
        ROOT / "build/agent_runs/ray_tracing/procedural_solid"
        / "psg23e_carrier_curve_groom_v1")).resolve()
    generated = output / "generated"
    review = output / "review"
    raw_root = output / "raw"
    staging = pathlib.Path("/private/tmp") / f"psg23e_visual_{os.getpid()}"
    for directory in (
        staging, generated / "assets/mesh_assets", generated / "requests",
        generated / "receipts", review, raw_root,
    ):
        directory.mkdir(parents=True, exist_ok=True)
    for tool in (
        args.groom_tool, args.region_tool, args.render_cli,
        args.stl_tool, args.import_harness,
    ):
        if not tool.resolve().exists():
            raise RuntimeError(f"missing PSG-23E dependency: {tool}")

    authored = staging / "authored"
    run([
        sys.executable, str(args.stl_tool.resolve()), "create",
        "--recipe", str(args.recipe.resolve()), "--out-root", str(authored),
    ])
    stl = (
        authored / "curated/psg23a_scalp_bust"
        / "source/psg23a_scalp_bust.stl"
    )
    imported = staging / "imported"
    run([
        str(args.import_harness.resolve()),
        "--stl", str(stl), "--out", str(imported),
        "--asset-id", "psg23a_scalp_bust",
        "--scene-id", "psg23e_fresh_scalp",
        "--object-id", "psg23e_scalp",
    ])
    mesh = (
        generated / "assets/mesh_assets"
        / "psg23a_scalp_bust.runtime.json"
    )
    mesh.write_bytes((
        imported / "assets/mesh_assets"
        / "psg23a_scalp_bust.runtime.json").read_bytes())
    source_sha = sha(mesh)
    region = generated / "scalp_hair.region.json"
    run([
        str(args.region_tool.resolve()),
        "--mesh", str(mesh),
        "--recipe", str(args.region_recipe.resolve()),
        "--out", str(region),
        "--summary-out", str(generated / "receipts/region.json"),
    ])

    render_contract = {
        "render": {
            "width": 900,
            "height": 700,
            "temporal_frames": 8,
            "integrator_3d": "disney_v2",
            "camera_zoom": 1.35,
        },
        "lighting": {
            "light_mode": 2,
            "environment_light_mode": "ambient",
            "ambient_strength": 0.72,
            "light_intensity": 5.2,
            "light_radius": 0.24,
            "top_fill_strength": 1.40,
            "background_brightness": 0.055,
            "background_color": {"r": 0.055, "g": 0.075, "b": 0.11},
        },
    }
    cells = []
    evidence = []
    pixel_sets = {}
    runtime_paths = {}
    for name, label, settings, color in VARIANTS:
        authoring = generated / f"{name}.groom.json"
        runtime = generated / f"{name}.curve_runtime.json"
        receipt = generated / "receipts" / f"{name}.json"
        scene_path = generated / f"{name}.scene.json"
        request_path = generated / "requests" / f"{name}.request.json"
        raw = raw_root / name
        command = [
            sys.executable, str(args.groom_tool.resolve()), "init",
            "--mesh", str(mesh), "--region", str(region),
            "--asset-id", f"psg23e_{name}",
            "--output", str(authoring),
        ]
        for setting in settings:
            command.extend(["--set", setting])
        run(command)
        compile_receipt = json.loads(run([
            sys.executable, str(args.groom_tool.resolve()), "compile",
            "--authoring", str(authoring),
            "--mesh", str(mesh), "--region", str(region),
            "--output", str(runtime), "--receipt", str(receipt),
        ]).splitlines()[0])
        write_json(
            scene_path,
            scene(name, f"psg23e_{name}", runtime, color))
        write_json(request_path, render_request(
            "psg23e_carrier_curve_groom",
            {
                "id": name,
                "camera_position": {"x": 1.72, "y": -2.08, "z": 2.52},
                "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.60},
            },
            scene_path, request_path, raw, render_contract))
        summary_path = raw / "render_summary.json"
        run_render_cli(args.render_cli.resolve(), request_path, summary_path)
        summary = load(summary_path)
        scalp_audit = object_audit(summary, f"{name}_scalp")
        curve_audit = object_audit(summary, f"{name}_curves")
        width, height, pixels = review_artifacts.read_bmp_rgb(
            raw / "frames/frame_0000.bmp")
        review_artifacts.write_png_rgb(
            review / f"{name}.png", width, height, pixels)
        metrics = image_metrics(pixels)
        if scalp_audit["primary_hit_pixels"] < 500:
            raise RuntimeError(f"{name}: scalp carrier is not visible")
        if curve_audit["primary_hit_pixels"] < 1200:
            raise RuntimeError(f"{name}: groom curves are not visible")
        if metrics["luma_standard_deviation"] < 7.0:
            raise RuntimeError(f"{name}: render is visually flat")
        cells.append((label, pixels))
        pixel_sets[name] = pixels
        runtime_paths[name] = runtime
        evidence.append({
            "name": name,
            "label": label,
            "source_file_sha256": source_sha,
            "authoring_sha256": compile_receipt["authoring_sha256"],
            "runtime_asset_sha256": compile_receipt["runtime_asset_sha256"],
            "strand_count": compile_receipt["strand_count"],
            "guide_count": compile_receipt["guide_count"],
            "clump_histogram": compile_receipt["clump_histogram"],
            "scalp_primary_hit_pixels": scalp_audit["primary_hit_pixels"],
            "curve_primary_hit_pixels": curve_audit["primary_hit_pixels"],
            "image_metrics": metrics,
        })
    pairs = {
        f"{VARIANTS[index - 1][0]}__{VARIANTS[index][0]}":
            changed_pixels(
                pixel_sets[VARIANTS[index - 1][0]],
                pixel_sets[VARIANTS[index][0]])
        for index in range(1, len(VARIANTS))
    }
    if any(value < 8000 for value in pairs.values()):
        raise RuntimeError("adjacent groom variants are not visually distinct")
    matrix = review / "psg23e_scalp_guide_clump_high_quality_matrix.png"
    write_labeled_contact_sheet(matrix, cells, columns=3)

    debug = draw_debug(load(runtime_paths["curled_clumps"]), 900, 700)
    debug_path = review / "curled_clumps_guide_assignment_debug.png"
    review_artifacts.write_png_rgb(debug_path, 900, 700, debug)
    repeat = generated / "curled_clumps.repeat.curve_runtime.json"
    repeat_receipt = generated / "receipts/curled_clumps.repeat.json"
    run([
        sys.executable, str(args.groom_tool.resolve()), "compile",
        "--authoring", str(generated / "curled_clumps.groom.json"),
        "--mesh", str(mesh), "--region", str(region),
        "--output", str(repeat), "--receipt", str(repeat_receipt),
    ])
    if sha(repeat) != sha(runtime_paths["curled_clumps"]):
        raise RuntimeError("deterministic repeat groom asset differs")
    if sha(mesh) != source_sha:
        raise RuntimeError("source mesh changed during PSG-23E proof")
    summary = {
        "schema": "ray_tracing.procedural_carrier_curve_groom_visual_proof",
        "schema_version": 1,
        "passed": True,
        "render_resolution_per_cell": [900, 700],
        "matrix_resolution": [2716, 1456],
        "matrix": str(matrix.relative_to(output)),
        "guide_assignment_debug": str(debug_path.relative_to(output)),
        "fresh_generated_scalp": True,
        "source_runtime_file_sha256": source_sha,
        "variants": evidence,
        "adjacent_changed_pixels": pairs,
        "deterministic_repeat_asset_sha256": sha(repeat),
        "editable_handles": [
            "carrier threshold", "strand count", "guide count",
            "length", "length variation", "lift", "comb direction",
            "comb strength", "part axis", "part strength", "bend",
            "curl", "clump strength", "clump tip spread", "radius profile",
        ],
        "authority": {
            "carrier_aware_roots": True,
            "guide_clump_operator": True,
            "serialized_curve_asset_ingested": True,
            "native_curve_blas_rendered": True,
            "triangle_tube_fallback_used": False,
            "hair_bsdf_added": False,
            "transparency_transport_changed": False,
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
