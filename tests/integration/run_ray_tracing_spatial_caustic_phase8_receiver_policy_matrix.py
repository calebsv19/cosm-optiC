#!/usr/bin/env python3
"""Run the Phase 8 non-fallback receiver-policy caustic matrix."""

from __future__ import annotations

import argparse
import json
import math
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


def default_review_root() -> Path:
    return workspace_root() / "_private_workspace_artifacts" / "agent_runs" / "ray_tracing" / "caustic_phase8_receiver_policy_matrix"


def default_manifest(root: Path) -> Path:
    return root / "tests" / "fixtures" / "caustic_probe_glass_sphere" / "matrix_manifest.json"


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--manifest", type=Path, default=default_manifest(root))
    parser.add_argument("--review-root", type=Path, default=default_review_root())
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


def luma(pixel: tuple[int, int, int]) -> float:
    r, g, b = pixel
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def roi_bounds(width: int, height: int, roi: dict) -> tuple[int, int, int, int]:
    x0 = max(0, min(width - 1, int(round(float(roi.get("x0", 0.0)) * width))))
    y0 = max(0, min(height - 1, int(round(float(roi.get("y0", 0.0)) * height))))
    x1 = max(x0 + 1, min(width, int(round(float(roi.get("x1", 1.0)) * width))))
    y1 = max(y0 + 1, min(height, int(round(float(roi.get("y1", 1.0)) * height))))
    return x0, y0, x1, y1


def receiver_metrics(frame_path: Path, roi: dict, hotspot_relative_luma: float) -> dict:
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    x0, y0, x1, y1 = roi_bounds(width, height, roi)
    values: list[float] = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            values.append(luma(pixels[y][x]))
    if not values:
        raise ValueError(f"{frame_path}: empty receiver ROI")
    values.sort()
    count = len(values)
    total = sum(values)
    mean = total / float(count)
    max_value = values[-1]
    min_value = values[0]
    p50 = values[count // 2]
    p95 = values[min(count - 1, int(math.floor(0.95 * float(count - 1))))]
    spread = max(0.0, max_value - mean)
    threshold = mean + spread * max(0.0, min(1.0, hotspot_relative_luma))
    hotspot_count = sum(1 for value in values if value >= threshold)
    safe_mean = mean if mean > 1e-9 else 1.0
    safe_p50 = p50 if p50 > 1e-9 else safe_mean
    return {
        "frame_path": str(frame_path),
        "image_width": width,
        "image_height": height,
        "receiver_roi_pixels": {
            "x0": x0,
            "y0": y0,
            "x1": x1,
            "y1": y1,
            "pixel_count": count,
        },
        "receiver_luma_min": min_value,
        "receiver_luma_mean": mean,
        "receiver_luma_p50": p50,
        "receiver_luma_p95": p95,
        "receiver_luma_max": max_value,
        "receiver_luma_concentration_ratio": p95 / safe_p50,
        "receiver_luma_max_mean_ratio": max_value / safe_mean,
        "hotspot_luma_threshold": threshold,
        "hotspot_pixel_count": hotspot_count,
        "hotspot_area_ratio": hotspot_count / float(count),
    }


def base_request(run_id: str, scene_path: Path, output_root: Path, summary_path: Path) -> dict:
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": run_id,
        "scene": {
            "runtime_scene_path": str(scene_path),
        },
        "volume": {
            "enabled": False,
        },
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": 128,
            "height": 96,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "integrator_3d": "disney_v2",
            "denoise_enabled": True,
        },
        "inspection": {
            "preset": "glass_review",
            "camera_position": {"x": 0.0, "y": -0.18, "z": -3.0},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 0.0},
            "camera_zoom": 0.92,
            "environment_light_mode": "ambient",
            "ambient_strength": 0.06,
            "top_fill_strength": 0.0,
            "light_intensity": 7.5,
            "light_radius": 0.10,
            "secondary_diffuse_samples_3d": 8,
            "transmission_samples_3d": 8,
            "caustic_debug_summary": True,
            "object_audit_enabled": True,
        },
        "output": {
            "root": str(output_root),
            "overwrite": True,
        },
        "progress": {
            "summary_path": str(summary_path),
            "progress_path": str(output_root / "render_progress.json"),
        },
    }


