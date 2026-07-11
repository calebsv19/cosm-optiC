#!/usr/bin/env python3
"""Run a water IOR refraction A/B fixture against a patterned receiver."""

from __future__ import annotations

import argparse
import json
import math
import os
import platform
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import run_ray_tracing_aquarium_transparent_receiver_fixture as receiver_fixture  # noqa: E402


WATER_IOR_PHYSICAL = 1.333
WATER_IOR_STRAIGHT_THROUGH = 1.0

VARIANTS = (
    {
        "id": "pattern_control",
        "label": "Pattern control",
        "include_glass": False,
        "include_water": False,
        "water_ior": None,
    },
    {
        "id": "water_ior_1000",
        "label": "Water IOR 1.000",
        "include_glass": False,
        "include_water": True,
        "water_ior": WATER_IOR_STRAIGHT_THROUGH,
    },
    {
        "id": "water_ior_1333",
        "label": "Water IOR 1.333",
        "include_glass": False,
        "include_water": True,
        "water_ior": WATER_IOR_PHYSICAL,
    },
    {
        "id": "glass_water_ior_1000",
        "label": "Glass + water IOR 1.000",
        "include_glass": True,
        "include_water": True,
        "water_ior": WATER_IOR_STRAIGHT_THROUGH,
    },
    {
        "id": "glass_water_ior_1333",
        "label": "Glass + water IOR 1.333",
        "include_glass": True,
        "include_water": True,
        "water_ior": WATER_IOR_PHYSICAL,
    },
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def workspace_root() -> Path:
    return repo_root().parent


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = (
        root
        / "build"
        / "toolchains"
        / "clang"
        / machine
        / "tools"
        / "cli"
        / "ray_tracing_render_headless"
    )
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def default_review_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "aquarium_water_ior_fixture"
    )


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--review-root", type=Path, default=default_review_root())
    parser.add_argument("--water-root", type=Path, default=receiver_fixture.default_water_root())
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--keep-going", action="store_true")
    return parser.parse_args()


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def build_pattern_scene(include_glass: bool) -> dict:
    scene = receiver_fixture.build_scene(include_glass)
    objects = scene["objects"]
    materials = scene["extensions"]["ray_tracing"]["authoring"]["object_materials"]

    for obj in objects:
        if obj.get("object_id") == "receiver_bright_orange_block":
            replacement = receiver_fixture.plane_object(
                "receiver_bright_orange_block",
                {"x": 0.0, "y": 0.02, "z": -0.30},
                {"x": 1.0, "y": 0.0, "z": 0.0},
                {"x": 0.0, "y": 1.0, "z": 0.0},
                {"x": 0.0, "y": 0.0, "z": 1.0},
                3.05,
                2.25,
            )
            obj.clear()
            obj.update(replacement)

    stripe_count = 17
    stripe_width = 0.035
    stripe_height = 2.25
    spacing = 0.145
    start_x = -1.16
    for index in range(stripe_count):
        x = start_x + (spacing * index)
        stripe_id = f"receiver_pattern_dark_stripe_{index:02d}"
        objects.append(
            receiver_fixture.plane_object(
                stripe_id,
                {"x": x, "y": 0.02, "z": -0.295},
                {"x": 1.0, "y": 0.0, "z": 0.0},
                {"x": 0.0, "y": 1.0, "z": 0.0},
                {"x": 0.0, "y": 0.0, "z": 1.0},
                stripe_width,
                stripe_height,
            )
        )
        materials.append(
            {
                "object_id": stripe_id,
                "material_id": 0,
                "object_color": receiver_fixture.rgb_u24(14, 16, 18),
                "alpha": 1.0,
                "roughness": 0.35,
                "reflectivity": 0.02,
                "emissive_strength": 0.0,
            }
        )

    scene["scene_id"] = "aquarium_water_ior_fixture"
    return scene


