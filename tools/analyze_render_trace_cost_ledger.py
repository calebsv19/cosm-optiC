#!/usr/bin/env python3
"""Rank RayTracing render trace cost ledger summaries.

This reads existing headless `render_summary.json` files. It does not render,
mutate requests, or change sampling policy.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"{path} did not contain a JSON object")
    return data


def as_int(value: Any) -> int:
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return int(value)
    if isinstance(value, str):
        try:
            return int(value)
        except ValueError:
            return 0
    return 0


def sorted_counts(counts: Any) -> list[tuple[str, int]]:
    if not isinstance(counts, dict):
        return []
    rows = [(str(key), as_int(value)) for key, value in counts.items()]
    rows = [(key, value) for key, value in rows if value > 0]
    rows.sort(key=lambda row: (-row[1], row[0]))
    return rows


def percent(value: int, total: int) -> float:
    return (float(value) * 100.0 / float(total)) if total > 0 else 0.0


def format_count(value: int) -> str:
    return f"{value:,}"


def ledger_from_summary(summary: dict[str, Any], path: Path) -> dict[str, Any]:
    ledger = summary.get("render_trace_cost_ledger")
    if not isinstance(ledger, dict):
        raise ValueError(f"{path} has no render_trace_cost_ledger object")
    if not ledger.get("enabled"):
        raise ValueError(f"{path} has render_trace_cost_ledger.enabled=false")
    return ledger


def rank_class_depth(ledger: dict[str, Any]) -> list[tuple[str, str, int]]:
    raw = ledger.get("ray_class_depth_counts")
    rows: list[tuple[str, str, int]] = []
    if not isinstance(raw, dict):
        return rows
    for ray_class, depth_counts in raw.items():
        if not isinstance(depth_counts, dict):
            continue
        for depth_label, value in depth_counts.items():
            count = as_int(value)
            if count > 0:
                rows.append((str(ray_class), str(depth_label), count))
    rows.sort(key=lambda row: (-row[2], row[0], row[1]))
    return rows


def summarize(path: Path, top: int) -> dict[str, Any]:
    summary = load_json(path)
    ledger = ledger_from_summary(summary, path)
    total = as_int(ledger.get("total_rays"))
    ray_classes = sorted_counts(ledger.get("ray_class_counts"))
    depths = sorted_counts(ledger.get("path_depth_counts"))
    materials = sorted_counts(ledger.get("material_family_counts"))
    class_depth = rank_class_depth(ledger)

    return {
        "path": str(path),
        "run_id": str(summary.get("run_id", "")),
        "integrator_3d": str(summary.get("integrator_3d", "")),
        "total_rays": total,
        "ray_classes": [
            {"name": name, "count": count, "percent_of_total": percent(count, total)}
            for name, count in ray_classes[:top]
        ],
        "depths": [
            {"name": name, "count": count, "percent_of_total": percent(count, total)}
            for name, count in depths[:top]
        ],
        "class_depth": [
            {
                "ray_class": ray_class,
                "depth": depth,
                "count": count,
                "percent_of_total": percent(count, total),
            }
            for ray_class, depth, count in class_depth[:top]
        ],
        "materials": [
            {"name": name, "count": count}
            for name, count in materials[:top]
        ],
    }


def print_section(title: str, rows: list[dict[str, Any]], total_label: str = "of rays") -> None:
    print(f"{title}:")
    if not rows:
        print("  none")
        return
    for row in rows:
        if "ray_class" in row:
            label = f"{row['ray_class']} / {row['depth']}"
        else:
            label = str(row["name"])
        count = as_int(row["count"])
        if "percent_of_total" in row:
            print(f"  {label}: {format_count(count)} ({row['percent_of_total']:.2f}% {total_label})")
        else:
            print(f"  {label}: {format_count(count)}")


def print_text_report(reports: list[dict[str, Any]]) -> None:
    for index, report in enumerate(reports):
        if index:
            print()
        title = report["run_id"] or Path(report["path"]).name
        print(f"Summary: {title}")
        print(f"  path: {report['path']}")
        if report["integrator_3d"]:
            print(f"  integrator_3d: {report['integrator_3d']}")
        print(f"  total_rays: {format_count(as_int(report['total_rays']))}")
        print_section("  Top ray classes", report["ray_classes"])
        print_section("  Top class/depth pairs", report["class_depth"])
        print_section("  Top depth buckets", report["depths"])
        print_section("  Top material-family hits", report["materials"], total_label="hit records")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("summary", nargs="+", type=Path, help="render_summary.json path")
    parser.add_argument("--top", type=int, default=5, help="rows per ranking section")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.top <= 0:
        raise SystemExit("--top must be positive")
    reports = [summarize(path, args.top) for path in args.summary]
    if args.json:
        json.dump({"reports": reports}, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
    else:
        print_text_report(reports)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
