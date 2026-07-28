#!/usr/bin/env python3
"""Compile and natively render the PSG-11 or PSG-12 solid family."""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_DIR = ROOT / "tests" / "integration"
if str(INTEGRATION_DIR) not in sys.path:
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


FIXTURE_ROOT = ROOT / "tests" / "fixtures" / "procedural_solid_graphs"
def default_tool(name: str) -> Path:
    return (
        ROOT / "build" / "toolchains" / "clang" / platform.machine() /
        "tools" / "cli" / name
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--solid-tool", type=Path,
        default=default_tool("procedural_solid_asset_tool"),
    )
    parser.add_argument(
        "--render-cli", type=Path,
        default=default_tool("ray_tracing_render_headless"),
    )
    parser.add_argument(
        "--output-root", type=Path,
    )
    parser.add_argument("--quality-adaptive", action="store_true")
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def run_checked(command: list[str]) -> None:
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )


def scene(
    scene_id: str,
    asset_id: str,
    color: dict[str, float],
    lane: str,
) -> dict:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": scene_id,
        "source_scene_id": scene_id,
        "compile_meta": {
            "compiler_version": f"procedural_solid_visual_proof_{lane}",
            "compiled_at_ns": 0,
            "normalization": "local_diagnostic",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [{
            "object_id": f"{asset_id}_object",
            "object_type": "mesh_asset_instance",
            "dimensional_mode": "full_3d",
            "transform": {
                "position": {"x": 0.0, "y": 0.0, "z": 0.0},
                "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
            },
            "geometry_ref": {"kind": "mesh_asset", "id": asset_id},
            "material_ref": {"id": "mat_solid"},
            "flags": {"visible": True, "locked": False, "selectable": True},
        }],
        "materials": [{
            "id": "mat_solid",
            "name": f"{lane.upper()} feature-preserved solid",
            "base_color": color,
            "roughness": 0.72,
            "metallic": 0.0,
        }],
        "lights": [],
        "extensions": {},
    }


def framed_views(receipt: dict) -> list[dict]:
    low = receipt["bounds_min"]
    high = receipt["bounds_max"]
    center = {
        axis: (low[axis] + high[axis]) * 0.5
        for axis in ("x", "y", "z")
    }
    radius = max(high[axis] - low[axis] for axis in ("x", "y", "z"))
    return [
        {
            "id": "hero",
            "label": "Hero",
            "camera_position": {
                "x": center["x"] + 1.3 * radius,
                "y": center["y"] - 1.65 * radius,
                "z": center["z"] + 1.1 * radius,
            },
            "camera_look_at": center,
        },
        {
            "id": "side",
            "label": "Y axis",
            "camera_position": {
                "x": center["x"],
                "y": center["y"] - 2.25 * radius,
                "z": center["z"] + 0.08 * radius,
            },
            "camera_look_at": center,
        },
        {
            "id": "top",
            "label": "Z axis",
            "camera_position": {
                "x": center["x"],
                "y": center["y"] - 0.005 * radius,
                "z": center["z"] + 2.25 * radius,
            },
            "camera_look_at": center,
        },
    ]


