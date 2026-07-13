#!/usr/bin/env python3
"""Rerun the real aquarium shell transparent-receiver diagnostic locally."""

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
import run_ray_tracing_aquarium_transparent_receiver_fixture as fixture  # noqa: E402


SOURCE_MATRIX = (
    {
        "id": "real_shell_glass_only",
        "label": "Real shell glass only",
        "source": "ap01_glass_only",
        "include_glass": True,
        "include_water": False,
        "expect_benchy_receiver": True,
    },
    {
        "id": "real_shell_water_only_uncoupled",
        "label": "Water only uncoupled",
        "source": "ap02_water_only_uncoupled",
        "include_glass": False,
        "include_water": True,
        "expect_benchy_receiver": False,
    },
    {
        "id": "real_shell_glass_water_uncoupled",
        "label": "Real shell + uncoupled water",
        "source": "ap03_glass_water_uncoupled",
        "include_glass": True,
        "include_water": True,
        "expect_benchy_receiver": True,
    },
    {
        "id": "real_shell_glass_water_benchy_coupled",
        "label": "Real shell + Benchy-coupled water",
        "source": "ap04_glass_water_benchy_coupled",
        "include_glass": True,
        "include_water": True,
        "expect_benchy_receiver": True,
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


def aquarium_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "physics_trio"
        / "aquarium_glass_room_v1"
    )


def default_source_root() -> Path:
    return aquarium_root() / "ray_tracing_ap_diagnostic_matrix_20260710c"


def default_review_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "aquarium_real_shell_transparent_receiver_diagnostic"
    )


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--source-root", type=Path, default=default_source_root())
    parser.add_argument("--review-root", type=Path, default=default_review_root())
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--keep-going", action="store_true")
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


def first_frame_path(summary: dict) -> Path:
    frame_path = Path(summary.get("outputs", {}).get("first_frame_path", ""))
    if frame_path.exists():
        return frame_path
    return Path(summary.get("output_root", "")) / "frames" / "frame_0200.bmp"


def object_audit_digest(summary: dict) -> dict:
    result: dict[str, dict] = {}
    generated_index = 0
    for entry in summary.get("object_audit", []):
        object_id = entry.get("object_id") or f"<generated:{generated_index}>"
        if not entry.get("object_id"):
            generated_index += 1
        result[object_id] = {
            "scene_object_index": entry.get("scene_object_index", -1),
            "object_type": entry.get("object_type", ""),
            "material_id": entry.get("material_id", -1),
            "alpha": entry.get("alpha", 1.0),
            "triangle_count": entry.get("triangle_count", 0),
            "primary_hit_pixels": entry.get("primary_hit_pixels", 0),
            "center_screen": entry.get("center_screen", {}),
        }
    return result


def copy_frame_png(summary: dict, review_root: Path, variant_id: str) -> tuple[Path, Path]:
    frame_path = first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{variant_id}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return frame_path, png_path


def luma(pixel: tuple[int, int, int]) -> float:
    r, g, b = pixel
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def object_center_metrics(frame_path: Path, object_entry: dict | None) -> dict:
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    center = (object_entry or {}).get("center_screen", {})
    cx = float(center.get("x", width * 0.5))
    cy = float(center.get("y", height * 0.5))
    radius_x = max(8, int(width * 0.04))
    radius_y = max(8, int(height * 0.06))
    x0 = max(0, int(round(cx)) - radius_x)
    x1 = min(width, int(round(cx)) + radius_x + 1)
    y0 = max(0, int(round(cy)) - radius_y)
    y1 = min(height, int(round(cy)) + radius_y + 1)
    samples: list[float] = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            samples.append(luma(pixels[y][x]))
    samples.sort()
    if not samples:
        return {"roi": {"x0": x0, "y0": y0, "x1": x1, "y1": y1}, "luma_mean": 0.0, "luma_p95": 0.0}
    return {
        "roi": {"x0": x0, "y0": y0, "x1": x1, "y1": y1},
        "luma_mean": sum(samples) / float(len(samples)),
        "luma_p95": samples[min(len(samples) - 1, int(math.floor(0.95 * float(len(samples) - 1))))],
        "luma_max": samples[-1],
    }


