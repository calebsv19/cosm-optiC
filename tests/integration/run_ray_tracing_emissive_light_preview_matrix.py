#!/usr/bin/env python3
"""Render the emissive-light preview matrix and record emitter policy metrics."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import run_ray_tracing_visual_matrix as visual_matrix  # noqa: E402


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def default_manifest() -> Path:
    return (
        repo_root()
        / "tests"
        / "fixtures"
        / "emissive_light_preview_matrix"
        / "matrix_manifest.json"
    )


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


def roi_luma(frame_path: Path, roi: dict, prefix: str = "roi") -> dict:
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    x0 = max(0, min(width, int(width * float(roi.get("x0", 0.0)))))
    y0 = max(0, min(height, int(height * float(roi.get("y0", 0.0)))))
    x1 = max(x0 + 1, min(width, int(width * float(roi.get("x1", 1.0)))))
    y1 = max(y0 + 1, min(height, int(height * float(roi.get("y1", 1.0)))))
    values = [luma(pixels[y][x]) for y in range(y0, y1) for x in range(x0, x1)]
    mean = sum(values) / len(values) if values else 0.0
    values_sorted = sorted(values)
    p95 = values_sorted[int((len(values_sorted) - 1) * 0.95)] if values_sorted else 0.0
    return {
        "width": width,
        "height": height,
        f"{prefix}_pixels": len(values),
        f"{prefix}_luma_mean": mean,
        f"{prefix}_luma_max": max(values) if values else 0.0,
        f"{prefix}_luma_p95": p95,
    }


def min_check(errors: list[str], metrics: dict, key: str, expected: dict, expected_key: str) -> None:
    if expected_key not in expected:
        return
    value = float(metrics.get(key, 0.0))
    required = float(expected[expected_key])
    if value < required:
        errors.append(f"{key}={value} below {expected_key}={required}")


def max_check(errors: list[str], metrics: dict, key: str, expected: dict, expected_key: str) -> None:
    if expected_key not in expected:
        return
    value = float(metrics.get(key, 0.0))
    limit = float(expected[expected_key])
    if value > limit:
        errors.append(f"{key}={value} above {expected_key}={limit}")


def run_visual_matrix(args: argparse.Namespace, review_root: Path) -> Path:
    command = [
        sys.executable,
        str(SCRIPT_DIR / "run_ray_tracing_visual_matrix.py"),
        "--manifest",
        str(args.manifest),
        "--review-root",
        str(review_root),
        "--contact-columns",
        str(args.contact_columns),
    ]
    if args.skip_render:
        command.append("--skip-render")
    subprocess.run(command, cwd=repo_root(), check=True)
    return review_root / "matrix_report.json"


def write_index(report_path: Path, report: dict) -> None:
    lines = [
        "# Emissive Light Preview Metrics",
        "",
        f"- matrix report: `{report['matrix_report']}`",
        f"- passed: `{report['passed']}`",
        "",
        "## Runs",
        "",
    ]
    for run in report["runs"]:
        metrics = run["metrics"]
        lines.append(
            f"- `{run['cell_id']}`: enabled `{metrics['enabled_lights']}`, "
            f"rect `{metrics['rect_lights']}`, mesh `{metrics['mesh_emissive_lights']}`, "
            f"material emitters `{metrics['material_emitters']}`, "
            f"sampler-only `{metrics['mesh_area_sampler_only']}`, "
            f"one-sided `{metrics['one_sided_lights']}`, "
            f"emissive candidates `{metrics['emissive_candidates']}`, "
            f"area candidates `{metrics['emissive_area_candidates']}`, "
            f"ROI mean `{metrics['roi_luma_mean']:.3f}`, "
            f"wall mean `{metrics.get('wall_luma_mean', 0.0):.3f}`"
        )
        if run["errors"]:
            for error in run["errors"]:
                lines.append(f"  - error: {error}")
    report_path.with_name("emitter_preview_index.md").write_text(
        "\n".join(lines) + "\n",
        encoding="utf-8",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=default_manifest())
    parser.add_argument("--review-root", type=Path, default=None)
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--contact-columns", type=int, default=2)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.manifest = args.manifest.resolve()
    manifest = load_json(args.manifest)
    review_root = (
        args.review_root.resolve()
        if args.review_root
        else visual_matrix.default_review_root(manifest, args.manifest).resolve()
    )
    matrix_report_path = run_visual_matrix(args, review_root)
    matrix_report = load_json(matrix_report_path)
    preview = manifest.get("emissive_preview", {})
    expectations = preview.get("expectations", {})
    roi = preview.get("roi_normalized", {})
    wall_roi = preview.get("wall_roi_normalized", {})
    runs = []
    passed = bool(matrix_report.get("passed", False))
    for run in matrix_report.get("runs", []):
        cell_id = run.get("cell_id", "")
        summary = load_json(Path(run["summary_copy"]))
        registered = summary.get("registered_lights", {})
        render_stats = summary.get("render_stats", {})
        shape_counts = registered.get("shape_counts", {})
        source_counts = registered.get("source_counts", {})
        emission_profile_counts = registered.get("emission_profile_counts", {})
        luma_metrics = roi_luma(Path(run["frame_path"]), roi)
        if wall_roi:
            wall_metrics = roi_luma(Path(run["frame_path"]), wall_roi, "wall")
            luma_metrics.update(
                {
                    key: value
                    for key, value in wall_metrics.items()
                    if key not in {"width", "height"}
                }
            )
        metrics = {
            "enabled_lights": int(registered.get("enabled_count", 0)),
            "rect_lights": int(shape_counts.get("rect", 0)),
            "mesh_emissive_lights": int(shape_counts.get("mesh_emissive", 0)),
            "material_emitters": int(source_counts.get("material_emitter", 0)),
            "material_emitter_enabled": int(
                registered.get("material_emitter_enabled_count", 0)
            ),
            "mesh_area_sampler_only": int(
                registered.get("mesh_area_sampler_only_count", 0)
            ),
            "one_sided_lights": int(emission_profile_counts.get("one_sided", 0)),
            "two_sided_lights": int(emission_profile_counts.get("two_sided", 0)),
            "omni_lights": int(emission_profile_counts.get("omni", 0)),
            "emissive_candidates": int(registered.get("emissive_candidate_count", 0)),
            "emissive_area": float(registered.get("emissive_area", 0.0)),
            "emissive_weight": float(registered.get("emissive_weight", 0.0)),
            "emissive_proxy_radius_max": float(
                registered.get("emissive_proxy_radius_max", 0.0)
            ),
            "emissive_area_candidates": int(
                render_stats.get("emissive_area_candidate_count", 0)
            ),
            "emissive_area_selected_candidates": int(
                render_stats.get("emissive_area_selected_candidates", 0)
            ),
        }
        metrics.update(luma_metrics)
        expected = expectations.get(cell_id, {})
        errors: list[str] = []
        min_check(errors, metrics, "enabled_lights", expected, "min_enabled_lights")
        min_check(errors, metrics, "rect_lights", expected, "min_rect_lights")
        min_check(
            errors,
            metrics,
            "mesh_emissive_lights",
            expected,
            "min_mesh_emissive_lights",
        )
        min_check(errors, metrics, "material_emitters", expected, "min_material_emitters")
        min_check(
            errors,
            metrics,
            "material_emitter_enabled",
            expected,
            "min_material_emitter_enabled",
        )
        max_check(
            errors,
            metrics,
            "material_emitter_enabled",
            expected,
            "max_material_emitter_enabled",
        )
        min_check(
            errors,
            metrics,
            "mesh_area_sampler_only",
            expected,
            "min_mesh_area_sampler_only",
        )
        min_check(errors, metrics, "one_sided_lights", expected, "min_one_sided_lights")
        min_check(errors, metrics, "two_sided_lights", expected, "min_two_sided_lights")
        min_check(errors, metrics, "omni_lights", expected, "min_omni_lights")
        max_check(
            errors,
            metrics,
            "mesh_area_sampler_only",
            expected,
            "max_mesh_area_sampler_only",
        )
        min_check(
            errors,
            metrics,
            "emissive_candidates",
            expected,
            "min_emissive_candidates",
        )
        min_check(
            errors,
            metrics,
            "emissive_area_candidates",
            expected,
            "min_emissive_area_candidates",
        )
        min_check(errors, metrics, "roi_luma_mean", expected, "min_roi_mean_luma")
        min_check(errors, metrics, "wall_luma_mean", expected, "min_wall_luma_mean")
        if errors:
            passed = False
        runs.append({"cell_id": cell_id, "metrics": metrics, "errors": errors})

    report = {
        "schema_version": "ray_tracing_emissive_light_preview_report_v1",
        "matrix_report": str(matrix_report_path),
        "contact_sheet": matrix_report.get("contact_sheet", ""),
        "passed": passed,
        "runs": runs,
    }
    report_path = review_root / "emitter_preview_report.json"
    write_json(report_path, report)
    write_index(report_path, report)
    print(report_path)
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
