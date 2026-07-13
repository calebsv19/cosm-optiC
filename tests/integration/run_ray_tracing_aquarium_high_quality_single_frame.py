#!/usr/bin/env python3
"""Render one local high-quality aquarium still with plain transmission."""

from __future__ import annotations

import argparse
import json
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
import run_ray_tracing_aquarium_real_shell_transparent_receiver_diagnostic as diagnostic  # noqa: E402
import run_ray_tracing_aquarium_transparent_receiver_fixture as fixture  # noqa: E402


SOURCE_VARIANT = "ap04_glass_water_benchy_coupled"
RUN_ID = "aquarium_real_shell_benchy_coupled_hq_single_frame"


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


def default_source_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "physics_trio"
        / "aquarium_glass_room_v1"
        / "ray_tracing_ap_diagnostic_matrix_20260710c"
        / SOURCE_VARIANT
    )


def default_review_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "aquarium_high_quality_single_frame_local"
    )


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--source-root", type=Path, default=default_source_root())
    parser.add_argument("--review-root", type=Path, default=default_review_root())
    parser.add_argument("--width", type=int, default=720)
    parser.add_argument("--height", type=int, default=405)
    parser.add_argument("--transmission-samples", type=int, default=32)
    parser.add_argument("--secondary-diffuse-samples", type=int, default=64)
    parser.add_argument("--object-audit-max-dimension", type=int, default=96)
    parser.add_argument("--skip-render", action="store_true")
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def render_request(cli: Path, request_path: Path, summary_path: Path, skip_render: bool) -> float | None:
    if skip_render:
        return None
    stdout_path = summary_path.parent / "stdout_summary.json"
    stderr_path = summary_path.parent / "stderr.txt"
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    env = dict(**__import__("os").environ)
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


def prepare_payload(source_root: Path, review_root: Path, args: argparse.Namespace) -> tuple[Path, Path]:
    scene_path = review_root / "scene_runtime.json"
    request_path = review_root / "request.json"
    render_root = review_root / "render"
    if not source_root.exists():
        raise FileNotFoundError(f"missing source variant root: {source_root}")
    shutil.copy2(source_root / "scene_runtime.json", scene_path)
    water_cache = source_root / "water_cache"
    if water_cache.exists():
        shutil.copytree(water_cache, review_root / "water_cache")

    request = diagnostic.rewrite_request(
        load_json(source_root / "request.json"),
        {
            "id": RUN_ID,
            "label": "Aquarium real shell Benchy-coupled HQ single frame",
            "include_glass": True,
            "include_water": True,
            "expect_benchy_receiver": True,
        },
        scene_path,
        render_root,
    )
    request["run_id"] = RUN_ID
    request["render"] = dict(request.get("render", {}))
    request["render"].update(
        {
            "width": args.width,
            "height": args.height,
            "frame_count": 1,
            "temporal_frames": 1,
            "denoise_enabled": False,
        }
    )
    request["inspection"] = dict(request.get("inspection", {}))
    request["inspection"].update(
        {
            "caustic_mode": "off",
            "caustic_sidecar_enabled": False,
            "caustic_sidecar_strength": 0.0,
            "transmission_samples_3d": args.transmission_samples,
            "secondary_diffuse_samples_3d": args.secondary_diffuse_samples,
            "object_audit_enabled": True,
            "object_audit_max_dimension": args.object_audit_max_dimension,
        }
    )
    request["output"] = {"root": str(render_root), "overwrite": True}
    request["progress"] = {
        "summary_path": str(render_root / "render_summary.json"),
        "progress_path": str(render_root / "render_progress.json"),
    }
    if request.get("volume", {}).get("enabled"):
        request["volume"]["source_kind"] = "scene_bundle"
        request["volume"]["source_path"] = str(
            review_root / "water_cache" / "Water Basin" / "scene_bundle.json"
        )
        request["volume"]["visible"] = False
        request["volume"]["affects_lighting"] = False
        request["volume"]["debug_overlay"] = False
    write_json(request_path, request)
    return request_path, render_root / "render_summary.json"


