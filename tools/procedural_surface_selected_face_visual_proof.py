#!/usr/bin/env python3
"""Render the PSG-18 flat-control versus selected-face displaced shell proof."""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
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
    runtime_scene,
    write_json,
    write_labeled_contact_sheet,
)


def default_tool(name: str) -> Path:
    return (
        ROOT / "build" / "toolchains" / "clang" / platform.machine() /
        "tools" / "cli" / name
    )


def parse_args() -> argparse.Namespace:
    fixtures = ROOT / "tests" / "fixtures"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--graph", type=Path,
        default=(
            fixtures / "procedural_surface_field_presets" /
            "central_mountain_peak.json"
        ),
    )
    parser.add_argument(
        "--base-recipe", type=Path,
        default=fixtures / "procedural_surface_rock_prism_psg0" / "recipe.json",
    )
    parser.add_argument(
        "--asset-tool", type=Path,
        default=default_tool("procedural_surface_field_preset_asset_tool"),
    )
    parser.add_argument(
        "--render-cli", type=Path,
        default=default_tool("ray_tracing_render_headless"),
    )
    parser.add_argument(
        "--output-root", type=Path,
        default=(
            ROOT / "build" / "agent_runs" / "ray_tracing" /
            "procedural_surface_selected_face" / "psg18"
        ),
    )
    return parser.parse_args()


def load(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def run(command: list[str]) -> None:
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )


def compile_shell(
    tool: Path,
    graph: Path,
    binding: Path,
    base_recipe: Path,
    output: Path,
    asset_id: str,
    amplitude: float,
) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    paths = {
        "recipe": output / "recipe.json",
        "asset": output / "runtime_mesh.json",
        "material": output / "material.json",
        "manifest": output / "derived_asset.json",
        "summary": output / "asset_summary.json",
    }
    run([
        str(tool),
        "--graph", str(graph),
        "--binding", str(binding),
        "--base-recipe", str(base_recipe),
        "--recipe-out", str(paths["recipe"]),
        "--asset-out", str(paths["asset"]),
        "--material-out", str(paths["material"]),
        "--manifest-out", str(paths["manifest"]),
        "--summary-out", str(paths["summary"]),
        "--width", "8.0",
        "--height", "8.0",
        "--depth", "0.8",
        "--target-edge", "0.20",
        "--amplitude", str(amplitude),
        "--edge-lock", "0.40",
        "--asset-id", asset_id,
        "--source-asset-id", "psg18_semantic_prism",
        "--selected-face", "positive_z",
    ])
    return {**load(paths["summary"]), "paths": paths}


def changed_pixels(
    left: list[list[tuple[int, int, int]]],
    right: list[list[tuple[int, int, int]]],
) -> int:
    return sum(
        a != b
        for left_row, right_row in zip(left, right)
        for a, b in zip(left_row, right_row)
    )


