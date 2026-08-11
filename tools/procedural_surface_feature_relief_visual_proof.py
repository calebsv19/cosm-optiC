#!/usr/bin/env python3
"""Render one coherent wall shell with signed PSG-24A spot relief."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
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
        default=fixtures / "procedural_surface_field_presets/pitted_concrete.json",
    )
    parser.add_argument(
        "--base-recipe", type=Path,
        default=fixtures / "procedural_surface_rock_prism_psg0/recipe.json",
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
            ROOT / "build/agent_runs/ray_tracing/"
            "procedural_surface_feature_relief/psg24r"
        ),
    )
    return parser.parse_args()


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


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
    feature_field: Path | None = None,
    source_mesh_digest: str | None = None,
    target_edge: float = 0.10,
) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    paths = {
        "recipe": output / "recipe.json",
        "asset": output / "runtime_mesh.json",
        "material": output / "material.json",
        "manifest": output / "derived_asset.json",
        "summary": output / "asset_summary.json",
    }
    command = [
        str(tool),
        "--graph", str(graph),
        "--binding", str(binding),
        "--base-recipe", str(base_recipe),
        "--recipe-out", str(paths["recipe"]),
        "--asset-out", str(paths["asset"]),
        "--material-out", str(paths["material"]),
        "--manifest-out", str(paths["manifest"]),
        "--summary-out", str(paths["summary"]),
        "--width", "6.0",
        "--height", "0.5",
        "--depth", "4.0",
        "--target-edge", str(target_edge),
        "--amplitude", str(amplitude),
        "--edge-lock", "0.28",
        "--asset-id", asset_id,
        "--source-asset-id", "psg24r_semantic_concrete_wall",
        "--selected-face", "positive_y",
    ]
    if feature_field and source_mesh_digest:
        command.extend([
            "--surface-feature-field", str(feature_field),
            "--feature-source-mesh-digest", source_mesh_digest,
            "--relief-scale", "1.0",
        ])
    run(command)
    return {**load(paths["summary"]), "paths": paths}


def triangle_centroid(mesh: dict, triangle: dict) -> tuple[float, float, float]:
    vertices = mesh["mesh"]["vertices"]
    points = [vertices[triangle[key]] for key in ("a", "b", "c")]
    return tuple(sum(point[axis] for point in points) / 3.0 for axis in ("x", "y", "z"))


def create_feature_field(control_asset: Path, source_digest: str, output: Path) -> None:
    mesh = load(control_asset)
    triangles = mesh["mesh"]["triangles"]
    desired = [
        (-2.10, -1.10, 0.48, -0.080),
        (-1.25, 0.45, 0.36, -0.055),
        (-0.25, -0.65, 0.42, -0.068),
        (0.80, 0.85, 0.52, -0.075),
        (2.05, -0.35, 0.34, -0.050),
        (-1.85, 1.05, 0.38, 0.052),
        (-0.55, 1.25, 0.46, 0.065),
        (0.55, -1.35, 0.40, 0.058),
        (1.55, 0.25, 0.50, 0.070),
        (2.25, 1.15, 0.32, 0.045),
    ]
    candidates = [
        (index, triangle, triangle_centroid(mesh, triangle))
        for index, triangle in enumerate(triangles)
        if triangle["surface_group_id"] == "positive_y"
    ]
    features = []
    used_triangles: set[int] = set()
    for feature_index, (target_x, target_z, radius, signed_height) in enumerate(desired):
        available = [entry for entry in candidates if entry[0] not in used_triangles]
        source_index, _triangle, centroid = min(
            available,
            key=lambda entry: math.hypot(
                entry[2][0] - target_x, entry[2][2] - target_z
            ),
        )
        used_triangles.add(source_index)
        features.append({
            "feature_id": 2401800 + feature_index,
            "population": 1 if signed_height < 0.0 else 2,
            "source_triangle": source_index,
            "barycentric_root": [1.0 / 3.0] * 3,
            "position": list(centroid),
            "normal": [0.0, 1.0, 0.0],
            "tangent": [1.0, 0.0, 0.0],
            "bitangent": [0.0, 0.0, 1.0],
            "radius": radius,
            "aspect": 0.72 + 0.08 * (feature_index % 4),
            "rotation": 0.31 * feature_index,
            "edge_softness": 0.18,
            "rim_width": 0.22,
            "height_or_depth": signed_height,
        })
    authoring_bytes = json.dumps(features, sort_keys=True, separators=(",", ":")).encode()
    write_json(output, {
        "schema": "surface_feature_field_v1",
        "schema_version": 1,
        "source_mesh_digest_sha256": source_digest,
        "authoring_digest_sha256": hashlib.sha256(authoring_bytes).hexdigest(),
        "seed": 24018,
        "normal_compatibility_cosine": 0.8,
        "features": features,
    })


def changed_pixels(left: list, right: list) -> int:
    return sum(
        a != b
        for left_row, right_row in zip(left, right)
        for a, b in zip(left_row, right_row)
    )


def difference_image(left: list, right: list) -> list:
    return [
        [tuple(abs(a[channel] - b[channel]) for channel in range(3))
         for a, b in zip(left_row, right_row)]
        for left_row, right_row in zip(left, right)
    ]


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
    binding = authored / "positive_y_wall.binding.json"
    field = authored / "formed_concrete_signed_spots.field.json"
    write_json(binding, {
        "schema": "ray_tracing.procedural_surface_binding",
        "schema_version": 1,
        "binding_id": "psg24r_positive_y_concrete_wall",
        "graph_program_id": load(graph)["program_id"],
        "selector": "surface_group",
        "surface_group_id": "positive_y",
        "up_axis": [0.0, 0.0, 1.0],
        "selector_min_dot": 0.7,
        "selector_feather": 0.0,
        "projection": "planar_xz",
        "projection_scale": 1.0,
        "displacement_direction": "source_normal",
        "displacement_scale": 1.0,
        "fallback_color_r": 0.38,
        "fallback_color_g": 0.37,
        "fallback_color_b": 0.35,
        "fallback_roughness": 0.90,
    })

    control = compile_shell(
        args.asset_tool.resolve(), graph, binding, args.base_recipe.resolve(),
        generated / "flat_control", "psg24r_flat_wall", 0.0,
    )
    create_feature_field(
        control["paths"]["asset"], control["mesh_digest_sha256"], field,
    )
    relief = compile_shell(
        args.asset_tool.resolve(), graph, binding, args.base_recipe.resolve(),
        generated / "signed_relief", "psg24r_signed_wall", 0.10,
        field, control["mesh_digest_sha256"],
    )
    repeat = compile_shell(
        args.asset_tool.resolve(), graph, binding, args.base_recipe.resolve(),
        generated / "repeat", "psg24r_signed_wall", 0.10,
        field, control["mesh_digest_sha256"],
    )

    failures: list[str] = []
    receipt = relief["signed_feature_relief"]
    if receipt["negative_depth_feature_count"] != 5:
        failures.append("negative feature count is not five")
    if receipt["positive_height_feature_count"] != 5:
        failures.append("positive feature count is not five")
    if receipt["negatively_displaced_vertex_count"] <= 0:
        failures.append("no vertices moved inward")
    if receipt["positively_displaced_vertex_count"] <= 0:
        failures.append("no vertices moved outward")
    if receipt["minimum_emitted_displacement_units"] >= -0.04:
        failures.append("inward relief is too shallow")
    if receipt["maximum_emitted_displacement_units"] <= 0.035:
        failures.append("outward relief is too shallow")
    if not receipt["one_coherent_derived_shell"]:
        failures.append("relief is not one coherent shell")
    if relief["selected_face_shell"][
        "maximum_unselected_face_absolute_displacement_units"
    ] != 0.0:
        failures.append("an unselected surface moved")
    if relief["boundary_edge_count"] != 0 or relief["euler_characteristic"] != 2:
        failures.append("relief shell is not closed Euler-2 topology")
    if control["triangle_count"] != relief["triangle_count"]:
        failures.append("control and relief topology counts differ")
    if control["mesh_digest_sha256"] == relief["mesh_digest_sha256"]:
        failures.append("signed relief did not change mesh identity")
    if relief["mesh_digest_sha256"] != repeat["mesh_digest_sha256"]:
        failures.append("repeat relief mesh digest differs")
    if relief["material_digest_sha256"] != repeat["material_digest_sha256"]:
        failures.append("repeat relief material digest differs")

    contract = {
        "proof_id": "procedural_surface_feature_relief_psg24r",
        "render": {
            "width": 1200,
            "height": 900,
            "integrator_3d": "direct_light",
            "temporal_frames": 4,
            "camera_zoom": 1.0,
        },
        "lighting": {
            "environment_light_mode": "ambient",
            "ambient_strength": 0.26,
            "top_fill_strength": 0.75,
            "light_intensity": 3.4,
            "light_radius": 0.0,
        },
        "light_rig": [
            {"id": "raking_key", "position": {"x": -4.0, "y": 5.0, "z": 5.5}, "intensity": 3.6},
            {"id": "lower_fill", "position": {"x": 4.5, "y": 2.5, "z": -2.0}, "intensity": 1.4},
        ],
    }
    views = [
        {
            "id": "hero",
            "camera_position": {"x": 3.8, "y": 8.5, "z": 3.1},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 0.0},
        },
        {
            "id": "grazing",
            "camera_position": {"x": 8.8, "y": 4.0, "z": 0.8},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 0.0},
        },
    ]
    pixels: dict[tuple[str, str], list] = {}
    audits: dict[tuple[str, str], dict] = {}
    render_rows = []
    for variant_name, summary in (
        ("flat control", control),
        ("signed relief", relief),
        ("exact repeat", repeat),
    ):
        safe_name = variant_name.replace(" ", "_")
        scene_path = generated / safe_name / "scene.json"
        object_id = f"psg24r_{safe_name}"
        write_json(
            scene_path,
            runtime_scene(
                f"psg24r_{safe_name}", object_id,
                summary["selected_face_shell"]["derived_asset_id"],
                summary["paths"]["asset"], summary["paths"]["manifest"],
                scene_path, summary, contract["light_rig"],
            ),
        )
        selected_views = views if variant_name != "exact repeat" else views[:1]
        for view in selected_views:
            cell_id = f"{safe_name}_{view['id']}"
            request_path = generated / safe_name / f"request_{view['id']}.json"
            run_root = raw_runs / cell_id
            write_json(
                request_path,
                render_request(
                    f"psg24r_{safe_name}", view, scene_path, request_path,
                    run_root, contract,
                ),
            )
            run_render_cli(
                args.render_cli.resolve(), request_path,
                run_root / "render_summary.json",
            )
            render_summary = load(run_root / "render_summary.json")
            audit = object_audit(render_summary, object_id)
            frame = run_root / "frames/frame_0000.bmp"
            width, height, image = review_artifacts.read_bmp_rgb(frame)
            png = review / f"{cell_id}.png"
            review_artifacts.write_png_rgb(png, width, height, image)
            if audit["triangle_count"] != summary["triangle_count"]:
                failures.append(f"{cell_id}: runtime topology drift")
            if audit["primary_hit_pixels"] < 25000:
                failures.append(f"{cell_id}: insufficient wall coverage")
            if image_metrics(image)["luma_standard_deviation"] < 5.0:
                failures.append(f"{cell_id}: insufficient tonal detail")
            pixels[(variant_name, view["id"])] = image
            audits[(variant_name, view["id"])] = audit
            render_rows.append({
                "variant": variant_name,
                "view": view["id"],
                "primary_hit_pixels": audit["primary_hit_pixels"],
                "png": str(png.relative_to(output_root)),
                "png_sha256": hashlib.sha256(png.read_bytes()).hexdigest(),
            })

    hero_changed = changed_pixels(
        pixels[("flat control", "hero")],
        pixels[("signed relief", "hero")],
    )
    grazing_changed = changed_pixels(
        pixels[("flat control", "grazing")],
        pixels[("signed relief", "grazing")],
    )
    repeat_changed = changed_pixels(
        pixels[("signed relief", "hero")],
        pixels[("exact repeat", "hero")],
    )
    if hero_changed < 8000:
        failures.append("hero relief changed too few pixels")
    if grazing_changed < 8000:
        failures.append("grazing relief changed too few pixels")
    if repeat_changed != 0:
        failures.append("repeat relief render differs")
    difference = difference_image(
        pixels[("signed relief", "hero")],
        pixels[("exact repeat", "hero")],
    )
    difference_png = review / "exact_repeat_difference.png"
    review_artifacts.write_png_rgb(difference_png, 1200, 900, difference)
    cells = [
        ("FLAT WALL CONTROL", pixels[("flat control", "hero")]),
        ("SIGNED RELIEF: PORES + AGGREGATE", pixels[("signed relief", "hero")]),
        ("FLAT WALL GRAZING", pixels[("flat control", "grazing")]),
        ("SIGNED RELIEF GRAZING", pixels[("signed relief", "grazing")]),
        ("EXACT REPEAT", pixels[("exact repeat", "hero")]),
        ("REPEAT DIFFERENCE (BLACK = ZERO)", difference),
    ]
    contact_sheet = review / "psg24r_signed_wall_relief_review.png"
    write_labeled_contact_sheet(contact_sheet, cells, columns=2)
    result = {
        "schema": "ray_tracing.procedural_surface_feature_relief_visual_proof",
        "schema_version": 1,
        "passed": not failures,
        "source_semantic_asset_id": "psg24r_semantic_concrete_wall",
        "selected_face": "positive_y",
        "render_resolution": [1200, 900],
        "control_mesh_digest_sha256": control["mesh_digest_sha256"],
        "relief_mesh_digest_sha256": relief["mesh_digest_sha256"],
        "repeat_mesh_digest_sha256": repeat["mesh_digest_sha256"],
        "signed_feature_relief": receipt,
        "comparisons": {
            "hero_changed_pixels": hero_changed,
            "grazing_changed_pixels": grazing_changed,
            "repeat_changed_pixels": repeat_changed,
        },
        "renders": render_rows,
        "contact_sheet": str(contact_sheet.relative_to(output_root)),
        "failures": failures,
        "authority": {
            "local_diagnostic_only": True,
            "source_cage_preserved": True,
            "one_replaceable_derived_shell": True,
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
