#!/usr/bin/env python3
"""Render a compact, editable formed-concrete preset matrix on one wall."""
from __future__ import annotations

import argparse
import hashlib
import json
import platform
import subprocess
from pathlib import Path

from procedural_surface_feature_relief_visual_proof import (
    changed_pixels, compile_shell, difference_image, load, render_request,
    review_artifacts, run_render_cli, runtime_scene, write_json,
    write_labeled_contact_sheet,
)

ROOT = Path(__file__).resolve().parents[1]


def relief_target_edge_is_resolved(target_edge: float, radii: list[float]) -> bool:
    """Keep the smallest relief from collapsing into a single triangle."""
    finite_radii = [float(radius) for radius in radii if float(radius) > 0.0]
    return bool(finite_radii) and target_edge <= min(finite_radii) * 0.6


def default_tool(name: str) -> Path:
    return ROOT / "build/toolchains/clang" / platform.machine() / "tools/cli" / name


def run(command: list[str]) -> None:
    subprocess.run(command, text=True, capture_output=True, check=True)


def render_variant(summary: dict, variant: str, view: dict, contract: dict,
                   binding: Path, output: Path, render_cli: Path) -> tuple[list, Path]:
    scene = output / "scene.json"
    request = output / "request.json"
    raw = output / "raw"
    object_id = f"formed_concrete_{variant}"
    write_json(scene, runtime_scene(
        object_id, object_id, summary["selected_face_shell"]["derived_asset_id"],
        summary["paths"]["asset"], summary["paths"]["manifest"], scene, summary,
        contract["light_rig"],
    ))
    write_json(request, render_request(object_id, view, scene, request, raw, contract))
    run_render_cli(render_cli, request, raw / "render_summary.json")
    width, height, pixels = review_artifacts.read_bmp_rgb(raw / "frames/frame_0000.bmp")
    png = output / f"{variant}_{view['id']}.png"
    review_artifacts.write_png_rgb(png, width, height, pixels)
    return pixels, png


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-tool", type=Path, default=default_tool("procedural_surface_field_preset_asset_tool"))
    parser.add_argument("--render-cli", type=Path, default=default_tool("ray_tracing_render_headless"))
    parser.add_argument("--output-root", type=Path, default=ROOT / "build/agent_runs/ray_tracing/formed_concrete_preset_matrix")
    parser.add_argument("--target-edge", type=float, default=0.04,
                        help="relief lattice spacing in object units")
    args = parser.parse_args()
    root = args.output_root.resolve()
    authored, generated, review = root / "authored", root / "generated", root / "review"
    for directory in (authored, generated, review): directory.mkdir(parents=True, exist_ok=True)
    graph = ROOT / "tests/fixtures/procedural_surface_field_presets/pitted_concrete.json"
    recipe = ROOT / "tests/fixtures/procedural_surface_rock_prism_psg0/recipe.json"
    binding = authored / "positive_y_wall.binding.json"
    write_json(binding, {
        "schema": "ray_tracing.procedural_surface_binding", "schema_version": 1,
        "binding_id": "formed_concrete_matrix_positive_y", "graph_program_id": load(graph)["program_id"],
        "selector": "surface_group", "surface_group_id": "positive_y", "up_axis": [0, 0, 1],
        "selector_min_dot": 0.7, "selector_feather": 0.0, "projection": "planar_xz",
        "projection_scale": 1.0, "displacement_direction": "source_normal", "displacement_scale": 1.0,
        "fallback_color_r": .38, "fallback_color_g": .37, "fallback_color_b": .35, "fallback_roughness": .9,
    })
    if args.target_edge <= 0.0:
        parser.error("--target-edge must be positive")
    control = compile_shell(args.asset_tool, graph, binding, recipe, generated / "control", "formed_concrete_control", 0.0,
                            target_edge=args.target_edge)
    source_digest = control["mesh_digest_sha256"]
    variants: dict[str, dict] = {}
    fixtures = ROOT / "tests/fixtures/procedural_surface_formed_concrete_presets"
    for name in ("low", "medium", "high"):
        field_root = generated / name / "field"
        run(["python3", str(ROOT / "tools/procedural_surface_formed_concrete_preset.py"),
             "--preset", str(fixtures / f"{name}.json"), "--mesh", str(control["paths"]["asset"]),
             "--source-mesh-digest", source_digest, "--output-root", str(field_root)])
        variants[name] = compile_shell(
            args.asset_tool, graph, binding, recipe, generated / name / "relief",
            f"formed_concrete_{name}", .10,
            field_root / "assets/surface_feature_field_v1.json", source_digest,
            target_edge=args.target_edge,
        )
        field = load(field_root / "assets/surface_feature_field_v1.json")
        if not relief_target_edge_is_resolved(
            args.target_edge, [feature["radius"] for feature in field["features"]]
        ):
            raise RuntimeError(
                f"target edge {args.target_edge} under-resolves {name} relief"
            )
    # The high preset is repeated to establish a concrete preset-to-render
    # determinism claim without treating a visual matrix as a material proof.
    high_repeat = compile_shell(
        args.asset_tool, graph, binding, recipe, generated / "high_repeat", "formed_concrete_high", .10,
        generated / "high/field/assets/surface_feature_field_v1.json", source_digest,
        target_edge=args.target_edge,
    )
    contract = {"render": {"width": 640, "height": 480, "integrator_3d": "direct_light", "temporal_frames": 1, "camera_zoom": 1.0},
                "lighting": {"environment_light_mode": "ambient", "ambient_strength": .30, "top_fill_strength": .8, "light_intensity": 3.6, "light_radius": 0.0},
                "light_rig": [{"id": "raking_key", "position": {"x": -4.0, "y": 5.0, "z": 5.5}, "intensity": 3.8},
                              {"id": "soft_fill", "position": {"x": 4.5, "y": 2.5, "z": -2.0}, "intensity": 1.6}]}
    hero = {"id": "hero", "camera_position": {"x": 3.8, "y": 8.5, "z": 3.1}, "camera_look_at": {"x": 0, "y": 0, "z": 0}}
    grazing = {"id": "grazing", "camera_position": {"x": 8.8, "y": 4.0, "z": .8}, "camera_look_at": {"x": 0, "y": 0, "z": 0}}
    images, rows = {}, []
    for name, summary in variants.items():
        pixels, png = render_variant(summary, name, hero, contract, binding, review, args.render_cli)
        images[(name, "hero")] = pixels
        rows.append((name.upper() + " NOMINAL COVERAGE", pixels, png))
    high_grazing, high_grazing_png = render_variant(variants["high"], "high", grazing, contract, binding, review, args.render_cli)
    images[("high", "grazing")] = high_grazing
    repeated, repeated_png = render_variant(high_repeat, "high_repeat", hero, contract, binding, review, args.render_cli)
    repeat_difference = difference_image(images[("high", "hero")], repeated)
    changed = changed_pixels(images[("high", "hero")], repeated)
    difference_png = review / "high_repeat_difference.png"
    review_artifacts.write_png_rgb(difference_png, 640, 480, repeat_difference)
    contact = review / "formed_concrete_preset_matrix.png"
    write_labeled_contact_sheet(contact, [
        ("LOW: 0.1% NOMINAL", images[("low", "hero")]),
        ("MEDIUM: 1% NOMINAL", images[("medium", "hero")]),
        ("HIGH: 5% NOMINAL", images[("high", "hero")]),
        ("HIGH GRAZING", high_grazing),
        ("HIGH EXACT REPEAT", repeated),
        ("REPEAT DIFFERENCE (BLACK = ZERO)", repeat_difference),
    ], columns=2)
    receipts = {name: load(generated / name / "field/receipts/formed_concrete_preset.receipt.json") for name in variants}
    summary = {"schema": "ray_tracing.formed_concrete_preset_visual_matrix_v1", "passed": changed == 0,
               "source_mesh_digest_sha256": source_digest,
               "realized_eligible_coverage": {name: receipt["coverage"]["realized_eligible_fraction"] for name, receipt in receipts.items()},
               "signed_relief": {name: variant["signed_feature_relief"] for name, variant in variants.items()},
               "matrix": str(contact.relative_to(root)), "repeat_changed_pixels": changed,
               "matrix_sha256": hashlib.sha256(contact.read_bytes()).hexdigest(),
               "target_edge_length_units": args.target_edge}
    write_json(root / "summary.json", summary)
    if changed: raise RuntimeError(f"high preset repeat changed {changed} pixels")
    print(root / "summary.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
