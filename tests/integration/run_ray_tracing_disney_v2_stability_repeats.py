#!/usr/bin/env python3
"""Run repeated Disney v2 promotion and skull smoke proofs for timing stability."""

from __future__ import annotations

import argparse
import json
import math
import platform
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


def ray_tracing_root() -> Path:
    return Path(__file__).resolve().parents[2]


def codework_root() -> Path:
    return Path(__file__).resolve().parents[3]


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = root / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless"
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def default_output_root() -> Path:
    return (
        codework_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "disney_v2_stability"
        / "d220_d221_repeats"
    )


def parse_args() -> argparse.Namespace:
    root = ray_tracing_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repeat", type=int, default=2)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--output-root", type=Path, default=default_output_root())
    parser.add_argument("--skip-promotion", action="store_true")
    parser.add_argument("--skip-skull", action="store_true")
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def run_command(cmd: list[str], cwd: Path) -> float:
    start = time.perf_counter()
    result = subprocess.run(cmd, cwd=str(cwd), stderr=subprocess.PIPE, text=True)
    elapsed = time.perf_counter() - start
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed with exit {result.returncode}: {' '.join(cmd)}\n{result.stderr}"
        )
    return elapsed


def series_stats(values: list[float]) -> dict:
    if not values:
        return {
            "count": 0,
            "min": 0.0,
            "max": 0.0,
            "mean": 0.0,
            "stddev": 0.0,
            "relative_spread": 0.0,
        }
    mean = sum(values) / len(values)
    variance = sum((value - mean) ** 2 for value in values) / len(values)
    spread = (max(values) - min(values)) / mean if mean > 1.0e-9 else 0.0
    return {
        "count": len(values),
        "min": min(values),
        "max": max(values),
        "mean": mean,
        "stddev": math.sqrt(variance),
        "relative_spread": spread,
    }


def run_promotion_repeats(root: Path,
                          cli: Path,
                          output_root: Path,
                          repeat: int) -> list[dict]:
    tool = root / "tools" / "measure_disney_v2_promotion_gates.py"
    runs = []
    for index in range(repeat):
        repeat_root = output_root / "d220_promotion_thresholds" / f"repeat_{index + 1:02d}"
        command_elapsed = run_command(
            [
                sys.executable,
                str(tool),
                "--cli",
                str(cli),
                "--output-root",
                str(repeat_root),
            ],
            root,
        )
        report_path = repeat_root / "promotion_gate_report.json"
        report = load_json(report_path)
        runs.append({
            "repeat_index": index + 1,
            "command_elapsed_seconds": command_elapsed,
            "report_path": str(report_path),
            "hard_gates_passed": bool(report.get("hard_gates_passed", False)),
            "promotion_ready": bool(report.get("promotion_ready", False)),
            "thresholds_passed": bool(
                report.get("performance_thresholds", {}).get("passed", False)
            ),
            "runs": report.get("runs", []),
            "threshold_results": report.get("performance_thresholds", {}).get("scene_results", []),
        })
    return runs


def prepare_skull(root: Path, output_root: Path) -> Path:
    prepare = root / "tests" / "integration" / "prepare_ray_tracing_skull_high_triangle_matrix.py"
    source_root = output_root / "skull_high_triangle_local" / "source_scene"
    run_command([sys.executable, str(prepare), "--output-root", str(source_root)], root)
    return source_root / "matrix_manifest.json"


def run_skull_repeats(root: Path,
                      cli: Path,
                      output_root: Path,
                      repeat: int) -> list[dict]:
    manifest = prepare_skull(root, output_root)
    runner = root / "tests" / "integration" / "run_ray_tracing_visual_matrix.py"
    runs = []
    for index in range(repeat):
        review_root = output_root / "skull_high_triangle_local" / f"repeat_{index + 1:02d}" / "matrix_review"
        command_elapsed = run_command(
            [
                sys.executable,
                str(runner),
                "--cli",
                str(cli),
                "--manifest",
                str(manifest),
                "--group",
                "skull_high_triangle_smoke",
                "--review-root",
                str(review_root),
            ],
            root,
        )
        report_path = review_root / "matrix_report.json"
        report = load_json(report_path)
        runs.append({
            "repeat_index": index + 1,
            "command_elapsed_seconds": command_elapsed,
            "report_path": str(report_path),
            "passed": bool(report.get("passed", False)),
            "runs": report.get("runs", []),
            "comparisons": report.get("comparisons", []),
        })
    return runs


def promotion_scene_series(promotion_runs: list[dict]) -> dict:
    series: dict[str, dict[str, list[float]]] = {}
    for repeat in promotion_runs:
        for run in repeat.get("runs", []):
            scene = run.get("scene_id", "")
            integrator = run.get("integrator", "")
            key = f"{scene}/{integrator}"
            series.setdefault(key, {"elapsed_seconds": []})
            series[key]["elapsed_seconds"].append(float(run.get("elapsed_seconds", 0.0)))
    return {
        key: {
            "elapsed_seconds": series_stats(values["elapsed_seconds"]),
        }
        for key, values in sorted(series.items())
    }