def clone_water_source_with_ior(source_root: Path, dest_root: Path, ior: float) -> Path:
    _ = source_root
    if dest_root.exists():
        shutil.rmtree(dest_root)
    dest_root.mkdir(parents=True, exist_ok=True)
    grid_w = 48
    grid_d = 32
    sample_origin_x = -1.85
    sample_origin_z = -1.25
    span_x = 3.70
    span_z = 2.50
    spacing_x = span_x / float(grid_w - 1)
    spacing_z = span_z / float(grid_d - 1)
    heights: list[float] = []
    normals: list[float] = []
    for z in range(grid_d):
        for x in range(grid_w):
            u = x / float(grid_w - 1)
            v = z / float(grid_d - 1)
            ripple = (
                0.30 * math.sin((u * math.tau * 1.15) + (v * math.tau * 0.35))
                + 0.20 * math.sin((v * math.tau * 1.70) - (u * math.tau * 0.25))
                + 0.42 * (u - 0.5)
            )
            heights.append(1.22 + ripple)
            normals.extend([0.0, 0.0, 1.0])
    surface_min = min(heights)
    surface_max = max(heights)
    surface_avg = sum(heights) / float(len(heights))
    surface = {
        "schema": "physics_sim_water_surface_heightfield_v1",
        "version": 1,
        "frame_index": 200,
        "time_seconds": 2.2333,
        "dt_seconds": 0.0111,
        "surface_representation": "heightfield",
        "layout": "row_major_z_x",
        "surface_axis": "y",
        "height_units": "meters",
        "grid_w": grid_w,
        "grid_d": grid_d,
        "sample_count": grid_w * grid_d,
        "volume_grid_w": grid_w,
        "volume_grid_h": 12,
        "volume_grid_d": grid_d,
        "origin_x": 0.0,
        "origin_y": 0.0,
        "origin_z": 0.0,
        "sample_origin_x": sample_origin_x,
        "sample_origin_z": sample_origin_z,
        "sample_spacing_x": spacing_x,
        "sample_spacing_z": spacing_z,
        "density_threshold": 0.5,
        "summary": {
            "wet_columns": grid_w * grid_d,
            "dry_columns": 0,
            "solid_columns": 0,
            "water_cells": grid_w * grid_d * 12,
            "surface_min_y": surface_min,
            "surface_max_y": surface_max,
            "surface_avg_y": surface_avg,
            "max_slope": 0.18,
            "finite_normals": True,
            "fixture_mode": "full_window_ior_refraction",
        },
        "heights_y": heights,
        "normals_xyz": normals,
    }
    write_json(dest_root / "water_surface_000200.json", surface)
    scene_bundle = {
        "bundle_type": "physics_scene_bundle_v1",
        "bundle_version": 1,
        "profile": "physics",
        "water_source": {
            "kind": "water_manifest",
            "path": "water_manifest_v1.json",
            "contract": "water_manifest_v1",
            "surface_representation": "heightfield",
        },
        "scene_metadata": {"asset_mapping_profile": "water_ior_fixture"},
    }
    write_json(dest_root / "scene_bundle.json", scene_bundle)
    manifest = {
        "schema": "physics_sim_water_manifest_v1",
        "version": 1,
        "mode": "water",
        "surface_representation": "heightfield",
        "surface_axis": "y",
        "height_units": "meters",
        "frame_contract": "water_surface_heightfield_v1",
        "grid_w": grid_w,
        "grid_d": grid_d,
        "volume_grid_w": grid_w,
        "volume_grid_h": 12,
        "volume_grid_d": grid_d,
        "origin_x": 0.0,
        "origin_y": 0.0,
        "origin_z": 0.0,
        "voxel_size": 0.05,
        "scene_up_x": 0.0,
        "scene_up_y": 1.0,
        "scene_up_z": 0.0,
        "density_threshold": 0.5,
        "preset": f"Water IOR {ior:.3f} refraction fixture",
        "review_surface": {
            "mode": "full_window_ior_refraction",
            "enabled": True,
            "configured_amplitude_m": 0.92,
        },
        "material": {
            "ior": float(ior),
            "absorption_distance_m": 8.0,
            "absorption_rgb": [0.0, 0.0, 0.0],
            "reflectivity": 0.02,
            "roughness": 0.02,
        },
        "frames": [
            {
                "frame_index": 200,
                "time_seconds": 2.2333,
                "dt_seconds": 0.0111,
                "path": "water_surface_000200.json",
                "frame_contract": "water_surface_heightfield_v1",
                "wet_columns": grid_w * grid_d,
                "dry_columns": 0,
                "solid_columns": 0,
                "water_cells": grid_w * grid_d * 12,
                "surface_min_y": surface_min,
                "surface_max_y": surface_max,
                "surface_avg_y": surface_avg,
                "max_slope": 0.18,
                "finite_normals": True,
            }
        ],
    }
    manifest_path = dest_root / "water_manifest_v1.json"
    write_json(manifest_path, manifest)
    return dest_root


