#!/usr/bin/env python3
"""Build the PSG-24D attached-deposit review pack on curved plaster."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import pathlib
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
INTEGRATION = ROOT / "tests/integration"
sys.path.insert(0, str(INTEGRATION))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
from procedural_imported_surface_inset_visual_proof import rasterize_depth  # noqa: E402
from procedural_surface_visual_proof import (  # noqa: E402
    image_metrics,
    object_audit,
    run_render_cli,
    write_labeled_contact_sheet,
)


DEFAULT_BASE = pathlib.Path(
    "/Users/calebsv/Desktop/CodeWork/_private_workspace_artifacts/"
    "procedural_object_iterations/psg24_stabilization_20260802/proof/"
    "psg24a_surface_feature_fields_v4")
DEFAULT_OUTPUT = pathlib.Path(
    "/Users/calebsv/Desktop/CodeWork/_private_workspace_artifacts/"
    "procedural_object_iterations/psg24_stabilization_20260802/proof/"
    "psg24d_attached_surface_feature_deposits_v1")


def read(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write(path: pathlib.Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str]) -> None:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}")


def changed_pixels(left: list, right: list) -> int:
    return sum(a != b for left_row, right_row in zip(left, right)
               for a, b in zip(left_row, right_row))


def difference(left: list, right: list) -> list:
    return [[tuple(abs(a[index] - b[index]) for index in range(3))
             for a, b in zip(left_row, right_row)]
            for left_row, right_row in zip(left, right)]


def object_document(object_id: str, asset_id: str) -> dict:
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
        "material_ref": {"id": "dried_mud_deposit"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def scene(base: pathlib.Path, name: str, asset_ids: list[str]) -> dict:
    value = copy.deepcopy(read(base / "generated/plaster_concrete_layered.scene.json"))
    value["scene_id"] = f"psg24d_{name}"
    value["source_scene_id"] = value["scene_id"]
    value["compile_meta"]["compiler_version"] = "psg24d_surface_feature_deposit_visual_proof"
    source_object = value["objects"][0]
    source_object["object_id"] = "psg24d_source_object"
    source_object["material_ref"] = {"id": "warm_plaster"}
    source_object.pop("procedural_solid_material_ref", None)
    value["objects"] = [source_object] + [
        object_document(f"psg24d_deposit_{index}", asset_id)
        for index, asset_id in enumerate(asset_ids)]
    value["materials"] = [
        {"id": "warm_plaster", "name": "Warm curved plaster",
         "base_color": {"r": 0.72, "g": 0.66, "b": 0.55},
         "roughness": 0.91, "metallic": 0.0},
        {"id": "dried_mud_deposit", "name": "Field-matched dried mud",
         "base_color": {"r": 0.34, "g": 0.17, "b": 0.075},
         "roughness": 0.88, "metallic": 0.0},
    ]
    return value


def render(render_cli: pathlib.Path, base: pathlib.Path,
           generated: pathlib.Path, review: pathlib.Path, name: str,
           asset_ids: list[str], request_name: str) -> tuple[list, list[dict], dict]:
    scene_path = generated / f"{name}.scene.json"
    write(scene_path, scene(base, name, asset_ids))
    request = copy.deepcopy(read(base / f"generated/requests/{request_name}.request.json"))
    raw = generated / "raw" / name
    request["run_id"] = f"psg24d_attached_deposit_{name}"
    request["scene"]["runtime_scene_path"] = str(scene_path.resolve())
    request["output"]["root"] = str(raw.resolve())
    request["progress"]["summary_path"] = str((raw / "render_summary.json").resolve())
    request["progress"]["progress_path"] = str((raw / "render_progress.json").resolve())
    request["inspection"]["ambient_strength"] = 0.62
    request["inspection"]["light_intensity"] = 1.48
    request_path = generated / "requests" / f"{name}.request.json"
    write(request_path, request)
    summary_path = raw / "render_summary.json"
    run_render_cli(render_cli, request_path, summary_path)
    summary = read(summary_path)
    width, height, pixels = review_artifacts.read_bmp_rgb(
        raw / "frames/frame_0000.bmp")
    review_artifacts.write_png_rgb(review / f"{name}.png", width, height, pixels)
    audits = [object_audit(summary, "psg24d_source_object")]
    audits.extend(object_audit(summary, f"psg24d_deposit_{index}")
                  for index in range(len(asset_ids)))
    return pixels, audits, image_metrics(pixels)


def choose_clear_features(features: list[dict]) -> list[int]:
    accepted: list[tuple[list[float], float, int]] = []
    for feature in features:
        radius = float(feature["radius"]) * 0.32
        height = float(feature["height_or_depth"]) * 0.85
        attachment = max(radius * 0.22, height * 1.10)
        center = [float(feature["position"][axis]) +
                  float(feature["normal"][axis]) * (height - attachment) * 0.5
                  for axis in range(3)]
        bound = max(radius * max(1.0, float(feature["aspect"])),
                    (height + attachment) * 0.5)
        if all(math.dist(center, prior_center) >= 1.02 * (bound + prior_bound)
               for prior_center, prior_bound, _ in accepted):
            accepted.append((center, bound, int(feature["feature_id"])))
    return [feature_id for _, _, feature_id in accepted]


def merged_debug_mesh(source: dict, assets: list[dict]) -> dict:
    vertices: list[dict] = []
    triangles: list[dict] = []
    for asset in assets:
        offset = len(vertices)
        vertices.extend(asset["mesh"]["vertices"])
        for triangle in asset["mesh"]["triangles"]:
            entry = dict(triangle)
            for key in ("a", "b", "c"):
                entry[key] = int(entry[key]) + offset
            triangles.append(entry)
    return {
        "local_bounds": source["local_bounds"],
        "mesh": {"vertices": vertices, "triangles": triangles},
    }


def palette(feature_id: int) -> tuple[int, int, int]:
    value = hashlib.sha256(str(feature_id).encode()).digest()
    return (72 + value[0] % 176, 72 + value[1] % 176,
            72 + value[2] % 176)


def with_host_context(debug: list, source: list) -> list:
    """Keep diagnostic color while retaining a readable dim host silhouette."""
    background = (13, 16, 21)
    return [[
        tuple(int(component * 0.42) for component in source_pixel)
        if debug_pixel == background else debug_pixel
        for debug_pixel, source_pixel in zip(debug_row, source_row)
    ] for debug_row, source_row in zip(debug, source)]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-root", type=pathlib.Path, default=DEFAULT_BASE)
    parser.add_argument("--output-root", type=pathlib.Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--selection-tool", type=pathlib.Path, default=(
        ROOT / "build/toolchains/clang/arm64/tools/cli/procedural_surface_feature_selection_tool"))
    parser.add_argument("--growth-tool", type=pathlib.Path, default=(
        ROOT / "build/toolchains/clang/arm64/tools/cli/procedural_imported_surface_growth_tool"))
    parser.add_argument("--render-cli", type=pathlib.Path, default=(
        ROOT / "build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless"))
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    base, output = args.base_root.resolve(), args.output_root.resolve()
    generated, review = output / "generated", output / "review"
    review.mkdir(parents=True, exist_ok=True)
    source = base / "generated/assets/mesh_assets/psg24_closed_curved_plaster.runtime.json"
    field = base / "field/assets/surface_feature_field_v1.json"
    region = base / "generated/regions/plaster_peel.region.json"
    for required in (source, field, region, args.selection_tool,
                     args.growth_tool, args.render_cli):
        if not required.is_file():
            raise RuntimeError(f"missing PSG-24D proof input: {required}")
    field_json = read(field)
    selected_ids = choose_clear_features([
        entry for entry in field_json["features"]
        if float(entry.get("height_or_depth", 0.0)) > 0.0])
    if len(selected_ids) < 4:
        raise RuntimeError("four separated positive-height spots are required")
    ids = ",".join(str(value) for value in selected_ids)
    compiles: list[pathlib.Path] = []
    compiler = ROOT / "tools/procedural_surface_feature_deposit_compiler.py"
    for suffix in ("primary", "repeat"):
        target = generated / f"compile_{suffix}"
        if target.exists():
            shutil.rmtree(target)
        run([sys.executable, str(compiler),
             "--selection-tool", str(args.selection_tool.resolve()),
             "--growth-tool", str(args.growth_tool.resolve()),
             "--mesh", str(source), "--field", str(field),
             "--base-region", str(region), "--feature-ids", ids,
             "--out-root", str(target),
             "--asset-prefix", "psg24d_curved_plaster_deposit",
             "--material-semantic", "dried_mud_deposit",
             "--radius-scale", "0.32", "--height-scale", "0.85",
             "--attachment-to-radius", "0.22",
             "--minimum-attachment-to-height", "1.10"])
        compiles.append(target)
    primary, repeated = compiles
    relative = [path.relative_to(primary) for path in primary.rglob("*")
                if path.is_file()]
    if set(relative) != {path.relative_to(repeated) for path in repeated.rglob("*")
                         if path.is_file()}:
        raise RuntimeError("repeat deposit inventory differs")
    differing = [str(path) for path in relative
                 if digest(primary / path) != digest(repeated / path)]
    if differing:
        raise RuntimeError(f"repeat deposit artifacts differ: {differing}")

    asset_root = generated / "assets/mesh_assets"
    asset_root.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, asset_root / source.name)
    provenance = read(primary / "provenance/surface_feature_deposit.provenance.json")
    asset_ids = [element["asset_id"] for element in provenance["elements"]]
    asset_documents: list[dict] = []
    for asset_id in asset_ids:
        source_asset = primary / f"assets/{asset_id}.runtime.json"
        shutil.copyfile(source_asset, asset_root / source_asset.name)
        asset_documents.append(read(source_asset))

    renders = {
        "source_control": render(args.render_cli, base, generated, review,
                                 "source_control", [], "plaster_concrete_layered"),
        "attached_deposit_hero": render(args.render_cli, base, generated, review,
                                         "attached_deposit_hero", asset_ids,
                                         "plaster_concrete_layered"),
        "attached_deposit_grazing": render(args.render_cli, base, generated, review,
                                            "attached_deposit_grazing", asset_ids,
                                            "plaster_concrete_grazing"),
        "attached_deposit_detail": render(args.render_cli, base, generated, review,
                                           "attached_deposit_detail", asset_ids,
                                           "plaster_concrete_detail"),
        "attached_deposit_repeat": render(args.render_cli, base, generated, review,
                                           "attached_deposit_repeat", asset_ids,
                                           "plaster_concrete_layered"),
    }
    width, height = 1280, 960
    debug_mesh = merged_debug_mesh(read(source), asset_documents)
    triangles = provenance["triangles"]
    role_colors = [(75, 174, 85) if entry["role"] == "exposed_growth"
                   else (240, 132, 58) for entry in triangles]
    feature_colors = [palette(int(entry["feature_id"])) for entry in triangles]
    source_colors = [
        (70 + int(entry["feature_source_triangle_index"]) * 37 % 170,
         70 + int(entry["feature_source_triangle_index"]) * 61 % 170,
         70 + int(entry["feature_source_triangle_index"]) * 83 % 170)
        for entry in triangles]
    debug_images = {
        "attachment_base_roles": rasterize_depth(debug_mesh, width, height, role_colors)[0],
        "element_clearance": rasterize_depth(debug_mesh, width, height, feature_colors,
                                              wireframe=True)[0],
        "source_triangle_provenance": rasterize_depth(
            debug_mesh, width, height, source_colors, wireframe=True)[0],
        "feature_id": rasterize_depth(debug_mesh, width, height, feature_colors)[0],
    }
    contextual_debug_images = {
        name: with_host_context(pixels, renders["source_control"][0])
        for name, pixels in debug_images.items()
    }
    for name, pixels in debug_images.items():
        review_artifacts.write_png_rgb(review / f"{name}.png", width, height, pixels)
        review_artifacts.write_png_rgb(
            review / f"{name}_with_host.png", width, height,
            contextual_debug_images[name])
    repeat_difference = difference(
        renders["attached_deposit_hero"][0],
        renders["attached_deposit_repeat"][0])
    review_artifacts.write_png_rgb(
        review / "exact_repeat_difference.png", width, height, repeat_difference)

    beauty_changed = changed_pixels(
        renders["source_control"][0], renders["attached_deposit_hero"][0])
    repeat_changed = changed_pixels(
        renders["attached_deposit_hero"][0],
        renders["attached_deposit_repeat"][0])
    receipt = read(primary / "receipts/surface_feature_deposit.receipt.json")
    failures: list[str] = []
    if beauty_changed < 3000:
        failures.append("attached deposits changed too few beauty pixels")
    if repeat_changed != 0:
        failures.append("repeat render differs")
    if receipt["forbidden_overlap_pair_count"] != 0 or not receipt["bounded_clearance_verified"]:
        failures.append("cross-asset clearance failed")
    if receipt["self_intersection_pair_count"] != 0:
        failures.append("self-intersection gate failed")
    if receipt["closed_positive_volume_component_count"] != len(asset_ids):
        failures.append("component count differs from selected features")
    if not receipt["material_agreement_verified"]:
        failures.append("field/deposit material agreement failed")
    visible_hits = [
        sum(renders[name][1][index + 1]["primary_hit_pixels"]
            for name in ("attached_deposit_hero", "attached_deposit_grazing",
                         "attached_deposit_detail"))
        for index in range(len(asset_ids))
    ]
    if any(hit_count <= 0 for hit_count in visible_hits):
        failures.append("one or more selected deposits has no primary-hit evidence")
    expected_source_triangles = len(read(source)["mesh"]["triangles"])
    for name, (_, audits, metrics) in renders.items():
        if audits[0]["triangle_count"] != expected_source_triangles:
            failures.append(f"{name}: source triangle count drift")
        if name != "source_control" and len(audits) != len(asset_ids) + 1:
            failures.append(f"{name}: missing separate deposit object")
        if metrics["luma_standard_deviation"] < 5.0:
            failures.append(f"{name}: visually flat")

    matrix = review / "psg24d_attached_deposit_review.png"
    write_labeled_contact_sheet(matrix, [
        ("SOURCE CONTROL", renders["source_control"][0]),
        ("ATTACHED DEPOSIT HERO", renders["attached_deposit_hero"][0]),
        ("FEATURE-SCALE DETAIL", renders["attached_deposit_detail"][0]),
        ("GRAZING PHYSICAL DEPTH", renders["attached_deposit_grazing"][0]),
        ("ATTACHMENT ROLES + HOST", contextual_debug_images["attachment_base_roles"]),
        ("CLEARANCE + HOST", contextual_debug_images["element_clearance"]),
        ("SOURCE PROVENANCE + HOST", contextual_debug_images["source_triangle_provenance"]),
        ("FEATURE ID + HOST", contextual_debug_images["feature_id"]),
        ("EXACT REPEAT DIFFERENCE", repeat_difference),
    ], columns=2)
    summary = {
        "schema": "ray_tracing.surface_feature_deposit_visual_proof_v1",
        "schema_version": 1,
        "passed": not failures,
        "render_resolution": [width, height],
        "selected_positive_feature_count": len(selected_ids),
        "separate_growth_asset_count": len(asset_ids),
        "growth_triangle_count": receipt["growth_triangle_count"],
        "attachment_base_triangle_count": receipt["attachment_base_triangle_count"],
        "minimum_cross_asset_clearance_units": receipt["minimum_cross_asset_clearance_units"],
        "forbidden_overlap_pair_count": receipt["forbidden_overlap_pair_count"],
        "self_intersection_pair_count": receipt["self_intersection_pair_count"],
        "material_agreement_verified": receipt["material_agreement_verified"],
        "repeat_compile_artifact_count": len(relative),
        "beauty_changed_pixels": beauty_changed,
        "repeat_changed_pixels": repeat_changed,
        "per_asset_primary_hits_across_review_cameras": visible_hits,
        "source_runtime_file_sha256": digest(source),
        "contact_sheet": str(matrix.relative_to(output)),
        "failures": failures,
        "authority": {
            "local_diagnostic_only": True,
            "source_runtime_mesh_immutable": True,
            "separate_growth_assets_created": True,
            "boolean_union_claimed": False,
            "saved_scene_mutated": False,
            "package_or_release_mutated": False,
        },
    }
    write(output / "proof_summary.json", summary)
    if failures:
        raise RuntimeError("; ".join(failures))
    print(output / "proof_summary.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
