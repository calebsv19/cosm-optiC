#!/usr/bin/env python3
"""Private BVH A/B runner for the high-triangle RayTracing stress lane.

The harness builds two headless binaries from the current workspace:

- A: the legacy sort-scratch baseline when current source is already MB1, or
  the source as it exists when the harness starts in pre-MB1 worktrees
- B: the landed/current MB1 source when current source is already MB1, or a
  temporary MB1 variant in pre-MB1 worktrees

The source file is restored before render samples run. Outputs stay under
ray_tracing/_private_workspace_artifacts/high_triangle_stress.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_ROOT = REPO_ROOT / "_private_workspace_artifacts/high_triangle_stress"
SOURCE_PATH = REPO_ROOT / "src/render/runtime_triangle_bvh_3d.c"
HEADLESS_BINARY = REPO_ROOT / "build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless"
STRESS_RUNNER = REPO_ROOT / "tools/run_private_high_triangle_stress.py"


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run(command: list[str], *, cwd: Path = REPO_ROOT) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(command, cwd=str(cwd), text=True, capture_output=True)
    if completed.returncode != 0:
        raise RuntimeError(
            "command failed:\n"
            + " ".join(command)
            + f"\nreturncode={completed.returncode}\nstdout={completed.stdout}\nstderr={completed.stderr}"
        )
    return completed


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise ValueError(f"expected one {label} replacement, found {count}")
    return text.replace(old, new, 1)


def remove_once(text: str, old: str, label: str) -> str:
    return replace_once(text, old, "", label)


def mb1_variant_source(source: str) -> str:
    text = source
    text = remove_once(
        text,
        """typedef struct {
    int index;
    double centroid;
} RuntimeTriangleBVH3DSortItem;

