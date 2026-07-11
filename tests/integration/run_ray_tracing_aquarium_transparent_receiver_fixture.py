#!/usr/bin/env python3
"""Run a tiny aquarium transparent-receiver fixture matrix."""

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


VARIANTS = (
    {
        "id": "receiver_only",
        "label": "Receiver only",
        "include_glass": False,
        "include_water": False,
    },
    {
        "id": "water_only",
        "label": "Water only",
        "include_glass": False,
        "include_water": True,
    },
    {
        "id": "glass_only",
        "label": "Glass only",
        "include_glass": True,
        "include_water": False,
    },
    {
        "id": "glass_water",
        "label": "Glass + water",
        "include_glass": True,
        "include_water": True,
    },
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def workspace_root() -> Path:
    return repo_root().parent


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = root / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless"
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def aquarium_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "physics_trio"
        / "aquarium_glass_room_v1"
    )


def default_water_root() -> Path:
    return (
        aquarium_root()
        / "ray_tracing_ap_diagnostic_matrix_20260710c"
        / "water_uncoupled"
        / "Water Basin"
    )


def default_review_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "aquarium_transparent_receiver_fixture"
    )


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--review-root", type=Path, default=default_review_root())
    parser.add_argument("--water-root", type=Path, default=default_water_root())
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


def rgb_u24(r: int, g: int, b: int) -> int:
    return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF)


def plane_object(object_id: str, origin: dict, axis_u: dict, axis_v: dict, normal: dict,
                 width: float, height: float) -> dict:
    return {
        "object_id": object_id,
        "object_type": "plane_primitive",
        "space_mode_intent": "3d",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": origin,
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "flags": {"visible": True, "locked": False, "selectable": True},
        "primitive": {
            "kind": "plane_primitive",
            "width": width,
            "height": height,
            "lock_to_construction_plane": False,
            "lock_to_bounds": False,
            "frame": {
                "origin": origin,
                "axis_u": axis_u,
                "axis_v": axis_v,
                "normal": normal,
            },
        },
    }


def prism_object(object_id: str, position: dict, width: float, height: float, depth: float) -> dict:
    return {
        "object_id": object_id,
        "object_type": "rect_prism_primitive",
        "space_mode_intent": "3d",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": position,
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "flags": {"visible": True, "locked": False, "selectable": True},
        "primitive": {
            "kind": "rect_prism_primitive",
            "width": width,
            "height": height,
            "depth": depth,
            "lock_to_construction_plane": False,
            "lock_to_bounds": False,
        },
    }


def build_scene(include_glass: bool) -> dict:
    objects = [
        plane_object(
            "receiver_bright_orange_block",
            {"x": 0.0, "y": -0.12, "z": 0.82},
            {"x": 1.0, "y": 0.0, "z": 0.0},
            {"x": 0.0, "y": 0.0, "z": 1.0},
            {"x": 0.0, "y": -1.0, "z": 0.0},
            2.45,
            1.35,
        ),
    ]
    if include_glass:
        glass = prism_object(
            "front_glass_test_pane",
            {"x": 0.0, "y": -0.95, "z": 0.78},
            3.75,
            1.55,
            0.035,
        )
        glass["primitive"]["frame"] = {
            "origin": {"x": 0.0, "y": -0.95, "z": 0.78},
            "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
            "axis_v": {"x": 0.0, "y": 0.0, "z": 1.0},
            "normal": {"x": 0.0, "y": -1.0, "z": 0.0},
        }
        objects.append(glass)

    object_materials = [
        {
            "object_id": "receiver_bright_orange_block",
            "material_id": 0,
            "object_color": rgb_u24(255, 104, 18),
            "alpha": 1.0,
            "roughness": 0.22,
            "reflectivity": 0.04,
            "emissive_strength": 0.65,
        },
    ]
    if include_glass:
        object_materials.append(
            {
                "object_id": "front_glass_test_pane",
                "material_id": 5,
                "object_color": rgb_u24(190, 235, 255),
                "alpha": 0.28,
                "glass_transport_override": True,
                "glass_transmission": 0.96,
                "glass_ior": 1.45,
                "glass_absorption_distance": 6.0,
                "glass_thin_walled": True,
                "roughness": 0.01,
                "reflectivity": 0.02,
            }
        )

    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "aquarium_transparent_receiver_fixture",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": objects,
        "materials": [],
        "lights": [],
        "cameras": [],
        "constraints": [],
        "hierarchy": [],
        "extensions": {
            "line_drawing": {
                "scene3d": {
                    "bounds": {
                        "enabled": True,
                        "clamp_on_edit": True,
                        "min": {"x": -3.0, "y": -2.0, "z": -0.5},
                        "max": {"x": 3.0, "y": 2.0, "z": 3.0},
                    }
                }
            },
            "ray_tracing": {
                "authoring": {
                    "environment": {
                        "light_mode": 2,
                        "ambient_strength": 0.28,
                        "top_fill_strength": 0.25,
                    },
                    "light_settings": {"intensity": 1.4, "radius": 0.45},
                    "object_materials": object_materials,
                }
            },
        },
    }


