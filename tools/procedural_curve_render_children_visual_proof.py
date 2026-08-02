#!/usr/bin/env python3
"""Render the PSG-23F guide-to-render-child density progression."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import platform
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests/integration"))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import procedural_carrier_curve_groom_visual_proof as groom_proof  # noqa: E402
from procedural_surface_visual_proof import (  # noqa: E402
    image_metrics,
    object_audit,
    render_request,
    run_render_cli,
    write_json,
    write_labeled_contact_sheet,
)


def binary(name: str) -> pathlib.Path:
    return ROOT / "build/toolchains/clang" / platform.machine() / "tools/cli" / name


def arguments() -> argparse.Namespace:
    fixture = ROOT / "tests/fixtures/procedural_imported_surface_strands_psg23a"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--child-tool", type=pathlib.Path,
        default=ROOT / "tools/procedural_curve_render_children_authoring.py")
    parser.add_argument(
        "--groom-tool", type=pathlib.Path,
        default=ROOT / "tools/procedural_carrier_curve_groom_authoring.py")
    parser.add_argument(
        "--region-tool", type=pathlib.Path,
        default=binary("procedural_imported_surface_region_tool"))
    parser.add_argument(
        "--render-cli", type=pathlib.Path,
        default=binary("ray_tracing_render_headless"))
    parser.add_argument(
        "--stl-tool", type=pathlib.Path,
        default=ROOT.parents[1] / "tools/procedural_object_authoring"
        / "procedural_stl_tool.py")
    parser.add_argument(
        "--import-harness", type=pathlib.Path,
        default=ROOT.parent / "line_drawing/build/toolchains/clang/bin"
        / "imported_mesh_harness")
    parser.add_argument(
        "--recipe", type=pathlib.Path,
        default=fixture / "scalp_bust.recipe.json")
    parser.add_argument(
        "--region-recipe", type=pathlib.Path,
        default=fixture / "scalp_hair.region_recipe.json")
    parser.add_argument("--output-root", type=pathlib.Path)
    return parser.parse_args()


def curve_object(
    object_id: str, asset_id: str, runtime: pathlib.Path,
) -> dict:
    return {
        "object_id": object_id,
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
            "sha256": groom_proof.sha(runtime),
        },
        "material_ref": {"id": "hair_material"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def scene(name: str, asset_id: str, runtime: pathlib.Path) -> dict:
    scalp = groom_proof.mesh_object(name)
    scalp["object_id"] = f"{name}_scalp"
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"psg23f_{name}",
        "source_scene_id": "psg23f_single_guide_groom",
        "compile_meta": {
            "compiler_version": "psg23f_render_child_visual_proof",
            "compiled_at_ns": 0,
            "normalization": "same_guides_deterministic_lod_children",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            scalp,
            curve_object(f"{name}_hair", asset_id, runtime),
        ],
        "materials": [
            {
                "id": "scalp_material",
                "name": "Warm neutral scalp",
                "base_color": {"r": 0.31, "g": 0.20, "b": 0.15},
                "roughness": 0.82,
                "metallic": 0.0,
            },
            {
                "id": "hair_material",
                "name": "Chestnut diagnostic fibers",
                "base_color": {"r": 0.30, "g": 0.075, "b": 0.025},
                "roughness": 0.46,
                "metallic": 0.0,
            },
        ],
        "lights": [],
        "extensions": {},
    }


def compile_children(
    args: argparse.Namespace,
    authoring: pathlib.Path,
    guides: pathlib.Path,
    mesh: pathlib.Path,
    generated: pathlib.Path,
    lod: str,
) -> tuple[pathlib.Path, dict]:
    runtime = generated / f"{lod}.curve_runtime.json"
    receipt = generated / "receipts" / f"{lod}.json"
    groom_proof.run([
        sys.executable, str(args.child_tool.resolve()), "compile",
        "--authoring", str(authoring),
        "--guide-asset", str(guides),
        "--mesh", str(mesh),
        "--lod", lod,
        "--output", str(runtime),
        "--receipt", str(receipt),
    ])
    return runtime, groom_proof.load(receipt)


def main() -> int:
    args = arguments()
    output = (args.output_root or (
        ROOT / "build/agent_runs/ray_tracing/procedural_solid"
        / "psg23f_render_child_density_v1")).resolve()
    generated = output / "generated"
    review = output / "review"
    raw_root = output / "raw"
    staging = pathlib.Path("/private/tmp") / f"psg23f_visual_{os.getpid()}"
    for directory in (
        staging, generated / "assets/mesh_assets",
        generated / "requests", generated / "receipts", review, raw_root,
    ):
        directory.mkdir(parents=True, exist_ok=True)
    for dependency in (
        args.child_tool, args.groom_tool, args.region_tool, args.render_cli,
        args.stl_tool, args.import_harness, args.recipe, args.region_recipe,
    ):
        if not dependency.resolve().exists():
            raise RuntimeError(f"missing PSG-23F dependency: {dependency}")

    authored = staging / "authored"
    groom_proof.run([
        sys.executable, str(args.stl_tool.resolve()), "create",
        "--recipe", str(args.recipe.resolve()), "--out-root", str(authored),
    ])
    stl = authored / "curated/psg23a_scalp_bust/source/psg23a_scalp_bust.stl"
    imported = staging / "imported"
    groom_proof.run([
        str(args.import_harness.resolve()),
        "--stl", str(stl), "--out", str(imported),
        "--asset-id", "psg23a_scalp_bust",
        "--scene-id", "psg23f_fresh_scalp",
        "--object-id", "psg23f_scalp",
    ])
    mesh = generated / "assets/mesh_assets/psg23a_scalp_bust.runtime.json"
    mesh.write_bytes((
        imported / "assets/mesh_assets/psg23a_scalp_bust.runtime.json"
    ).read_bytes())
    source_sha = groom_proof.sha(mesh)
    region = generated / "scalp_hair.region.json"
    groom_proof.run([
        str(args.region_tool.resolve()),
        "--mesh", str(mesh),
        "--recipe", str(args.region_recipe.resolve()),
        "--out", str(region),
        "--summary-out", str(generated / "receipts/region.json"),
    ])

    guide_authoring = generated / "guide_groom.json"
    guides = generated / "guides.curve_runtime.json"
    groom_proof.run([
        sys.executable, str(args.groom_tool.resolve()), "init",
        "--mesh", str(mesh), "--region", str(region),
        "--asset-id", "psg23f_guides",
        "--output", str(guide_authoring),
        "--set", "groom.strand_count=104",
        "--set", "groom.guide_count=16",
        "--set", "groom.points_per_strand=10",
        "--set", "groom.length=0.61",
        "--set", "groom.clump_strength=0.42",
        "--set", "groom.clump_tip_spread=0.032",
        "--set", "groom.part_strength=0.82",
        "--set", "groom.comb_direction=[0.15,-1.0,0.0]",
        "--set", "groom.comb_strength=0.38",
        "--set", "groom.bend=0.18",
        "--set", "groom.curl=0.035",
        "--set", "groom.root_radius=0.009",
        "--set", "groom.tip_radius=0.002",
    ])
    groom_proof.run([
        sys.executable, str(args.groom_tool.resolve()), "compile",
        "--authoring", str(guide_authoring),
        "--mesh", str(mesh), "--region", str(region),
        "--output", str(guides),
        "--receipt", str(generated / "receipts/guides.json"),
    ])
    guide_sha = groom_proof.sha(guides)

    child_authoring = generated / "render_children.json"
    groom_proof.run([
        sys.executable, str(args.child_tool.resolve()), "init",
        "--guide-asset", str(guides), "--mesh", str(mesh),
        "--asset-id", "psg23f_dense_hair",
        "--output", str(child_authoring),
        "--set", "lod.preview_children_per_parent=4",
        "--set", "lod.interactive_children_per_parent=16",
        "--set", "lod.final_children_per_parent=48",
        "--set", "children.root_barycentric_spread=0.28",
        "--set", "children.length_variation=0.09",
        "--set", "children.shape_variation=0.026",
        "--set", "children.root_radius_scale=0.20",
        "--set", "children.tip_radius_scale=0.10",
    ])

    variants: list[tuple[str, str, pathlib.Path, dict]] = [
        ("guides", "104 THICK GUIDES", guides, {
            "parent_strand_count": 104,
            "render_child_count": 104,
            "primitive_count": 936,
            "maximum_radius": 0.009,
            "lod_level": "guides",
        }),
    ]
    for lod, label in (
        ("preview", "PREVIEW / 416"),
        ("interactive", "INTERACTIVE / 1,664"),
        ("final", "FINAL / 4,992"),
    ):
        runtime, receipt = compile_children(
            args, child_authoring, guides, mesh, generated, lod)
        variants.append((lod, label, runtime, receipt))

    render_contract = {
        "render": {
            "width": 900,
            "height": 700,
            "temporal_frames": 1,
            "integrator_3d": "disney_v2",
            "camera_zoom": 1.48,
        },
        "lighting": {
            "light_mode": 2,
            "environment_light_mode": "ambient",
            "ambient_strength": 0.68,
            "light_intensity": 5.8,
            "light_radius": 0.30,
            "top_fill_strength": 1.55,
            "background_brightness": 0.045,
            "background_color": {"r": 0.035, "g": 0.045, "b": 0.065},
        },
    }
    cells = []
    evidence = []
    for name, label, runtime, receipt in variants:
        scene_path = generated / f"{name}.scene.json"
        request_path = generated / "requests" / f"{name}.request.json"
        raw = raw_root / name
        runtime_asset_id = groom_proof.load(runtime)["asset_id"]
        write_json(scene_path, scene(name, runtime_asset_id, runtime))
        write_json(request_path, render_request(
            "psg23f_render_child_density",
            {
                "id": name,
                "camera_position": {"x": 1.68, "y": -2.18, "z": 2.45},
                "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.58},
            },
            scene_path, request_path, raw, render_contract))
        summary_path = raw / "render_summary.json"
        run_render_cli(args.render_cli.resolve(), request_path, summary_path)
        summary = groom_proof.load(summary_path)
        scalp_audit = object_audit(summary, f"{name}_scalp")
        hair_audit = object_audit(summary, f"{name}_hair")
        width, height, pixels = review_artifacts.read_bmp_rgb(
            raw / "frames/frame_0000.bmp")
        review_artifacts.write_png_rgb(
            review / f"{name}.png", width, height, pixels)
        metrics = image_metrics(pixels)
        if scalp_audit["primary_hit_pixels"] < 500:
            raise RuntimeError(f"{name}: scalp carrier not visible")
        if hair_audit["primary_hit_pixels"] < 1500:
            raise RuntimeError(f"{name}: hair curves not visible")
        if metrics["luma_standard_deviation"] < 7.0:
            raise RuntimeError(f"{name}: render is visually flat")
        cells.append((label, pixels))
        evidence.append({
            "name": name,
            "lod_level": receipt["lod_level"],
            "parent_strand_count": receipt["parent_strand_count"],
            "render_child_count": receipt["render_child_count"],
            "primitive_count": receipt["primitive_count"],
            "maximum_radius": receipt["maximum_radius"],
            "hair_primary_hit_pixels": hair_audit["primary_hit_pixels"],
            "scalp_primary_hit_pixels": scalp_audit["primary_hit_pixels"],
            "native_prepare_frame_ms":
                summary["timing_breakdown"]["native_prepare_frame_ms"],
            "render_trace_ms":
                summary["timing_breakdown"]["render_trace_ms"],
            "image_metrics": metrics,
        })

    child_counts = [
        item["render_child_count"] for item in evidence[1:]]
    if child_counts != sorted(child_counts) or len(set(child_counts)) != 3:
        raise RuntimeError("render child LOD counts are not strictly increasing")
    if evidence[-1]["maximum_radius"] >= evidence[0]["maximum_radius"] * 0.21:
        raise RuntimeError("render children are not materially thinner than guides")
    if evidence[-1]["hair_primary_hit_pixels"] <= \
            evidence[1]["hair_primary_hit_pixels"] * 1.10:
        raise RuntimeError("final LOD does not materially increase hair coverage")
    if groom_proof.sha(mesh) != source_sha or groom_proof.sha(guides) != guide_sha:
        raise RuntimeError("PSG-23F mutated its source mesh or guide asset")

    matrix = review / "psg23f_render_child_density_high_quality_matrix.png"
    write_labeled_contact_sheet(matrix, cells, columns=2)
    summary = {
        "schema": "ray_tracing.procedural_curve_render_children_visual_proof",
        "schema_version": 1,
        "passed": True,
        "render_resolution_per_cell": [900, 700],
        "matrix": str(matrix.relative_to(output)),
        "final_hero": str((review / "final.png").relative_to(output)),
        "fresh_generated_scalp": True,
        "same_source_mesh_sha256": source_sha,
        "same_guide_asset_sha256": guide_sha,
        "variants": evidence,
        "authority": {
            "deterministic_render_child_interpolation": True,
            "stable_lod_subsets": True,
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