def main() -> int:
    args = parse_args()
    lane = "psg12" if args.quality_adaptive else "psg11"
    output_root = (
        args.output_root
        if args.output_root
        else ROOT / "build" / "agent_runs" / "ray_tracing" /
            "procedural_solid" / lane
    ).resolve()
    solid_tool = args.solid_tool.resolve()
    render_cli = args.render_cli.resolve()
    generated = output_root / "generated"
    assets = generated / "assets" / "mesh_assets"
    receipts = generated / "receipts"
    review = output_root / "review"
    raw_runs = output_root / "raw_runs"
    for directory in (assets, receipts, review, raw_runs):
        directory.mkdir(parents=True, exist_ok=True)

    cases = [
        {
            "id": "transformed_box",
            "label": "Object transform",
            "graph": "transformed_box.json",
            "euler": 2,
            "color": {"r": 0.61, "g": 0.66, "b": 0.72},
        },
        {
            "id": "twisted_tapered_column",
            "label": "Twist and taper",
            "graph": "twisted_tapered_column.json",
            "euler": 2,
            "color": {"r": 0.55, "g": 0.43, "b": 0.31},
        },
        {
            "id": "rounded_block_with_tunnel",
            "label": "Boolean tunnel",
            "graph": "rounded_block_with_tunnel.json",
            "euler": 0,
            "color": {"r": 0.49, "g": 0.54, "b": 0.58},
        },
        {
            "id": "blended_double_sphere",
            "label": "Smooth union",
            "graph": "blended_double_sphere.json",
            "euler": 2,
            "color": {"r": 0.38, "g": 0.58, "b": 0.44},
        },
        {
            "id": "source_mesh_twist",
            "label": "Source mesh twist",
            "graph": "source_mesh_twist.json",
            "euler": 2,
            "color": {"r": 0.58, "g": 0.43, "b": 0.64},
            "source": True,
            "cells": 24,
        },
    ]
    render_contract = {
        "render": {
            "width": 480,
            "height": 360,
            "temporal_frames": 1,
            "integrator_3d": "direct_light",
            "camera_zoom": 1.12,
        },
        "lighting": {
            "environment_light_mode": "ambient",
            "ambient_strength": 0.38,
            "top_fill_strength": 1.3,
            "light_intensity": 4.2,
            "light_radius": 0.14,
        },
    }
    failures: list[str] = []
    results: list[dict] = []
    cells = []
    hero_pixels: dict[str, list] = {}
    receipt_by_id: dict[str, dict] = {}

    source_mesh = FIXTURE_ROOT / "source_cube.runtime.json"

    for case in cases:
        asset_id = f"{lane}_{case['id']}"
        asset_path = assets / f"{asset_id}.runtime.json"
        receipt_path = receipts / f"{case['id']}.json"
        command = [
            str(solid_tool),
            "--graph", str(FIXTURE_ROOT / case["graph"]),
            "--out", str(asset_path),
            "--summary-out", str(receipt_path),
            "--asset-id", asset_id,
            "--cells", str(case.get("cells", 24)),
            "--maximum-cells", "48",
            "--feature-size", "0.18",
            "--collision-authority", "derived_shell",
        ]
        if args.quality_adaptive:
            command.extend([
                "--quality-adaptive",
                "--quality-maximum-cells", "96",
            ])
        else:
            command.append("--local-adaptive")
        if case.get("source"):
            command.extend(["--source", f"source_cube={source_mesh}"])
        run_checked(command)
        receipt = load_json(receipt_path)
        receipt_by_id[case["id"]] = receipt
        case_views = framed_views(receipt)
        if (
            receipt["local_adaptive"] == args.quality_adaptive or
            receipt["quality_adaptive"] != args.quality_adaptive or
            not receipt["local_converged"] or
            receipt["local_pass_count"] < 2 or
            receipt["boundary_edge_count"] != 0 or
            receipt["nonmanifold_edge_count"] != 0 or
            receipt["connected_component_count"] != 1 or
            receipt["euler_characteristic"] != case["euler"] or
            receipt["signed_volume_units3"] <= 0.0 or
            not receipt["conforming_cell_self_intersection_free"]
        ):
            failures.append(f"{case['id']}: topology receipt failed")
        selected_pass = receipt["local_passes"][
            receipt["local_selected_pass"]
        ]
        if (
            selected_pass["active_cell_ratio"] >= 0.55 or
            selected_pass["transition_surface_crossing_count"] != 0 or
            receipt["evaluated_sample_count"] >= receipt["sample_count"] or
            (
                not args.quality_adaptive and
                receipt["feature_improvement_ratio"] <= 0.5
            ) or
            not receipt["feature_topology_preserved"] or
            receipt["feature_vertex_count"] <= 0 or
            receipt["region_count"] <= 0
        ):
            failures.append(f"{case['id']}: {lane.upper()} quality receipt failed")
        if args.quality_adaptive and (
            not receipt["quality_refinement_triggered"] or
            not receipt["quality_refinement_selected"] or
            receipt["quality_refinement_improvement_ratio"] <= 0.08 or
            receipt["quality_selected_signed_distance_rms_units"] >=
                receipt["quality_baseline_signed_distance_rms_units"] or
            receipt["quality_selected_composite_score"] >=
                receipt["quality_baseline_composite_score"] or
            receipt["crease_qef_improvement_ratio"] <= 0.10 or
            not receipt["crease_topology_preserved"] or
            receipt["shading_split_vertex_count"] <= 0 or
            receipt["shading_hard_corner_improvement_ratio"] <= 0.25 or
            not receipt["shading_geometric_topology_preserved"]
        ):
            failures.append(f"{case['id']}: PSG-12 quality receipt failed")
        if (
            case["id"] == "rounded_block_with_tunnel" and
            receipt["cut_triangle_count"] <= 0
        ):
            failures.append(f"{case['id']}: cut region missing")
        if (
            case["id"] == "blended_double_sphere" and
            receipt["blend_triangle_count"] <= 0
        ):
            failures.append(f"{case['id']}: blend region missing")
        if (
            case.get("source") and
            (
                receipt["source_query_count"] <= 0 or
                receipt["accelerated_source_query_count"] !=
                receipt["source_query_count"]
            )
        ):
            failures.append(f"{case['id']}: source acceleration missing")
        scene_path = generated / f"{case['id']}.scene.json"
        write_json(
            scene_path,
            scene(
                f"{lane}_{case['id']}", asset_id, case["color"], lane
            ),
        )
        for view in case_views:
            cell_id = f"{case['id']}_{view['id']}"
            run_root = raw_runs / cell_id
            request_path = generated / f"{cell_id}.request.json"
            summary_path = run_root / "render_summary.json"
            write_json(
                request_path,
                render_request(
                    f"{lane}_solid_domain", view, scene_path, request_path,
                    run_root, render_contract,
                ),
            )
            run_render_cli(render_cli, request_path, summary_path)
            render_summary = load_json(summary_path)
            audit = object_audit(render_summary, f"{asset_id}_object")
            frame = run_root / "frames" / "frame_0000.bmp"
            width, height, pixels = review_artifacts.read_bmp_rgb(frame)
            png = review / f"{cell_id}.png"
            review_artifacts.write_png_rgb(png, width, height, pixels)
            metrics = image_metrics(pixels)
            if view["id"] == "hero":
                hero_pixels[case["id"]] = pixels
            if audit["triangle_count"] != receipt["triangle_count"]:
                failures.append(f"{cell_id}: runtime triangle parity failed")
            if audit["primary_hit_pixels"] < 1800:
                failures.append(f"{cell_id}: insufficient visible coverage")
            if metrics["luma_standard_deviation"] < 8.0:
                failures.append(f"{cell_id}: insufficient form contrast")
            cells.append((f"{case['label']} - {view['label']}", pixels))
            results.append({
                "case": case["id"],
                "view": view["id"],
                "triangle_count": audit["triangle_count"],
                "primary_hit_pixels": audit["primary_hit_pixels"],
                "luma_standard_deviation":
                    metrics["luma_standard_deviation"],
                "image": str(png),
            })

    changed_pixels: dict[str, int] = {}
    control = hero_pixels["transformed_box"]
    for case in cases[1:]:
        subject = hero_pixels[case["id"]]
        changed = sum(
            left != right
            for left_row, right_row in zip(control, subject)
            for left, right in zip(left_row, right_row)
        )
        changed_pixels[case["id"]] = changed
        if changed < 1800:
            failures.append(f"{case['id']}: lacks visible shape distinction")

    contact = review / f"procedural_solid_{lane}.png"
    write_labeled_contact_sheet(contact, cells, columns=3)
    summary = {
        "schema": f"ray_tracing.procedural_solid_visual_proof_{lane}",
        "schema_version": 1,
        "passed": not failures,
        "authority": "local_diagnostic_only",
        "cases": cases,
        "receipts": receipt_by_id,
        "views": results,
        "changed_hero_pixels": changed_pixels,
        "contact_sheet": str(contact),
        "failures": failures,
    }
    write_json(output_root / "proof_summary.json", summary)
    (output_root / "index.md").write_text(
        "\n".join([
            f"# {lane.upper()} feature-preserving solid proof",
            "",
            "UI-free deterministic refinement, zero-set feature projection, "
            "retained/cut/blend regions, accelerated imported source "
            "evaluation, and feature-aware shading across the solid family.",
            "",
            f"![{lane.upper()} solid proof]({contact})",
            "",
            "Local diagnostic proof only; no saved scene, package, version, "
            "release, or promotion state changed.",
            "",
        ]),
        encoding="utf-8",
    )
    print(json.dumps(summary, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
