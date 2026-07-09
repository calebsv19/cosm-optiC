#!/usr/bin/env python3
"""Run the T5 tile/adaptive visual and metric proof matrix."""

from __future__ import annotations

import argparse
import copy
import json
import platform
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402


VARIANTS = {
    "reference_full": {
        "use_tiled_renderer": False,
        "adaptive_sampling_enabled": False,
    },
    "tiled_no_adaptive": {
        "use_tiled_renderer": True,
        "adaptive_sampling_enabled": False,
    },
    "tiled_adaptive": {
        "use_tiled_renderer": True,
        "adaptive_sampling_enabled": True,
    },
}


SCENES = [
    {
        "id": "flat_stable_background",
        "base_request": "tests/fixtures/agent_render_preflight_request.json",
        "integrator_3d": "disney_v2",
    },
    {
        "id": "high_triangle_tlas_blas",
        "base_request": "tests/fixtures/agent_render_mesh_asset_spheres_tlas_request.json",
        "integrator_3d": "disney_v2",
    },
    {
        "id": "mirror_plane",
        "base_request": "tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/request_plane_denoise_off_1.json",
        "integrator_3d": "disney_v2",
    },
    {
        "id": "glass_caustic_receiver",
        "base_request": "tests/fixtures/caustic_probe_glass_sphere/request_disney_v2.json",
        "integrator_3d": "disney_v2",
    },
    {
        "id": "emissive_lightbox",
        "base_request": "tests/fixtures/emissive_light_preview_matrix/request_lightbox_prism_disney_v2.json",
        "integrator_3d": "disney_v2",
    },
]


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


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument(
        "--review-root",
        type=Path,
        default=workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "tile_adaptive_t5_matrix",
    )
    parser.add_argument("--scene", action="append", default=[])
    parser.add_argument("--width", type=int, default=160)
    parser.add_argument("--height", type=int, default=96)
    parser.add_argument("--temporal-frames", type=int, default=6)
    parser.add_argument("--tile-size", type=int, default=16)
    parser.add_argument("--keep-going", action="store_true")
    parser.add_argument("--skip-render", action="store_true")
    return parser.parse_args()


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def selected_scenes(scene_ids: list[str]) -> list[dict[str, Any]]:
    if not scene_ids:
        return SCENES
    wanted = set(scene_ids)
    return [scene for scene in SCENES if scene["id"] in wanted]


def resolve_scene_path(base_request: dict[str, Any], base_path: Path) -> str:
    scene_path = Path(base_request["scene"]["runtime_scene_path"])
    if scene_path.is_absolute():
        return str(scene_path)
    return str((base_path.parent / scene_path).resolve())


def build_request(
    scene: dict[str, Any],
    variant_id: str,
    variant: dict[str, Any],
    base_path: Path,
    review_root: Path,
    width: int,
    height: int,
    temporal_frames: int,
    tile_size: int,
) -> tuple[Path, Path, Path]:
    base = load_json(base_path)
    request = copy.deepcopy(base)
    run_id = f"tile_adaptive_t5_{scene['id']}_{variant_id}"
    out_root = review_root / "runs" / scene["id"] / variant_id
    request_path = review_root / "generated_requests" / f"request_{scene['id']}_{variant_id}.json"
    summary_path = out_root / "render_summary.json"
    progress_path = out_root / "render_progress.json"

    request["run_id"] = run_id
    request["scene"] = {"runtime_scene_path": resolve_scene_path(base, base_path)}
    request.setdefault("render", {})
    request["render"].update({
        "start_frame": 0,
        "frame_count": 1,
        "width": width,
        "height": height,
        "normalized_t": 0.0,
        "temporal_frames": temporal_frames,
        "integrator_3d": scene.get("integrator_3d", "disney_v2"),
        "denoise_enabled": False,
        "use_tiled_renderer": variant["use_tiled_renderer"],
        "tile_size": tile_size,
        "adaptive_sampling_enabled": variant["adaptive_sampling_enabled"],
    })
    request.setdefault("inspection", {})
    request["inspection"]["object_audit_enabled"] = False
    request.setdefault("output", {})
    request["output"].update({"root": str(out_root), "overwrite": True})
    request["progress"] = {
        "summary_path": str(summary_path),
        "progress_path": str(progress_path),
    }
    write_json(request_path, request)
    return request_path, out_root, summary_path


