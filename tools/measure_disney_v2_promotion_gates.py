#!/usr/bin/env python3
"""Measure Disney v2 promotion gates without changing route defaults."""

from __future__ import annotations

import argparse
import json
import platform
import subprocess
import sys
import time
from pathlib import Path


SCENES = (
    {
        "id": "primitive_glass_corridor",
        "request": "tests/fixtures/disney_v2_d25/primitive_glass_corridor_request.json",
        "required_objects": ("plane_floor", "prism_center", "prism_offset"),
        "mesh_required": False,
    },
    {
        "id": "imported_mesh_material",
        "request": "tests/fixtures/disney_v2_d25/imported_mesh_material_request.json",
        "required_objects": ("obj_sphere_pressure",),
        "mesh_required": True,
    },
    {
        "id": "imported_mesh_pressure_mrt8",
        "request": "tests/fixtures/disney_v2_visual_matrix/imported_mesh_pressure_mrt8/request_disney_v2.json",
        "required_objects": ("obj_sphere_pressure_mrt8",),
        "mesh_required": True,
    },
)

INTEGRATORS = ("disney", "disney_v2")


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[1]
    machine = platform.machine()
    default_cli = root / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless"
    if not default_cli.exists():
        default_cli = root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=root, help="RayTracing repo root")
    parser.add_argument("--cli", type=Path, default=default_cli, help="headless render CLI path")
    parser.add_argument(
        "--output-root",
        type=Path,
        default=root / "build" / "agent_runs" / "ray_tracing" / "disney_v2_d26_promotion_gates",
        help="measurement output directory",
    )
    parser.add_argument(
        "--thresholds",
        type=Path,
        default=root / "tests" / "fixtures" / "disney_v2_promotion_thresholds.json",
        help="candidate performance threshold JSON",
    )
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def frame_has_bmp_magic(path: Path) -> bool:
    try:
        with path.open("rb") as f:
            return f.read(2) == b"BM"
    except OSError:
        return False


def build_request(root: Path, output_root: Path, scene: dict, integrator: str) -> tuple[Path, Path]:
    request_path = root / scene["request"]
    request = load_json(request_path)
    run_id = f"disney_v2_d26_{scene['id']}_{integrator}"
    out_dir = output_root / scene["id"] / integrator
    runtime_scene_path = request.get("scene", {}).get("runtime_scene_path", "")
    if runtime_scene_path and not Path(runtime_scene_path).is_absolute():
        request.setdefault("scene", {})["runtime_scene_path"] = str(
            (request_path.parent / runtime_scene_path).resolve()
        )
    request["run_id"] = run_id
    request.setdefault("render", {})["integrator_3d"] = integrator
    request["output"] = {
        "root": str(out_dir),
        "overwrite": True,
    }
    request["progress"] = {
        "summary_path": str(out_dir / "render_summary.json"),
        "progress_path": str(out_dir / "render_progress.json"),
    }
    generated_request = output_root / "requests" / f"{scene['id']}_{integrator}.json"
    write_json(generated_request, request)
    return generated_request, out_dir


def render(cli: Path, request_path: Path, out_dir: Path) -> tuple[dict, float]:
    out_dir.mkdir(parents=True, exist_ok=True)
    stdout_path = out_dir / "stdout_summary.json"
    summary_path = out_dir / "render_summary.json"
    start = time.perf_counter()
    with stdout_path.open("w", encoding="utf-8") as stdout:
        result = subprocess.run(
            [str(cli), "--request", str(request_path), "--render", "--summary", str(summary_path)],
            stdout=stdout,
            stderr=subprocess.PIPE,
            text=True,
        )
    if result.returncode != 0:
        stderr_path = out_dir / "stderr.txt"
        stderr_path.write_text(result.stderr or "", encoding="utf-8")
        raise RuntimeError(
            f"render failed for {request_path} with exit {result.returncode}; "
            f"stderr: {stderr_path}"
        )
    elapsed = time.perf_counter() - start
    summary = load_json(summary_path)
    return summary, elapsed


def audit_by_object(summary: dict) -> dict:
    return {entry.get("object_id"): entry for entry in summary.get("object_audit", [])}


