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


def policy_summary(ledger: dict[str, Any], top: int) -> dict[str, Any]:
    policy = ledger.get("direct_light_visibility_policy")
    if not isinstance(policy, dict):
        return {}
    source_evaluations = as_int(policy.get("source_evaluations"))
    visibility_queries = as_int(policy.get("visibility_sample_queries"))
    evaluated_samples = as_int(policy.get("evaluated_samples"))
    return {
        "source_evaluations": source_evaluations,
        "evaluated_samples": evaluated_samples,
        "visibility_sample_queries": visibility_queries,
        "visibility_sample_queries_per_source": (
            float(visibility_queries) / float(source_evaluations)
            if source_evaluations > 0 else 0.0
        ),
        "evaluated_samples_per_source": (
            float(evaluated_samples) / float(source_evaluations)
            if source_evaluations > 0 else 0.0
        ),
        "luma_span_avg": float(policy.get("luma_span_avg", 0.0) or 0.0),
        "luma_span_max": float(policy.get("luma_span_max", 0.0) or 0.0),
        "callers": [
            {"name": name, "count": count}
            for name, count in sorted_counts(policy.get("caller_counts"))[:top]
        ],
        "source_kinds": [
            {"name": name, "count": count}
            for name, count in sorted_counts(policy.get("source_kind_counts"))[:top]
        ],
        "origins": [
            {"name": name, "count": count}
            for name, count in sorted_counts(policy.get("source_origin_counts"))[:top]
        ],
        "profiles": [
            {"name": name, "count": count}
            for name, count in sorted_counts(policy.get("emission_profile_counts"))[:top]
        ],
        "outcomes": [
            {"name": name, "count": count}
            for name, count in sorted_counts(policy.get("outcome_counts"))[:top]
        ],
        "stop_reasons": [
            {"name": name, "count": count}
            for name, count in sorted_counts(policy.get("stop_reason_counts"))[:top]
        ],
        "sample_buckets": [
            {"name": name, "count": count}
            for name, count in sorted_counts(policy.get("sample_bucket_counts"))[:top]
        ],
        "distance_buckets": [
            {"name": name, "count": count}
            for name, count in sorted_counts(policy.get("distance_bucket_counts"))[:top]
        ],
        "importance_buckets": [
            {"name": name, "count": count}
            for name, count in sorted_counts(policy.get("importance_bucket_counts"))[:top]
        ],
        "material_emitter_rect_policy": policy.get("material_emitter_rect_policy")
        if isinstance(policy.get("material_emitter_rect_policy"), dict) else {},
    }


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
        "top": top,
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
        "direct_light_visibility_policy": policy_summary(ledger, top),
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
        policy = report.get("direct_light_visibility_policy")
        if isinstance(policy, dict) and policy:
            top = as_int(report.get("top"))
            print("  Direct-light visibility policy:")
            print(f"    source_evaluations: {format_count(as_int(policy['source_evaluations']))}")
            print(f"    evaluated_samples: {format_count(as_int(policy['evaluated_samples']))}")
            print(
                "    visibility_sample_queries: "
                f"{format_count(as_int(policy['visibility_sample_queries']))}"
            )
            print(
                "    visibility_sample_queries_per_source: "
                f"{float(policy['visibility_sample_queries_per_source']):.3f}"
            )
            print(
                "    evaluated_samples_per_source: "
                f"{float(policy['evaluated_samples_per_source']):.3f}"
            )
            print(f"    luma_span_avg: {float(policy['luma_span_avg']):.6f}")
            print(f"    luma_span_max: {float(policy['luma_span_max']):.6f}")
            print_section("    Callers", policy["callers"], total_label="source evaluations")
            print_section("    Source kinds", policy["source_kinds"], total_label="source evaluations")
            print_section("    Source origins", policy["origins"], total_label="source evaluations")
            print_section("    Emission profiles", policy["profiles"], total_label="source evaluations")
            print_section("    Outcomes", policy["outcomes"], total_label="source evaluations")
            print_section("    Stop reasons", policy["stop_reasons"], total_label="source evaluations")
            print_section("    Sample buckets", policy["sample_buckets"], total_label="source evaluations")
            print_section("    Distance buckets", policy["distance_buckets"], total_label="source evaluations")
            print_section(
                "    Importance buckets",
                policy["importance_buckets"],
                total_label="source evaluations",
            )
            rect_policy = policy.get("material_emitter_rect_policy")
            if isinstance(rect_policy, dict) and rect_policy:
                rect_sources = as_int(rect_policy.get("source_evaluations"))
                rect_samples = as_int(rect_policy.get("evaluated_samples"))
                rect_queries = as_int(rect_policy.get("visibility_sample_queries"))
                print("    Material-emitter rect policy:")
                print(f"      source_evaluations: {format_count(rect_sources)}")
                print(f"      evaluated_samples: {format_count(rect_samples)}")
                print(f"      visibility_sample_queries: {format_count(rect_queries)}")
                print(
                    "      evaluated_samples_per_source: "
                    f"{(float(rect_samples) / float(rect_sources)) if rect_sources else 0.0:.3f}"
                )
                print(
                    "      visibility_sample_queries_per_source: "
                    f"{(float(rect_queries) / float(rect_sources)) if rect_sources else 0.0:.3f}"
                )
                print_section(
                    "      Distance buckets",
                    [
                        {"name": name, "count": count}
                        for name, count in sorted_counts(
                            rect_policy.get("distance_bucket_counts")
                        )[:top]
                    ],
                    total_label="material-emitter rect sources",
                )
                print_section(
                    "      Importance buckets",
                    [
                        {"name": name, "count": count}
                        for name, count in sorted_counts(
                            rect_policy.get("importance_bucket_counts")
                        )[:top]
                    ],
                    total_label="material-emitter rect sources",
                )
                print_section(
                    "      Evaluated samples by distance",
                    [
                        {"name": name, "count": count}
                        for name, count in sorted_counts(
                            rect_policy.get("evaluated_samples_by_distance")
                        )[:top]
                    ],
                    total_label="evaluated samples",
                )
                print_section(
                    "      Visibility queries by distance",
                    [
                        {"name": name, "count": count}
                        for name, count in sorted_counts(
                            rect_policy.get("visibility_sample_queries_by_distance")
                        )[:top]
                    ],
                    total_label="visibility queries",
                )
                print_section(
                    "      Evaluated samples by importance",
                    [
                        {"name": name, "count": count}
                        for name, count in sorted_counts(
                            rect_policy.get("evaluated_samples_by_importance")
                        )[:top]
                    ],
                    total_label="evaluated samples",
                )
                print_section(
                    "      Visibility queries by importance",
                    [
                        {"name": name, "count": count}
                        for name, count in sorted_counts(
                            rect_policy.get("visibility_sample_queries_by_importance")
                        )[:top]
                    ],
                    total_label="visibility queries",
                )


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