def generated_scene_path(review_root: Path, scene_id: str, sphere_z: float) -> Path:
    fixture_root = repo_root() / "tests" / "fixtures" / "caustic_probe_glass_sphere"
    source_path = fixture_root / "scene_runtime.json"
    scene = load_json(source_path)
    scene["scene_id"] = f"caustic_probe_glass_sphere_phase8_{scene_id}"
    for obj in scene.get("objects", []):
        if obj.get("object_id") == "glass_sphere":
            obj.setdefault("transform", {}).setdefault("position", {})["z"] = sphere_z
            line_drawing = obj.setdefault("extensions", {}).setdefault("line_drawing", {})
            line_drawing["runtime_mesh_path"] = str(
                (fixture_root / "../mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_256x128.runtime.json").resolve()
            )
    path = review_root / "generated_scenes" / f"{scene_id}.json"
    write_json(path, scene)
    return path


def matrix_requests(review_root: Path) -> list[dict]:
    run_root = review_root / "runs"
    raised_near_scene = generated_scene_path(review_root, "raised_near", 1.08)
    raised_high_scene = generated_scene_path(review_root, "raised_high", 1.28)
    specs = [
        {
            "cell_id": "off_raised_near",
            "run_id": "caustic_phase8_off_raised_near",
            "scene_path": raised_near_scene,
            "inspection": {
                "caustic_mode": "off",
                "caustic_surface_enabled": False,
                "caustic_sidecar_enabled": False,
                "caustic_sample_budget": 0,
            },
        },
        {
            "cell_id": "surface_cache_raised_near_no_fallback",
            "run_id": "caustic_phase8_surface_cache_raised_near_no_fallback",
            "scene_path": raised_near_scene,
            "inspection": {
                "caustic_mode": "transport",
                "caustic_surface_enabled": True,
                "caustic_sidecar_enabled": False,
                "caustic_sample_budget": 1024,
                "caustic_max_path_depth": 2,
                "caustic_surface_energy_scale": 8.0,
                "caustic_surface_footprint_scale": 16.0,
                "caustic_surface_receiver_fallback_enabled": False,
            },
        },
        {
            "cell_id": "off_raised_high",
            "run_id": "caustic_phase8_off_raised_high",
            "scene_path": raised_high_scene,
            "inspection": {
                "caustic_mode": "off",
                "caustic_surface_enabled": False,
                "caustic_sidecar_enabled": False,
                "caustic_sample_budget": 0,
            },
        },
        {
            "cell_id": "surface_cache_raised_high_no_fallback",
            "run_id": "caustic_phase8_surface_cache_raised_high_no_fallback",
            "scene_path": raised_high_scene,
            "inspection": {
                "caustic_mode": "transport",
                "caustic_surface_enabled": True,
                "caustic_sidecar_enabled": False,
                "caustic_sample_budget": 1024,
                "caustic_max_path_depth": 2,
                "caustic_surface_energy_scale": 8.0,
                "caustic_surface_footprint_scale": 16.0,
                "caustic_surface_receiver_fallback_enabled": False,
            },
        },
    ]
    requests = []
    for spec in specs:
        cell_id = spec["cell_id"]
        output_root = run_root / cell_id
        summary_path = output_root / "render_summary.json"
        request = base_request(spec["run_id"], spec["scene_path"], output_root, summary_path)
        request["inspection"].update(spec["inspection"])
        request_path = review_root / "generated_requests" / f"request_{cell_id}.json"
        write_json(request_path, request)
        requests.append({"cell_id": cell_id, "request_path": request_path, "summary_path": summary_path})
    return requests


def render_request(cli: Path, request_path: Path, summary_path: Path, skip_render: bool) -> float | None:
    if skip_render:
        return None
    stdout_path = summary_path.parent / "stdout_summary.json"
    stderr_path = summary_path.parent / "stderr.txt"
    summary_path.parent.mkdir(parents=True, exist_ok=True)
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
        raise RuntimeError(f"render failed for {request_path} with exit {result.returncode}; stderr: {stderr_path}")
    return elapsed


def first_frame_path(summary: dict) -> Path:
    frame_path = Path(summary.get("outputs", {}).get("first_frame_path", ""))
    if frame_path.exists():
        return frame_path
    return Path(summary.get("output_root", "")) / "frames" / "frame_0000.bmp"


def copy_frame_png(summary: dict, review_root: Path, cell_id: str) -> tuple[Path, Path]:
    frame_path = first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{cell_id}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return frame_path, png_path


def rgb_sum(value: dict) -> float:
    return float(value.get("r", 0.0)) + float(value.get("g", 0.0)) + float(value.get("b", 0.0))