def make_contact_sheet(review_root: Path, runs: list[dict]) -> Path:
    images = []
    for run in runs:
        width, height, pixels = review_artifacts.read_bmp_rgb(Path(run["frame_path"]))
        images.append((width, height, pixels, run))
    if not images:
        raise ValueError("no images for contact sheet")
    tile_w = max(width for width, _, _, _ in images)
    tile_h = max(height for _, height, _, _ in images)
    gap = 10
    sheet_w = len(images) * tile_w + (len(images) + 1) * gap
    sheet_h = tile_h + gap * 2
    bg = (246, 246, 244)
    sheet = [[bg for _ in range(sheet_w)] for _ in range(sheet_h)]
    for index, (width, height, pixels, _run) in enumerate(images):
        x0 = gap + index * (tile_w + gap)
        y0 = gap
        for y in range(height):
            for x in range(width):
                sheet[y0 + y][x0 + x] = pixels[y][x]
    path = review_root / "aquarium_real_shell_transparent_receiver_diagnostic_sheet.png"
    review_artifacts.write_png_rgb(path, sheet_w, sheet_h, sheet)
    return path


def rewrite_request(source_request: dict, variant: dict, scene_path: Path, render_root: Path) -> dict:
    request = dict(source_request)
    request["run_id"] = variant["id"]
    request["scene"] = dict(request.get("scene", {}))
    request["scene"]["runtime_scene_path"] = str(scene_path)
    request["output"] = {"root": str(render_root), "overwrite": True}
    request["progress"] = {
        "summary_path": str(render_root / "render_summary.json"),
        "progress_path": str(render_root / "render_progress.json"),
    }
    request["inspection"] = dict(request.get("inspection", {}))
    request["inspection"].update(
        {
            "caustic_mode": "off",
            "caustic_sidecar_enabled": False,
            "caustic_sidecar_strength": 0.0,
            "object_audit_enabled": True,
            "object_audit_max_dimension": 64,
        }
    )
    request["volume"] = dict(request.get("volume", {}))
    request["volume"]["visible"] = False
    request["volume"]["affects_lighting"] = False
    request["volume"]["debug_overlay"] = False
    return request


def receiver_object_hit_count(ledger: dict, object_id: str) -> int:
    for entry in ledger.get("receiver_object_hits", []):
        if entry.get("object_id") == object_id:
            return int(entry.get("hit_count", 0))
    return 0


