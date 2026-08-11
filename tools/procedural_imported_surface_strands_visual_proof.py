#!/usr/bin/env python3
"""Generate the PSG-23A rooted strand/fiber visual proof."""

from __future__ import annotations

import argparse
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
from procedural_imported_surface_inset_visual_proof import rasterize_depth  # noqa: E402
from procedural_surface_visual_proof import (  # noqa: E402
    image_metrics,
    object_audit,
    render_request,
    run_render_cli,
    write_json,
    write_labeled_contact_sheet,
)


def default_tool(name: str) -> Path:
    return (
        ROOT / "build" / "toolchains" / "clang" / platform.machine()
        / "tools" / "cli" / name
    )


def parse_args() -> argparse.Namespace:
    fixture = (
        ROOT / "tests/fixtures/procedural_imported_surface_strands_psg23a"
    )
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--strand-tool", type=Path,
        default=default_tool("procedural_imported_surface_strands_tool"))
    parser.add_argument(
        "--region-tool", type=Path,
        default=default_tool("procedural_imported_surface_region_tool"))
    parser.add_argument(
        "--render-cli", type=Path,
        default=default_tool("ray_tracing_render_headless"))
    parser.add_argument(
        "--stl-tool", type=Path,
        default=(
            ROOT.parent / "tools/procedural_object_authoring"
            / "procedural_stl_tool.py"))
    parser.add_argument(
        "--import-harness", type=Path,
        default=(
            ROOT.parent / "line_drawing/build/toolchains/clang/bin"
            / "imported_mesh_harness"))
    parser.add_argument(
        "--contract", type=Path, default=fixture / "visual_contract.json")
    parser.add_argument(
        "--recipe", type=Path, default=fixture / "scalp_bust.recipe.json")
    parser.add_argument(
        "--region-recipe", type=Path,
        default=fixture / "scalp_hair.region_recipe.json")
    parser.add_argument("--output-root", type=Path)
    return parser.parse_args()


def run(command: list[str]) -> str:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(
            f"command failed: {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}")
    return result.stdout


def receipt(command: list[str]) -> dict:
    return json.loads(run(command).splitlines()[0])


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def object_document(
    object_id: str, asset_id: str, material_id: str,
) -> dict:
    return {
        "object_id": object_id,
        "object_type": "mesh_asset_instance",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": {"x": 0.0, "y": 0.0, "z": 0.0},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "geometry_ref": {"kind": "mesh_asset", "id": asset_id},
        "material_ref": {"id": material_id},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def make_scene(scene_id: str, include_strands: bool) -> dict:
    objects = [
        object_document(
            f"{scene_id}_source", "psg23a_scalp_bust", "sculpted_clay")
    ]
    if include_strands:
        objects.append(object_document(
            f"{scene_id}_strands",
            "psg23a_scalp_rooted_strands", "fiber_green"))
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": scene_id,
        "source_scene_id": scene_id,
        "compile_meta": {
            "compiler_version": "psg23a_rooted_strand_visual_proof",
            "compiled_at_ns": 0,
            "normalization": "fresh_scalp_rooted_fiber_diagnostic",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": objects,
        "materials": [
            {
                "id": "sculpted_clay",
                "name": "Neutral sculpted carrier",
                "base_color": {"r": 0.50, "g": 0.43, "b": 0.35},
                "roughness": 0.93,
                "metallic": 0.0,
            },
            {
                "id": "fiber_green",
                "name": "Rooted moss and hair fibers",
                "base_color": {"r": 0.12, "g": 0.30, "b": 0.08},
                "roughness": 0.76,
                "metallic": 0.0,
            },
        ],
        "lights": [],
        "extensions": {},
    }


def views() -> dict[str, dict]:
    return {
        "hero": {
            "camera_position": {"x": 3.00, "y": 2.70, "z": 1.62},
            "camera_look_at": {"x": 0.06, "y": 0.08, "z": 1.10},
        },
        "grazing": {
            "camera_position": {"x": 3.35, "y": 0.70, "z": 1.48},
            "camera_look_at": {"x": 0.38, "y": 0.12, "z": 1.22},
        },
        "crown": {
            "camera_position": {"x": 2.24, "y": 2.02, "z": 2.82},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.46},
        },
    }