def base_request(variant: dict, scene_path: Path, output_root: Path, summary_path: Path,
                 water_root: Path) -> dict:
    request = {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": variant["id"],
        "scene": {"runtime_scene_path": str(scene_path)},
        "volume": {
            "enabled": bool(variant["include_water"]),
            "visible": False,
            "affects_lighting": False,
            "debug_overlay": False,
        },
        "render": {
            "start_frame": 200,
            "frame_count": 1,
            "width": 240,
            "height": 135,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "integrator_3d": "disney_v2",
            "transmission_samples_3d": 2,
            "secondary_diffuse_samples_3d": 2,
            "denoise_enabled": False,
        },
        "inspection": {
            "trace_route": "tlas_blas",
            "camera_position": {"x": 0.0, "y": -3.35, "z": 0.86},
            "camera_look_at": {"x": 0.0, "y": -0.12, "z": 0.82},
            "camera_zoom": 1.18,
            "environment_light_mode": "ambient",
            "environment_brightness": 0.02,
            "background_brightness": 0.0,
            "background_color": {"r": 0.0, "g": 0.0, "b": 0.0},
            "ambient_strength": 0.28,
            "top_fill_strength": 0.25,
            "light_intensity": 1.4,
            "light_radius": 0.45,
            "forward_decay": 160.0,
            "caustic_mode": "off",
            "caustic_sidecar_enabled": False,
            "caustic_sidecar_strength": 0.0,
            "object_audit_enabled": True,
            "object_audit_max_dimension": 64,
        },
        "output": {"root": str(output_root), "overwrite": True},
        "progress": {
            "summary_path": str(summary_path),
            "progress_path": str(output_root / "render_progress.json"),
        },
    }
    if variant["include_water"]:
        request["volume"].update(
            {
                "source_kind": "scene_bundle",
                "source_path": str(water_root / "scene_bundle.json"),
            }
        )
    return request


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
    frame_path = Path(summary.get("outputs", {}).get("first_frame_path", ""))
    if frame_path.exists():
        return frame_path
    return Path(summary.get("output_root", "")) / "frames" / "frame_0200.bmp"