def validate_run(scene: dict, integrator: str, summary: dict, elapsed_seconds: float) -> dict:
    out_first = Path(summary.get("outputs", {}).get("first_frame_path", ""))
    audit = audit_by_object(summary)
    missing_objects = [obj for obj in scene["required_objects"] if obj not in audit]
    bvh = summary.get("bvh_summary", {})
    stats = summary.get("render_stats", {})
    frame_ok = frame_has_bmp_magic(out_first)
    mesh_triangle_count = 0
    if scene["mesh_required"] and scene["required_objects"]:
        mesh_triangle_count = int(audit.get(scene["required_objects"][0], {}).get("triangle_count", 0))
    checks = {
        "route_label_ok": summary.get("integrator_3d") == integrator,
        "rendered_ok": summary.get("rendered_frames") is True and summary.get("frames_rendered") == 1,
        "nonzero_ok": int(stats.get("nonzero_pixels", 0)) > 0,
        "frame_ok": frame_ok,
        "audit_ok": len(audit) > 0 and not missing_objects,
        "bvh_ok": bool(bvh.get("ready", False)) and int(bvh.get("trace_overflows", 0)) == 0,
        "mesh_ok": (not scene["mesh_required"]) or mesh_triangle_count > 0,
        "elapsed_ok": elapsed_seconds > 0.0,
    }
    return {
        "scene_id": scene["id"],
        "integrator": integrator,
        "summary_path": str(out_first.parent.parent / "render_summary.json"),
        "frame_path": str(out_first),
        "elapsed_seconds": elapsed_seconds,
        "checks": checks,
        "render_stats": {
            "visible_pixels": int(stats.get("visible_pixels", 0)),
            "nonzero_pixels": int(stats.get("nonzero_pixels", 0)),
            "secondary_rays": int(stats.get("secondary_rays", 0)),
            "secondary_hits": int(stats.get("secondary_hits", 0)),
            "emissive_area_candidate_count": int(stats.get("emissive_area_candidate_count", 0)),
            "emissive_area_selected_candidates": int(
                stats.get("emissive_area_selected_candidates", 0)
            ),
            "emissive_area_visibility_rays": int(
                stats.get("emissive_area_visibility_rays", 0)
            ),
            "emissive_area_primary_samples": int(stats.get("emissive_area_primary_samples", 0)),
            "emissive_area_recursive_samples": int(
                stats.get("emissive_area_recursive_samples", 0)
            ),
            "emissive_area_recursive_policy_skips": int(
                stats.get("emissive_area_recursive_policy_skips", 0)
            ),
            "emissive_area_recursive_candidate_cap_skips": int(
                stats.get("emissive_area_recursive_candidate_cap_skips", 0)
            ),
            "emissive_area_recursive_triangle_cap_skips": int(
                stats.get("emissive_area_recursive_triangle_cap_skips", 0)
            ),
            "emissive_area_recursive_candidate_cap": int(
                stats.get("emissive_area_recursive_candidate_cap", 0)
            ),
            "emissive_area_recursive_triangle_cap": int(
                stats.get("emissive_area_recursive_triangle_cap", 0)
            ),
            "emissive_area_full_scan_fallbacks": int(
                stats.get("emissive_area_full_scan_fallbacks", 0)
            ),
            "temporal_committed_subpasses": int(stats.get("temporal_committed_subpasses", 0)),
            "max_radiance": float(stats.get("max_radiance", 0.0)),
            "max_rgb": stats.get("max_rgb", []),
        },
        "bvh_summary": {
            "triangle_count": int(bvh.get("triangle_count", 0)),
            "node_count": int(bvh.get("node_count", 0)),
            "leaf_count": int(bvh.get("leaf_count", 0)),
            "trace_calls": int(bvh.get("trace_calls", 0)),
            "trace_overflows": int(bvh.get("trace_overflows", 0)),
        },
        "missing_objects": missing_objects,
        "mesh_triangle_count": mesh_triangle_count,
    }