def render(
    render_cli: Path,
    contract: dict,
    generated: Path,
    review: Path,
    name: str,
    view: dict,
    include_strands: bool,
) -> tuple[list, dict, dict, Path]:
    scene = generated / f"{name}.scene.json"
    request = generated / "requests" / f"{name}.request.json"
    raw = generated / "raw" / name
    write_json(scene, make_scene(name, include_strands))
    write_json(request, render_request(
        contract["proof_id"], {**view, "id": name},
        scene, request, raw, {
            "render": contract["render"],
            "lighting": contract["lighting"],
        }))
    summary_path = raw / "render_summary.json"
    frame = raw / "frames/frame_0000.bmp"
    run_render_cli(render_cli, request, summary_path)
    summary = load(summary_path)
    audit = object_audit(
        summary,
        f"{name}_{'strands' if include_strands else 'source'}")
    width, height, pixels = review_artifacts.read_bmp_rgb(frame)
    png = review / f"{name}.png"
    review_artifacts.write_png_rgb(png, width, height, pixels)
    return pixels, audit, image_metrics(pixels), png


def changed_pixels(left: list, right: list) -> int:
    return sum(
        a != b
        for left_row, right_row in zip(left, right)
        for a, b in zip(left_row, right_row)
    )


def debug_views(
    tubes: dict, provenance: dict, width: int, height: int,
) -> tuple[list, list, list]:
    role_palette = {
        "root_cap": (238, 118, 54),
        "strand_shaft": (72, 180, 88),
        "tip_cap": (244, 218, 82),
    }
    roles = rasterize_depth(
        tubes, width, height,
        [role_palette[item["role"]] for item in provenance["triangles"]],
    )[0]
    strand_palette = [
        (76, 160, 214), (126, 202, 96), (226, 164, 70),
        (194, 96, 184), (91, 205, 190), (226, 104, 92),
        (144, 124, 218), (222, 205, 85),
    ]
    strand_ids = rasterize_depth(
        tubes, width, height, [
            strand_palette[
                item["strand_index"] % len(strand_palette)]
            for item in provenance["triangles"]
        ],
    )[0]
    source_map = rasterize_depth(
        tubes, width, height, [
            (
                70 + (item["source_triangle_index"] * 37) % 170,
                70 + (item["source_triangle_index"] * 61) % 170,
                70 + (item["source_triangle_index"] * 83) % 170,
            )
            for item in provenance["triangles"]
        ], wireframe=True,
    )[0]
    return roles, strand_ids, source_map


