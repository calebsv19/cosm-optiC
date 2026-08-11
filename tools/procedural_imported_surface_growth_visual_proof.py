#!/usr/bin/env python3
"""Generate the PSG-22 attached imported-surface growth visual proof."""

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
from procedural_imported_surface_inset_visual_proof import (  # noqa: E402
    rasterize_depth,
)
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
        ROOT / "tests/fixtures/procedural_imported_surface_growth_psg22"
    )
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--growth-tool", type=Path,
        default=default_tool("procedural_imported_surface_growth_tool"))
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
        "--recipe", type=Path,
        default=fixture / "mossy_garden_finial.recipe.json")
    parser.add_argument(
        "--region-recipe", type=Path,
        default=fixture / "moss_growth.region_recipe.json")
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
    output = run(command)
    return json.loads(output.splitlines()[0])


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def object_document(
    object_id: str,
    asset_id: str,
    material_id: str,
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


def load_runtime_binding(path: Path, expected_digest_sha256: str) -> dict:
    """Load a v2 binding without permitting proof-only substitutions."""
    if digest(path) != expected_digest_sha256:
        raise ValueError("attachment runtime binding digest is stale")
    binding = load(path)
    if (binding.get("schema") !=
            "ray_tracing.surface_authoring_attachment_runtime_binding" or
            binding.get("schema_version") != 1):
        raise ValueError("attachment runtime binding schema is unsupported")
    for side in ("source", "attachment"):
        item = binding.get(side)
        if not isinstance(item, dict) or not all(
                isinstance(item.get(key), str) and item[key]
                for key in ("object_id", "asset_id", "path", "digest_sha256")):
            raise ValueError(f"attachment runtime binding {side} identity is invalid")
        if not isinstance(item.get("material"), dict) or not isinstance(
                item["material"].get("id"), str):
            raise ValueError(f"attachment runtime binding {side} material is invalid")
    target = binding["attachment"]["material"]
    if not all(isinstance(target.get(key), str) and target[key]
               for key in ("resource_id", "resource_digest_sha256",
                           "receipt_digest_sha256")):
        raise ValueError("attachment runtime binding material target is invalid")
    if (not isinstance(binding.get("lighting"), dict) or
            binding["lighting"].get("environment_light_mode") != "ambient"):
        raise ValueError("attachment runtime binding lighting is invalid")
    return binding


def make_scene(scene_id: str, include_growth: bool,
               runtime_binding: dict | None = None) -> dict:
    source = (runtime_binding or {}).get("source", {})
    attachment = (runtime_binding or {}).get("attachment", {})
    source_object_id = source.get("object_id", f"{scene_id}_source")
    source_asset_id = source.get("asset_id", "psg22_garden_finial")
    source_material = source.get("material", {
        "id": "weathered_stone", "name": "Weathered garden stone",
        "base_color": {"r": 0.48, "g": 0.44, "b": 0.37},
        "roughness": 0.91, "metallic": 0.0})
    attachment_object_id = attachment.get("object_id", f"{scene_id}_growth")
    attachment_asset_id = attachment.get("asset_id", "psg22_finial_moss_growth")
    attachment_material = attachment.get("material", {
        "id": "living_moss", "name": "Attached moss geometry",
        "base_color": {"r": 0.16, "g": 0.34, "b": 0.10},
        "roughness": 0.97, "metallic": 0.0})
    objects = [object_document(source_object_id, source_asset_id,
                               source_material["id"])]
    if include_growth:
        objects.append(object_document(attachment_object_id, attachment_asset_id,
                                       attachment_material["id"]))
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": scene_id,
        "source_scene_id": scene_id,
        "compile_meta": {
            "compiler_version":
                "psg22_imported_surface_growth_visual_proof",
            "compiled_at_ns": 0,
            "normalization": "fresh_imported_stl_attached_growth_diagnostic",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": objects,
        "materials": [source_material, attachment_material],
        "lights": [],
        "extensions": {},
    }


def views() -> dict[str, dict]:
    return {
        "hero": {
            "camera_position": {"x": 3.05, "y": 2.78, "z": 1.68},
            "camera_look_at": {"x": 0.28, "y": 0.38, "z": 1.00},
        },
        "grazing": {
            "camera_position": {"x": 3.42, "y": 1.08, "z": 1.28},
            "camera_look_at": {"x": 0.54, "y": 0.42, "z": 0.88},
        },
    }


def render(
    render_cli: Path,
    contract: dict,
    generated: Path,
    review: Path,
    name: str,
    view: dict,
    include_growth: bool,
    runtime_binding: dict | None = None,
) -> tuple[list, dict, dict, Path]:
    scene = generated / f"{name}.scene.json"
    request = generated / "requests" / f"{name}.request.json"
    raw = generated / "raw" / name
    write_json(scene, make_scene(name, include_growth, runtime_binding))
    lighting = (runtime_binding or {}).get("lighting", contract["lighting"])
    write_json(request, render_request(
        contract["proof_id"], {**view, "id": name},
        scene, request, raw, {
            "render": contract["render"],
            "lighting": lighting,
        }))
    summary_path = raw / "render_summary.json"
    frame = raw / "frames/frame_0000.bmp"
    run_render_cli(render_cli, request, summary_path)
    summary = load(summary_path)
    audit_id = f"{name}_{'growth' if include_growth else 'source'}"
    audit = object_audit(summary, audit_id)
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
    growth: dict,
    provenance: dict,
    width: int,
    height: int,
) -> tuple[list, list, list]:
    role_palette = {
        "exposed_growth": (83, 166, 61),
        "attachment_base": (232, 132, 64),
    }
    role_colors = [
        role_palette[item["role"]] for item in provenance["triangles"]
    ]
    roles = rasterize_depth(growth, width, height, role_colors)[0]
    element_palette = [
        (76, 160, 214), (126, 202, 96), (226, 164, 70),
        (194, 96, 184), (91, 205, 190), (226, 104, 92),
    ]
    element_colors = [
        element_palette[
            item["growth_element_index"] % len(element_palette)]
        for item in provenance["triangles"]
    ]
    clearance = rasterize_depth(growth, width, height, element_colors)[0]
    provenance_colors = [
        (
            70 + (item["source_triangle_index"] * 37) % 170,
            70 + (item["source_triangle_index"] * 61) % 170,
            70 + (item["source_triangle_index"] * 83) % 170,
        )
        for item in provenance["triangles"]
    ]
    source_map = rasterize_depth(
        growth, width, height, provenance_colors, wireframe=True)[0]
    return roles, clearance, source_map


