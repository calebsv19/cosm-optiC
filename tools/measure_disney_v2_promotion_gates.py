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
            "nonzero `{nonzero}`, BVH triangles `{triangles}`, frame `{frame}`".format(
                scene=run["scene_id"],
                integrator=run["integrator"],
                elapsed=run["elapsed_seconds"],
                nonzero=run["render_stats"]["nonzero_pixels"],
                triangles=run["bvh_summary"]["triangle_count"],
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
        "same_scene_geometry_parity": all(
            comparison["bvh_triangle_count_match"] for comparison in comparisons
        ),
    }
    promotion_blockers = [
        "Needs at least two imported-mesh proof scenes with documented visible improvement or meaningful new behavior over shipped Disney.",
        "Needs an explicit temporal denoise/pruning policy decision for Disney v2.",
        "Needs focused emissive-material surface-hit coverage beyond finite-radius runtime light hits.",
        "Needs stable performance thresholds signed off against proof scenes before route default changes.",
    ]
    report = {
        "schema_version": "ray_tracing_disney_v2_promotion_gate_report_v1",
        "output_root": str(output_root),
        "hard_gates_passed": all(hard_gates.values()),
        "promotion_ready": False,
        "gates": hard_gates,
        "runs": runs,
        "comparisons": comparisons,
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
