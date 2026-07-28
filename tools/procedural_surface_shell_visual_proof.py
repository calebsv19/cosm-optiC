#!/usr/bin/env python3
"""Compile and natively render one arbitrary-shell PSG-8 proof."""

from __future__ import annotations

import argparse
import hashlib
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


SOURCE = (
    ROOT / "tests" / "fixtures" /
    "mesh_asset_runtime_imported_tetrahedron" / "assets" / "mesh_assets" /
    "asset_imported_tetrahedron_01.runtime.json"
)
GRAPH = (
    ROOT / "tests" / "fixtures" / "procedural_surface_field_presets" /
    "wind_shaped_sand.json"
)
BINDING = (
    ROOT / "tests" / "fixtures" / "procedural_surface_field_presets" /
    "wind_shaped_sand.top.subtle.binding.json"
)
GRAPH_DIGEST = (
    "9f4b7cc576c4b5e172891bb10fb250bb4796ee4556c6201d4e209ae2833b9689"
)
BINDING_DIGEST = (
    "77402f5160737a30a34d4a4f243c72a3cba6206c8330bd872988ca6ef067f3b4"
)


def default_tool(name: str) -> Path:
    return (
        ROOT / "build" / "toolchains" / "clang" / platform.machine() /
        "tools" / "cli" / name
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--shell-tool", type=Path,
        default=default_tool("procedural_surface_shell_tool"),
    )
    parser.add_argument(
        "--render-cli", type=Path,
        default=default_tool("ray_tracing_render_headless"),
    )
    parser.add_argument(
        "--output-root", type=Path,
        default=(
            ROOT / "build" / "agent_runs" / "ray_tracing" /
            "procedural_surface_shell" / "psg8"
        ),
    )
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def scene(scene_id: str, asset_id: str) -> dict:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": scene_id,
        "source_scene_id": scene_id,
        "compile_meta": {
            "compiler_version": "procedural_surface_shell_visual_proof_v1",
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
            "material_ref": {"id": "mat_sand"},
            "flags": {"visible": True, "locked": False, "selectable": True},
        }],
        "materials": [{
            "id": "mat_sand",
            "name": "PSG-8 sand shell",
            "base_color": {"r": 0.72, "g": 0.57, "b": 0.34},
            "roughness": 0.78,
            "metallic": 0.0,
        }],
        "lights": [],
        "extensions": {},
    }