""",
        "sort scratch item typedef",
    )
    text = remove_once(
        text,
        "    RuntimeTriangleBVH3DSortItem* sortScratch;\n",
        "sort scratch field",
    )
    text = replace_once(
        text,
        """    if (!bvh || !bvh->indices || !bvh->centroids || !bvh->sortScratch ||
        start < 0 || count <= 0 || start + count > bvh->indexCount) {
        char diag[256];
        snprintf(diag,
                 sizeof(diag),
                 "partition indices invalid input: start=%d count=%d index_count=%d axis=%d has_indices=%s has_centroids=%s has_sort_scratch=%s",
                 start,
                 count,
                 bvh ? bvh->indexCount : -1,
                 axis,
                 (bvh && bvh->indices) ? "true" : "false",
                 (bvh && bvh->centroids) ? "true" : "false",
                 (bvh && bvh->sortScratch) ? "true" : "false");
""",
        """    if (!bvh || !bvh->indices || !bvh->centroids || start < 0 || count <= 0 ||
        start + count > bvh->indexCount) {
        char diag[256];
        snprintf(diag,
                 sizeof(diag),
                 "partition indices invalid input: start=%d count=%d index_count=%d axis=%d has_indices=%s has_centroids=%s",
                 start,
                 count,
                 bvh ? bvh->indexCount : -1,
                 axis,
                 (bvh && bvh->indices) ? "true" : "false",
                 (bvh && bvh->centroids) ? "true" : "false");
""",
        "partition input guard",
    )
    text = remove_once(text, "    free(mesh->bvh->sortScratch);\n", "clear sort scratch free")
    text = remove_once(
        text,
        """    free(bvh->sortScratch);
    bvh->sortScratch = NULL;
""",
        "build scratch free",
    )
    text = remove_once(text, "    copy->sortScratch = NULL;\n", "copy sort scratch init")
    text = remove_once(
        text,
        """    if (source_bvh->sortScratch && source_bvh->indexCount > 0) {
        copy->sortScratch =
            (RuntimeTriangleBVH3DSortItem*)malloc(sizeof(*copy->sortScratch) *
                                                  (size_t)source_bvh->indexCount);
        if (!copy->sortScratch) {
            runtime_triangle_bvh_destroy(copy);
            return false;
        }
        memcpy(copy->sortScratch,
               source_bvh->sortScratch,
               sizeof(*copy->sortScratch) * (size_t)source_bvh->indexCount);
    }
""",
        "copy sort scratch allocation",
    )
    text = remove_once(
        text,
        """    bvh->sortScratch =
        (RuntimeTriangleBVH3DSortItem*)malloc(sizeof(*bvh->sortScratch) *
                                              (size_t)bvh->indexCount);
""",
        "sort scratch allocation",
    )
    text = replace_once(
        text,
        """    if (!bvh->indices || !bvh->centroids || !bvh->sortScratch) {
        char diag[384];
        snprintf(diag,
                 sizeof(diag),
                 "build scratch allocation failed: triangle_count=%d indices=%s centroids=%s sort_scratch=%s index_bytes=%llu centroid_bytes=%llu sort_scratch_bytes=%llu",
                 bvh->indexCount,
                 bvh->indices ? "ok" : "failed",
                 bvh->centroids ? "ok" : "failed",
                 bvh->sortScratch ? "ok" : "failed",
                 (unsigned long long)(sizeof(*bvh->indices) *
                                      (size_t)bvh->indexCount),
                 (unsigned long long)(sizeof(*bvh->centroids) *
                                      (size_t)bvh->indexCount),
                 (unsigned long long)(sizeof(*bvh->sortScratch) *
                                      (size_t)bvh->indexCount));
""",
        """    if (!bvh->indices || !bvh->centroids) {
        char diag[384];
        snprintf(diag,
                 sizeof(diag),
                 "build scratch allocation failed: triangle_count=%d indices=%s centroids=%s index_bytes=%llu centroid_bytes=%llu",
                 bvh->indexCount,
                 bvh->indices ? "ok" : "failed",
                 bvh->centroids ? "ok" : "failed",
                 (unsigned long long)(sizeof(*bvh->indices) *
                                      (size_t)bvh->indexCount),
                 (unsigned long long)(sizeof(*bvh->centroids) *
                                      (size_t)bvh->indexCount));
""",
        "allocation guard",
    )
    text = replace_once(
        text,
        """    bvh->sortScratchBytes =
        (uint64_t)sizeof(*bvh->sortScratch) * (uint64_t)bvh->indexCount;
""",
        "    bvh->sortScratchBytes = 0u;\n",
        "sort scratch byte accounting",
    )
    return text


def legacy_sort_scratch_source(source: str) -> str:
    text = source
    text = replace_once(
        text,
        """typedef struct {
    Vec3 min;
    Vec3 max;
    int left;
""",
        """typedef struct {
    int index;
    double centroid;
} RuntimeTriangleBVH3DSortItem;

typedef struct {
    Vec3 min;
    Vec3 max;
    int left;
""",
        "sort scratch item typedef restore",
    )
    text = replace_once(
        text,
        "    Vec3* centroids;\n",
        "    Vec3* centroids;\n    RuntimeTriangleBVH3DSortItem* sortScratch;\n",
        "sort scratch field restore",
    )
    text = replace_once(
        text,
        """    if (!bvh || !bvh->indices || !bvh->centroids || start < 0 || count <= 0 ||
        start + count > bvh->indexCount) {
        char diag[256];
        snprintf(diag,
                 sizeof(diag),
                 "partition indices invalid input: start=%d count=%d index_count=%d axis=%d has_indices=%s has_centroids=%s",
                 start,
                 count,
                 bvh ? bvh->indexCount : -1,
                 axis,
                 (bvh && bvh->indices) ? "true" : "false",
                 (bvh && bvh->centroids) ? "true" : "false");
""",
        """    if (!bvh || !bvh->indices || !bvh->centroids || !bvh->sortScratch ||
        start < 0 || count <= 0 || start + count > bvh->indexCount) {
        char diag[256];
        snprintf(diag,
                 sizeof(diag),
                 "partition indices invalid input: start=%d count=%d index_count=%d axis=%d has_indices=%s has_centroids=%s has_sort_scratch=%s",
                 start,
                 count,
                 bvh ? bvh->indexCount : -1,
                 axis,
                 (bvh && bvh->indices) ? "true" : "false",
                 (bvh && bvh->centroids) ? "true" : "false",
                 (bvh && bvh->sortScratch) ? "true" : "false");
""",
        "partition input guard restore",
    )
    text = replace_once(
        text,
        """    free(mesh->bvh->centroids);
    free(mesh->bvh);
""",
        """    free(mesh->bvh->centroids);
    free(mesh->bvh->sortScratch);
    free(mesh->bvh);
""",
        "clear sort scratch free restore",
    )
    text = replace_once(
        text,
        """    free(bvh->centroids);
    bvh->centroids = NULL;
""",
        """    free(bvh->centroids);
    bvh->centroids = NULL;
    free(bvh->sortScratch);
    bvh->sortScratch = NULL;
""",
        "build scratch free restore",
    )
    text = replace_once(
        text,
        """    copy->indices = NULL;
    copy->centroids = NULL;
""",
        """    copy->indices = NULL;
    copy->centroids = NULL;
    copy->sortScratch = NULL;
""",
        "copy sort scratch init restore",
    )
    text = replace_once(
        text,
        """    if (source_bvh->centroids && source_bvh->indexCount > 0) {
        copy->centroids = (Vec3*)malloc(sizeof(*copy->centroids) *
                                        (size_t)source_bvh->indexCount);
        if (!copy->centroids) {
            runtime_triangle_bvh_destroy(copy);
            return false;
        }
        memcpy(copy->centroids,
               source_bvh->centroids,
               sizeof(*copy->centroids) * (size_t)source_bvh->indexCount);
    }
    dst->bvh = copy;
""",
        """    if (source_bvh->centroids && source_bvh->indexCount > 0) {
        copy->centroids = (Vec3*)malloc(sizeof(*copy->centroids) *
                                        (size_t)source_bvh->indexCount);
        if (!copy->centroids) {
            runtime_triangle_bvh_destroy(copy);
            return false;
        }
        memcpy(copy->centroids,
               source_bvh->centroids,
               sizeof(*copy->centroids) * (size_t)source_bvh->indexCount);
    }
    if (source_bvh->sortScratch && source_bvh->indexCount > 0) {
        copy->sortScratch =
            (RuntimeTriangleBVH3DSortItem*)malloc(sizeof(*copy->sortScratch) *
                                                  (size_t)source_bvh->indexCount);
        if (!copy->sortScratch) {
            runtime_triangle_bvh_destroy(copy);
            return false;
        }
        memcpy(copy->sortScratch,
               source_bvh->sortScratch,
               sizeof(*copy->sortScratch) * (size_t)source_bvh->indexCount);
    }
    dst->bvh = copy;
""",
        "copy sort scratch allocation restore",
    )
    text = replace_once(
        text,
        """    bvh->indices = (int*)malloc(sizeof(*bvh->indices) * (size_t)bvh->indexCount);
    bvh->centroids = (Vec3*)malloc(sizeof(*bvh->centroids) * (size_t)bvh->indexCount);
""",
        """    bvh->indices = (int*)malloc(sizeof(*bvh->indices) * (size_t)bvh->indexCount);
    bvh->centroids = (Vec3*)malloc(sizeof(*bvh->centroids) * (size_t)bvh->indexCount);
    bvh->sortScratch =
        (RuntimeTriangleBVH3DSortItem*)malloc(sizeof(*bvh->sortScratch) *
                                              (size_t)bvh->indexCount);
""",
        "sort scratch allocation restore",
    )
    text = replace_once(
        text,
        """    if (!bvh->indices || !bvh->centroids) {
        char diag[384];
        snprintf(diag,
                 sizeof(diag),
                 "build scratch allocation failed: triangle_count=%d indices=%s centroids=%s index_bytes=%llu centroid_bytes=%llu",
                 bvh->indexCount,
                 bvh->indices ? "ok" : "failed",
                 bvh->centroids ? "ok" : "failed",
                 (unsigned long long)(sizeof(*bvh->indices) *
                                      (size_t)bvh->indexCount),
                 (unsigned long long)(sizeof(*bvh->centroids) *
                                      (size_t)bvh->indexCount));
""",
        """    if (!bvh->indices || !bvh->centroids || !bvh->sortScratch) {
        char diag[384];
        snprintf(diag,
                 sizeof(diag),
                 "build scratch allocation failed: triangle_count=%d indices=%s centroids=%s sort_scratch=%s index_bytes=%llu centroid_bytes=%llu sort_scratch_bytes=%llu",
                 bvh->indexCount,
                 bvh->indices ? "ok" : "failed",
                 bvh->centroids ? "ok" : "failed",
                 bvh->sortScratch ? "ok" : "failed",
                 (unsigned long long)(sizeof(*bvh->indices) *
                                      (size_t)bvh->indexCount),
                 (unsigned long long)(sizeof(*bvh->centroids) *
                                      (size_t)bvh->indexCount),
                 (unsigned long long)(sizeof(*bvh->sortScratch) *
                                      (size_t)bvh->indexCount));
""",
        "allocation guard restore",
    )
    text = replace_once(
        text,
        "    bvh->sortScratchBytes = 0u;\n",
        """    bvh->sortScratchBytes =
        (uint64_t)sizeof(*bvh->sortScratch) * (uint64_t)bvh->indexCount;
""",
        "sort scratch byte accounting restore",
    )
    return text


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def stats(values: list[float]) -> dict[str, float]:
    mean = statistics.fmean(values)
    return {
        "min": min(values),
        "max": max(values),
        "mean": mean,
        "spread": max(values) - min(values),
        "spread_percent_of_mean": ((max(values) - min(values)) / mean * 100.0) if mean else 0.0,
        "stdev_population": statistics.pstdev(values) if len(values) > 1 else 0.0,
    }


def summarize_rows(rows: list[dict[str, Any]]) -> dict[str, Any]:
    by_variant: dict[str, list[dict[str, Any]]] = {"A": [], "B": []}
    for row in rows:
        by_variant[row["variant"]].append(row)
    summary: dict[str, Any] = {}
    for variant, variant_rows in by_variant.items():
        summary[variant] = {
            "count": len(variant_rows),
            "all_passed": all(row["pass"] for row in variant_rows),
            "bvh_build_cpu_ms": stats([row["bvh_build_cpu_ms"] for row in variant_rows]),
            "elapsed_wall_seconds": stats([row["elapsed_wall_seconds"] for row in variant_rows]),
            "range_bounds_cpu_ms": stats([row["range_bounds_cpu_ms"] for row in variant_rows]),
            "sort_cpu_ms": stats([row["sort_cpu_ms"] for row in variant_rows]),
            "node_append_cpu_ms": stats([row["node_append_cpu_ms"] for row in variant_rows]),
            "build_scratch_bytes": stats([row["bvh_build_scratch_bytes"] for row in variant_rows]),
            "sort_scratch_bytes": stats([row["sort_scratch_bytes"] for row in variant_rows]),
        }
    a_mean = summary["A"]["bvh_build_cpu_ms"]["mean"]
    b_mean = summary["B"]["bvh_build_cpu_ms"]["mean"]
    a_scratch = summary["A"]["build_scratch_bytes"]["mean"]
    b_scratch = summary["B"]["build_scratch_bytes"]["mean"]
    summary["comparison"] = {
        "b_vs_a_build_cpu_delta_ms": b_mean - a_mean,
        "b_vs_a_build_cpu_delta_percent": ((b_mean - a_mean) / a_mean * 100.0) if a_mean else 0.0,
        "b_vs_a_build_scratch_delta_bytes": b_scratch - a_scratch,
        "functional_invariants_clean": all(
            row["pass"]
            and row["bvh_triangle_count"] == 1681364
            and row["bvh_node_count"] == 1048575
            and row["bvh_leaf_count"] == 524288
            and row["bvh_max_depth"] == 20
            and row["primary_hit_pixels"] == 6912
            and row["trace_overflows"] == 0
            and row["flat_fallback_calls"] == 0
            and row["overflow_fallback_calls"] == 0
            and row["frame_valid_bmp"]
            for row in rows
        ),
    }
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sidecar", type=Path)
    parser.add_argument("--run-id", default=f"bvh_ab_mb1_sort_scratch_{time.strftime('%Y-%m-%d')}")
    parser.add_argument("--pairs", type=int, default=3)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=384)
    parser.add_argument("--object-audit-max-dimension", type=int, default=320)
    parser.add_argument("--temporal-frames", type=int, default=1)
    args = parser.parse_args()

    if args.pairs <= 0:
        raise ValueError("--pairs must be positive")
    sidecar = args.sidecar.expanduser()
    if not sidecar.is_absolute():
        sidecar = (REPO_ROOT.parent / sidecar).resolve()
    if not sidecar.exists():
        raise FileNotFoundError(f"missing sidecar root: {sidecar}")

    run_root = (OUTPUT_ROOT / args.run_id).resolve()
    binary_root = run_root / "binaries"
    binary_root.mkdir(parents=True, exist_ok=True)
    a_binary = binary_root / "ray_tracing_render_headless_A_legacy_sort_scratch"
    b_binary = binary_root / "ray_tracing_render_headless_B_mb1_sort_scratch_removed"

    original_source = SOURCE_PATH.read_text(encoding="utf-8")
    original_hash = sha256_text(original_source)
    current_is_legacy = "RuntimeTriangleBVH3DSortItem" in original_source
    if current_is_legacy:
        a_source = original_source
        b_source = mb1_variant_source(original_source)
        harness_mode = "pre_mb1_current_to_mb1_variant"
    else:
        a_source = legacy_sort_scratch_source(original_source)
        b_source = original_source
        harness_mode = "mb1_current_to_legacy_baseline"
    a_hash = sha256_text(a_source)
    b_hash = sha256_text(b_source)

    build_logs: dict[str, dict[str, str]] = {}
    try:
        SOURCE_PATH.write_text(a_source, encoding="utf-8")
        completed = run(
            ["make", "-B", "-C", str(REPO_ROOT), "ray-tracing-render-headless"],
            cwd=REPO_ROOT.parent,
        )
        build_logs["A"] = {"stdout": completed.stdout, "stderr": completed.stderr}
        shutil.copy2(HEADLESS_BINARY, a_binary)

        SOURCE_PATH.write_text(b_source, encoding="utf-8")
        completed = run(
            ["make", "-B", "-C", str(REPO_ROOT), "ray-tracing-render-headless"],
            cwd=REPO_ROOT.parent,
        )
        build_logs["B"] = {"stdout": completed.stdout, "stderr": completed.stderr}
        shutil.copy2(HEADLESS_BINARY, b_binary)
    finally:
        SOURCE_PATH.write_text(original_source, encoding="utf-8")

    if sha256_file(SOURCE_PATH) != original_hash:
        raise RuntimeError("source restore failed after B build")

    completed = run(
        ["make", "-B", "-C", str(REPO_ROOT), "ray-tracing-render-headless"],
        cwd=REPO_ROOT.parent,
    )
    build_logs["restore"] = {"stdout": completed.stdout, "stderr": completed.stderr}

    rows: list[dict[str, Any]] = []
    for pair_index in range(1, args.pairs + 1):
        for variant, binary in (("A", a_binary), ("B", b_binary)):
            sample_run_id = f"{args.run_id}_{variant.lower()}{pair_index}"
            command = [
                sys.executable,
                str(STRESS_RUNNER),
                str(sidecar),
                "--binary",
                str(binary),
                "--run-id",
                sample_run_id,
                "--width",
                str(args.width),
                "--height",
                str(args.height),
                "--object-audit-max-dimension",
                str(args.object_audit_max_dimension),
                "--temporal-frames",
                str(args.temporal_frames),
                "--integrator",
                "direct_light",
            ]
            run(command)
            report_path = OUTPUT_ROOT / sample_run_id / "stress_report.json"
            summary_path = OUTPUT_ROOT / sample_run_id / "render_summary.json"
            report = load_json(report_path)
            render_summary = load_json(summary_path)
            metrics = report["metrics"]
            bvh = render_summary["bvh_summary"]
            rows.append(
                {
                    "pair": pair_index,
                    "variant": variant,
                    "run_id": sample_run_id,
                    "pass": bool(report["pass"]),
                    "bvh_build_cpu_ms": float(metrics["bvh_build_cpu_ms"]),
                    "elapsed_wall_seconds": float(metrics["elapsed_wall_seconds"]),
                    "bvh_build_scratch_bytes": int(metrics["bvh_build_scratch_bytes"]),
                    "bvh_total_bytes": int(metrics["bvh_total_bytes"]),
                    "bvh_triangle_count": int(metrics["bvh_triangle_count"]),
                    "bvh_node_count": int(metrics["bvh_node_count"]),
                    "bvh_leaf_count": int(metrics["bvh_leaf_count"]),
                    "bvh_max_depth": int(metrics["bvh_max_depth"]),
                    "primary_hit_pixels": int(metrics["primary_hit_pixels"]),
                    "trace_overflows": int(metrics["trace_overflows"]),
                    "flat_fallback_calls": int(metrics["flat_fallback_calls"]),
                    "overflow_fallback_calls": int(metrics["overflow_fallback_calls"]),
                    "frame_valid_bmp": bool(metrics["frame_valid_bmp"]),
                    "range_bounds_cpu_ms": float(bvh.get("range_bounds_cpu_ms", 0.0)),
                    "sort_cpu_ms": float(bvh.get("sort_cpu_ms", 0.0)),
                    "node_append_cpu_ms": float(bvh.get("node_append_cpu_ms", 0.0)),
                    "build_unaccounted_cpu_ms": float(bvh.get("build_unaccounted_cpu_ms", 0.0)),
                    "centroid_bytes": int(bvh.get("centroid_bytes", 0)),
                    "sort_scratch_bytes": int(bvh.get("sort_scratch_bytes", 0)),
                    "report": str(report_path.resolve()),
                    "render_summary": str(summary_path.resolve()),
                    "binary": str(binary),
                }
            )

    summary = {
        "schema": "ray_tracing_private_bvh_ab_compare_v1",
        "created_at": time.strftime("%Y-%m-%d"),
        "run_id": args.run_id,
        "sidecar": str(sidecar),
        "harness_mode": harness_mode,
        "pairs": args.pairs,
        "sequence": [f"A{index}/B{index}" for index in range(1, args.pairs + 1)],
        "source_hashes": {
            "original_source": original_hash,
            "A_legacy_sort_scratch": a_hash,
            "B_mb1_sort_scratch_removed": b_hash,
            "restored_source": sha256_file(SOURCE_PATH),
        },
        "binaries": {
            "A_legacy_sort_scratch": str(a_binary),
            "A_legacy_sort_scratch_sha256": sha256_file(a_binary),
            "B_mb1_sort_scratch_removed": str(b_binary),
            "B_mb1_sort_scratch_removed_sha256": sha256_file(b_binary),
        },
        "build_logs": build_logs,
        "rows": rows,
        "stats": summarize_rows(rows),
    }
    comparison = summary["stats"]["comparison"]
    if comparison["functional_invariants_clean"] and comparison["b_vs_a_build_cpu_delta_percent"] <= 5.0:
        summary["decision"] = "B_candidate_reopen"
    else:
        summary["decision"] = "B_rejected"
    summary_path = run_root / "ab_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({k: summary[k] for k in ("run_id", "decision", "stats")}, indent=2, sort_keys=True))
    return 0 if summary["stats"]["comparison"]["functional_invariants_clean"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