def main() -> int:
    args = parse_args()
    contract = load(args.contract.resolve())
    output = (args.output_root or (
        ROOT / "build/agent_runs/ray_tracing/procedural_solid"
        / "psg23a_imported_surface_strands_v1")).resolve()
    generated = output / "generated"
    review = output / "review"
    staging = Path("/private/tmp") / f"psg23a_visual_{os.getpid()}"
    for directory in (
        staging, generated / "fresh_stl",
        generated / "assets/mesh_assets", generated / "regions",
        generated / "receipts", generated / "requests",
        generated / "raw", review,
    ):
        directory.mkdir(parents=True, exist_ok=True)
    tools = (
        args.strand_tool, args.region_tool, args.render_cli,
        args.stl_tool, args.import_harness,
    )
    missing = [str(tool.resolve()) for tool in tools
               if not tool.resolve().exists()]
    if missing:
        raise RuntimeError(f"missing PSG-23A tools: {missing}")

    authored = staging / "authored"
    run([
        sys.executable, str(args.stl_tool.resolve()), "create",
        "--recipe", str(args.recipe.resolve()),
        "--out-root", str(authored),
    ])
    stl = (
        authored / "curated/psg23a_scalp_bust"
        / "source/psg23a_scalp_bust.stl"
    )
    durable_stl = generated / "fresh_stl" / f"run_{os.getpid()}.stl"
    durable_stl.write_bytes(stl.read_bytes())
    imported = staging / "imported"
    run([
        str(args.import_harness.resolve()),
        "--stl", str(stl),
        "--out", str(imported),
        "--asset-id", "psg23a_scalp_bust",
        "--scene-id", "psg23a_imported_surface_strands",
        "--object-id", "psg23a_scalp",
    ])
    source_mesh = (
        generated / "assets/mesh_assets"
        / "psg23a_scalp_bust.runtime.json"
    )
    source_mesh.write_bytes((
        imported / "assets/mesh_assets"
        / "psg23a_scalp_bust.runtime.json"
    ).read_bytes())
    source_sha = digest(source_mesh)
    region = generated / "regions/scalp_hair.region.json"
    receipt([
        str(args.region_tool.resolve()),
        "--mesh", str(source_mesh),
        "--recipe", str(args.region_recipe.resolve()),
        "--out", str(region),
        "--summary-out", str(generated / "receipts/region.json"),
    ])
    tube_mesh = (
        generated / "assets/mesh_assets"
        / "psg23a_scalp_rooted_strands.runtime.json"
    )
    strand_path = generated / "receipts/strands.asset.json"
    strand_receipt_path = generated / "receipts/strands.json"
    provenance_path = generated / "receipts/provenance.json"
    command = [
        str(args.strand_tool.resolve()),
        "--mesh", str(source_mesh),
        "--region", str(region),
        "--tube-out", str(tube_mesh),
        "--strand-out", str(strand_path),
        "--strand-asset-id", "psg23a_scalp_rooted_strands",
        "--summary-out", str(strand_receipt_path),
        "--provenance-out", str(provenance_path),
        "--threshold", "0.58",
        "--length", "0.34",
        "--root-radius", "0.016",
        "--tip-radius", "0.005",
        "--root-penetration", "0.012",
        "--bend", "0.24",
        "--curl", "0.10",
        "--max-strands", "24",
    ]
    strand_receipt = receipt(command)
    repeat = staging / "repeat"
    repeat.mkdir(parents=True, exist_ok=True)
    replacements = {
        str(tube_mesh): str(repeat / "strands.runtime.json"),
        str(strand_path): str(repeat / "strands.asset.json"),
        str(strand_receipt_path): str(repeat / "strands.json"),
        str(provenance_path): str(repeat / "provenance.json"),
    }
    repeat_receipt = receipt(
        [replacements.get(item, item) for item in command])
    if strand_receipt != repeat_receipt:
        raise RuntimeError("repeat strand receipt differs")
    for first, second in (
        (tube_mesh, repeat / "strands.runtime.json"),
        (strand_path, repeat / "strands.asset.json"),
        (provenance_path, repeat / "provenance.json"),
    ):
        if digest(first) != digest(second):
            raise RuntimeError(f"repeat artifact differs: {first.name}")
    if digest(source_mesh) != source_sha:
        raise RuntimeError("source mesh changed during strand compile")

    view_set = views()
    renders = {
        "source_control": render(
            args.render_cli.resolve(), contract, generated, review,
            "source_control", view_set["hero"], False),
        "rooted_strands_hero": render(
            args.render_cli.resolve(), contract, generated, review,
            "rooted_strands_hero", view_set["hero"], True),
        "rooted_strands_grazing": render(
            args.render_cli.resolve(), contract, generated, review,
            "rooted_strands_grazing", view_set["grazing"], True),
        "rooted_strands_crown": render(
            args.render_cli.resolve(), contract, generated, review,
            "rooted_strands_crown", view_set["crown"], True),
        "rooted_strands_repeat": render(
            args.render_cli.resolve(), contract, generated, review,
            "rooted_strands_repeat", view_set["hero"], True),
    }
    width = contract["render"]["width"]
    height = contract["render"]["height"]
    tubes = load(tube_mesh)
    provenance = load(provenance_path)
    role_image, strand_ids, source_map = debug_views(
        tubes, provenance, width, height)
    for name, pixels in (
        ("root_shaft_tip_roles", role_image),
        ("strand_ids", strand_ids),
        ("root_source_triangle_provenance", source_map),
    ):
        review_artifacts.write_png_rgb(
            review / f"{name}.png", width, height, pixels)
    beauty_changed = changed_pixels(
        renders["source_control"][0], renders["rooted_strands_hero"][0])
    repeat_changed = changed_pixels(
        renders["rooted_strands_hero"][0],
        renders["rooted_strands_repeat"][0])
    assertions = contract["assertions"]
    failures: list[str] = []
    for field in (
        "root_attachment_verified",
        "finite_continuous_strands",
        "overlap_gate_passed",
        "self_intersection_gate_passed",
        "closed_valid_tube_shells",
    ):
        if not strand_receipt[field]:
            failures.append(f"{field} failed")
    if strand_receipt["strand_count"] < assertions["minimum_strands"]:
        failures.append("too few strands")
    if beauty_changed < assertions["minimum_beauty_changed_pixels"]:
        failures.append("strand beauty changed too little")
    if repeat_changed > assertions["maximum_repeat_changed_pixels"]:
        failures.append("repeat pixels differ")
    for name, (_, audit, metrics, _) in renders.items():
        expected = (
            strand_receipt["source_triangle_count"]
            if name == "source_control"
            else strand_receipt["tube_triangle_count"]
        )
        if audit["triangle_count"] != expected:
            failures.append(f"{name}: runtime triangle count drift")
        if audit["primary_hit_pixels"] < 100:
            failures.append(f"{name}: insufficient visible geometry")
        if metrics["luma_standard_deviation"] < 5.0:
            failures.append(f"{name}: visually flat")
    matrix = review / "psg23a_rooted_strands_high_quality_matrix.png"
    write_labeled_contact_sheet(matrix, [
        ("SOURCE CONTROL", renders["source_control"][0]),
        ("ROOTED FIBERS", renders["rooted_strands_hero"][0]),
        ("GRAZING SILHOUETTE", renders["rooted_strands_grazing"][0]),
        ("CROWN / ROOT DISTRIBUTION", renders["rooted_strands_crown"][0]),
        ("ROOT / SHAFT / TIP", role_image),
        ("STRAND IDS", strand_ids),
        ("SOURCE TRIANGLE ROOTS", source_map),
        ("EXACT REPEAT", renders["rooted_strands_repeat"][0]),
    ], columns=4)
    summary = {
        "schema":
            "ray_tracing.procedural_imported_surface_strands_visual_proof",
        "schema_version": 1,
        "passed": not failures,
        "render_resolution": [width, height],
        "fresh_source_stl_sha256": digest(durable_stl),
        "source_runtime_file_sha256": source_sha,
        "strand_data_digest_sha256":
            strand_receipt["strand_data_digest_sha256"],
        "tube_mesh_digest_sha256":
            strand_receipt["tube_mesh_digest_sha256"],
        "provenance_digest_sha256":
            strand_receipt["provenance_digest_sha256"],
        "strand_count": strand_receipt["strand_count"],
        "control_point_count": strand_receipt["control_point_count"],
        "tube_triangle_count": strand_receipt["tube_triangle_count"],
        "minimum_root_clearance_units":
            strand_receipt["minimum_root_clearance_units"],
        "beauty_changed_pixels": beauty_changed,
        "repeat_changed_pixels": repeat_changed,
        "contact_sheet": str(matrix.relative_to(output)),
        "failures": failures,
        "authority": {
            "local_diagnostic_only": True,
            "fresh_imported_object_created": True,
            "source_runtime_mesh_immutable": True,
            "typed_strand_asset_created": True,
            "triangle_tube_proof_backend_only": True,
            "native_curve_runtime_added": False,
            "hair_bsdf_added": False,
            "saved_scene_mutated": False,
            "package_or_release_mutated": False,
        },
    }
    write_json(output / "proof_summary.json", summary)
    if failures:
        raise RuntimeError("; ".join(failures))
    print(output / "proof_summary.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