def render_request(cli: Path, request_path: Path, out_root: Path, summary_path: Path) -> float:
    out_root.mkdir(parents=True, exist_ok=True)
    stdout_path = out_root / "stdout_summary.json"
    stderr_path = out_root / "stderr.txt"
    start = time.perf_counter()
    with stdout_path.open("w", encoding="utf-8") as stdout:
        result = subprocess.run(
            [str(cli), "--request", str(request_path), "--render", "--summary", str(summary_path)],
            stdout=stdout,
            stderr=subprocess.PIPE,
            text=True,
        )
    elapsed = time.perf_counter() - start
    stderr_path.write_text(result.stderr or "", encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(f"render failed for {request_path}: {stderr_path}")
    return elapsed


def frame_path(summary: dict[str, Any], out_root: Path) -> Path:
    value = summary.get("outputs", {}).get("first_frame_path", "")
    path = Path(value)
    if path.is_absolute():
        return path
    return out_root / "frames" / "frame_0000.bmp"


def channel_delta(left: tuple[int, int, int], right: tuple[int, int, int]) -> float:
    return (
        abs(left[0] - right[0]) +
        abs(left[1] - right[1]) +
        abs(left[2] - right[2])
    ) / 3.0


def seam_metrics(pixels: list[list[tuple[int, int, int]]], tile_size: int) -> dict[str, Any]:
    height = len(pixels)
    width = len(pixels[0]) if height else 0
    boundary_sum = 0.0
    boundary_count = 0
    interior_sum = 0.0
    interior_count = 0
    worst_boundary = 0.0
    worst_boundary_axis = ""
    worst_boundary_index = 0

    for y in range(height):
        for x in range(1, width):
            delta = channel_delta(pixels[y][x - 1], pixels[y][x])
            if x % tile_size == 0:
                boundary_sum += delta
                boundary_count += 1
                if delta > worst_boundary:
                    worst_boundary = delta
                    worst_boundary_axis = "x"
                    worst_boundary_index = x
            else:
                interior_sum += delta
                interior_count += 1
    for y in range(1, height):
        for x in range(width):
            delta = channel_delta(pixels[y - 1][x], pixels[y][x])
            if y % tile_size == 0:
                boundary_sum += delta
                boundary_count += 1
                if delta > worst_boundary:
                    worst_boundary = delta
                    worst_boundary_axis = "y"
                    worst_boundary_index = y
            else:
                interior_sum += delta
                interior_count += 1

    boundary_mean = boundary_sum / max(1, boundary_count)
    interior_mean = interior_sum / max(1, interior_count)
    return {
        "tile_size": tile_size,
        "boundary_mean_abs_rgb_delta": boundary_mean,
        "interior_mean_abs_rgb_delta": interior_mean,
        "boundary_excess_abs_rgb_delta": max(0.0, boundary_mean - interior_mean),
        "boundary_to_interior_ratio": boundary_mean / max(1.0e-6, interior_mean),
        "worst_boundary_abs_rgb_delta": worst_boundary,
        "worst_boundary_axis": worst_boundary_axis,
        "worst_boundary_index": worst_boundary_index,
        "boundary_sample_count": boundary_count,
        "interior_sample_count": interior_count,
    }


def stats_digest(summary: dict[str, Any]) -> dict[str, Any]:
    stats = summary.get("render_stats", {})
    keys = [
        "visible_pixels",
        "nonzero_pixels",
        "temporal_committed_subpasses",
        "temporal_pixels_rendered",
        "temporal_pixels_skipped",
        "temporal_active_pixels",
        "temporal_skipped_pixels",
        "temporal_active_tiles",
        "temporal_inactive_tiles",
        "temporal_planned_parent_tiles",
        "temporal_emitted_tile_jobs",
        "temporal_dispatched_tile_jobs",
        "temporal_completed_tile_jobs",
        "temporal_progress_dirty_tile_batches",
        "temporal_progress_dirty_tiles",
        "temporal_dirty_preview_presents",
        "temporal_final_preview_presents",
        "temporal_history_promotes",
        "temporal_adaptive_state_measured_pixels",
        "temporal_adaptive_state_stable_pixels",
        "temporal_adaptive_state_active_pixels",
        "temporal_adaptive_state_probe_pixels",
        "temporal_adaptive_state_high_risk_pixels",
        "temporal_adaptive_state_active_tiles",
        "temporal_adaptive_state_high_risk_tiles",
    ]
    return {key: stats.get(key, 0) for key in keys}


def write_png_copy(frame: Path, out_path: Path) -> list[list[tuple[int, int, int]]]:
    width, height, pixels = review_artifacts.read_bmp_rgb(frame)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(out_path, width, height, pixels)
    return pixels


def write_diff(review_root: Path,
               scene_id: str,
               after_id: str,
               before_pixels: list[list[tuple[int, int, int]]],
               after_pixels: list[list[tuple[int, int, int]]]) -> dict[str, Any]:
    out_dir = review_root / "comparisons" / scene_id / f"reference_full_vs_{after_id}"
    out_dir.mkdir(parents=True, exist_ok=True)
    diff4 = review_artifacts.abs_diff_pixels(before_pixels, after_pixels, 4)
    diff8 = review_artifacts.abs_diff_pixels(before_pixels, after_pixels, 8)
    side_w, side_h, side = review_artifacts.side_by_side(before_pixels, after_pixels, diff4)
    diff4_path = out_dir / "diff_abs_amplified4x.png"
    diff8_path = out_dir / "diff_abs_amplified8x.png"
    side_path = out_dir / "side_by_side_reference_diff4x.png"
    review_artifacts.write_png_rgb(diff4_path, len(diff4[0]), len(diff4), diff4)
    review_artifacts.write_png_rgb(diff8_path, len(diff8[0]), len(diff8), diff8)
    review_artifacts.write_png_rgb(side_path, side_w, side_h, side)
    metrics = review_artifacts.diff_metrics(before_pixels, after_pixels)
    metrics.update({
        "scene_id": scene_id,
        "before": "reference_full",
        "after": after_id,
        "diff_png": str(diff4_path),
        "diff_8x_png": str(diff8_path),
        "side_by_side_png": str(side_path),
    })
    write_json(out_dir / "diff_metrics.json", metrics)
    return metrics


def write_index(report: dict[str, Any], path: Path) -> None:
    lines = [
        "# RayTracing T5 Tile/Adaptive Visual Metric Matrix",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- review root: `{report['review_root']}`",
        f"- passed: `{report['passed']}`",
        "",
        "## Scene Results",
        "",
    ]
    for scene in report["scenes"]:
        lines.append(f"### {scene['scene_id']}")
        for run in scene["runs"]:
            if "error" in run:
                lines.append(
                    f"- `{run['variant']}`: error `{run['error']}`"
                )
                continue
            stats = run["stats"]
            seam = run["seam_metrics"]
            lines.append(
                f"- `{run['variant']}`: elapsed `{run['elapsed_seconds']:.3f}s`, "
                f"nonzero `{stats['nonzero_pixels']}`, "
                f"rendered/skipped `{stats['temporal_pixels_rendered']}/"
                f"{stats['temporal_pixels_skipped']}`, "
                f"active/stable/probe/high-risk "
                f"`{stats['temporal_adaptive_state_active_pixels']}/"
                f"{stats['temporal_adaptive_state_stable_pixels']}/"
                f"{stats['temporal_adaptive_state_probe_pixels']}/"
                f"{stats['temporal_adaptive_state_high_risk_pixels']}`, "
                f"tile jobs `{stats['temporal_dispatched_tile_jobs']}/"
                f"{stats['temporal_completed_tile_jobs']}`, "
                f"seam excess `{seam['boundary_excess_abs_rgb_delta']:.3f}`, "
                f"PNG `{Path(run['png_path']).relative_to(path.parent)}`"
            )
        for comparison in scene["comparisons"]:
            lines.append(
                f"- comparison `{comparison['after']}`: changed "
                f"`{comparison['changed_pixels']}/{comparison['pixels']}`, "
                f"mean luma `{comparison['mean_abs_luma']:.3f}`, "
                f"max delta `{comparison['max_abs_channel_delta']}`, "
                f"side `{Path(comparison['side_by_side_png']).relative_to(path.parent)}`"
            )
        lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    root = repo_root()
    cli = args.cli.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2
    if args.width <= 0 or args.height <= 0 or args.temporal_frames <= 0 or args.tile_size <= 0:
        print("width, height, temporal frames, and tile size must be positive", file=sys.stderr)
        return 2

    if review_root.exists() and not args.skip_render:
        shutil.rmtree(review_root)
    review_root.mkdir(parents=True, exist_ok=True)

    report: dict[str, Any] = {
        "schema_version": "ray_tracing_tile_adaptive_t5_matrix_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "review_root": str(review_root),
        "width": args.width,
        "height": args.height,
        "temporal_frames": args.temporal_frames,
        "tile_size": args.tile_size,
        "scenes": [],
        "passed": True,
    }

    for scene in selected_scenes(args.scene):
        scene_report: dict[str, Any] = {
            "scene_id": scene["id"],
            "base_request": str((root / scene["base_request"]).resolve()),
            "runs": [],
            "comparisons": [],
        }
        pixels_by_variant: dict[str, list[list[tuple[int, int, int]]]] = {}
        base_path = (root / scene["base_request"]).resolve()
        for variant_id, variant in VARIANTS.items():
            try:
                request_path, out_root, summary_path = build_request(
                    scene,
                    variant_id,
                    variant,
                    base_path,
                    review_root,
                    args.width,
                    args.height,
                    args.temporal_frames,
                    args.tile_size,
                )
                elapsed = 0.0 if args.skip_render else render_request(cli, request_path, out_root, summary_path)
                summary = load_json(summary_path)
                frame = frame_path(summary, out_root)
                png_path = review_root / "frames" / scene["id"] / f"{variant_id}.png"
                pixels = write_png_copy(frame, png_path)
                pixels_by_variant[variant_id] = pixels
                checks = {
                    "rendered": summary.get("rendered_frames") is True,
                    "frame_exists": frame.exists(),
                    "nonzero": int(summary.get("render_stats", {}).get("nonzero_pixels", 0)) > 0,
                    "integrator": summary.get("integrator_3d") == scene.get("integrator_3d", "disney_v2"),
                }
                if not all(checks.values()):
                    report["passed"] = False
                scene_report["runs"].append({
                    "variant": variant_id,
                    "request_path": str(request_path),
                    "summary_path": str(summary_path),
                    "frame_path": str(frame),
                    "png_path": str(png_path),
                    "elapsed_seconds": elapsed,
                    "checks": checks,
                    "stats": stats_digest(summary),
                    "seam_metrics": seam_metrics(pixels, args.tile_size),
                })
            except Exception as exc:  # noqa: BLE001 - matrix tool should report all selected cells.
                report["passed"] = False
                scene_report["runs"].append({
                    "variant": variant_id,
                    "error": str(exc),
                })
                print(f"{scene['id']} / {variant_id}: {exc}", file=sys.stderr)
                if not args.keep_going:
                    report["scenes"].append(scene_report)
                    write_json(review_root / "tile_adaptive_t5_matrix_report.json", report)
                    write_index(report, review_root / "index.md")
                    print(review_root / "tile_adaptive_t5_matrix_report.json")
                    return 1
        if "reference_full" in pixels_by_variant:
            for variant_id in ("tiled_no_adaptive", "tiled_adaptive"):
                if variant_id in pixels_by_variant:
                    scene_report["comparisons"].append(
                        write_diff(review_root,
                                   scene["id"],
                                   variant_id,
                                   pixels_by_variant["reference_full"],
                                   pixels_by_variant[variant_id])
                    )
        report["scenes"].append(scene_report)

    write_json(review_root / "tile_adaptive_t5_matrix_report.json", report)
    write_index(report, review_root / "index.md")
    print(review_root / "tile_adaptive_t5_matrix_report.json")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
