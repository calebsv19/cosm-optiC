#!/usr/bin/env python3
"""Build PSG-24B's native curved-plaster scratch-field review pack."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INTEGRATION = ROOT / "tests" / "integration"
if str(INTEGRATION) not in sys.path:
    sys.path.insert(0, str(INTEGRATION))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
from procedural_surface_visual_proof import (  # noqa: E402
    image_metrics,
    object_audit,
    run_render_cli,
    write_labeled_contact_sheet,
)


DEFAULT_BASE = Path(
    "/Users/calebsv/Desktop/CodeWork/_private_workspace_artifacts/"
    "procedural_object_iterations/psg24_stabilization_20260802/proof/"
    "psg24a_surface_feature_fields_v4"
)
DEFAULT_OUTPUT = Path(
    "/Users/calebsv/Desktop/CodeWork/_private_workspace_artifacts/"
    "procedural_object_iterations/psg24_stabilization_20260802/proof/"
    "psg24b_surface_curve_fields_v1"
)


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def run(command: list[str]) -> None:
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{completed.stdout}{completed.stderr}")


def changed_pixels(left: list, right: list) -> int:
    return sum(a != b for left_row, right_row in zip(left, right)
               for a, b in zip(left_row, right_row))


def difference(left: list, right: list) -> list:
    return [[tuple(abs(a[index] - b[index]) for index in range(3))
             for a, b in zip(left_row, right_row)]
            for left_row, right_row in zip(left, right)]


def resize(pixels: list, width: int, height: int) -> list:
    source_height = len(pixels)
    source_width = len(pixels[0])
    return [[pixels[min(source_height - 1, y * source_height // height)]
             [min(source_width - 1, x * source_width // width)]
             for x in range(width)] for y in range(height)]


def color_id(value: int) -> tuple[int, int, int]:
    digest = hashlib.sha256(str(value).encode()).digest()
    return (72 + digest[0] % 176, 72 + digest[1] % 176,
            72 + digest[2] % 176)


def draw_line(canvas: list, first: tuple[int, int], second: tuple[int, int],
              color: tuple[int, int, int], radius: int = 1) -> None:
    x0, y0 = first
    x1, y1 = second
    steps = max(abs(x1 - x0), abs(y1 - y0), 1)
    for step in range(steps + 1):
        amount = step / steps
        x = round(x0 + (x1 - x0) * amount)
        y = round(y0 + (y1 - y0) * amount)
        for dy in range(-radius, radius + 1):
            for dx in range(-radius, radius + 1):
                if dx * dx + dy * dy <= radius * radius:
                    px, py = x + dx, y + dy
                    if 0 <= py < len(canvas) and 0 <= px < len(canvas[0]):
                        canvas[py][px] = color


def diagnostic(field: dict, mesh: dict, mode: str,
               width: int = 640, height: int = 480) -> list:
    vertices = mesh["mesh"]["vertices"]
    xs = [float(vertex["x"]) for vertex in vertices]
    zs = [float(vertex["z"]) for vertex in vertices]
    padding = 28
    x_span = max(xs) - min(xs)
    z_span = max(zs) - min(zs)

    def project(point: list[float]) -> tuple[int, int]:
        x = padding + (point[0] - min(xs)) / x_span * (width - 2 * padding)
        y = height - padding - ((point[2] - min(zs)) / z_span *
                                (height - 2 * padding))
        return round(x), round(y)

    canvas = [[(22, 24, 29) for _ in range(width)] for _ in range(height)]
    for segment in field["segments"]:
        if mode == "curve_id":
            color = color_id(int(segment["curve_id"]))
            radius = 3
        elif mode == "segment_id":
            color = color_id(int(segment["segment_id"]))
            radius = 2
        elif mode == "depth":
            maximum = max(float(item["depth_start"])
                          for item in field["segments"])
            amount = float(segment["depth_start"]) / maximum
            color = (round(40 + 80 * amount), round(90 + 120 * amount),
                     round(130 + 125 * amount))
            radius = max(1, round(float(segment["width_start"]) * 95))
        else:
            tangent = segment["tangent"]
            color = (round(128 + 127 * tangent[0]),
                     round(128 + 127 * tangent[1]),
                     round(128 + 127 * tangent[2]))
            radius = 2
        draw_line(canvas, project(segment["start"]), project(segment["end"]),
                  color, radius)
    return canvas


def render_clone(render_cli: Path, base_generated: Path, generated: Path,
                 review: Path, source_name: str, name: str,
                 field_path: Path | None) -> tuple[list, dict, Path]:
    scene = copy.deepcopy(read(base_generated / f"{source_name}.scene.json"))
    reference = scene["objects"][0]["procedural_solid_material_ref"]
    reference.pop("surface_feature_field_path", None)
    reference.pop("surface_feature_curve_field_path", None)
    if field_path is not None:
        reference["surface_feature_curve_field_path"] = str(
            field_path.resolve())
    scene["scene_id"] = f"psg24b_{name}"
    # Keep the accepted proof's scene-to-assets relative depth. The runtime
    # mesh registry resolves generated/assets beside this scene document.
    scene_path = generated / f"{name}.scene.json"
    write(scene_path, scene)

    request = copy.deepcopy(
        read(base_generated / "requests" / f"{source_name}.request.json"))
    raw = generated / "raw" / name
    request["run_id"] = f"psg24b_surface_curve_field_{name}"
    request["scene"]["runtime_scene_path"] = str(scene_path.resolve())
    request["output"]["root"] = str(raw.resolve())
    request["progress"]["summary_path"] = str(
        (raw / "render_summary.json").resolve())
    request["progress"]["progress_path"] = str(
        (raw / "render_progress.json").resolve())
    request_path = generated / "requests" / f"{name}.request.json"
    write(request_path, request)
    summary_path = raw / "render_summary.json"
    run_render_cli(render_cli, request_path, summary_path)
    summary = read(summary_path)
    frame = raw / "frames" / "frame_0000.bmp"
    width, height, pixels = review_artifacts.read_bmp_rgb(frame)
    png = review / f"{name}.png"
    png.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png, width, height, pixels)
    return pixels, object_audit(summary, "psg19_statue_object"), png


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-root", type=Path, default=DEFAULT_BASE)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--authoring", type=Path, default=(
        ROOT / "tests/fixtures/procedural_surface_feature_fields_psg24b/"
        "curved_plaster_scratches.authoring.json"))
    parser.add_argument("--render-cli", type=Path, default=(
        ROOT / "build/toolchains/clang/arm64/tools/cli/"
        "ray_tracing_render_headless"))
    parser.add_argument("--reuse-field", action="store_true",
                        help="reuse an already compiled field/bundle")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    base_summary = read(args.base_root / "proof_summary.json")
    if base_summary.get("status") != "passed":
        raise RuntimeError("accepted PSG-24A base proof is required")
    generated = args.output_root / "generated"
    review = args.output_root / "review"
    field_root = args.output_root / "field"
    mesh_path = (args.base_root / "generated/assets/mesh_assets/"
                 "psg24_closed_curved_plaster.runtime.json")
    solid_receipt = args.base_root / "generated/receipts/solid.json"
    if not args.reuse_field:
        run([
            sys.executable,
            str(ROOT / "tools/procedural_surface_feature_curve_authoring.py"),
            "--authoring", str(args.authoring),
            "--output-root", str(field_root),
            "--mesh", str(mesh_path),
            "--solid-receipt", str(solid_receipt),
        ])
    bundle_tool = Path(
        "/Users/calebsv/Desktop/CodeWork/tools/procedural_object_authoring/"
        "procedural_object_bundle.py")
    bundle = field_root / "bundle.json"
    if not args.reuse_field:
        run([sys.executable, str(bundle_tool), "compile", "--spec",
             str(field_root / "bundle.authoring.json"), "--output", str(bundle)])
    run([sys.executable, str(bundle_tool), "validate", "--bundle", str(bundle)])
    inspect_path = field_root / "bundle.inspect.json"
    completed = subprocess.run(
        [sys.executable, str(bundle_tool), "inspect", "--bundle", str(bundle)],
        text=True, capture_output=True, check=True)
    inspect_path.write_text(completed.stdout, encoding="utf-8")

    field_path = field_root / "assets/surface_feature_curve_field_v1.json"
    base_generated = args.base_root / "generated"
    shutil.copytree(base_generated / "assets", generated / "assets",
                    dirs_exist_ok=True)
    render_specs = [
        ("all_plaster_control", "scratch_control", None),
        ("plaster_concrete_layered", "scratch_beauty_hero", field_path),
        ("plaster_concrete_detail", "scratch_beauty_detail", field_path),
        ("plaster_concrete_grazing", "scratch_beauty_grazing", field_path),
        ("plaster_concrete_repeat", "scratch_beauty_repeat", field_path),
        ("feature_coverage_mask", "scratch_coverage", field_path),
        ("feature_interior_mask", "scratch_interior_floor", field_path),
        ("feature_rim_mask", "scratch_edge_rim", field_path),
    ]
    pixels: dict[str, list] = {}
    audits: dict[str, dict] = {}
    images: dict[str, str] = {}
    for source, name, active_field in render_specs:
        pixels[name], audits[name], path = render_clone(
            args.render_cli, base_generated, generated, review,
            source, name, active_field)
        images[name] = str(path)

    repeat_delta = changed_pixels(
        pixels["scratch_beauty_hero"], pixels["scratch_beauty_repeat"])
    control_delta = changed_pixels(
        pixels["scratch_control"], pixels["scratch_beauty_hero"])
    mask_delta = changed_pixels(
        pixels["scratch_interior_floor"], pixels["scratch_edge_rim"])
    repeat_difference = difference(
        pixels["scratch_beauty_hero"], pixels["scratch_beauty_repeat"])
    repeat_path = review / "exact_repeat_difference.png"
    review_artifacts.write_png_rgb(
        repeat_path, len(repeat_difference[0]), len(repeat_difference),
        repeat_difference)

    field = read(field_path)
    mesh = read(mesh_path)
    diagnostic_paths = {}
    for mode in ("curve_id", "segment_id", "depth", "direction"):
        diagnostic_pixels = diagnostic(field, mesh, mode)
        path = review / f"diagnostic_{mode}.png"
        review_artifacts.write_png_rgb(
            path, len(diagnostic_pixels[0]), len(diagnostic_pixels),
            diagnostic_pixels)
        diagnostic_paths[mode] = str(path)

    cell_width, cell_height = 320, 240
    sheet_cells = [
        ("CONTROL", resize(pixels["scratch_control"], cell_width, cell_height)),
        ("SCRATCH BEAUTY", resize(pixels["scratch_beauty_hero"], cell_width, cell_height)),
        ("DETAIL", resize(pixels["scratch_beauty_detail"], cell_width, cell_height)),
        ("GRAZING", resize(pixels["scratch_beauty_grazing"], cell_width, cell_height)),
        ("COVERAGE", resize(pixels["scratch_coverage"], cell_width, cell_height)),
        ("INTERIOR FLOOR", resize(pixels["scratch_interior_floor"], cell_width, cell_height)),
        ("EDGE RIM", resize(pixels["scratch_edge_rim"], cell_width, cell_height)),
        ("EXACT REPEAT DIFF", resize(repeat_difference, cell_width, cell_height)),
        ("CURVE ID", resize(diagnostic(field, mesh, "curve_id"), cell_width, cell_height)),
        ("SEGMENT ID", resize(diagnostic(field, mesh, "segment_id"), cell_width, cell_height)),
        ("SIGNED DEPTH", resize(diagnostic(field, mesh, "depth"), cell_width, cell_height)),
        ("TANGENT DIRECTION", resize(diagnostic(field, mesh, "direction"), cell_width, cell_height)),
    ]
    contact_sheet = review / "psg24b_surface_curve_fields_contact_sheet.png"
    write_labeled_contact_sheet(contact_sheet, sheet_cells, columns=4)

    receipt = read(
        field_root / "receipts/surface_feature_curve_field.receipt.json")
    triangles = {int(audit["triangle_count"]) for audit in audits.values()}
    identity_views = (
        "scratch_control", "scratch_beauty_hero", "scratch_beauty_repeat",
        "scratch_coverage", "scratch_interior_floor", "scratch_edge_rim")
    primary_hit_counts = {
        int(audits[name]["primary_hit_pixels"]) for name in identity_views}
    coverage = receipt["coverage"]
    gates = {
        "curve_and_branch_populations_positive": (
            receipt["curve_count"] > 0 and receipt["branch_curve_count"] > 0),
        "eligible_coverage_at_least_three_percent": (
            coverage["eligible_measured"] >= 0.03),
        "clean_base_at_least_forty_five_percent": (
            coverage["clean_base_measured"] >= 0.45),
        "interior_and_rim_distinct": mask_delta > 0,
        "curve_continuity": (
            receipt["continuity"]["maximum_consecutive_endpoint_gap"] <= 1e-6),
        "profile_error_bounded": (
            receipt["profile_accuracy"]["maximum_width_interpolation_error"] <= 1e-6 and
            receipt["profile_accuracy"]["maximum_depth_profile_error"] <= 1e-6),
        "provenance_and_frames_valid": all((
            receipt["provenance"]["all_source_triangles_in_range"],
            receipt["provenance"]["stable_curve_ids_nonzero"],
            receipt["provenance"]["stable_segment_ids_unique"],
            receipt["provenance"]["maximum_tangent_frame_error"] <= 1e-6)),
        "opposing_fold_assignments_zero": (
            receipt["normal_compatibility"]
            ["opposing_fold_incompatible_assignments"] == 0),
        "candidate_search_bounded": (
            not receipt["candidate_search"]["full_scan_permitted"] and
            receipt["candidate_search"]["observed_max_cell_candidates"] <=
            receipt["candidate_search"]["max_candidates_per_hit"]),
        "exact_repeat_render_zero_pixels": repeat_delta == 0,
        "material_and_normal_influence_visible": control_delta > 0,
        "geometry_identity_unchanged": triangles == {3968},
        "primary_hit_coverage_unchanged": len(primary_hit_counts) == 1,
        "beauty_resolution_at_least_1200x900": (
            len(pixels["scratch_beauty_hero"]) >= 900 and
            len(pixels["scratch_beauty_hero"][0]) >= 1200),
    }
    summary = {
        "schema": "psg24b_surface_curve_field_proof_summary_v1",
        "status": "passed" if all(gates.values()) else "failed",
        "gates": gates,
        "metrics": {
            "curve_count": receipt["curve_count"],
            "branch_curve_count": receipt["branch_curve_count"],
            "segment_count": receipt["segment_count"],
            "eligible_coverage": coverage["eligible_measured"],
            "clean_base": coverage["clean_base_measured"],
            "control_to_beauty_changed_pixels": control_delta,
            "interior_to_rim_changed_pixels": mask_delta,
            "repeat_changed_pixels": repeat_delta,
            "observed_triangle_counts": sorted(triangles),
            "same_camera_primary_hit_pixel_counts": sorted(primary_hit_counts),
            "beauty_image_metrics": image_metrics(
                pixels["scratch_beauty_hero"]),
        },
        "field_receipt": str((field_root / "receipts/"
                              "surface_feature_curve_field.receipt.json")),
        "bundle": str(bundle),
        "bundle_inspect": str(inspect_path),
        "images": images,
        "diagnostics": diagnostic_paths,
        "exact_repeat_difference": str(repeat_path),
        "contact_sheet": str(contact_sheet),
        "claim_boundary": (
            "Material and PSG-17-style shading-normal grooves only; source "
            "topology, silhouette, acceleration, and hit coverage remain fixed. "
            "No PSG-24C physical inset or PSG-24D attached geometry claim."),
    }
    write(args.output_root / "proof_summary.json", summary)
    print(json.dumps({"status": summary["status"],
                      "proof_summary": str(args.output_root / "proof_summary.json"),
                      "contact_sheet": str(contact_sheet)}, sort_keys=True))
    return 0 if summary["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