def validate_run(variant: dict, run: dict) -> list[str]:
    failures: list[str] = []
    ledger = run["ledger"]
    objects = run["object_audit"]
    benchy_hits = receiver_object_hit_count(ledger, "benchy_floating_inside_aquarium")
    if "benchy_floating_inside_aquarium" not in objects:
        failures.append("benchy missing from object audit")
    if variant["include_glass"]:
        shell = objects.get("aquarium_glass_shell", {})
        if shell.get("triangle_count", 0) <= 0:
            failures.append("aquarium_glass_shell missing from object audit")
        if shell.get("primary_hit_pixels", 0) <= 0:
            failures.append("aquarium_glass_shell recorded zero primary hit pixels")
        if ledger["transparent_surface_hits"] <= 0:
            failures.append("glass variant recorded zero transparent surface hits")
    if variant["include_water"]:
        has_water = any(entry.get("object_type") == "water_surface" for entry in objects.values())
        if not has_water:
            failures.append("water variant did not audit generated water surface")
    if variant["expect_benchy_receiver"] and benchy_hits <= 0:
        failures.append("transparent path did not reach benchy receiver object")
    if variant["include_glass"] or variant["include_water"]:
        if ledger["transmission_rays"] <= 0:
            failures.append("transparent variant recorded zero transmission rays")
        if ledger["receiver_hits"] <= 0:
            failures.append("transparent variant recorded zero receiver hits")
        if ledger["contributing_samples"] <= 0:
            failures.append("transparent variant recorded zero contributing samples")
    stats = run.get("render_stats", {})
    if stats.get("caustic_sidecar_enabled"):
        failures.append("caustic sidecar unexpectedly enabled")
    if int(stats.get("caustic_sidecar_samples", 0)) != 0:
        failures.append("caustic sidecar samples were nonzero")
    return failures


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Aquarium Real Shell Transparent Receiver Diagnostic",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- contact sheet: `{Path(report['contact_sheet']).name}`",
        f"- report: `{Path(report['report_path']).name}`",
        "",
        "## Runs",
        "",
    ]
    for run in report["runs"]:
        ledger = run["ledger"]
        lines.append(
            f"- `{run['variant_id']}`: transmission `{ledger['transmission_rays']}`, "
            f"transparent hits `{ledger['transparent_surface_hits']}`, "
            f"receiver hits `{ledger['receiver_hits']}`, "
            f"benchy receiver hits `{run['benchy_receiver_hits']}`, "
            f"benchy luma p95 `{run['benchy_center_metrics']['luma_p95']:.2f}`"
        )
    if report["failures"]:
        lines.extend(["", "## Failures", ""])
        for failure in report["failures"]:
            lines.append(f"- {failure}")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    source_root = args.source_root.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not args.cli.exists():
        raise FileNotFoundError(f"missing ray_tracing_render_headless binary: {args.cli}")
    if review_root.exists():
        shutil.rmtree(review_root)
    review_root.mkdir(parents=True, exist_ok=True)

    runs: list[dict] = []
    failures: list[str] = []
    for variant in SOURCE_MATRIX:
        source_variant_root = source_root / variant["source"]
        variant_root = review_root / "runs" / variant["id"]
        scene_path = variant_root / "scene_runtime.json"
        request_path = variant_root / "request.json"
        render_root = variant_root / "render"
        summary_path = render_root / "render_summary.json"
        if not source_variant_root.exists():
            raise FileNotFoundError(f"missing source variant root: {source_variant_root}")
        variant_root.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_variant_root / "scene_runtime.json", scene_path)
        water_cache = source_variant_root / "water_cache"
        if water_cache.exists():
            shutil.copytree(water_cache, variant_root / "water_cache")
        request = rewrite_request(
            load_json(source_variant_root / "request.json"),
            variant,
            scene_path,
            render_root,
        )
        if request.get("volume", {}).get("enabled"):
            water_bundle = variant_root / "water_cache" / "Water Basin" / "scene_bundle.json"
            request["volume"]["source_kind"] = "scene_bundle"
            request["volume"]["source_path"] = str(water_bundle)
        write_json(request_path, request)
        elapsed = render_request(args.cli, request_path, summary_path, args.skip_render)
        if args.skip_render:
            continue
        summary = load_json(summary_path)
        frame_path, png_path = copy_frame_png(summary, review_root, variant["id"])
        objects = object_audit_digest(summary)
        ledger = fixture.ledger_digest(summary)
        benchy_entry = objects.get("benchy_floating_inside_aquarium")
        run = {
            "variant_id": variant["id"],
            "label": variant["label"],
            "source_variant": variant["source"],
            "scene_path": str(scene_path),
            "request_path": str(request_path),
            "summary_path": str(summary_path),
            "frame_path": str(frame_path),
            "png_path": str(png_path),
            "render_seconds": elapsed,
            "ledger": ledger,
            "object_audit": objects,
            "render_stats": summary.get("render_stats", {}),
            "benchy_receiver_hits": receiver_object_hit_count(
                ledger,
                "benchy_floating_inside_aquarium",
            ),
            "benchy_center_metrics": object_center_metrics(frame_path, benchy_entry),
        }
        run_failures = validate_run(variant, run)
        failures.extend(f"{variant['id']}: {failure}" for failure in run_failures)
        runs.append(run)
        if run_failures and not args.keep_going:
            break

    contact_sheet = str(make_contact_sheet(review_root, runs)) if runs else ""
    report_path = review_root / "aquarium_real_shell_transparent_receiver_diagnostic_report.json"
    report = {
        "schema": "codework_aquarium_real_shell_transparent_receiver_diagnostic_report_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "source_root": str(source_root),
        "review_root": str(review_root),
        "contact_sheet": contact_sheet,
        "report_path": str(report_path),
        "passed": not failures and bool(runs) and not args.skip_render,
        "failures": failures,
        "runs": runs,
    }
    write_json(report_path, report)
    write_index(
        review_root / "aquarium_real_shell_transparent_receiver_diagnostic_index.md",
        report,
    )
    if failures:
        print(f"diagnostic failed with {len(failures)} failure(s); report: {report_path}", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"diagnostic report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