def compare_scene(scene_id: str, disney: dict, disney_v2: dict) -> dict:
    disney_time = max(float(disney["elapsed_seconds"]), 1.0e-9)
    v2_time = float(disney_v2["elapsed_seconds"])
    return {
        "scene_id": scene_id,
        "bvh_triangle_count_match": disney["bvh_summary"]["triangle_count"]
        == disney_v2["bvh_summary"]["triangle_count"],
        "both_nonzero": disney["render_stats"]["nonzero_pixels"] > 0
        and disney_v2["render_stats"]["nonzero_pixels"] > 0,
        "disney_v2_to_disney_elapsed_ratio": v2_time / disney_time,
        "visible_pixel_delta": disney_v2["render_stats"]["visible_pixels"]
        - disney["render_stats"]["visible_pixels"],
        "max_radiance_delta": disney_v2["render_stats"]["max_radiance"]
        - disney["render_stats"]["max_radiance"],
    }


def merged_scene_thresholds(thresholds: dict, scene_id: str) -> dict:
    merged = dict(thresholds.get("global", {}))
    merged.update(thresholds.get("scenes", {}).get(scene_id, {}))
    return merged


def evaluate_thresholds(runs: list[dict],
                        comparisons: list[dict],
                        thresholds: dict) -> dict:
    runs_by_key = {(run["scene_id"], run["integrator"]): run for run in runs}
    scene_results = []
    for comparison in comparisons:
        scene_id = comparison["scene_id"]
        limits = merged_scene_thresholds(thresholds, scene_id)
        disney_v2 = runs_by_key.get((scene_id, "disney_v2"), {})
        bvh = disney_v2.get("bvh_summary", {})
        stats = disney_v2.get("render_stats", {})
        ratio = float(comparison["disney_v2_to_disney_elapsed_ratio"])
        elapsed = float(disney_v2.get("elapsed_seconds", 0.0))
        max_ratio = float(limits.get("max_disney_v2_to_disney_elapsed_ratio", 0.0))
        max_elapsed = float(limits.get("max_disney_v2_elapsed_seconds", 0.0))
        min_nonzero = int(limits.get("min_nonzero_pixels", 1))
        min_visible = int(limits.get("min_visible_pixels", 1))
        min_secondary_rays = int(limits.get("min_secondary_rays", 0))
        min_secondary_hits = int(limits.get("min_secondary_hits", 0))
        min_max_radiance = float(limits.get("min_max_radiance", 0.0))
        max_max_radiance = float(limits.get("max_max_radiance", 0.0))
        min_triangles = int(limits.get("min_bvh_triangle_count", 0))
        max_overflows = int(limits.get("max_bvh_trace_overflows", 0))
        result = {
            "scene_id": scene_id,
            "disney_v2_elapsed_seconds": elapsed,
            "max_disney_v2_elapsed_seconds": max_elapsed,
            "elapsed_ok": max_elapsed <= 0.0 or elapsed <= max_elapsed,
            "disney_v2_to_disney_elapsed_ratio": ratio,
            "max_disney_v2_to_disney_elapsed_ratio": max_ratio,
            "ratio_ok": max_ratio <= 0.0 or ratio <= max_ratio,
            "nonzero_pixels": int(stats.get("nonzero_pixels", 0)),
            "min_nonzero_pixels": min_nonzero,
            "nonzero_ok": int(stats.get("nonzero_pixels", 0)) >= min_nonzero,
            "visible_pixels": int(stats.get("visible_pixels", 0)),
            "min_visible_pixels": min_visible,
            "visible_ok": int(stats.get("visible_pixels", 0)) >= min_visible,
            "secondary_rays": int(stats.get("secondary_rays", 0)),
            "min_secondary_rays": min_secondary_rays,
            "secondary_rays_ok": int(stats.get("secondary_rays", 0)) >= min_secondary_rays,
            "secondary_hits": int(stats.get("secondary_hits", 0)),
            "min_secondary_hits": min_secondary_hits,
            "secondary_hits_ok": int(stats.get("secondary_hits", 0)) >= min_secondary_hits,
            "max_radiance": float(stats.get("max_radiance", 0.0)),
            "min_max_radiance": min_max_radiance,
            "max_max_radiance": max_max_radiance,
            "radiance_floor_ok": float(stats.get("max_radiance", 0.0)) >= min_max_radiance,
            "radiance_ceiling_ok": max_max_radiance <= 0.0 or
            float(stats.get("max_radiance", 0.0)) <= max_max_radiance,
            "bvh_triangle_count": int(bvh.get("triangle_count", 0)),
            "min_bvh_triangle_count": min_triangles,
            "triangle_count_ok": int(bvh.get("triangle_count", 0)) >= min_triangles,
            "bvh_trace_overflows": int(bvh.get("trace_overflows", 0)),
            "max_bvh_trace_overflows": max_overflows,
            "trace_overflow_ok": int(bvh.get("trace_overflows", 0)) <= max_overflows,
        }
        result["passed"] = (
            result["elapsed_ok"]
            and result["ratio_ok"]
            and result["nonzero_ok"]
            and result["visible_ok"]
            and result["secondary_rays_ok"]
            and result["secondary_hits_ok"]
            and result["radiance_floor_ok"]
            and result["radiance_ceiling_ok"]
            and result["triangle_count_ok"]
            and result["trace_overflow_ok"]
        )
        scene_results.append(result)
    performance_passed = all(
        result["elapsed_ok"] and result["ratio_ok"] and result["trace_overflow_ok"]
        for result in scene_results
    )
    quality_passed = all(
        result["nonzero_ok"]
        and result["visible_ok"]
        and result["secondary_rays_ok"]
        and result["secondary_hits_ok"]
        and result["radiance_floor_ok"]
        and result["radiance_ceiling_ok"]
        and result["triangle_count_ok"]
        for result in scene_results
    )
    return {
        "schema_version": thresholds.get(
            "schema_version",
            "ray_tracing_disney_v2_promotion_thresholds_v1",
        ),
        "description": thresholds.get("description", ""),
        "scene_results": scene_results,
        "performance_passed": performance_passed,
        "quality_passed": quality_passed,
        "passed": all(result["passed"] for result in scene_results),
    }