def main() -> int:
    args = parse_args()
    output_root = args.output_root.resolve()
    shell_tool = args.shell_tool.resolve()
    render_cli = args.render_cli.resolve()
    generated = output_root / "generated"
    assets = generated / "assets" / "mesh_assets"
    review = output_root / "review"
    raw_runs = output_root / "raw_runs"
    for directory in (assets, review, raw_runs):
        directory.mkdir(parents=True, exist_ok=True)

    control_id = "asset_imported_tetrahedron_01"
    derived_id = "psg8_tetra_sand"
    control_asset = assets / f"{control_id}.runtime.json"
    derived_asset = assets / f"{derived_id}.runtime.json"
    receipt_path = generated / "derived_shell_receipt.json"
    shutil.copy2(SOURCE, control_asset)
    command = [
        str(shell_tool),
        "--source", str(SOURCE),
        "--graph", str(GRAPH),
        "--binding", str(BINDING),
        "--out", str(derived_asset),
        "--summary-out", str(receipt_path),
        "--asset-id", derived_id,
        "--expected-source-digest", sha256(SOURCE),
        "--expected-graph-digest", GRAPH_DIGEST,
        "--expected-binding-digest", BINDING_DIGEST,
        "--target-edge", "0.04",
        "--max-levels", "4",
    ]
    compiled = subprocess.run(command, text=True, capture_output=True)
    if compiled.returncode != 0:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    receipt = load_json(receipt_path)
    failures: list[str] = []
    if (
        receipt["boundary_edge_count"] != 0 or
        receipt["nonmanifold_edge_count"] != 0 or
        receipt["connected_component_count"] != 1 or
        receipt["euler_characteristic"] != 2 or
        receipt["signed_volume_units3"] <= 0.0
    ):
        failures.append("derived arbitrary shell failed topology invariants")

    views = [
        {
            "id": "hero",
            "label": "Hero",
            "camera_position": {"x": 2.0, "y": -2.5, "z": 1.7},
            "camera_look_at": {"x": 0.35, "y": 0.35, "z": 0.35},
        },
        {
            "id": "side",
            "label": "Side",
            "camera_position": {"x": 3.0, "y": 0.35, "z": 0.35},
            "camera_look_at": {"x": 0.35, "y": 0.35, "z": 0.35},
        },
        {
            "id": "top",
            "label": "Top",
            "camera_position": {"x": 0.35, "y": 0.35, "z": 3.2},
            "camera_look_at": {"x": 0.35, "y": 0.35, "z": 0.35},
        },
    ]
    contract = {
        "render": {
            "width": 480,
            "height": 360,
            "temporal_frames": 1,
            "integrator_3d": "direct_light",
            "camera_zoom": 1.1,
        },
        "lighting": {
            "environment_light_mode": "ambient",
            "ambient_strength": 0.42,
            "top_fill_strength": 1.35,
            "light_intensity": 4.0,
            "light_radius": 0.12,
        },
    }
    cells = []
    pixels_by_cell: dict[tuple[str, str], list] = {}
    results = []
    for asset_id in (control_id, derived_id):
        scene_path = generated / f"{asset_id}.scene.json"
        write_json(scene_path, scene(asset_id, asset_id))
        for view in views:
            cell_id = f"{asset_id}_{view['id']}"
            run_root = raw_runs / cell_id
            request_path = generated / f"{cell_id}.request.json"
            summary_path = run_root / "render_summary.json"
            write_json(
                request_path,
                render_request(
                    "psg8_arbitrary_shell", view, scene_path, request_path,
                    run_root, contract,
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
            pixels_by_cell[(asset_id, view["id"])] = pixels
            cells.append((f"{asset_id} {view['label']}", pixels))
            results.append({
                "asset_id": asset_id,
                "view": view["id"],
                "triangle_count": audit["triangle_count"],
                "primary_hit_pixels": audit["primary_hit_pixels"],
                "luma_standard_deviation": metrics["luma_standard_deviation"],
                "image": str(png),
            })
            expected_triangles = (
                receipt["triangle_count"] if asset_id == derived_id else 4
            )
            if audit["triangle_count"] != expected_triangles:
                failures.append(f"{cell_id}: triangle count drift")
            if audit["primary_hit_pixels"] < 5000:
                failures.append(f"{cell_id}: insufficient visible coverage")

    changed = {}
    for view in views:
        left = pixels_by_cell[(control_id, view["id"])]
        right = pixels_by_cell[(derived_id, view["id"])]
        count = sum(
            a != b
            for left_row, right_row in zip(left, right)
            for a, b in zip(left_row, right_row)
        )
        changed[view["id"]] = count
        if count < 1500:
            failures.append(f"{view['id']}: derived shell lacks visible change")

    contact = review / "procedural_surface_arbitrary_shell.png"
    write_labeled_contact_sheet(contact, cells, columns=3)
    summary = {
        "schema": "ray_tracing.procedural_surface_shell_visual_proof",
        "schema_version": 1,
        "passed": not failures,
        "authority": "local_diagnostic_only",
        "receipt": receipt,
        "views": results,
        "changed_pixels": changed,
        "contact_sheet": str(contact),
        "failures": failures,
    }
    write_json(output_root / "proof_summary.json", summary)
    (output_root / "index.md").write_text(
        "\n".join([
            "# PSG-8 arbitrary shell proof",
            "",
            "Digest-guarded top-facing field application on an imported "
            "tetrahedron source mesh.",
            "",
            f"![PSG-8 arbitrary shell]({contact})",
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