def skull_series(skull_runs: list[dict]) -> dict:
    series: dict[str, dict[str, list[float]]] = {}
    for repeat in skull_runs:
        for run in repeat.get("runs", []):
            key = f"{run.get('cell_id', '')}/{run.get('integrator_3d', '')}"
            series.setdefault(key, {"elapsed_seconds": [], "triangles": [], "overflows": []})
            series[key]["elapsed_seconds"].append(float(run.get("elapsed_seconds", 0.0) or 0.0))
            stats = run.get("stats", {})
            series[key]["triangles"].append(float(stats.get("bvh_triangle_count", 0)))
            series[key]["overflows"].append(float(stats.get("bvh_trace_overflows", 0)))
    return {
        key: {
            "elapsed_seconds": series_stats(values["elapsed_seconds"]),
            "bvh_triangle_count": series_stats(values["triangles"]),
            "bvh_trace_overflows": series_stats(values["overflows"]),
        }
        for key, values in sorted(series.items())
    }


def write_markdown(path: Path, report: dict) -> None:
    lines = [
        "# Disney V2 D2.20/D2.21 Stability Repeats",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- repeat count: `{report['repeat_count']}`",
        f"- passed: `{str(report['passed']).lower()}`",
        "",
        "## Policy",
        "",
        "- Skull/high-triangle proof remains private/manual and does not become a shipped fixture.",
        "- Skull smoke stays direct-light versus Disney v2 for now; shipped Disney is intentionally excluded for cost.",
        "- Low/moderate imported-mesh fixtures remain the recurring candidate promotion gate.",
        "",
    ]
    if report["promotion_repeats"]:
        lines.extend(["## D2.20 Promotion Gate Series", ""])
        for key, stats in report["promotion_series"].items():
            elapsed = stats["elapsed_seconds"]
            lines.append(
                f"- `{key}`: mean `{elapsed['mean']:.3f}s`, min `{elapsed['min']:.3f}s`, "
                f"max `{elapsed['max']:.3f}s`, spread `{elapsed['relative_spread']:.3f}`"
            )
    if report["skull_repeats"]:
        lines.extend(["", "## D2.21 Skull Smoke Series", ""])
        for key, stats in report["skull_series"].items():
            elapsed = stats["elapsed_seconds"]
            triangles = stats["bvh_triangle_count"]
            overflows = stats["bvh_trace_overflows"]
            lines.append(
                f"- `{key}`: mean `{elapsed['mean']:.3f}s`, min `{elapsed['min']:.3f}s`, "
                f"max `{elapsed['max']:.3f}s`, triangles `{triangles['mean']:.0f}`, "
                f"overflows `{overflows['max']:.0f}`"
            )
    lines.extend(["", "## Artifacts", ""])
    lines.append(f"- JSON: `{report['report_path']}`")
    for repeat in report["promotion_repeats"]:
        lines.append(f"- promotion repeat {repeat['repeat_index']}: `{repeat['report_path']}`")
    for repeat in report["skull_repeats"]:
        lines.append(f"- skull repeat {repeat['repeat_index']}: `{repeat['report_path']}`")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    if args.repeat < 1:
        print("--repeat must be >= 1", file=sys.stderr)
        return 2
    root = ray_tracing_root()
    cli = args.cli.resolve()
    output_root = args.output_root.resolve()
    if not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2

    output_root.mkdir(parents=True, exist_ok=True)
    promotion_repeats = [] if args.skip_promotion else run_promotion_repeats(
        root,
        cli,
        output_root,
        args.repeat,
    )
    skull_repeats = [] if args.skip_skull else run_skull_repeats(root, cli, output_root, args.repeat)
    report_path = output_root / "stability_report.json"
    report = {
        "schema_version": "ray_tracing_disney_v2_stability_repeats_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "repeat_count": args.repeat,
        "output_root": str(output_root),
        "report_path": str(report_path),
        "policy": {
            "skull_high_triangle_fixture": "private_manual_not_shipped",
            "skull_route_comparison": "direct_light_vs_disney_v2_only",
            "recurring_gate": "low_to_moderate_imported_mesh_thresholds",
        },
        "promotion_repeats": promotion_repeats,
        "promotion_series": promotion_scene_series(promotion_repeats),
        "skull_repeats": skull_repeats,
        "skull_series": skull_series(skull_repeats),
        "passed": (
            all(run["hard_gates_passed"] and run["thresholds_passed"] for run in promotion_repeats)
            and all(run["passed"] for run in skull_repeats)
        ),
    }
    write_json(report_path, report)
    write_markdown(output_root / "stability_report.md", report)
    print(report_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
