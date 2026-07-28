#!/usr/bin/env python3
"""Build and render a deterministic multi-view procedural-surface proof pack."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import platform
import shutil
import statistics
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_DIR = ROOT / "tests" / "integration"
if str(INTEGRATION_DIR) not in sys.path:
    sys.path.insert(0, str(INTEGRATION_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
from procedural_surface_material_preview import render_material_artifact  # noqa: E402


FONT_5X7 = {
    " ": ("00000",) * 7,
    ".": ("00000", "00000", "00000", "00000", "00000", "00110", "00110"),
    "+": ("00000", "00100", "00100", "11111", "00100", "00100", "00000"),
    "-": ("00000", "00000", "00000", "11111", "00000", "00000", "00000"),
    "0": ("01110", "10001", "10011", "10101", "11001", "10001", "01110"),
    "1": ("00100", "01100", "00100", "00100", "00100", "00100", "01110"),
    "2": ("01110", "10001", "00001", "00010", "00100", "01000", "11111"),
    "3": ("11110", "00001", "00001", "01110", "00001", "00001", "11110"),
    "4": ("00010", "00110", "01010", "10010", "11111", "00010", "00010"),
    "5": ("11111", "10000", "10000", "11110", "00001", "00001", "11110"),
    "6": ("01110", "10000", "10000", "11110", "10001", "10001", "01110"),
    "7": ("11111", "00001", "00010", "00100", "01000", "01000", "01000"),
    "8": ("01110", "10001", "10001", "01110", "10001", "10001", "01110"),
    "9": ("01110", "10001", "10001", "01111", "00001", "00001", "01110"),
    "A": ("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
    "B": ("11110", "10001", "10001", "11110", "10001", "10001", "11110"),
    "C": ("01111", "10000", "10000", "10000", "10000", "10000", "01111"),
    "D": ("11110", "10001", "10001", "10001", "10001", "10001", "11110"),
    "E": ("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
    "F": ("11111", "10000", "10000", "11110", "10000", "10000", "10000"),
    "G": ("01111", "10000", "10000", "10111", "10001", "10001", "01111"),
    "H": ("10001", "10001", "10001", "11111", "10001", "10001", "10001"),
    "I": ("11111", "00100", "00100", "00100", "00100", "00100", "11111"),
    "K": ("10001", "10010", "10100", "11000", "10100", "10010", "10001"),
    "L": ("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
    "M": ("10001", "11011", "10101", "10101", "10001", "10001", "10001"),
    "N": ("10001", "11001", "10101", "10011", "10001", "10001", "10001"),
    "O": ("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
    "P": ("11110", "10001", "10001", "11110", "10000", "10000", "10000"),
    "Q": ("01110", "10001", "10001", "10001", "10101", "10010", "01101"),
    "R": ("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
    "S": ("01111", "10000", "10000", "01110", "00001", "00001", "11110"),
    "T": ("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
    "U": ("10001", "10001", "10001", "10001", "10001", "10001", "01110"),
    "V": ("10001", "10001", "10001", "10001", "10001", "01010", "00100"),
    "W": ("10001", "10001", "10001", "10101", "10101", "11011", "10001"),
    "X": ("10001", "10001", "01010", "00100", "01010", "10001", "10001"),
    "Y": ("10001", "10001", "01010", "00100", "00100", "00100", "00100"),
    "Z": ("11111", "00001", "00010", "00100", "01000", "10000", "11111"),
}


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")


def default_build_tool(name: str) -> Path:
    machine = platform.machine()
    candidate = (
        ROOT / "build" / "toolchains" / "clang" / machine /
        "tools" / "cli" / name
    )
    if candidate.exists():
        return candidate
    return ROOT / "build" / machine / "tools" / "cli" / name


def parse_args() -> argparse.Namespace:
    fixture = ROOT / "tests" / "fixtures" / "procedural_surface_rock_prism_psg0"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, default=fixture / "recipe.json")
    parser.add_argument(
        "--contract", type=Path, default=fixture / "visual_proof_contract.json"
    )
    parser.add_argument(
        "--asset-tool",
        type=Path,
        default=default_build_tool("procedural_surface_preview_asset_tool"),
    )
    parser.add_argument(
        "--render-cli",
        type=Path,
        default=default_build_tool("ray_tracing_render_headless"),
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        default=(
            ROOT / "build" / "agent_runs" / "ray_tracing" /
            "procedural_surface_visual_proof" / "psg3v"
        ),
    )
    return parser.parse_args()


def run_checked(command: list[str]) -> None:
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )

def run_render_cli(cli: Path, request_path: Path, summary_path: Path) -> None:
    run_checked([
        str(cli), "--request", str(request_path), "--render",
        "--summary", str(summary_path),
    ])


def run_expect_rejected(command: list[str], stderr_path: Path) -> str:
    result = subprocess.run(command, text=True, capture_output=True)
    stderr_path.parent.mkdir(parents=True, exist_ok=True)
    stderr_path.write_text(result.stderr or "", encoding="utf-8")
    if result.returncode == 0:
        raise RuntimeError(f"expected rejection: {' '.join(command)}")
    return result.stderr or ""


def export_asset(
    tool: Path,
    recipe: Path,
    asset_path: Path,
    summary_path: Path,
    cage: dict,
    asset_id: str,
    source_asset_id: str,
    material_path: Path,
    manifest_path: Path,
) -> dict:
    asset_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    run_checked([
        str(tool),
        "--recipe", str(recipe),
        "--asset-out", str(asset_path),
        "--summary-out", str(summary_path),
        "--material-out", str(material_path),
        "--manifest-out", str(manifest_path),
        "--width", str(cage["width_units"]),
        "--height", str(cage["height_units"]),
        "--depth", str(cage["depth_units"]),
        "--asset-id", asset_id,
        "--source-asset-id", source_asset_id,
    ])
    return load_json(summary_path)


def runtime_scene(
    scene_id: str,
    object_id: str,
    asset_id: str,
    runtime_mesh_path: Path,
    procedural_manifest_path: Path,
    scene_path: Path,
    asset_summary: dict,
    light_rig: list[dict],
) -> dict:
    relative_mesh_path = Path(
        os.path.relpath(runtime_mesh_path, scene_path.parent)
    ).as_posix()
    relative_manifest_path = Path(
        os.path.relpath(procedural_manifest_path, scene_path.parent)
    ).as_posix()
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": scene_id,
        "source_scene_id": scene_id,
        "compile_meta": {
            "compiler_version": "procedural_surface_visual_proof_v1",
            "compiled_at_ns": 0,
            "normalization": "psg3v_diagnostic_fixture",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [{
            "object_id": object_id,
            "object_type": "mesh_asset_instance",
            "space_mode_intent": "3d",
            "dimensional_mode": "full_3d",
            "transform": {
                "position": {"x": 0.0, "y": 0.0, "z": 0.0},
                "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
            },
            "geometry_ref": {
                "kind": "mesh_asset",
                "id": asset_id,
                "variant": "runtime_default",
            },
            "procedural_surface_ref": {
                "manifest_path": relative_manifest_path,
                "source_authority": "semantic_cage",
                "derived_asset_policy": "replaceable_cache",
            },
            "material_ref": {"id": "mat_neutral_surface"},
            "flags": {"visible": True, "locked": False, "selectable": True},
            "extensions": {
                "line_drawing": {
                    "geometry_source": "mesh_asset_instance",
                    "source_lane": "procedural_surface_visual_proof",
                    "mesh_asset_id": asset_id,
                    "runtime_mesh_path": relative_mesh_path,
                    "runtime_vertex_count": asset_summary["vertex_count"],
                    "runtime_triangle_count": asset_summary["triangle_count"],
                    "local_bounds": {
                        "min": asset_summary["bounds_min"],
                        "max": asset_summary["bounds_max"],
                    },
                }
            },
        }],
        "hierarchy": [],
        "materials": [{
            "material_id": "mat_neutral_surface",
            "kind": "lambert",
            "albedo": [0.72, 0.65, 0.52],
        }],
        "lights": [
            {
                "light_id": light["id"],
                "kind": "point",
                "position": light["position"],
                "intensity": light["intensity"],
                "radius": 0.0,
            }
            for light in light_rig
        ],
        "cameras": [{
            "camera_id": "surface_review_camera",
            "kind": "perspective",
            "position": {"x": 6.4, "y": -7.2, "z": 5.0},
            "target": {"x": 0.0, "y": 0.0, "z": 0.0},
        }],
        "constraints": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "environment": {
                        "light_mode": 2,
                        "ambient_strength": 0.28,
                        "top_fill_strength": 1.35,
                    },
                    "object_materials": [{
                        "object_id": object_id,
                        "material_id": 0,
                        "object_color": 0xB8AA8C,
                        "roughness": 0.78,
                        "reflectivity": 0.03,
                    }],
                }
            }
        },
    }


def render_request(
    proof_id: str,
    view: dict,
    scene_path: Path,
    request_path: Path,
    output_root: Path,
    contract: dict,
) -> dict:
    render = contract["render"]
    lighting = contract["lighting"]
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": f"{proof_id}_{view['id']}",
        "scene": {
            "runtime_scene_path": str(
                Path(os.path.relpath(scene_path, request_path.parent))
            )
        },
        "volume": {"enabled": False},
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": render["width"],
            "height": render["height"],
            "normalized_t": 0.0,
            "temporal_frames": render["temporal_frames"],
            "integrator_3d": render["integrator_3d"],
            "denoise_enabled": False,
        },
        "inspection": {
            "camera_position": view["camera_position"],
            "camera_look_at": view["camera_look_at"],
            "camera_zoom": render["camera_zoom"],
            **lighting,
            "object_audit_enabled": True,
        },
        "output": {"root": str(output_root), "overwrite": True},
        "progress": {
            "summary_path": str(output_root / "render_summary.json"),
            "progress_path": str(output_root / "render_progress.json"),
        },
    }


def draw_label(
    canvas: list[list[tuple[int, int, int]]],
    text: str,
    origin_x: int,
    origin_y: int,
    scale: int = 2,
) -> None:
    x = origin_x
    for character in text.upper():
        glyph = FONT_5X7.get(character, FONT_5X7[" "])
        for gy, row in enumerate(glyph):
            for gx, bit in enumerate(row):
                if bit != "1":
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        canvas[origin_y + gy * scale + sy][
                            x + gx * scale + sx
                        ] = (238, 238, 232)
        x += 6 * scale


def write_labeled_contact_sheet(
    output_path: Path,
    cells: list[tuple[str, list[list[tuple[int, int, int]]]]],
    columns: int = 4,
) -> None:
    image_width = len(cells[0][1][0])
    image_height = len(cells[0][1])
    label_height = 24
    separator = 8
    rows = math.ceil(len(cells) / columns)
    width = columns * image_width + (columns - 1) * separator
    height = rows * (image_height + label_height) + (rows - 1) * separator
    canvas = [[(25, 27, 30) for _ in range(width)] for _ in range(height)]
    for index, (label, pixels) in enumerate(cells):
        column = index % columns
        row = index // columns
        x0 = column * (image_width + separator)
        y0 = row * (image_height + label_height + separator)
        label_pixel_width = len(label) * 12 - 2
        draw_label(
            canvas, label, x0 + max(4, (image_width - label_pixel_width) // 2),
            y0 + 5,
        )
        for y, pixel_row in enumerate(pixels):
            canvas[y0 + label_height + y][x0:x0 + image_width] = pixel_row
    output_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(output_path, width, height, canvas)


def image_metrics(pixels: list[list[tuple[int, int, int]]]) -> dict:
    luma = [
        0.2126 * red + 0.7152 * green + 0.0722 * blue
        for row in pixels
        for red, green, blue in row
    ]
    return {
        "luma_mean": statistics.fmean(luma),
        "luma_standard_deviation": statistics.pstdev(luma),
        "luma_min": min(luma),
        "luma_max": max(luma),
    }


def object_audit(summary: dict, object_id: str) -> dict:
    matches = [
        entry for entry in summary.get("object_audit", [])
        if entry.get("object_id") == object_id
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"expected one object_audit entry for {object_id}; got {len(matches)}"
        )
    return matches[0]


def build_index(
    output_path: Path,
    contract: dict,
    final_summary: dict,
) -> None:
    lines = [
        f"# {contract['title']}",
        "",
        contract["visual_intent"],
        "",
        f"![Labeled procedural-surface contact sheet]({final_summary['contact_sheet']})",
        "",
        "## Review contract",
        "",
        f"- Expected signal: {contract['expected_signal']}",
        f"- Control: {contract['control_policy']}",
        f"- Reject when: {contract['rejection_condition']}",
        "",
        "## Views",
        "",
    ]
    for view in final_summary["views"]:
        lines.append(
            f"- `{view['id']}` ({view['direction']}): "
            f"`{view['primary_hit_pixels']}` object-hit pixels, "
            f"luma deviation `{view['image_metrics']['luma_standard_deviation']:.3f}`, "
            f"PNG `{view['png_path']}`"
        )
    lines.extend([
        "",
        "## Deterministic subject",
        "",
        f"- mesh digest: `{final_summary['procedural_asset']['mesh_digest_sha256']}`",
        f"- vertices: `{final_summary['procedural_asset']['vertex_count']}`",
        f"- triangles: `{final_summary['procedural_asset']['triangle_count']}`",
        f"- boundary edges: `{final_summary['procedural_asset']['boundary_edge_count']}`",
        f"- Euler characteristic: `{final_summary['procedural_asset']['euler_characteristic']}`",
        f"- control/subject changed pixels: `{final_summary['control_comparison']['changed_pixels']}`",
        "",
        "## PSG-4/PSG-5 coupled material and runtime identity",
        "",
        f"- recipe digest: `{final_summary['material_identity']['recipe_digest_sha256']}`",
        f"- shell digest: `{final_summary['material_identity']['shell_digest_sha256']}`",
        f"- material digest: `{final_summary['material_identity']['material_digest_sha256']}`",
        "- cells: geometry-only native render, material-only retained-field preview, "
        "coupled result, snow likelihood, and roughness",
        f"- cache identity: `{final_summary['derived_asset_identity']['procedural']['cache_identity_sha256']}`",
        "- native headless views consume the persisted material through per-hit "
        "interpolation; material-only/snow/roughness cells remain diagnostic views",
        f"- save/reopen frame equal: `{final_summary['reload_and_invalidation']['reopen_frame_equal']}`",
        f"- changed recipe rejected: `{final_summary['reload_and_invalidation']['recipe_change_rejected']}`",
        "",
    ])
    output_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    recipe_path = args.recipe.resolve()
    contract_path = args.contract.resolve()
    asset_tool = args.asset_tool.resolve()
    render_cli = args.render_cli.resolve()
    output_root = args.output_root.resolve()
    if not asset_tool.exists():
        raise RuntimeError(f"missing preview asset tool: {asset_tool}")
    if not render_cli.exists():
        raise RuntimeError(f"missing headless renderer: {render_cli}")

    contract = load_json(contract_path)
    source_recipe = load_json(recipe_path)
    generated = output_root / "generated"
    assets = generated / "assets"
    scenes = generated / "scenes"
    requests = generated / "requests"
    raw_runs = output_root / "raw_runs"
    review_root = output_root / "review"
    for directory in (assets, scenes, requests, raw_runs, review_root):
        directory.mkdir(parents=True, exist_ok=True)

    procedural_recipe_path = generated / "procedural_recipe.json"
    control_recipe_path = generated / "zero_displacement_control_recipe.json"
    write_json(procedural_recipe_path, source_recipe)
    control_recipe = json.loads(json.dumps(source_recipe))
    control_recipe["geometry"]["displacement_amplitude_units"] = 0.0
    write_json(control_recipe_path, control_recipe)

    asset_specs = {
        "procedural": {
            "recipe": procedural_recipe_path,
            "asset_id": "procedural_rock_prism_psg3v",
            "object_id": "procedural_rock_prism",
        },
        "control": {
            "recipe": control_recipe_path,
            "asset_id": "procedural_rock_prism_control_psg3v",
            "object_id": "procedural_rock_prism_control",
        },
    }
    asset_summaries = {}
    material_paths = {}
    derived_manifest_paths = {}
    scene_paths = {}
    for role, spec in asset_specs.items():
        asset_path = assets / f"{role}.runtime.json"
        summary_path = assets / f"{role}_asset_summary.json"
        material_path = assets / f"{role}_material_artifact.json"
        derived_manifest_path = assets / f"{role}_derived_asset.json"
        asset_summary = export_asset(
            asset_tool, spec["recipe"], asset_path, summary_path,
            contract["cage"], spec["asset_id"], f"{spec['asset_id']}_cage",
            material_path, derived_manifest_path,
        )
        scene_path = scenes / f"scene_{role}.json"
        write_json(
            scene_path,
            runtime_scene(
                f"{contract['proof_id']}_{role}",
                spec["object_id"],
                spec["asset_id"],
                asset_path,
                derived_manifest_path,
                scene_path,
                asset_summary,
                contract["light_rig"],
            ),
        )
        asset_summaries[role] = asset_summary
        material_paths[role] = material_path
        derived_manifest_paths[role] = derived_manifest_path
        scene_paths[role] = scene_path

    request_names = []
    for view in contract["views"]:
        request_path = requests / f"request_{view['id']}.json"
        output_path = raw_runs / view["id"]
        write_json(
            request_path,
            render_request(
                contract["proof_id"], view, scene_paths[view["asset_role"]],
                request_path, output_path, contract,
            ),
        )
        request_names.append(request_path.name)

    manifest_path = requests / "matrix_manifest.json"
    write_json(manifest_path, {
        "schema_version": "ray_tracing_visual_matrix_manifest_v1",
        "matrix_id": contract["proof_id"],
        "title": contract["title"],
        "default_review_root": str(review_root),
        "groups": [{"id": "canonical_views", "requests": request_names}],
        "comparisons": [{
            "id": "displacement_control",
            "group": "canonical_views",
            "before": "request_control_hero.json",
            "after": "request_procedural_hero.json",
            "purpose": contract["control_policy"],
        }],
    })
    run_checked([
        sys.executable,
        str(INTEGRATION_DIR / "run_ray_tracing_visual_matrix.py"),
        "--manifest", str(manifest_path),
        "--cli", str(render_cli),
        "--review-root", str(review_root),
        "--contact-columns", "4",
    ])

    matrix_report = load_json(review_root / "matrix_report.json")
    assertion_contract = contract["assertions"]
    expected_digest = load_json(
        recipe_path.parent / "expected_prism_mesh_summary.json"
    )["mesh_digest_sha256"]
    failures = []
    if not matrix_report["passed"]:
        failures.append("generic visual-matrix checks failed")
    if asset_summaries["procedural"]["mesh_digest_sha256"] != expected_digest:
        failures.append("procedural asset digest differs from frozen PSG-3 shell")
    if asset_summaries["control"]["maximum_absolute_displacement_units"] != 0.0:
        failures.append("control asset has nonzero displacement")

    matrix_runs = {run["cell_id"]: run for run in matrix_report["runs"]}
    contact_cells = []
    view_results = []
    for view in contract["views"]:
        run = matrix_runs[view["id"]]
        summary = load_json(Path(run["summary_path"]))
        role = view["asset_role"]
        derived_identity = load_json(derived_manifest_paths[role])
        runtime_readback = summary.get("procedural_surface_runtime", {})
        audit = object_audit(summary, asset_specs[role]["object_id"])
        frame_path = Path(run["frame_path"])
        _, _, pixels = review_artifacts.read_bmp_rgb(frame_path)
        metrics = image_metrics(pixels)
        if audit["triangle_count"] != assertion_contract["expected_triangle_count"]:
            failures.append(f"{view['id']}: unexpected triangle count")
        if audit["primary_hit_pixels"] < assertion_contract["minimum_primary_hit_pixels"]:
            failures.append(f"{view['id']}: too few object-hit pixels")
        if abs(
            audit["primary_hit_pixels"] - view["expected_primary_hit_pixels"]
        ) > assertion_contract["primary_hit_pixel_tolerance"]:
            failures.append(f"{view['id']}: object-hit silhouette drift")
        if metrics["luma_standard_deviation"] < assertion_contract[
            "minimum_image_luma_standard_deviation"
        ]:
            failures.append(f"{view['id']}: image lacks tonal variation")
        if summary.get("bvh_summary", {}).get("trace_overflows", 0) != 0:
            failures.append(f"{view['id']}: BVH trace overflow")
        if not runtime_readback.get("loaded"):
            failures.append(f"{view['id']}: procedural runtime binding missing")
        if runtime_readback.get("cache_identity_sha256") != (
            derived_identity["cache_identity_sha256"]
        ):
            failures.append(f"{view['id']}: procedural cache identity drift")
        if runtime_readback.get("collision_owner") != "semantic_cage":
            failures.append(f"{view['id']}: collision owner drift")
        contact_cells.append((view["label"], pixels))
        view_results.append({
            "id": view["id"],
            "label": view["label"],
            "asset_role": role,
            "direction": view["direction"],
            "camera_position": view["camera_position"],
            "camera_look_at": view["camera_look_at"],
            "primary_hit_pixels": audit["primary_hit_pixels"],
            "expected_primary_hit_pixels": view["expected_primary_hit_pixels"],
            "triangle_count": audit["triangle_count"],
            "image_metrics": metrics,
            "png_path": str(Path(run["png_path"]).relative_to(output_root)),
            "render_summary_path": str(
                Path(run["summary_copy"]).relative_to(output_root)
            ),
            "png_sha256": hashlib.sha256(
                Path(run["png_path"]).read_bytes()
            ).hexdigest(),
        })

    comparison = matrix_report["comparisons"][0]
    if comparison["changed_pixels"] < assertion_contract[
        "minimum_control_changed_pixels"
    ]:
        failures.append("displaced hero is too similar to zero-displacement control")

    hero_run = matrix_runs["procedural_hero"]
    hero_request = load_json(requests / "request_procedural_hero.json")
    reopen_root = raw_runs / "procedural_hero_reopen"
    reopen_request_path = requests / "request_procedural_hero_reopen.json"
    reopen_summary_path = reopen_root / "render_summary.json"
    hero_request["run_id"] = f"{contract['proof_id']}_procedural_hero_reopen"
    hero_request["output"]["root"] = str(reopen_root)
    hero_request["progress"]["summary_path"] = str(reopen_summary_path)
    hero_request["progress"]["progress_path"] = str(
        reopen_root / "render_progress.json"
    )
    write_json(reopen_request_path, hero_request)
    run_render_cli(render_cli, reopen_request_path, reopen_summary_path)
    reopen_summary = load_json(reopen_summary_path)
    original_frame = Path(hero_run["frame_path"])
    reopen_frame = reopen_root / "frames" / "frame_0000.bmp"
    reopen_frame_equal = (
        hashlib.sha256(original_frame.read_bytes()).hexdigest() ==
        hashlib.sha256(reopen_frame.read_bytes()).hexdigest()
    )
    reopen_identity_equal = (
        reopen_summary.get("procedural_surface_runtime", {}).get(
            "cache_identity_sha256"
        ) ==
        load_json(derived_manifest_paths["procedural"])[
            "cache_identity_sha256"
        ]
    )
    if not reopen_frame_equal:
        failures.append("save/reopen headless frame identity drift")
    if not reopen_identity_equal:
        failures.append("save/reopen procedural cache identity drift")

    changed_recipe = json.loads(json.dumps(source_recipe))
    changed_recipe["seed"] = int(changed_recipe["seed"]) + 1
    changed_recipe_path = generated / "recipe_change_invalidation.json"
    write_json(changed_recipe_path, changed_recipe)
    changed_manifest = load_json(derived_manifest_paths["procedural"])
    changed_manifest["recipe_reference"]["path"] = str(changed_recipe_path)
    changed_manifest_path = assets / "procedural_recipe_changed_stale.json"
    write_json(changed_manifest_path, changed_manifest)
    changed_scene = load_json(scene_paths["procedural"])
    changed_scene["objects"][0]["procedural_surface_ref"]["manifest_path"] = (
        Path(os.path.relpath(changed_manifest_path, scenes)).as_posix()
    )
    changed_scene_path = scenes / "scene_procedural_recipe_changed_stale.json"
    write_json(changed_scene_path, changed_scene)
    changed_request = json.loads(json.dumps(hero_request))
    changed_request["scene"]["runtime_scene_path"] = str(
        Path(os.path.relpath(changed_scene_path, requests))
    )
    changed_request_path = requests / "request_recipe_change_rejection.json"
    write_json(changed_request_path, changed_request)
    recipe_change_stderr = run_expect_rejected(
        [str(render_cli), "--request", str(changed_request_path), "--preflight"],
        raw_runs / "recipe_change_rejection" / "stderr.txt",
    )
    recipe_change_rejected = "recipe reference is stale" in recipe_change_stderr
    if not recipe_change_rejected:
        failures.append("recipe-change stale-result rejection missing")

    hero_view = contract["views"][0]
    material_preview_specs = [
        ("MATERIAL ONLY", "control", "combined", hero_view),
        ("COUPLED RESULT", "procedural", "combined", hero_view),
        ("SNOW MASK", "procedural", "snow", hero_view),
        ("ROUGHNESS", "procedural", "roughness", hero_view),
    ]
    material_preview_results = []
    material_preview_pixels = {}
    expected_material = load_json(
        recipe_path.parent / "expected_material_summary.json"
    )
    for label, role, mode, view in material_preview_specs:
        preview_id = label.lower().replace(" ", "_")
        preview_path = review_root / f"procedural_surface_{preview_id}.png"
        result = render_material_artifact(
            material_paths[role],
            preview_path,
            [
                view["camera_position"]["x"],
                view["camera_position"]["y"],
                view["camera_position"]["z"],
            ],
            [
                view["camera_look_at"]["x"],
                view["camera_look_at"]["y"],
                view["camera_look_at"]["z"],
            ],
            contract["render"]["width"],
            contract["render"]["height"],
            mode,
        )
        if result["covered_pixels"] < assertion_contract["minimum_primary_hit_pixels"]:
            failures.append(f"{preview_id}: too few material-preview pixels")
        expected_shell = (
            expected_digest if role == "procedural"
            else asset_summaries["control"]["mesh_digest_sha256"]
        )
        if result["shell_digest_sha256"] != expected_shell:
            failures.append(f"{preview_id}: material/shell identity drift")
        if (
            role == "procedural" and
            result["material_digest_sha256"] !=
            expected_material["material_digest_sha256"]
        ):
            failures.append(f"{preview_id}: material digest drift")
        material_preview_pixels[preview_id] = result["pixels"]
        contact_cells.append((label, result["pixels"]))
        material_preview_results.append({
            "id": preview_id,
            "label": label,
            "asset_role": role,
            "mode": mode,
            "covered_pixels": result["covered_pixels"],
            "recipe_digest_sha256": result["recipe_digest_sha256"],
            "shell_digest_sha256": result["shell_digest_sha256"],
            "material_digest_sha256": result["material_digest_sha256"],
            "png_path": str(preview_path.relative_to(output_root)),
            "png_sha256": hashlib.sha256(preview_path.read_bytes()).hexdigest(),
        })
    geometry_pixels = next(
        pixels for label, pixels in contact_cells
        if label == "PROCEDURAL HERO"
    )
    combined_pixels = material_preview_pixels["coupled_result"]
    material_only_pixels = material_preview_pixels["material_only"]
    material_changed_pixels = sum(
        geometry_pixels[y][x] != combined_pixels[y][x]
        for y in range(len(geometry_pixels))
        for x in range(len(geometry_pixels[0]))
    )
    combined_geometry_changed_pixels = sum(
        material_only_pixels[y][x] != combined_pixels[y][x]
        for y in range(len(material_only_pixels))
        for x in range(len(material_only_pixels[0]))
    )
    if material_changed_pixels < assertion_contract["minimum_control_changed_pixels"]:
        failures.append("combined material is too similar to geometry-only hero")
    if combined_geometry_changed_pixels < assertion_contract[
        "minimum_control_changed_pixels"
    ]:
        failures.append("combined result is too similar to material-only control")
    contact_path = review_root / "procedural_surface_contact_sheet_labeled.png"
    write_labeled_contact_sheet(contact_path, contact_cells)
    final_summary = {
        "schema_version": "procedural_surface_visual_proof_summary_v1",
        "proof_id": contract["proof_id"],
        "passed": not failures,
        "failures": failures,
        "visual_intent": contract["visual_intent"],
        "expected_signal": contract["expected_signal"],
        "rejection_condition": contract["rejection_condition"],
        "control_policy": contract["control_policy"],
        "contract_path": str(contract_path),
        "recipe_path": str(recipe_path),
        "contact_sheet": str(contact_path.relative_to(output_root)),
        "matrix_report": str(
            (review_root / "matrix_report.json").relative_to(output_root)
        ),
        "procedural_asset": asset_summaries["procedural"],
        "control_asset": asset_summaries["control"],
        "control_comparison": {
            "changed_pixels": comparison["changed_pixels"],
            "pixels": comparison["pixels"],
            "max_abs_channel_delta": comparison["max_abs_channel_delta"],
            "mean_abs_all_channels": comparison["mean_abs_all_channels"],
            "side_by_side_png": str(
                Path(comparison["side_by_side_png"]).relative_to(output_root)
            ),
        },
        "material_identity": {
            "recipe_digest_sha256":
                expected_material["recipe_digest_sha256"],
            "shell_digest_sha256":
                expected_material["shell_digest_sha256"],
            "material_digest_sha256":
                expected_material["material_digest_sha256"],
        },
        "derived_asset_identity": {
            role: load_json(path)
            for role, path in derived_manifest_paths.items()
        },
        "reload_and_invalidation": {
            "reopen_frame_equal": reopen_frame_equal,
            "reopen_cache_identity_equal": reopen_identity_equal,
            "recipe_change_rejected": recipe_change_rejected,
            "recipe_change_rejection_stderr": str(
                (raw_runs / "recipe_change_rejection" / "stderr.txt")
                .relative_to(output_root)
            ),
        },
        "coupled_comparisons": {
            "geometry_only_to_combined_changed_pixels":
                material_changed_pixels,
            "material_only_to_combined_changed_pixels":
                combined_geometry_changed_pixels,
        },
        "material_previews": material_preview_results,
        "views": view_results,
    }
    write_json(output_root / "procedural_surface_visual_proof_summary.json",
               final_summary)
    build_index(output_root / "index.md", contract, final_summary)
    print(output_root / "procedural_surface_visual_proof_summary.json")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