def luma(pixel: tuple[int, int, int]) -> float:
    r, g, b = pixel
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def receiver_metrics(frame_path: Path) -> dict:
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    # Tight central-left ROI around the orange receiver in this fixed fixture.
    x0 = max(0, int(width * 0.30))
    x1 = min(width, int(width * 0.58))
    y0 = max(0, int(height * 0.34))
    y1 = min(height, int(height * 0.70))
    orange_scores: list[float] = []
    lumas: list[float] = []
    orange_pixels = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b = pixels[y][x]
            orange = float(r) - (0.65 * float(g)) - (0.35 * float(b))
            orange_scores.append(orange)
            lumas.append(luma((r, g, b)))
            if r > 120 and r > g * 1.35 and r > b * 1.8:
                orange_pixels += 1
    if not lumas:
        raise ValueError(f"{frame_path}: empty receiver ROI")
    lumas.sort()
    orange_scores.sort()
    count = len(lumas)
    return {
        "frame_path": str(frame_path),
        "image_width": width,
        "image_height": height,
        "receiver_roi": {
            "x0": x0,
            "y0": y0,
            "x1": x1,
            "y1": y1,
            "pixel_count": count,
        },
        "receiver_luma_mean": sum(lumas) / float(count),
        "receiver_luma_p95": lumas[min(count - 1, int(math.floor(0.95 * float(count - 1))))],
        "receiver_luma_max": lumas[-1],
        "receiver_orange_score_p95": orange_scores[min(count - 1, int(math.floor(0.95 * float(count - 1))))],
        "receiver_orange_pixel_count": orange_pixels,
        "receiver_orange_area_ratio": orange_pixels / float(count),
    }


def copy_frame_png(summary: dict, review_root: Path, variant_id: str) -> tuple[Path, Path]:
    frame_path = first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{variant_id}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return frame_path, png_path


def ledger_digest(summary: dict) -> dict:
    ledger = summary.get("render_trace_cost_ledger", {})
    transmission = ledger.get("transmission_path_policy", {})
    return {
        "transmission_rays": int(ledger.get("ray_class_counts", {}).get("transmission", 0)),
        "path_evaluations": int(transmission.get("path_evaluations", 0)),
        "requested_samples": int(transmission.get("requested_samples", 0)),
        "sample_evaluations": int(transmission.get("sample_evaluations", 0)),
        "contributing_samples": int(transmission.get("contributing_samples", 0)),
        "receiver_hits": int(transmission.get("receiver_hits", 0)),
        "ior_diagnostics": transmission.get("ior_diagnostics", {}),
        "receiver_object_hits": transmission.get("receiver_object_hits", []),
        "receiver_samples": int(transmission.get("receiver_samples", 0)),
        "transparent_surface_hits": int(transmission.get("transparent_surface_hits", 0)),
        "avg_transparent_surfaces_per_sample": float(
            transmission.get("avg_transparent_surfaces_per_sample", 0.0)
        ),
        "max_transparent_surfaces_in_sample": int(
            transmission.get("max_transparent_surfaces_in_sample", 0)
        ),
        "termination_counts": transmission.get("termination_counts", {}),
        "contribution_bucket_counts": transmission.get("contribution_bucket_counts", {}),
        "transparent_surface_material_counts": transmission.get(
            "transparent_surface_material_counts",
            {},
        ),
        "surface_kind_material_counts": transmission.get("surface_kind_material_counts", {}),
    }


def object_audit_digest(summary: dict) -> dict:
    result: dict[str, dict] = {}
    for entry in summary.get("object_audit", []):
        object_id = entry.get("object_id") or "<generated>"
        result[object_id] = {
            "scene_object_index": entry.get("scene_object_index", -1),
            "object_type": entry.get("object_type", ""),
            "material_id": entry.get("material_id", -1),
            "alpha": entry.get("alpha", 1.0),
            "triangle_count": entry.get("triangle_count", 0),
            "primary_hit_pixels": entry.get("primary_hit_pixels", 0),
        }
    return result


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
    for index, (width, height, pixels, run) in enumerate(images):
        x0 = gap + index * (tile_w + gap)
        y0 = gap + label_h
        for y in range(height):
            for x in range(width):
                sheet[y0 + y][x0 + x] = pixels[y][x]
    path = review_root / "aquarium_transparent_receiver_fixture_sheet.png"
    review_artifacts.write_png_rgb(path, sheet_w, sheet_h, sheet)
    return path


