#!/usr/bin/env python3
"""Render the L4 caustic probe matrix and measure receiver-region luma."""

from __future__ import annotations

import argparse
import json
import math
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import run_ray_tracing_visual_matrix as visual_matrix  # noqa: E402


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = root / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless"
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def default_manifest(root: Path) -> Path:
    return root / "tests" / "fixtures" / "caustic_probe_glass_sphere" / "matrix_manifest.json"


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=default_manifest(root))
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--review-root", type=Path, default=None)
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
            "pixel_count": count
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
        "hotspot_area_ratio": hotspot_count / float(count)
    }


def run_visual_matrix(args: argparse.Namespace, review_root: Path) -> Path:
    cmd = [
        sys.executable,
        str(SCRIPT_DIR / "run_ray_tracing_visual_matrix.py"),
        "--manifest",
        str(args.manifest),
        "--cli",
        str(args.cli),
        "--review-root",
        str(review_root),
        "--contact-columns",
        "3"
    ]
    if args.skip_render:
        cmd.append("--skip-render")
    if args.keep_going:
        cmd.append("--keep-going")
    subprocess.run(cmd, check=True)
    return review_root / "matrix_report.json"


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Caustic Probe Glass Sphere Metrics",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- expected current state: `{report['expected_current_state']}`",
        f"- visual matrix report: `{report['matrix_report_path']}`",
        f"- contact sheet: `{report['contact_sheet']}`",
        "",
        "## Receiver Metrics",
        ""
    ]
    for run in report["runs"]:
        metrics = run["caustic_metrics"]
        lines.append(
            f"- `{run['cell_id']}` / `{run['integrator_3d']}`: "
            f"concentration `{metrics['receiver_luma_concentration_ratio']:.4f}`, "
            f"max/mean `{metrics['receiver_luma_max_mean_ratio']:.4f}`, "
            f"hotspot area `{metrics['hotspot_area_ratio']:.4f}`"
        )
    lines.extend([
        "",
        "These values are baseline measurements. The current matrix has no caustic-sidecar request.",
        ""
    ])
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    root = repo_root()
    manifest_path = args.manifest.resolve()
    manifest = load_json(manifest_path)
    review_root = (
        args.review_root.resolve()
        if args.review_root
        else visual_matrix.default_review_root(manifest, manifest_path).resolve()
    )

    matrix_report_path = run_visual_matrix(args, review_root)
    matrix_report = load_json(matrix_report_path)
    probe = manifest.get("caustic_probe", {})
    roi = probe.get("receiver_roi_normalized", {})
    hotspot_relative_luma = float(probe.get("hotspot_relative_luma", 0.82))

    runs = []
    failed = not bool(matrix_report.get("passed", False))
    for run in matrix_report.get("runs", []):
        frame_path = Path(run.get("frame_path", ""))
        if not frame_path.is_absolute():
            frame_path = (root / frame_path).resolve()
        metrics = receiver_metrics(frame_path, roi, hotspot_relative_luma)
        per_run_path = review_root / "caustic_metrics" / f"{run['cell_id']}.json"
        write_json(per_run_path, metrics)
        if metrics["receiver_luma_mean"] <= 0.0 or metrics["receiver_luma_max"] <= 0.0:
            failed = True
        runs.append({
            "cell_id": run.get("cell_id", ""),
            "integrator_3d": run.get("integrator_3d", ""),
            "run_id": run.get("run_id", ""),
            "frame_path": str(frame_path),
            "metrics_path": str(per_run_path),
            "caustic_solver_enabled": False,
            "caustic_metrics": metrics
        })

    report = {
        "schema_version": "ray_tracing_caustic_probe_report_v1",
        "matrix_id": manifest.get("matrix_id", manifest_path.parent.name),
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "manifest_path": str(manifest_path),
        "matrix_report_path": str(matrix_report_path),
        "contact_sheet": matrix_report.get("contact_sheet", ""),
        "expected_current_state": probe.get("expected_current_state", "no_caustic_solver"),
        "receiver_roi_normalized": roi,
        "hotspot_relative_luma": hotspot_relative_luma,
        "runs": runs,
        "passed": not failed and len(runs) > 0
    }
    write_json(review_root / "caustic_probe_report.json", report)
    write_index(review_root / "caustic_probe_index.md", report)
    print(review_root / "caustic_probe_report.json")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
