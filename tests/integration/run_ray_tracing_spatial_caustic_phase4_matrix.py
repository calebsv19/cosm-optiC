#!/usr/bin/env python3
"""Run the Phase 4 spatial-caustic transport/bootstrap A/B proof matrix."""

from __future__ import annotations

import argparse
import json
import platform
import shutil
import struct
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
    return workspace_root() / "_private_workspace_artifacts" / "agent_runs" / "ray_tracing" / "caustic_phase4_transport_matrix"


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
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


def write_uniform_vf3d(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    w = h = d = 16
    cell_count = w * h * d
    magic = (ord("V") << 24) | (ord("F") << 16) | (ord("3") << 8) | ord("D")
    header = struct.pack(
        "@IIIIIdQdfffffffIIII",
        magic,
        1,
        w,
        h,
        d,
        0.0,
        0,
        1.0 / 24.0,
        -1.5,
        -1.5,
        0.0,
        3.0 / float(w),
        0.0,
        0.0,
        1.0,
        0,
        0,
        0,
        0,
    )
    if struct.calcsize("@IIIIIdQdfffffffIIII") != 92:
        raise RuntimeError("unexpected native vf3d header packing")
    density = [0.18] * cell_count
    zero_float = [0.0] * cell_count
    solid = bytes(cell_count)
    with path.open("wb") as f:
        f.write(header)
        f.write(b"\0\0\0\0")
        f.write(struct.pack(f"@{cell_count}f", *density))
        f.write(struct.pack(f"@{cell_count}f", *zero_float))
        f.write(struct.pack(f"@{cell_count}f", *zero_float))
        f.write(struct.pack(f"@{cell_count}f", *zero_float))
        f.write(struct.pack(f"@{cell_count}f", *zero_float))
        f.write(solid)


def base_request(run_id: str, output_root: Path, summary_path: Path, volume_path: Path) -> dict:
    fixture_root = repo_root() / "tests" / "fixtures" / "caustic_probe_glass_sphere"
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": run_id,
        "scene": {
            "runtime_scene_path": str(fixture_root / "scene_runtime.json"),
        },
        "volume": {
            "enabled": True,
            "source_kind": "raw_vf3d",
            "source_path": str(volume_path),
            "affects_lighting": True,
            "debug_overlay": False,
        },
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": 160,
            "height": 112,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "integrator_3d": "direct_light",
        },
        "inspection": {
            "camera_position": {"x": 0.0, "y": -4.8, "z": 2.05},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 0.62},
            "camera_zoom": 1.02,
            "environment_light_mode": "ambient",
            "ambient_strength": 0.06,
            "top_fill_strength": 0.0,
            "light_intensity": 7.5,
            "light_radius": 0.10,
            "volume_density_scale": 0.65,
            "volume_density_gamma": 1.0,
            "volume_scatter_gain": 1.6,
            "volume_absorption_gain": 0.08,
            "volume_opacity_clamp": 0.75,
            "volume_step_scale": 1.2,
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


def matrix_requests(review_root: Path, volume_path: Path) -> list[dict]:
    run_root = review_root / "runs"
    specs = [
        {
            "cell_id": "baseline_off",
            "run_id": "caustic_phase4_baseline_off",
            "integrator": "direct_light",
            "inspection": {
                "caustic_mode": "off",
                "caustic_volume_enabled": False,
                "caustic_sample_budget": 0,
            },
        },
        {
            "cell_id": "bootstrap_volume",
            "run_id": "caustic_phase4_bootstrap_volume_cache",
            "integrator": "direct_light",
            "inspection": {
                "caustic_mode": "spatial_cache",
                "caustic_volume_enabled": True,
                "caustic_sample_budget": 768,
            },
        },
        {
            "cell_id": "transport_volume",
            "run_id": "caustic_phase4_transport_volume_cache",
            "integrator": "direct_light",
            "inspection": {
                "caustic_mode": "transport",
                "caustic_volume_enabled": True,
                "caustic_sample_budget": 768,
                "caustic_max_path_depth": 2,
            },
        },
        {
            "cell_id": "combined_reference",
            "run_id": "caustic_phase4_combined_bootstrap_sidecar_reference",
            "integrator": "disney_v2",
            "inspection": {
                "caustic_mode": "spatial_cache",
                "caustic_volume_enabled": True,
                "caustic_sidecar_enabled": True,
                "caustic_sidecar_strength": 2.5,
                "caustic_sample_budget": 768,
                "secondary_diffuse_samples_3d": 4,
                "transmission_samples_3d": 4,
            },
            "render": {
                "temporal_frames": 2,
                "denoise_enabled": True,
            },
        },
    ]
    requests = []
    for spec in specs:
        cell_id = spec["cell_id"]
        output_root = run_root / cell_id
        summary_path = output_root / "render_summary.json"
        request = base_request(spec["run_id"], output_root, summary_path, volume_path)
        request["render"]["integrator_3d"] = spec["integrator"]
        request["render"].update(spec.get("render", {}))
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