def write_png(summary: dict, review_root: Path) -> tuple[Path, Path]:
    frame_path = diagnostic.first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "aquarium_hq_single_frame.png"
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return frame_path, png_path


def write_index(report_path: Path, report: dict) -> None:
    lines = [
        "# Aquarium High-Quality Local Single Frame",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- frame: `{Path(report['png_path']).name}`",
        f"- report: `{Path(report['report_path']).name}`",
        f"- render seconds: `{report['render_seconds']:.3f}`",
        f"- resolution: `{report['render']['width']}x{report['render']['height']}`",
        f"- transmission samples: `{report['inspection']['transmission_samples_3d']}`",
        f"- secondary diffuse samples: `{report['inspection']['secondary_diffuse_samples_3d']}`",
        f"- Benchy receiver hits: `{report['benchy_receiver_hits']}`",
        f"- Benchy luma p95: `{report['benchy_center_metrics']['luma_p95']:.2f}`",
        "",
    ]
    if report["failures"]:
        lines.append("## Failures")
        lines.append("")
        for failure in report["failures"]:
            lines.append(f"- {failure}")
        lines.append("")
    report_path.with_name("aquarium_hq_single_frame_index.md").write_text(
        "\n".join(lines),
        encoding="utf-8",
    )


def main() -> int:
    args = parse_args()
    source_root = args.source_root.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not args.cli.exists():
        raise FileNotFoundError(f"missing ray_tracing_render_headless binary: {args.cli}")
    if review_root.exists():
        shutil.rmtree(review_root)
    review_root.mkdir(parents=True, exist_ok=True)

    request_path, summary_path = prepare_payload(source_root, review_root, args)
    elapsed = render_request(args.cli, request_path, summary_path, args.skip_render)
    if args.skip_render:
        return 0

    summary = load_json(summary_path)
    frame_path, png_path = write_png(summary, review_root)
    objects = diagnostic.object_audit_digest(summary)
    ledger = fixture.ledger_digest(summary)
    benchy_entry = objects.get("benchy_floating_inside_aquarium")
    benchy_hits = diagnostic.receiver_object_hit_count(
        ledger,
        "benchy_floating_inside_aquarium",
    )
    failures: list[str] = []
    render_stats = summary.get("render_stats", {})
    if benchy_hits <= 0:
        failures.append("transparent path did not reach Benchy receiver object")
    if ledger.get("transmission_rays", 0) <= 0:
        failures.append("zero transmission rays")
    if ledger.get("transparent_surface_hits", 0) <= 0:
        failures.append("zero transparent surface hits")
    if ledger.get("contributing_samples", 0) <= 0:
        failures.append("zero contributing samples")
    if render_stats.get("caustic_sidecar_enabled"):
        failures.append("caustic sidecar unexpectedly enabled")
    if int(render_stats.get("caustic_sidecar_samples", 0)) != 0:
        failures.append("caustic sidecar samples were nonzero")

    report_path = review_root / "aquarium_hq_single_frame_report.json"
    report = {
        "schema": "codework_aquarium_hq_single_frame_report_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "source_root": str(source_root),
        "review_root": str(review_root),
        "request_path": str(request_path),
        "summary_path": str(summary_path),
        "frame_path": str(frame_path),
        "png_path": str(png_path),
        "report_path": str(report_path),
        "render_seconds": elapsed,
        "passed": not failures,
        "failures": failures,
        "render": summary.get("render", {}),
        "inspection": summary.get("inspection", {}),
        "render_stats": render_stats,
        "water_surface": summary.get("water_surface", {}),
        "ledger": ledger,
        "object_audit": objects,
        "benchy_receiver_hits": benchy_hits,
        "benchy_center_metrics": diagnostic.object_center_metrics(frame_path, benchy_entry),
    }
    write_json(report_path, report)
    write_index(report_path, report)
    print(f"high-quality frame report: {report_path}")
    if failures:
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
