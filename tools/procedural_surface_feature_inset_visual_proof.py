#!/usr/bin/env python3
"""Build PSG-24C's physical feature-inset review pack on curved plaster."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
INTEGRATION = ROOT / "tests/integration"
sys.path.insert(0, str(INTEGRATION))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
from procedural_imported_surface_inset_visual_proof import (  # noqa: E402
    depth_delta_debug,
    role_debug,
)
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
    "psg24c_physical_surface_feature_insets_v1")


def read(path: pathlib.Path) -> dict:
    return json.loads(path.read_text())


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


def feature_debug(mesh: dict, provenance: dict, width: int, height: int) -> list:
    palette: dict[int, tuple[int, int, int]] = {0: (72, 67, 61)}
    for feature_id in provenance["selected_feature_ids"]:
        value = hashlib.sha256(str(feature_id).encode()).digest()
        palette[feature_id] = (72 + value[0] % 176, 72 + value[1] % 176,
                               72 + value[2] % 176)
    colors = [palette[int(entry["feature_id"])] for entry in provenance["triangles"]]
    from procedural_imported_surface_inset_visual_proof import rasterize_depth
    return rasterize_depth(mesh, width, height, colors)[0]


def render(render_cli: pathlib.Path, base: pathlib.Path, generated: pathlib.Path,
           review: pathlib.Path, name: str, asset_id: str,
           request_name: str) -> tuple[list, dict, pathlib.Path]:
    scene = copy.deepcopy(read(base / f"generated/{request_name}.scene.json"))
    scene["scene_id"] = f"psg24c_{name}"
    scene["source_scene_id"] = scene["scene_id"]
    scene["compile_meta"]["compiler_version"] = "psg24c_surface_feature_inset_visual_proof"
    object_document = scene["objects"][0]
    object_document["geometry_ref"]["id"] = asset_id
    object_document["object_id"] = "psg24c_plaster_object"
    object_document.pop("procedural_solid_material_ref", None)
    scene["materials"] = [{
        "id": "fallback", "name": "warm retained plaster",
        "base_color": {"r": 0.73, "g": 0.55, "b": 0.36},
        "roughness": 0.88, "metallic": 0.0,
    }]
    # Runtime mesh lookup is rooted beside the scene at generated/assets.
    scene_path = generated / f"{name}.scene.json"
    write(scene_path, scene)
    request = copy.deepcopy(read(base / f"generated/requests/{request_name}.request.json"))
    raw = generated / "raw" / name
    request["run_id"] = f"psg24c_physical_feature_inset_{name}"
    request["scene"]["runtime_scene_path"] = str(scene_path.resolve())
    request["output"]["root"] = str(raw.resolve())
    request["progress"]["summary_path"] = str((raw / "render_summary.json").resolve())
    request["progress"]["progress_path"] = str((raw / "render_progress.json").resolve())
    request["inspection"]["ambient_strength"] = 0.58
    request["inspection"]["light_intensity"] = 1.55
    request_path = generated / "requests" / f"{name}.request.json"
    write(request_path, request)
    summary_path = raw / "render_summary.json"
    run_render_cli(render_cli, request_path, summary_path)
    summary = read(summary_path)
    frame = raw / "frames/frame_0000.bmp"
    width, height, pixels = review_artifacts.read_bmp_rgb(frame)
    output = review / f"{name}.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(output, width, height, pixels)
    return pixels, object_audit(summary, "psg24c_plaster_object"), output


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-root", type=pathlib.Path, default=DEFAULT_BASE)
    parser.add_argument("--output-root", type=pathlib.Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--selection-tool", type=pathlib.Path, default=(
        ROOT / "build/toolchains/clang/arm64/tools/cli/procedural_surface_feature_selection_tool"))
    parser.add_argument("--inset-tool", type=pathlib.Path, default=(
        ROOT / "build/toolchains/clang/arm64/tools/cli/procedural_imported_surface_inset_tool"))
    parser.add_argument("--render-cli", type=pathlib.Path, default=(
        ROOT / "build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless"))
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    base, output = args.base_root.resolve(), args.output_root.resolve()
    generated, review = output / "generated", output / "review"
    source = base / "generated/assets/mesh_assets/psg24_closed_curved_plaster.runtime.json"
    field = base / "field/assets/surface_feature_field_v1.json"
    region = base / "generated/regions/plaster_peel.region.json"
    for required in (source, field, region, args.selection_tool, args.inset_tool, args.render_cli):
        if not required.is_file():
            raise RuntimeError(f"missing PSG-24C proof input: {required}")
    field_json = read(field)
    selected_ids = [str(entry["feature_id"]) for entry in field_json["features"]
                    if float(entry.get("height_or_depth", 0.0)) < 0.0][:8]
    if len(selected_ids) < 4:
        raise RuntimeError("at least four negative-depth spot features are required")
    ids = ",".join(selected_ids)
    compiles = []
    for suffix in ("primary", "repeat"):
        target = generated / f"compile_{suffix}"
        run([sys.executable, str(ROOT / "tools/procedural_surface_feature_inset_compiler.py"),
             "--selection-tool", str(args.selection_tool.resolve()),
             "--inset-tool", str(args.inset_tool.resolve()), "--mesh", str(source),
             "--field", str(field), "--base-region", str(region),
             "--feature-ids", ids, "--out-root", str(target),
             "--derived-asset-id", "psg24c_curved_plaster_feature_inset",
             "--region-id", "psg24c_selected_large_spots",
             "--threshold", "0.2", "--depth", "0.035",
             "--depth-variation", "0.25", "--minimum-component-triangles", "2"])
        compiles.append(target)
    primary, repeated = compiles
    relative = [path.relative_to(primary) for path in primary.rglob("*")
                if path.is_file() and path.name != "bundle.json"]
    repeated_relative = {path.relative_to(repeated) for path in repeated.rglob("*")
                         if path.is_file() and path.name != "bundle.json"}
    if set(relative) != repeated_relative:
        raise RuntimeError("repeat compile artifact inventory differs")
    differing = [str(path) for path in relative if digest(primary / path) != digest(repeated / path)]
    if differing:
        raise RuntimeError(f"repeat feature-inset artifacts differ: {differing}")
    asset_root = generated / "assets/mesh_assets"
    asset_root.mkdir(parents=True, exist_ok=True)
    source_copy = asset_root / source.name
    derived_source = primary / "assets/psg24c_curved_plaster_feature_inset.runtime.json"
    derived_copy = asset_root / derived_source.name
    shutil.copyfile(source, source_copy)
    shutil.copyfile(derived_source, derived_copy)

    renders = {
        "source_control_hero": render(args.render_cli, base, generated, review,
            "source_control_hero", "psg24_closed_curved_plaster", "plaster_concrete_layered"),
        "feature_inset_hero": render(args.render_cli, base, generated, review,
            "feature_inset_hero", "psg24c_curved_plaster_feature_inset", "plaster_concrete_layered"),
        "feature_inset_grazing": render(args.render_cli, base, generated, review,
            "feature_inset_grazing", "psg24c_curved_plaster_feature_inset", "plaster_concrete_grazing"),
        "feature_inset_repeat": render(args.render_cli, base, generated, review,
            "feature_inset_repeat", "psg24c_curved_plaster_feature_inset", "plaster_concrete_layered"),
    }
    width, height = 1280, 960
    source_mesh, derived_mesh = read(source_copy), read(derived_copy)
    provenance = read(primary / "provenance/surface_feature_inset.provenance.json")
    role_pixels = role_debug(derived_mesh, provenance, width, height, wireframe=False)
    wire_pixels = role_debug(derived_mesh, provenance, width, height, wireframe=True)
    depth_pixels, depth_changed = depth_delta_debug(source_mesh, derived_mesh, width, height)
    feature_pixels = feature_debug(derived_mesh, provenance, width, height)
    repeat_difference = difference(renders["feature_inset_hero"][0], renders["feature_inset_repeat"][0])
    debug = {
        "topology_roles": role_pixels, "source_triangle_provenance": wire_pixels,
        "source_vs_derived_depth": depth_pixels, "selected_feature_ids": feature_pixels,
        "exact_repeat_difference": repeat_difference,
    }
    debug_paths = {}
    for name, pixels in debug.items():
        path = review / f"{name}.png"
        review_artifacts.write_png_rgb(path, width, height, pixels)
        debug_paths[name] = path
    beauty_changed = changed_pixels(renders["source_control_hero"][0], renders["feature_inset_hero"][0])
    repeat_changed = changed_pixels(renders["feature_inset_hero"][0], renders["feature_inset_repeat"][0])
    receipt = read(primary / "receipts/surface_feature_inset.receipt.json")
    inset = read(primary / "receipts/inset.receipt.json")
    failures = []
    if repeat_changed != 0: failures.append("repeat render differs")
    if depth_changed < 1000: failures.append("physical depth change is visually too small")
    if beauty_changed < 1000: failures.append("beauty does not visibly consume the inset shell")
    if receipt["selected_component_count"] < 2: failures.append("multiple retained islands missing")
    if not inset["closed_valid_shell"]: failures.append("derived shell is not closed manifold")
    if receipt["local_patch_reduction_ratio"] <= 0.5: failures.append("local support is not bounded")
    for name, (_, audit, _) in renders.items():
        expected = inset["source_triangle_count"] if name == "source_control_hero" else inset["derived_triangle_count"]
        if audit["triangle_count"] != expected: failures.append(f"{name}: runtime topology not consumed")
        if audit["primary_hit_pixels"] < 120000: failures.append(f"{name}: insufficient object coverage")
        if image_metrics(renders[name][0])["luma_standard_deviation"] < 8.0: failures.append(f"{name}: visually flat")
    matrix = review / "psg24c_physical_surface_feature_inset_contact_sheet.png"
    write_labeled_contact_sheet(matrix, [
        ("SOURCE CONTROL", renders["source_control_hero"][0]),
        ("PHYSICAL FEATURE INSETS", renders["feature_inset_hero"][0]),
        ("GRAZING DEPTH", renders["feature_inset_grazing"][0]),
        ("RETAINED / WALL / FLOOR", role_pixels),
        ("SOURCE VS DERIVED DEPTH", depth_pixels),
        ("SELECTED FEATURE IDS", feature_pixels),
        ("SOURCE TRIANGLE PROVENANCE", wire_pixels),
        ("EXACT REPEAT DIFFERENCE", repeat_difference),
    ], columns=2)
    summary = {
        "schema": "ray_tracing.psg24c_surface_feature_inset_visual_proof_v1",
        "status": "passed" if not failures else "failed",
        "render_resolution": [width, height],
        "source_mesh_digest_sha256": receipt["source_mesh_digest_sha256"],
        "field_digest_sha256": receipt["field_digest_sha256"],
        "selected_feature_ids": receipt["selected_feature_ids"],
        "selected_component_count": receipt["selected_component_count"],
        "local_patch_triangle_count": receipt["local_patch_triangle_count"],
        "source_triangle_count": receipt["source_triangle_count"],
        "local_patch_reduction_ratio": receipt["local_patch_reduction_ratio"],
        "retained_triangle_count": receipt["retained_triangle_count"],
        "transition_wall_triangle_count": receipt["transition_wall_triangle_count"],
        "inset_floor_triangle_count": receipt["inset_floor_triangle_count"],
        "derived_triangle_count": inset["derived_triangle_count"],
        "minimum_inset_depth_units": inset["minimum_inset_depth_units"],
        "maximum_inset_depth_units": inset["maximum_inset_depth_units"],
        "projected_depth_changed_pixels": depth_changed,
        "beauty_changed_pixels": beauty_changed,
        "repeat_changed_pixels": repeat_changed,
        "byte_identical_repeat_artifact_count": len(relative),
        "contact_sheet": str(matrix.relative_to(output)),
        "review_images": {name: str(data[2].relative_to(output)) for name, data in renders.items()} | {
            name: str(path.relative_to(output)) for name, path in debug_paths.items()},
        "failures": failures,
        "authority": {"local_diagnostic_only": True, "saved_scene_mutated": False,
                      "package_or_release_mutated": False, "psg24d_started": False},
    }
    write(output / "proof_summary.json", summary)
    if failures:
        raise RuntimeError("; ".join(failures))
    print(output / "proof_summary.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