def caustic_digest(summary: dict) -> dict:
    inspection = summary.get("inspection", {})
    state = inspection.get("caustic_state", {})
    stats = summary.get("render_stats", {})
    sampled_surface = state.get("surface_caustic_sampled_radiance", {})
    sidecar_total = float(stats.get("total_caustic_sidecar_radiance", 0.0))
    return {
        "mode": state.get("mode", "unknown"),
        "analytic_sidecar_requested": bool(state.get("analytic_sidecar_requested", False)),
        "temporary_bootstrap_active": bool(state.get("temporary_analytic_bridge", False)),
        "transport_active": bool(state.get("transport_path_emission_active", False)),
        "transport_evaluated_paths": int(state.get("transport_evaluated_path_count", 0)),
        "transport_emitted_paths": int(state.get("transport_emitted_path_count", 0)),
        "surface_cache_requested": bool(state.get("surface_cache_requested", False)),
        "surface_radiance_scale": float(state.get("surface_radiance_scale", 1.0)),
        "surface_footprint_scale": float(state.get("surface_footprint_scale", 1.0)),
        "surface_receiver_fallback_enabled": bool(state.get("surface_receiver_fallback_enabled", True)),
        "surface_receiver_fallback_count": int(state.get("transport_surface_receiver_fallback_count", 0)),
        "surface_cache_bound": bool(state.get("surface_cache_bound", False)),
        "surface_cache_allocated": bool(state.get("surface_cache_allocated", False)),
        "surface_cache_record_count": int(state.get("surface_cache_record_count", 0)),
        "surface_cache_deposit_accepted": int(state.get("surface_cache_deposit_accepted_count", 0)),
        "surface_cache_sample_lookup_count": int(state.get("surface_cache_sample_lookup_count", 0)),
        "surface_cache_sample_contributing_count": int(state.get("surface_cache_sample_contributing_count", 0)),
        "surface_cache_total_radiance": state.get("surface_cache_total_radiance", {}),
        "surface_cache_max_record_radiance": float(state.get("surface_cache_max_record_radiance", 0.0)),
        "surface_caustic_sampled_radiance": sampled_surface,
        "surface_caustic_sampled_radiance_sum": rgb_sum(sampled_surface),
        "caustic_sidecar_enabled": int(stats.get("caustic_sidecar_enabled", 0)) > 0,
        "caustic_sidecar_samples": int(stats.get("caustic_sidecar_samples", 0)),
        "caustic_sidecar_contributing_samples": int(stats.get("caustic_sidecar_contributing_samples", 0)),
        "total_caustic_sidecar_radiance": sidecar_total,
        "max_caustic_sidecar_radiance": float(stats.get("max_caustic_sidecar_radiance", 0.0)),
    }


def baseline_for_cell(cell_id: str) -> str:
    if "raised_high" in cell_id:
        return "off_raised_high"
    return "off_raised_near"


def metric_deltas(runs: dict[str, dict]) -> dict:
    result: dict[str, dict] = {}
    for cell_id, run in runs.items():
        baseline_id = baseline_for_cell(cell_id)
        baseline = runs.get(baseline_id, run)["metrics"]
        metrics = run["metrics"]
        result[cell_id] = {
            "baseline_cell_id": baseline_id,
            "mean_delta_vs_off": metrics["receiver_luma_mean"] - baseline["receiver_luma_mean"],
            "max_delta_vs_off": metrics["receiver_luma_max"] - baseline["receiver_luma_max"],
            "concentration_delta_vs_off": (
                metrics["receiver_luma_concentration_ratio"] -
                baseline["receiver_luma_concentration_ratio"]
            ),
            "max_mean_delta_vs_off": (
                metrics["receiver_luma_max_mean_ratio"] -
                baseline["receiver_luma_max_mean_ratio"]
            ),
            "hotspot_area_delta_vs_off": (
                metrics["hotspot_area_ratio"] - baseline["hotspot_area_ratio"]
            ),
        }
    return result