def main() -> int:
    args = parse_args()
    output_root = args.output_root.resolve()
    authored = output_root / "authored"
    generated = output_root / "generated"
    raw_runs = output_root / "raw_runs"
    review = output_root / "review"
    for directory in (authored, generated, raw_runs, review):
        directory.mkdir(parents=True, exist_ok=True)

    graph = args.graph.resolve()
    graph_program_id = load(graph)["program_id"]
    binding = authored / "positive_z_selected_face.binding.json"
    write_json(binding, {
        "schema": "ray_tracing.procedural_surface_binding",
        "schema_version": 1,
        "binding_id": "psg18_positive_z_selected_face",
        "graph_program_id": graph_program_id,
        "selector": "surface_group",
        "surface_group_id": "positive_z",
        "up_axis": [0.0, 0.0, 1.0],
        "selector_min_dot": 0.7,
        "selector_feather": 0.0,
        "projection": "planar_xy",
        "projection_scale": 1.0,
        "displacement_direction": "world_up",
        "displacement_scale": 1.0,
        "fallback_color_r": 0.255,
        "fallback_color_g": 0.205,
        "fallback_color_b": 0.145,
        "fallback_roughness": 0.84,
    })

    control = compile_shell(
        args.asset_tool.resolve(), graph, binding, args.base_recipe.resolve(),
        generated / "flat_control", "psg18_flat_refined_control", 0.0,
    )
    displaced = compile_shell(
        args.asset_tool.resolve(), graph, binding, args.base_recipe.resolve(),
        generated / "displaced", "psg18_mountain_shell", 2.2,
    )
    repeated = compile_shell(
        args.asset_tool.resolve(), graph, binding, args.base_recipe.resolve(),
        generated / "repeat", "psg18_mountain_shell", 2.2,
    )
    failures: list[str] = []
    control_receipt = control["selected_face_shell"]
    displaced_receipt = displaced["selected_face_shell"]
    if control_receipt["geometry_displacement_active"]:
        failures.append("flat control reports active displacement")
    if not displaced_receipt["geometry_displacement_active"]:
        failures.append("displaced shell reports no geometry displacement")
    if displaced_receipt[
        "maximum_selected_face_absolute_displacement_units"
    ] < 1.5:
        failures.append("selected face displacement is too small")
    if displaced_receipt[
        "maximum_unselected_face_absolute_displacement_units"
    ] != 0.0:
        failures.append("unselected surface moved")
    if displaced_receipt["derived_selected_face_triangle_count"] <= 2:
        failures.append("selected face was not refined")
    for name, summary in (("control", control), ("displaced", displaced)):
        if (
            summary["boundary_edge_count"] != 0 or
            summary["connected_component_count"] != 1 or
            summary["euler_characteristic"] != 2
        ):
            failures.append(f"{name} shell is not a closed manifold")
    if control["triangle_count"] != displaced["triangle_count"]:
        failures.append("control and displaced topology counts differ")
    if control["mesh_digest_sha256"] == displaced["mesh_digest_sha256"]:
        failures.append("geometry digest did not respond to displacement")
    if (
        displaced["mesh_digest_sha256"] != repeated["mesh_digest_sha256"] or
        displaced["material_digest_sha256"] !=
        repeated["material_digest_sha256"]
    ):
        failures.append("repeat compilation is not deterministic")
    if displaced["snow_likelihood_max"] <= control["snow_likelihood_max"]:
        failures.append("snow did not respond to displaced height and slope")

    contract = {
        "proof_id": "procedural_surface_selected_face_psg18",
        "render": {
            "width": 1200,
            "height": 900,
            "integrator_3d": "direct_light",
            "temporal_frames": 8,
            "camera_zoom": 1.0,
        },
        "lighting": {
            "environment_light_mode": "ambient",
            "ambient_strength": 0.20,
            "top_fill_strength": 0.82,
            "light_intensity": 3.0,
            "light_radius": 0.0,
        },
        "light_rig": [
            {
                "id": "ridge_key",
                "position": {"x": -7.0, "y": -8.5, "z": 9.0},
                "intensity": 3.2,
            },
            {
                "id": "profile_fill",
                "position": {"x": 8.0, "y": -3.0, "z": 5.0},
                "intensity": 1.6,
            },
        ],
    }
    views = [
        {
            "id": "hero",
            "camera_position": {"x": 9.8, "y": -11.0, "z": 7.4},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 0.65},
        },
        {
            "id": "profile",
            "camera_position": {"x": 12.8, "y": -0.8, "z": 1.45},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 0.55},
        },
    ]
    pixels: dict[tuple[str, str], list[list[tuple[int, int, int]]]] = {}
    audits: dict[tuple[str, str], dict] = {}
    cells = []
    render_results = []
    for variant_name, summary in (
        ("flat control", control),
        ("displaced shell", displaced),
    ):
        safe_name = variant_name.replace(" ", "_")
        scene_path = generated / safe_name / "scene.json"
        object_id = f"psg18_{safe_name}"
        write_json(
            scene_path,
            runtime_scene(
                f"psg18_{safe_name}", object_id,
                summary["selected_face_shell"]["derived_asset_id"],
                summary["paths"]["asset"], summary["paths"]["manifest"],
                scene_path, summary, contract["light_rig"],
            ),
        )
        for view in views:
            cell_id = f"{safe_name}_{view['id']}"
            request_path = generated / safe_name / f"request_{view['id']}.json"
            run_root = raw_runs / cell_id
            write_json(
                request_path,
                render_request(
                    f"psg18_{safe_name}", view, scene_path, request_path,
                    run_root, contract,
                ),
            )
            run_render_cli(
                args.render_cli.resolve(), request_path,
                run_root / "render_summary.json",
            )
            render_summary = load(run_root / "render_summary.json")
            audit = object_audit(render_summary, object_id)
            frame = run_root / "frames" / "frame_0000.bmp"
            width, height, image = review_artifacts.read_bmp_rgb(frame)
            png = review / f"{cell_id}.png"
            review_artifacts.write_png_rgb(png, width, height, image)
            if audit["triangle_count"] != summary["triangle_count"]:
                failures.append(f"{cell_id}: runtime topology drift")
            if audit["primary_hit_pixels"] < 75000:
                failures.append(f"{cell_id}: insufficient geometry coverage")
            if image_metrics(image)["luma_standard_deviation"] < 6.0:
                failures.append(f"{cell_id}: insufficient tonal detail")
            if not render_summary.get(
                "procedural_surface_runtime", {}
            ).get("loaded"):
                failures.append(f"{cell_id}: derived material did not load")
            pixels[(variant_name, view["id"])] = image
            audits[(variant_name, view["id"])] = audit
            cells.append((f"{variant_name} {view['id']}", image))
            render_results.append({
                "variant": variant_name,
                "view": view["id"],
                "primary_hit_pixels": audit["primary_hit_pixels"],
                "png": str(png.relative_to(output_root)),
                "png_sha256": hashlib.sha256(png.read_bytes()).hexdigest(),
            })

    comparisons = {}
    for view in views:
        view_id = view["id"]
        changed = changed_pixels(
            pixels[("flat control", view_id)],
            pixels[("displaced shell", view_id)],
        )
        hit_delta = abs(
            audits[("flat control", view_id)]["primary_hit_pixels"] -
            audits[("displaced shell", view_id)]["primary_hit_pixels"]
        )
        comparisons[view_id] = {
            "changed_pixels": changed,
            "primary_hit_pixel_delta": hit_delta,
        }
        if changed < 60000:
            failures.append(f"{view_id}: displaced rendering changed too little")
        minimum_hit_delta = 4000 if view_id == "hero" else 20000
        if hit_delta < minimum_hit_delta:
            failures.append(
                f"{view_id}: silhouette/geometry hit coverage changed too little"
            )

    contact_sheet = review / "psg18_selected_face_displacement.png"
    write_labeled_contact_sheet(contact_sheet, cells, columns=2)
    result = {
        "schema": "ray_tracing.procedural_surface_selected_face_visual_proof",
        "schema_version": 1,
        "passed": not failures,
        "render_resolution": [1200, 900],
        "temporal_frames": 8,
        "source_semantic_asset_id": "psg18_semantic_prism",
        "selected_face": "positive_z",
        "control": {k: v for k, v in control.items() if k != "paths"},
        "displaced": {k: v for k, v in displaced.items() if k != "paths"},
        "repeat_mesh_digest_sha256": repeated["mesh_digest_sha256"],
        "comparisons": comparisons,
        "renders": render_results,
        "contact_sheet": str(contact_sheet.relative_to(output_root)),
        "failures": failures,
        "authority": {
            "local_diagnostic_only": True,
            "fresh_derived_objects_created": True,
            "saved_scene_mutated": False,
            "package_or_release_mutated": False,
        },
    }
    write_json(output_root / "summary.json", result)
    if failures:
        raise RuntimeError("; ".join(failures))
    print(output_root / "summary.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