def render_request(cli: Path, request_path: Path, summary_path: Path, skip_render: bool) -> float | None:
    if skip_render:
        return None
    stdout_path = summary_path.parent / "stdout_summary.json"
    stderr_path = summary_path.parent / "stderr.txt"
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env["RAY_TRACING_RENDER_TRACE_COST_LEDGER"] = "1"
    start = time.perf_counter()
    with stdout_path.open("w", encoding="utf-8") as stdout:
        result = subprocess.run(
            [str(cli), "--request", str(request_path), "--render", "--summary", str(summary_path)],
            stdout=stdout,
            stderr=subprocess.PIPE,
            env=env,
            text=True,
        )
    elapsed = time.perf_counter() - start
    stderr_path.write_text(result.stderr or "", encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(
            f"render failed for {request_path} with exit {result.returncode}; stderr: {stderr_path}"
        )
    return elapsed


def first_frame_path(summary: dict) -> Path:
    return receiver_fixture.first_frame_path(summary)


def copy_frame_png(summary: dict, review_root: Path, variant_id: str) -> tuple[Path, Path]:
    return receiver_fixture.copy_frame_png(summary, review_root, variant_id)


def receiver_roi(width: int, height: int) -> tuple[int, int, int, int]:
    return (
        max(0, int(width * 0.18)),
        max(0, int(height * 0.20)),
        min(width, int(width * 0.82)),
        min(height, int(height * 0.83)),
    )


def orange_score(pixel: tuple[int, int, int]) -> float:
    r, g, b = pixel
    return float(r) - (0.65 * float(g)) - (0.35 * float(b))


def pattern_metrics(frame_path: Path) -> dict:
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    x0, y0, x1, y1 = receiver_roi(width, height)
    orange_scores: list[float] = []
    dark_count = 0
    orange_count = 0
    weighted_x = 0.0
    weight_sum = 0.0
    column_dark_counts = [0 for _ in range(max(0, x1 - x0))]
    for y in range(y0, y1):
        for x in range(x0, x1):
            pixel = pixels[y][x]
            score = orange_score(pixel)
            orange_scores.append(score)
            r, g, b = pixel
            luma = (0.2126 * r) + (0.7152 * g) + (0.0722 * b)
            if score > 20.0:
                orange_count += 1
                weighted_x += float(x) * score
                weight_sum += score
            if luma < 34.0:
                dark_count += 1
                column_dark_counts[x - x0] += 1
    orange_scores.sort()
    count = max(1, len(orange_scores))
    dark_columns = [
        x0 + i
        for i, value in enumerate(column_dark_counts)
        if value >= max(3, int((y1 - y0) * 0.12))
    ]
    return {
        "frame_path": str(frame_path),
        "image_width": width,
        "image_height": height,
        "receiver_roi": {"x0": x0, "y0": y0, "x1": x1, "y1": y1, "pixel_count": count},
        "orange_score_p95": orange_scores[min(count - 1, int(math.floor(0.95 * (count - 1))))],
        "orange_pixel_count": orange_count,
        "dark_pixel_count": dark_count,
        "orange_centroid_x": weighted_x / weight_sum if weight_sum > 1e-9 else -1.0,
        "dark_column_count": len(dark_columns),
        "dark_column_first": dark_columns[0] if dark_columns else -1,
        "dark_column_last": dark_columns[-1] if dark_columns else -1,
    }


def compare_frames(reference_path: Path, candidate_path: Path) -> dict:
    ref_w, ref_h, ref_pixels = review_artifacts.read_bmp_rgb(reference_path)
    cand_w, cand_h, cand_pixels = review_artifacts.read_bmp_rgb(candidate_path)
    if ref_w != cand_w or ref_h != cand_h:
        raise ValueError("comparison images have different dimensions")
    x0, y0, x1, y1 = receiver_roi(ref_w, ref_h)
    total_delta = 0.0
    high_delta = 0
    count = 0
    signed_orange_delta = 0.0
    for y in range(y0, y1):
        for x in range(x0, x1):
            rp = ref_pixels[y][x]
            cp = cand_pixels[y][x]
            delta = (abs(cp[0] - rp[0]) + abs(cp[1] - rp[1]) + abs(cp[2] - rp[2])) / 3.0
            total_delta += delta
            if delta >= 8.0:
                high_delta += 1
            signed_orange_delta += orange_score(cp) - orange_score(rp)
            count += 1
    count = max(1, count)
    ref_metrics = pattern_metrics(reference_path)
    cand_metrics = pattern_metrics(candidate_path)
    return {
        "reference": str(reference_path),
        "candidate": str(candidate_path),
        "mean_abs_delta": total_delta / float(count),
        "high_delta_pixel_ratio": high_delta / float(count),
        "signed_orange_delta_mean": signed_orange_delta / float(count),
        "orange_centroid_shift_px": cand_metrics["orange_centroid_x"] -
        ref_metrics["orange_centroid_x"],
        "dark_column_count_delta": cand_metrics["dark_column_count"] -
        ref_metrics["dark_column_count"],
    }


def ledger_digest(summary: dict) -> dict:
    return receiver_fixture.ledger_digest(summary)


def object_audit_digest(summary: dict) -> dict:
    return receiver_fixture.object_audit_digest(summary)


def make_contact_sheet(review_root: Path, runs: list[dict]) -> Path:
    images = []
    for run in runs:
        width, height, pixels = review_artifacts.read_bmp_rgb(Path(run["frame_path"]))
        images.append((width, height, pixels, run))
    if not images:
        raise ValueError("no images for contact sheet")
    tile_w = max(width for width, _, _, _ in images)
    tile_h = max(height for _, height, _, _ in images)
    label_h = 34
    gap = 10
    sheet_w = len(images) * tile_w + (len(images) + 1) * gap
    sheet_h = tile_h + label_h + gap * 2
    bg = (246, 246, 244)
    sheet = [[bg for _ in range(sheet_w)] for _ in range(sheet_h)]
    for index, (width, height, pixels, _run) in enumerate(images):
        x0 = gap + index * (tile_w + gap)
        y0 = gap + label_h
        for y in range(height):
            for x in range(width):
                sheet[y0 + y][x0 + x] = pixels[y][x]
    path = review_root / "aquarium_water_ior_fixture_sheet.png"
    review_artifacts.write_png_rgb(path, sheet_w, sheet_h, sheet)
    return path


def validate_run(variant: dict, run: dict) -> list[str]:
    failures: list[str] = []
    metrics = run["metrics"]
    ledger = run["ledger"]
    objects = run["object_audit"]
    if metrics["orange_score_p95"] <= 20.0:
        failures.append("pattern target orange signal too weak")
    if metrics["dark_column_count"] < 2:
        failures.append("pattern target stripes not visible")
    if variant["include_water"]:
        if "<generated>" not in objects:
            failures.append("water variant did not audit generated water surface")
        if ledger["transmission_rays"] <= 0:
            failures.append("water variant recorded zero transmission rays")
        if ledger["receiver_hits"] <= 0:
            failures.append("water variant recorded zero receiver hits")
    if variant["include_glass"] and "front_glass_test_pane" not in objects:
        failures.append("glass variant did not audit front glass pane")
    return failures


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Aquarium Water IOR Fixture",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- contact sheet: `{Path(report['contact_sheet']).name}`",
        f"- report: `{path.name.replace('_index.md', '_report.json')}`",
        "",
        "## Runs",
        "",
    ]
    for run in report["runs"]:
        metrics = run.get("metrics", {})
        lines.append(
            f"- `{run['variant_id']}`: water IOR `{run.get('water_ior')}`, "
            f"orange p95 `{metrics.get('orange_score_p95', 0.0):.3f}`, "
            f"dark columns `{metrics.get('dark_column_count', 0)}`"
        )
    if report["comparisons"]:
        lines.extend(["", "## Comparisons", ""])
        for item in report["comparisons"]:
            lines.append(
                f"- `{item['id']}`: mean delta `{item['mean_abs_delta']:.3f}`, "
                f"high-delta ratio `{item['high_delta_pixel_ratio']:.3f}`, "
                f"centroid shift `{item['orange_centroid_shift_px']:.3f}px`"
            )
    if report["failures"]:
        lines.extend(["", "## Failures", ""])
        for failure in report["failures"]:
            lines.append(f"- {failure}")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    review_root = args.review_root.resolve()
    water_root = args.water_root.resolve()
    if not args.skip_render and not args.cli.exists():
        raise FileNotFoundError(f"missing ray_tracing_render_headless binary: {args.cli}")
    if not water_root.joinpath("scene_bundle.json").exists():
        raise FileNotFoundError(f"missing water scene bundle: {water_root / 'scene_bundle.json'}")

    if review_root.exists():
        shutil.rmtree(review_root)
    review_root.mkdir(parents=True, exist_ok=True)

    runs: list[dict] = []
    failures: list[str] = []
    for variant in VARIANTS:
        variant_root = review_root / "runs" / variant["id"]
        scene_path = variant_root / "scene_runtime.json"
        request_path = variant_root / "request.json"
        summary_path = variant_root / "render" / "render_summary.json"
        variant_water_root = water_root
        if variant["include_water"]:
            variant_water_root = clone_water_source_with_ior(
                water_root,
                variant_root / "water_source",
                float(variant["water_ior"]),
            )
        scene = build_pattern_scene(bool(variant["include_glass"]))
        request = receiver_fixture.base_request(
            variant,
            scene_path,
            variant_root / "render",
            summary_path,
            variant_water_root,
        )
        request["render"]["transmission_samples_3d"] = 4
        request["inspection"]["camera_position"] = {"x": 0.0, "y": -2.85, "z": 2.70}
        request["inspection"]["camera_look_at"] = {"x": 0.0, "y": 0.02, "z": -0.06}
        request["inspection"]["camera_zoom"] = 0.90
        write_json(scene_path, scene)
        write_json(request_path, request)
        elapsed = render_request(args.cli, request_path, summary_path, args.skip_render)
        if args.skip_render:
            runs.append(
                {
                    "variant_id": variant["id"],
                    "label": variant["label"],
                    "water_ior": variant["water_ior"],
                    "scene_path": str(scene_path),
                    "request_path": str(request_path),
                    "render_seconds": elapsed,
                    "skipped": True,
                }
            )
            continue
        summary = load_json(summary_path)
        frame_path, png_path = copy_frame_png(summary, review_root, variant["id"])
        run = {
            "variant_id": variant["id"],
            "label": variant["label"],
            "water_ior": variant["water_ior"],
            "scene_path": str(scene_path),
            "request_path": str(request_path),
            "summary_path": str(summary_path),
            "frame_path": str(frame_path),
            "png_path": str(png_path),
            "render_seconds": elapsed,
            "skipped": False,
            "ledger": ledger_digest(summary),
            "object_audit": object_audit_digest(summary),
            "metrics": pattern_metrics(frame_path),
        }
        run_failures = validate_run(variant, run)
        failures.extend(f"{variant['id']}: {failure}" for failure in run_failures)
        runs.append(run)
        if run_failures and not args.keep_going:
            break

    comparisons: list[dict] = []
    run_by_id = {run["variant_id"]: run for run in runs if not run.get("skipped")}
    for comparison_id, reference_id, candidate_id in (
        ("water_ior_1000_vs_1333", "water_ior_1000", "water_ior_1333"),
        ("glass_water_ior_1000_vs_1333", "glass_water_ior_1000", "glass_water_ior_1333"),
    ):
        if reference_id in run_by_id and candidate_id in run_by_id:
            item = compare_frames(
                Path(run_by_id[reference_id]["frame_path"]),
                Path(run_by_id[candidate_id]["frame_path"]),
            )
            item["id"] = comparison_id
            item["reference_variant_id"] = reference_id
            item["candidate_variant_id"] = candidate_id
            comparisons.append(item)
            if item["mean_abs_delta"] <= 0.5 and item["high_delta_pixel_ratio"] <= 0.002:
                failures.append(f"{comparison_id}: IOR comparison produced negligible image delta")

    contact_sheet = ""
    if not args.skip_render:
        contact_sheet = str(make_contact_sheet(review_root, [run for run in runs if not run["skipped"]]))
    report = {
        "schema": "codework_aquarium_water_ior_fixture_report_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "review_root": str(review_root),
        "source_water_root": str(water_root),
        "contact_sheet": contact_sheet,
        "passed": not failures and not args.skip_render,
        "failures": failures,
        "comparisons": comparisons,
        "runs": runs,
    }
    report_path = review_root / "aquarium_water_ior_fixture_report.json"
    write_json(report_path, report)
    write_index(review_root / "aquarium_water_ior_fixture_index.md", report)
    if failures:
        print(f"fixture failed with {len(failures)} failure(s); report: {report_path}", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"fixture report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