def frame_delta_metrics(runs: dict[str, dict]) -> dict:
    result: dict[str, dict] = {}
    for cell_id, run in runs.items():
        baseline_id = baseline_for_cell(cell_id)
        if baseline_id not in runs:
            continue
        baseline_path = Path(runs[baseline_id]["frame_path"])
        width, height, baseline_pixels = review_artifacts.read_bmp_rgb(baseline_path)
        frame_path = Path(run["frame_path"])
        frame_width, frame_height, pixels = review_artifacts.read_bmp_rgb(frame_path)
        if frame_width != width or frame_height != height:
            raise ValueError(f"{cell_id}: frame dimensions do not match off baseline")
        positive_deltas: list[float] = []
        changed_count = 0
        positive_count = 0
        total_delta = 0.0
        max_delta = 0.0
        for y in range(height):
            for x in range(width):
                base_luma = luma(baseline_pixels[y][x])
                cell_luma = luma(pixels[y][x])
                delta = cell_luma - base_luma
                total_delta += delta
                if abs(delta) > 0.5:
                    changed_count += 1
                if delta > 0.5:
                    positive_count += 1
                    positive_deltas.append(delta)
                    if delta > max_delta:
                        max_delta = delta
        positive_deltas.sort()
        p95 = 0.0
        if positive_deltas:
            p95 = positive_deltas[min(len(positive_deltas) - 1,
                                      int(math.floor(0.95 * float(len(positive_deltas) - 1))))]
        result[cell_id] = {
            "changed_pixel_count": changed_count,
            "positive_pixel_count": positive_count,
            "positive_area_ratio": positive_count / float(width * height),
            "mean_luma_delta": total_delta / float(width * height),
            "max_luma_delta": max_delta,
            "positive_luma_delta_p95": p95,
        }
    return result


def validate_cell(cell_id: str, digest: dict) -> list[str]:
    failures: list[str] = []
    if cell_id.startswith("off_"):
        if digest["transport_active"] or digest["caustic_sidecar_enabled"]:
            failures.append("off baseline unexpectedly activated transport or analytic sidecar")
        if digest["surface_cache_record_count"] != 0:
            failures.append("off baseline produced surface-cache records")
    elif cell_id.startswith("surface_cache_"):
        if not digest["transport_active"]:
            failures.append("surface-cache cell did not activate transport")
        if digest["caustic_sidecar_enabled"]:
            failures.append("surface-cache cell unexpectedly enabled analytic sidecar")
        if digest["surface_receiver_fallback_enabled"]:
            failures.append("surface-cache cell did not disable receiver fallback")
        if digest["surface_receiver_fallback_count"] != 0:
            failures.append("surface-cache cell used receiver fallback")
        if digest["surface_footprint_scale"] < 15.9 or digest["surface_radiance_scale"] < 7.9:
            failures.append("surface-cache cell did not report requested calibration scales")
        if digest["surface_cache_record_count"] <= 0 or digest["surface_cache_deposit_accepted"] <= 0:
            failures.append("surface-cache cell did not populate receiver records")
        if digest["surface_cache_sample_contributing_count"] <= 0:
            failures.append("surface-cache cell did not contribute receiver samples")
        if digest["surface_caustic_sampled_radiance_sum"] <= 0.0:
            failures.append("surface-cache cell sampled zero surface caustic radiance")
    return failures