def validate_run(variant: dict, run: dict) -> list[str]:
    failures: list[str] = []
    ledger = run["ledger"]
    objects = run["object_audit"]
    metrics = run["metrics"]
    receiver = objects.get("receiver_bright_orange_block", {})
    if receiver.get("triangle_count", 0) <= 0:
        failures.append("receiver object missing from audit")
    if not variant["include_glass"] and not variant["include_water"]:
        if receiver.get("primary_hit_pixels", 0) <= 0:
            failures.append("receiver_only did not expose receiver as primary hit")
    else:
        if ledger["transmission_rays"] <= 0:
            failures.append("transparent variant recorded zero transmission rays")
        if ledger["receiver_hits"] <= 0:
            failures.append("transparent variant recorded zero receiver hits")
        if not any(
            entry.get("object_id") == "receiver_bright_orange_block"
            and int(entry.get("hit_count", 0)) > 0
            for entry in ledger.get("receiver_object_hits", [])
        ):
            failures.append("transparent variant did not hit receiver object")
        if ledger["contributing_samples"] <= 0:
            failures.append("transparent variant recorded zero contributing samples")
    if metrics["receiver_orange_score_p95"] <= 20.0:
        failures.append("receiver orange ROI signal too weak")
    if variant["include_water"] and "<generated>" not in objects:
        failures.append("water variant did not audit generated water surface")
    if variant["include_glass"] and "front_glass_test_pane" not in objects:
        failures.append("glass variant did not audit front glass pane")
    return failures


def write_index(path: Path, report: dict) -> None:
    root = path.parent.resolve()
    lines = [
        "# Aquarium Transparent Receiver Fixture",
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
        if run.get("skipped"):
            lines.append(
                f"- `{run['variant_id']}`: render skipped, "
                f"scene `{Path(run['scene_path']).resolve()}`"
            )
            continue
        png = Path(run["png_path"]).resolve()
        try:
            png_text = png.relative_to(root).as_posix()
        except ValueError:
            png_text = str(png)
        ledger = run["ledger"]
        metrics = run["metrics"]
        lines.append(
            f"- `{run['variant_id']}`: transmission `{ledger['transmission_rays']}`, "
            f"receiver hits `{ledger['receiver_hits']}`, "
            f"receiver objects `{len(ledger.get('receiver_object_hits', []))}`, "
            f"orange area `{metrics['receiver_orange_area_ratio']:.4f}`, "
            f"frame `{png_text}`"
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
        scene = build_scene(bool(variant["include_glass"]))
        request = base_request(
            variant,
            scene_path,
            variant_root / "render",
            summary_path,
            water_root,
        )
        write_json(scene_path, scene)
        write_json(request_path, request)
        elapsed = render_request(args.cli, request_path, summary_path, args.skip_render)
        if args.skip_render:
            runs.append(
                {
                    "variant_id": variant["id"],
                    "label": variant["label"],
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
            "scene_path": str(scene_path),
            "request_path": str(request_path),
            "summary_path": str(summary_path),
            "frame_path": str(frame_path),
            "png_path": str(png_path),
            "render_seconds": elapsed,
            "skipped": False,
            "ledger": ledger_digest(summary),
            "object_audit": object_audit_digest(summary),
            "metrics": receiver_metrics(frame_path),
        }
        run_failures = validate_run(variant, run)
        failures.extend(f"{variant['id']}: {failure}" for failure in run_failures)
        runs.append(run)
        if run_failures and not args.keep_going:
            break

    contact_sheet = ""
    if not args.skip_render:
        contact_sheet = str(make_contact_sheet(review_root, [run for run in runs if not run["skipped"]]))
    report = {
        "schema": "codework_aquarium_transparent_receiver_fixture_report_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "review_root": str(review_root),
        "water_root": str(water_root),
        "contact_sheet": contact_sheet,
        "passed": not failures and not args.skip_render,
        "failures": failures,
        "runs": runs,
    }
    report_path = review_root / "aquarium_transparent_receiver_fixture_report.json"
    write_json(report_path, report)
    write_index(review_root / "aquarium_transparent_receiver_fixture_index.md", report)
    if failures:
        print(f"fixture failed with {len(failures)} failure(s); report: {report_path}", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"fixture report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