def write_markdown(path: Path, report: dict) -> None:
    lines = [
        "# Disney V2 D2.6 Promotion Gate Report",
        "",
        f"- promotion_ready: `{str(report['promotion_ready']).lower()}`",
        f"- hard_gates_passed: `{str(report['hard_gates_passed']).lower()}`",
        f"- output_root: `{report['output_root']}`",
        "",
        "## Gate Status",
        "",
    ]
    for gate, passed in report["gates"].items():
        lines.append(f"- `{gate}`: `{str(passed).lower()}`")
    lines.extend(["", "## Scene Measurements", ""])
    for run in report["runs"]:
        lines.append(
            "- `{scene}` / `{integrator}`: elapsed `{elapsed:.6f}s`, "
            "nonzero `{nonzero}`, BVH triangles `{triangles}`, "
            "emissive candidates `{emissive_candidates}`, "
            "selected `{emissive_selected}`, visibility rays `{emissive_visibility}`, "
            "recursive skips `{emissive_recursive_skips}`, "
            "fallbacks `{emissive_fallbacks}`, frame `{frame}`".format(
                scene=run["scene_id"],
                integrator=run["integrator"],
                elapsed=run["elapsed_seconds"],
                nonzero=run["render_stats"]["nonzero_pixels"],
                triangles=run["bvh_summary"]["triangle_count"],
                emissive_candidates=run["render_stats"]["emissive_area_candidate_count"],
                emissive_selected=run["render_stats"]["emissive_area_selected_candidates"],
                emissive_visibility=run["render_stats"]["emissive_area_visibility_rays"],
                emissive_recursive_skips=run["render_stats"][
                    "emissive_area_recursive_policy_skips"
                ],
                emissive_fallbacks=run["render_stats"]["emissive_area_full_scan_fallbacks"],
                frame=run["frame_path"],
            )
        )
    lines.extend(["", "## Comparisons", ""])
    for comparison in report["comparisons"]:
        lines.append(
            "- `{scene}`: elapsed ratio v2/disney `{ratio:.3f}`, "
            "BVH triangle match `{match}`, visible delta `{visible}`".format(
                scene=comparison["scene_id"],
                ratio=comparison["disney_v2_to_disney_elapsed_ratio"],
                match=str(comparison["bvh_triangle_count_match"]).lower(),
                visible=comparison["visible_pixel_delta"],
            )
        )
    lines.extend(["", "## Performance Thresholds", ""])
    threshold_report = report.get("performance_thresholds", {})
    lines.append(f"- threshold file: `{report.get('threshold_path', '')}`")
    lines.append(f"- thresholds_passed: `{str(threshold_report.get('passed', False)).lower()}`")
    for result in threshold_report.get("scene_results", []):
        lines.append(
            "- `{scene}`: elapsed `{elapsed:.3f}s` <= `{max_elapsed:.3f}s`, "
            "ratio `{ratio:.3f}` <= `{max_ratio:.3f}`, triangles `{triangles}`, "
            "secondary `{secondary_rays}`/`{secondary_hits}`, max radiance `{max_radiance:.3f}`".format(
                scene=result["scene_id"],
                elapsed=result["disney_v2_elapsed_seconds"],
                max_elapsed=result["max_disney_v2_elapsed_seconds"],
                ratio=result["disney_v2_to_disney_elapsed_ratio"],
                max_ratio=result["max_disney_v2_to_disney_elapsed_ratio"],
                triangles=result["bvh_triangle_count"],
                secondary_rays=result["secondary_rays"],
                secondary_hits=result["secondary_hits"],
                max_radiance=result["max_radiance"],
            )
        )
    lines.extend(["", "## Promotion Blockers", ""])
    for blocker in report["promotion_blockers"]:
        lines.append(f"- {blocker}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    cli = args.cli.resolve()
    output_root = args.output_root.resolve()
    if not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2
    if not args.thresholds.exists():
        print(f"missing threshold file: {args.thresholds}", file=sys.stderr)
        return 2
    thresholds = load_json(args.thresholds)

    runs = []
    summaries = {}
    for scene in SCENES:
        for integrator in INTEGRATORS:
            request_path, out_dir = build_request(root, output_root, scene, integrator)
            summary, elapsed = render(cli, request_path, out_dir)
            run = validate_run(scene, integrator, summary, elapsed)
            runs.append(run)
            summaries[(scene["id"], integrator)] = run

    comparisons = []
    for scene in SCENES:
        comparisons.append(
            compare_scene(
                scene["id"],
                summaries[(scene["id"], "disney")],
                summaries[(scene["id"], "disney_v2")],
            )
        )
    threshold_report = evaluate_thresholds(runs, comparisons, thresholds)
    imported_mesh_scene_count = len({scene["id"] for scene in SCENES if scene["mesh_required"]})

    hard_gates = {
        "route_isolation": all(run["checks"]["route_label_ok"] for run in runs),
        "render_health": all(
            run["checks"]["rendered_ok"] and run["checks"]["nonzero_ok"] and run["checks"]["frame_ok"]
            for run in runs
        ),
        "audit_health": all(run["checks"]["audit_ok"] for run in runs),
        "bvh_health": all(run["checks"]["bvh_ok"] for run in runs),
        "imported_mesh_participation": all(run["checks"]["mesh_ok"] for run in runs),
        "performance_measured": all(run["checks"]["elapsed_ok"] for run in runs),
        "performance_thresholds": bool(threshold_report.get("performance_passed", False)),
        "quality_thresholds": bool(threshold_report.get("quality_passed", False)),
        "imported_mesh_scene_count": imported_mesh_scene_count >= 2,
        "same_scene_geometry_parity": all(
            comparison["bvh_triangle_count_match"] for comparison in comparisons
        ),
    }
    promotion_blockers = [
        "Needs human visual signoff documenting visible improvement or meaningful new behavior over shipped Disney for the imported-mesh proof scenes.",
        "Needs skull-scale or other external high-triangle scene signoff once the large sidecar path is portable/readable in the local proof workspace.",
        "Needs repeated performance/convergence-threshold runs before any route-default change; this report only records candidate local thresholds.",
    ]
    report = {
        "schema_version": "ray_tracing_disney_v2_promotion_gate_report_v1",
        "output_root": str(output_root),
        "threshold_path": str(args.thresholds.resolve()),
        "hard_gates_passed": all(hard_gates.values()),
        "promotion_ready": False,
        "gates": hard_gates,
        "runs": runs,
        "comparisons": comparisons,
        "performance_thresholds": threshold_report,
        "promotion_blockers": promotion_blockers,
    }
    write_json(output_root / "promotion_gate_report.json", report)
    write_markdown(output_root / "promotion_gate_report.md", report)
    if not report["hard_gates_passed"]:
        print(json.dumps(report["gates"], indent=2, sort_keys=True), file=sys.stderr)
        return 1
    print(output_root / "promotion_gate_report.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