def write_index(path: Path, report: dict) -> None:
    root = path.parent.resolve()
    lines = [
        "# Phase 8 Receiver Policy Matrix",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- report: `{path.name.replace('_index.md', '_report.json')}`",
        "",
        "## Runs",
        "",
    ]
    for run in report["runs"]:
        metrics = run["metrics"]
        digest = run["caustic"]
        png = Path(run["png_path"]).resolve()
        try:
            png_text = png.relative_to(root).as_posix()
        except ValueError:
            png_text = str(png)
        lines.append(
            f"- `{run['cell_id']}`: mode `{digest['mode']}`, "
            f"transport `{digest['transport_active']}`, sidecar `{digest['caustic_sidecar_enabled']}`, "
            f"scale `{digest['surface_radiance_scale']:.2f}/{digest['surface_footprint_scale']:.2f}`, "
            f"fallback `{digest['surface_receiver_fallback_enabled']}`/"
            f"`{digest['surface_receiver_fallback_count']}`, "
            f"surface records `{digest['surface_cache_record_count']}`, "
            f"surface contrib `{digest['surface_cache_sample_contributing_count']}`, "
            f"sidecar contrib `{digest['caustic_sidecar_contributing_samples']}`, "
            f"mean `{metrics['receiver_luma_mean']:.4f}`, "
            f"max/mean `{metrics['receiver_luma_max_mean_ratio']:.4f}`, PNG `{png_text}`"
        )
    lines.extend(["", "## Deltas Vs Off", ""])
    for cell_id, deltas in report["metric_deltas_vs_off"].items():
        lines.append(
            f"- `{cell_id}`: mean `{deltas['mean_delta_vs_off']:.4f}`, "
            f"max `{deltas['max_delta_vs_off']:.4f}`, "
            f"concentration `{deltas['concentration_delta_vs_off']:.4f}`, "
            f"hotspot area `{deltas['hotspot_area_delta_vs_off']:.4f}`"
        )
    lines.extend(["", "## Frame Deltas Vs Off", ""])
    for cell_id, deltas in report["frame_deltas_vs_off"].items():
        lines.append(
            f"- `{cell_id}`: changed `{deltas['changed_pixel_count']}`, "
            f"positive `{deltas['positive_pixel_count']}`, "
            f"mean `{deltas['mean_luma_delta']:.4f}`, "
            f"max `{deltas['max_luma_delta']:.4f}`, "
            f"p95+ `{deltas['positive_luma_delta_p95']:.4f}`"
        )
    if report["failures"]:
        lines.extend(["", "## Failures", ""])
        for failure in report["failures"]:
            lines.append(f"- {failure}")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    cli = args.cli.resolve()
    manifest_path = args.manifest.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2
    manifest = load_json(manifest_path)
    probe = manifest.get("caustic_probe", {})
    roi = probe.get("receiver_roi_normalized", {})
    hotspot_relative_luma = float(probe.get("hotspot_relative_luma", 0.82))

    review_root.mkdir(parents=True, exist_ok=True)
    requests = matrix_requests(review_root)

    runs = []
    runs_by_cell: dict[str, dict] = {}
    failures: list[str] = []
    for item in requests:
        cell_id = item["cell_id"]
        try:
            elapsed = render_request(cli, item["request_path"], item["summary_path"], args.skip_render)
            summary = load_json(item["summary_path"])
            frame_path, png_path = copy_frame_png(summary, review_root, cell_id)
            summary_copy = review_root / "summaries" / f"summary_{cell_id}.json"
            summary_copy.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item["summary_path"], summary_copy)
            metrics = receiver_metrics(frame_path, roi, hotspot_relative_luma)
            metrics_path = review_root / "receiver_metrics" / f"{cell_id}.json"
            write_json(metrics_path, metrics)
            digest = caustic_digest(summary)
            cell_failures = validate_cell(cell_id, digest)
            failures.extend([f"{cell_id}: {failure}" for failure in cell_failures])
            run = {
                "cell_id": cell_id,
                "request_path": str(item["request_path"]),
                "summary_path": str(item["summary_path"]),
                "summary_copy": str(summary_copy),
                "frame_path": str(frame_path),
                "png_path": str(png_path),
                "metrics_path": str(metrics_path),
                "elapsed_seconds": elapsed,
                "caustic": digest,
                "metrics": metrics,
                "failures": cell_failures,
            }
            runs.append(run)
            runs_by_cell[cell_id] = run
        except Exception as exc:
            failures.append(f"{cell_id}: {exc}")
            if not args.keep_going:
                break

    deltas = metric_deltas(runs_by_cell) if runs_by_cell else {}
    frame_deltas = frame_delta_metrics(runs_by_cell) if runs_by_cell else {}
    for cell_id in (
        "surface_cache_raised_near_no_fallback",
        "surface_cache_raised_high_no_fallback",
    ):
        frame_delta = frame_deltas.get(cell_id, {})
        metric_delta = deltas.get(cell_id, {})
        if runs_by_cell.get(cell_id) and frame_delta.get("positive_pixel_count", 0) <= 0:
            failures.append(f"{cell_id}: rendered frame did not change versus paired off baseline")
        if runs_by_cell.get(cell_id) and frame_delta.get("mean_luma_delta", 0.0) <= 0.0:
            failures.append(f"{cell_id}: mean frame luma delta did not rise above paired off baseline")
        if runs_by_cell.get(cell_id) and metric_delta.get("mean_delta_vs_off", 0.0) <= 0.0:
            failures.append(f"{cell_id}: receiver ROI mean did not rise above paired off baseline")
    report = {
        "schema_version": "ray_tracing_spatial_caustic_phase8_receiver_policy_matrix_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "manifest_path": str(manifest_path),
        "review_root": str(review_root),
        "receiver_roi_normalized": roi,
        "hotspot_relative_luma": hotspot_relative_luma,
        "runs": runs,
        "metric_deltas_vs_off": deltas,
        "frame_deltas_vs_off": frame_deltas,
        "failures": failures,
        "passed": len(failures) == 0 and len(runs) == len(requests),
    }
    write_json(review_root / "phase8_receiver_policy_matrix_report.json", report)
    write_index(review_root / "phase8_receiver_policy_matrix_index.md", report)
    print(review_root / "phase8_receiver_policy_matrix_report.json")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