def copy_frame_png(summary: dict, review_root: Path, cell_id: str) -> str:
    frame_path = Path(summary.get("outputs", {}).get("first_frame_path", ""))
    if not frame_path.exists():
        frame_path = Path(summary.get("output_root", "")) / "frames" / "frame_0000.bmp"
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{cell_id}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return str(png_path)


def caustic_digest(summary: dict) -> dict:
    inspection = summary.get("inspection", {})
    state = inspection.get("caustic_state", {})
    return {
        "mode": state.get("mode", "unknown"),
        "temporary_bootstrap_active": bool(state.get("temporary_analytic_bridge", False)),
        "transport_active": bool(state.get("transport_path_emission_active", False)),
        "transport_lights": int(state.get("transport_light_count", 0)),
        "transport_evaluated_paths": int(state.get("transport_evaluated_path_count", 0)),
        "transport_emitted_paths": int(state.get("transport_emitted_path_count", 0)),
        "transport_volume_segments": int(state.get("transport_volume_segment_count", 0)),
        "cache_bound": bool(state.get("volume_cache_bound", False)),
        "cache_allocated": bool(state.get("volume_cache_allocated", False)),
        "cache_cells": int(state.get("volume_cache_cell_count", 0)),
        "cache_nonzero_cells": int(state.get("volume_cache_nonzero_cell_count", 0)),
        "cache_deposit_attempts": int(state.get("volume_cache_deposit_attempt_count", 0)),
        "cache_deposit_accepted": int(state.get("volume_cache_deposit_accepted_count", 0)),
        "cache_total_radiance": state.get("volume_cache_total_radiance", {}),
        "cache_max_radiance": float(state.get("volume_cache_max_cell_radiance", 0.0)),
        "scatter_bound": bool(state.get("volume_scatter_caustic_sampling_bound", False)),
        "scatter_samples": int(state.get("volume_scatter_caustic_sample_count", 0)),
        "scatter_contributing_samples": int(state.get("volume_scatter_caustic_contributing_sample_count", 0)),
        "scatter_radiance": state.get("volume_scatter_caustic_radiance", {}),
        "sample_budget": int(state.get("sample_budget", 0)),
    }