def main() -> int:
    args = parse_args()
    contract = load(args.contract.resolve())
    output = (args.output_root or (
        ROOT / "build/agent_runs/ray_tracing/procedural_solid"
        / "psg22_imported_surface_growth_v1")).resolve()
    generated = output / "generated"
    review = output / "review"
    staging = Path("/private/tmp") / f"psg22_visual_{os.getpid()}"
    for directory in (
        staging, generated / "fresh_stl",
        generated / "assets/mesh_assets", generated / "regions",
        generated / "receipts", generated / "requests",
        generated / "raw", review,
    ):
        directory.mkdir(parents=True, exist_ok=True)
    tools = (
        args.growth_tool, args.region_tool, args.render_cli,
        args.stl_tool, args.import_harness,
    )
    missing = [str(tool.resolve()) for tool in tools
               if not tool.resolve().exists()]
    if missing:
        raise RuntimeError(f"missing PSG-22 tools: {missing}")

    authored = staging / "authored"
    run([
        sys.executable, str(args.stl_tool.resolve()), "create",
        "--recipe", str(args.recipe.resolve()),
        "--out-root", str(authored),
    ])
    stl = (
        authored / "curated/psg22_garden_finial"
        / "source/psg22_garden_finial.stl"
    )
    durable_stl = generated / "fresh_stl" / f"run_{os.getpid()}.stl"
    durable_stl.write_bytes(stl.read_bytes())
    imported = staging / "imported"
    run([
        str(args.import_harness.resolve()),
        "--stl", str(stl),
        "--out", str(imported),
        "--asset-id", "psg22_garden_finial",
        "--scene-id", "psg22_imported_surface_growth",
        "--object-id", "psg22_finial",
    ])
    source_mesh = (
        generated / "assets/mesh_assets"
        / "psg22_garden_finial.runtime.json"
    )
    source_mesh.write_bytes((
        imported / "assets/mesh_assets"
        / "psg22_garden_finial.runtime.json"
    ).read_bytes())
    source_sha = digest(source_mesh)
    region = generated / "regions/moss_growth.region.json"
    receipt([
        str(args.region_tool.resolve()),
        "--mesh", str(source_mesh),
        "--recipe", str(args.region_recipe.resolve()),
        "--out", str(region),
        "--summary-out", str(generated / "receipts/region.json"),
    ])
    growth_mesh = (
        generated / "assets/mesh_assets"
        / "psg22_finial_moss_growth.runtime.json"
    )
    growth_receipt_path = generated / "receipts/growth.json"
    provenance_path = generated / "receipts/provenance.json"
    command = [
        str(args.growth_tool.resolve()),
        "--mesh", str(source_mesh),
        "--region", str(region),
        "--out", str(growth_mesh),
        "--growth-asset-id", "psg22_finial_moss_growth",
        "--summary-out", str(growth_receipt_path),
        "--provenance-out", str(provenance_path),
        "--threshold", "0.62",
        "--radius", "0.15",
        "--height", "0.120",
        "--attachment-depth", "0.025",
        "--max-elements", "12",
    ]
    growth_receipt = receipt(command)
    repeat = staging / "repeat"
    repeat.mkdir(parents=True, exist_ok=True)
    replacements = {
        str(growth_mesh): str(repeat / "growth.runtime.json"),
        str(growth_receipt_path): str(repeat / "growth.json"),
        str(provenance_path): str(repeat / "provenance.json"),
    }
    repeat_receipt = receipt(
        [replacements.get(item, item) for item in command])
    if growth_receipt != repeat_receipt:
        raise RuntimeError("repeat growth receipt differs")
    for first, second in (
        (growth_mesh, repeat / "growth.runtime.json"),
        (provenance_path, repeat / "provenance.json"),
    ):
        if digest(first) != digest(second):
            raise RuntimeError(f"repeat artifact differs: {first.name}")
    if digest(source_mesh) != source_sha:
        raise RuntimeError("source mesh changed during growth compile")

    view_set = views()
    renders = {
        "source_control": render(
            args.render_cli.resolve(), contract, generated, review,
            "source_control", view_set["hero"], False),
        "attached_growth_hero": render(
            args.render_cli.resolve(), contract, generated, review,
            "attached_growth_hero", view_set["hero"], True),
        "attached_growth_grazing": render(
            args.render_cli.resolve(), contract, generated, review,
            "attached_growth_grazing", view_set["grazing"], True),
        "attached_growth_repeat": render(
            args.render_cli.resolve(), contract, generated, review,
            "attached_growth_repeat", view_set["hero"], True),
    }
    width = contract["render"]["width"]
    height = contract["render"]["height"]
    growth = load(growth_mesh)
    provenance = load(provenance_path)
    role_image, clearance_image, source_map = debug_views(
        growth, provenance, width, height)
    for name, pixels in (
        ("attachment_roles", role_image),
        ("element_clearance", clearance_image),
        ("source_triangle_provenance", source_map),
    ):
        review_artifacts.write_png_rgb(review / f"{name}.png",
                                       width, height, pixels)
    beauty_changed = changed_pixels(
        renders["source_control"][0], renders["attached_growth_hero"][0])
    repeat_changed = changed_pixels(
        renders["attached_growth_hero"][0],
        renders["attached_growth_repeat"][0])
    assertions = contract["assertions"]
    failures: list[str] = []
    if not growth_receipt["attachment_penetration_verified"]:
        failures.append("attachment penetration gate failed")
    if growth_receipt["boundary_edge_count"] != 0:
        failures.append("growth shell has open edges")
    if growth_receipt["nonmanifold_edge_count"] != 0:
        failures.append("growth shell has nonmanifold edges")
    if growth_receipt["inter_element_overlap_pair_count"] != 0:
        failures.append("growth elements overlap")
    if growth_receipt["self_intersection_pair_count"] != 0:
        failures.append("growth shell self-intersects")
    if growth_receipt["growth_element_count"] < (
        assertions["minimum_growth_elements"]
    ):
        failures.append("too few growth elements")
    if beauty_changed < assertions["minimum_beauty_changed_pixels"]:
        failures.append("growth beauty changed too little")
    if repeat_changed > assertions["maximum_repeat_changed_pixels"]:
        failures.append("repeat pixels differ")
    for name, (_, audit, metrics, _) in renders.items():
        expected = (
            growth_receipt["source_triangle_count"]
            if name == "source_control"
            else growth_receipt["growth_triangle_count"]
        )
        if audit["triangle_count"] != expected:
            failures.append(f"{name}: runtime triangle count drift")
        if audit["primary_hit_pixels"] < 100:
            failures.append(f"{name}: insufficient visible geometry")
        if metrics["luma_standard_deviation"] < 5.0:
            failures.append(f"{name}: visually flat")
    matrix = review / "psg22_attached_growth_high_quality_matrix.png"
    write_labeled_contact_sheet(matrix, [
        ("SOURCE CONTROL", renders["source_control"][0]),
        ("ATTACHED MOSS", renders["attached_growth_hero"][0]),
        ("GRAZING SILHOUETTE", renders["attached_growth_grazing"][0]),
        ("ATTACHMENT BASE", role_image),
        ("ELEMENT CLEARANCE", clearance_image),
        ("SOURCE PROVENANCE", source_map),
        ("EXACT REPEAT", renders["attached_growth_repeat"][0]),
    ], columns=4)
    summary = {
        "schema":
            "ray_tracing.procedural_imported_surface_growth_visual_proof",
        "schema_version": 1,
        "passed": not failures,
        "render_resolution": [width, height],
        "fresh_source_stl_sha256": digest(durable_stl),
        "source_runtime_file_sha256": source_sha,
        "growth_mesh_digest_sha256":
            growth_receipt["growth_mesh_digest_sha256"],
        "provenance_digest_sha256":
            growth_receipt["provenance_digest_sha256"],
        "growth_element_count": growth_receipt["growth_element_count"],
        "growth_triangle_count": growth_receipt["growth_triangle_count"],
        "attachment_base_triangle_count":
            growth_receipt["attachment_base_triangle_count"],
        "minimum_inter_element_clearance_units":
            growth_receipt["minimum_inter_element_clearance_units"],
        "beauty_changed_pixels": beauty_changed,
        "repeat_changed_pixels": repeat_changed,
        "contact_sheet": str(matrix.relative_to(output)),
        "failures": failures,
        "authority": {
            "local_diagnostic_only": True,
            "fresh_imported_object_created": True,
            "source_runtime_mesh_immutable": True,
            "separate_growth_asset_created": True,
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