def validate_cell(cell_id: str, digest: dict) -> list[str]:
    failures: list[str] = []
    if cell_id == "baseline_off":
        if digest["temporary_bootstrap_active"] or digest["transport_active"]:
            failures.append("baseline unexpectedly activated a caustic source")
        if digest["cache_nonzero_cells"] != 0:
            failures.append("baseline produced nonzero caustic cache cells")
    elif cell_id == "bootstrap_volume":
        if not digest["temporary_bootstrap_active"]:
            failures.append("bootstrap volume did not report the temporary analytic bridge")
        if digest["transport_active"]:
            failures.append("bootstrap volume unexpectedly activated transport")
        if digest["cache_nonzero_cells"] <= 0 or digest["cache_deposit_accepted"] <= 0:
            failures.append("bootstrap volume did not populate the caustic cache")
        if digest["scatter_contributing_samples"] <= 0:
            failures.append("bootstrap volume was not sampled by VF3D scatter")
    elif cell_id == "transport_volume":
        if digest["temporary_bootstrap_active"]:
            failures.append("transport fell back to the temporary analytic bridge")
        if not digest["transport_active"]:
            failures.append("transport path emission did not activate")
        if digest["transport_evaluated_paths"] <= 0 or digest["transport_emitted_paths"] <= 0:
            failures.append("transport did not evaluate and emit paths")
        if digest["cache_nonzero_cells"] <= 0 or digest["cache_deposit_accepted"] <= 0:
            failures.append("transport did not populate the caustic cache")
        if digest["scatter_contributing_samples"] <= 0:
            failures.append("transport cache was not sampled by VF3D scatter")
    elif cell_id == "combined_reference":
        if not digest["temporary_bootstrap_active"]:
            failures.append("combined reference did not report the bootstrap bridge")
        if digest["cache_nonzero_cells"] <= 0:
            failures.append("combined reference did not populate the cache")
    return failures


def write_index(path: Path, report: dict) -> None:
    root = path.parent
    lines = [
        "# Phase 4 Spatial Caustic Transport Matrix",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- vf3d fixture: `{report['vf3d_path']}`",
        f"- passed: `{report['passed']}`",
        "",
        "## Runs",
        "",
    ]
    for run in report["runs"]:
        digest = run["caustic"]
        png = Path(run["png_path"]).resolve()
        try:
            png_text = png.relative_to(root.resolve()).as_posix()
        except ValueError:
            png_text = str(png)
        lines.append(
            f"- `{run['cell_id']}`: mode `{digest['mode']}`, "
            f"bootstrap `{digest['temporary_bootstrap_active']}`, "
            f"transport `{digest['transport_active']}`, "
            f"evaluated `{digest['transport_evaluated_paths']}`, "
            f"emitted `{digest['transport_emitted_paths']}`, "
            f"cache cells `{digest['cache_nonzero_cells']}`, "
            f"scatter contrib `{digest['scatter_contributing_samples']}`, "
            f"PNG `{png_text}`"
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
    review_root = args.review_root.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2

    review_root.mkdir(parents=True, exist_ok=True)
    vf3d_path = review_root / "generated_volume" / "phase4_uniform_fog.vf3d"
    write_uniform_vf3d(vf3d_path)
    requests = matrix_requests(review_root, vf3d_path)

    runs = []
    failures: list[str] = []
    for item in requests:
        cell_id = item["cell_id"]
        try:
            elapsed = render_request(cli, item["request_path"], item["summary_path"], args.skip_render)
            summary = load_json(item["summary_path"])
            png_path = copy_frame_png(summary, review_root, cell_id)
            summary_copy = review_root / "summaries" / f"summary_{cell_id}.json"
            summary_copy.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item["summary_path"], summary_copy)
            digest = caustic_digest(summary)
            cell_failures = validate_cell(cell_id, digest)
            failures.extend([f"{cell_id}: {failure}" for failure in cell_failures])
            runs.append({
                "cell_id": cell_id,
                "request_path": str(item["request_path"]),
                "summary_path": str(item["summary_path"]),
                "summary_copy": str(summary_copy),
                "png_path": png_path,
                "elapsed_seconds": elapsed,
                "caustic": digest,
                "failures": cell_failures,
            })
        except Exception as exc:
            failures.append(f"{cell_id}: {exc}")
            if not args.keep_going:
                break

    report = {
        "schema_version": "ray_tracing_spatial_caustic_phase4_matrix_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "vf3d_path": str(vf3d_path),
        "runs": runs,
        "failures": failures,
        "passed": len(failures) == 0 and len(runs) == len(requests),
    }
    write_json(review_root / "phase4_caustic_matrix_report.json", report)
    write_index(review_root / "phase4_caustic_matrix_index.md", report)
    print(review_root / "phase4_caustic_matrix_report.json")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
