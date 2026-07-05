#!/usr/bin/env python3
"""Generate and run a TLAS/BLAS repeated-instance stress matrix.

The default invocation writes private proof artifacts under
ray_tracing/_private_workspace_artifacts/high_triangle_stress.  The Make target
passes --output-root so routine local verification writes under build/.  The
requests intentionally omit an inspection trace-route override so the program
default is tested.
"""

from __future__ import annotations

import argparse
import copy
import json
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[1]
FIXTURE_ROOT = REPO_ROOT / "tests/fixtures/mesh_asset_runtime_spheres"
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "_private_workspace_artifacts/high_triangle_stress"
DEFAULT_BINARY_CANDIDATES = [
    REPO_ROOT / "build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless",
    REPO_ROOT / "build/arm64/tools/cli/ray_tracing_render_headless",
    REPO_ROOT / "build/tools/cli/ray_tracing_render_headless",
]


@dataclass(frozen=True)
class MatrixCase:
    label: str
    asset_id: str
    triangle_count: int
    repeat_count: int
    scale: float
    spacing: float
    camera_y: float
    camera_z: float
    camera_zoom: float


DEFAULT_CASES = [
    MatrixCase("mrt10_single_default", "asset_sphere_256x128", 65024, 1, 0.65, 1.25, -6.2, 2.2, 1.0),
    MatrixCase("mrt8_repeat_2", "asset_sphere_128x64", 16128, 2, 0.55, 1.15, -6.8, 2.3, 0.95),
    MatrixCase("mrt8_repeat_4", "asset_sphere_128x64", 16128, 4, 0.45, 1.05, -7.2, 2.45, 0.9),
    MatrixCase("mrt10_repeat_2", "asset_sphere_256x128", 65024, 2, 0.5, 1.2, -7.0, 2.45, 0.9),
    MatrixCase("mrt6_repeat_8", "asset_sphere_64x32", 3968, 8, 0.38, 0.85, -8.0, 2.55, 0.82),
]


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def default_binary() -> Path:
    for candidate in DEFAULT_BINARY_CANDIDATES:
        if candidate.exists():
            return candidate
    return DEFAULT_BINARY_CANDIDATES[0]


def has_bmp_header(path: Path) -> bool:
    try:
        with path.open("rb") as handle:
            return handle.read(2) == b"BM"
    except OSError:
        return False


def base_room_objects(base_scene: dict[str, Any]) -> list[dict[str, Any]]:
    objects = base_scene.get("objects", [])
    if not isinstance(objects, list):
        raise ValueError("base scene objects must be a list")
    room = [copy.deepcopy(obj) for obj in objects if obj.get("object_type") != "mesh_asset_instance"]
    if len(room) != 3:
        raise ValueError(f"expected 3 room primitives in base scene, found {len(room)}")
    return room


def make_mesh_object(case: MatrixCase, index: int) -> dict[str, Any]:
    offset = (index - (case.repeat_count - 1) * 0.5) * case.spacing
    return {
        "object_id": f"obj_{case.label}_{index + 1:02d}",
        "object_type": "mesh_asset_instance",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": {"x": offset, "y": 0.3, "z": 0.75},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": case.scale, "y": case.scale, "z": case.scale},
        },
        "geometry_ref": {"kind": "mesh_asset", "id": case.asset_id, "variant": "runtime_default"},
        "material_ref": {"id": "mat_sphere_pressure"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def generated_scene(base_scene: dict[str, Any], case: MatrixCase) -> dict[str, Any]:
    scene = copy.deepcopy(base_scene)
    scene["scene_id"] = f"scene_{case.label}"
    scene["source_scene_id"] = "scene_mesh_asset_sphere_pressure_mrt10"
    scene["objects"] = base_room_objects(base_scene) + [
        make_mesh_object(case, index) for index in range(case.repeat_count)
    ]
    scene["hierarchy"] = []
    scene["cameras"] = [
        {
            "camera_id": "cam_main",
            "kind": "perspective",
            "position": {"x": 0.0, "y": case.camera_y, "z": case.camera_z},
            "target": {"x": 0.0, "y": 0.35, "z": 0.85},
            "yaw": 0.0,
            "look_pitch": -0.08,
        }
    ]

    ray_extension = scene.setdefault("extensions", {}).setdefault("ray_tracing", {})
    authoring = ray_extension.setdefault("authoring", {})
    authoring["camera_focus_target"] = {"x": 0.0, "y": 0.35, "z": 0.9}
    material_rows = [
        row
        for row in authoring.get("object_materials", [])
        if isinstance(row, dict) and not str(row.get("object_id", "")).startswith("obj_sphere")
    ]
    for index in range(case.repeat_count):
        material_rows.append(
            {
                "object_id": f"obj_{case.label}_{index + 1:02d}",
                "material_id": 0,
                "object_color": 14002250,
                "roughness": 0.22,
            }
        )
    authoring["object_materials"] = material_rows
    return scene


def make_request(
    *,
    case: MatrixCase,
    scene_path: Path,
    run_root: Path,
    width: int,
    height: int,
    temporal_frames: int,
    object_audit_max_dimension: int,
) -> dict[str, Any]:
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": case.label,
        "scene": {"runtime_scene_path": str(scene_path.resolve())},
        "volume": {"enabled": False},
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": width,
            "height": height,
            "normalized_t": 0.0,
            "temporal_frames": temporal_frames,
            "integrator_3d": "direct_light",
        },
        "inspection": {
            "camera_position": {"x": 0.0, "y": case.camera_y, "z": case.camera_z},
            "camera_look_at": {"x": 0.0, "y": 0.35, "z": 0.9},
            "camera_zoom": case.camera_zoom,
            "environment_light_mode": "ambient",
            "ambient_strength": 0.35,
            "top_fill_strength": 1.5,
            "light_intensity": 4.0,
            "light_radius": 0.12,
            "object_audit_enabled": True,
            "object_audit_max_dimension": object_audit_max_dimension,
        },
        "output": {"root": str(run_root), "overwrite": True},
        "progress": {
            "summary_path": str(run_root / "render_summary.json"),
            "progress_path": str(run_root / "render_progress.json"),
        },
    }


def stage_mesh_assets(case_root: Path) -> None:
    source_root = FIXTURE_ROOT / "assets/mesh_assets"
    target_root = case_root / "assets/mesh_assets"
    target_root.mkdir(parents=True, exist_ok=True)
    for source in source_root.glob("*.runtime.json"):
        shutil.copy2(source, target_root / source.name)


def audit_mesh_hits(summary: dict[str, Any], case: MatrixCase) -> tuple[int, int, int]:
    entries = summary.get("object_audit", [])
    if not isinstance(entries, list):
        return 0, 0, 0
    total_hits = 0
    total_triangles = 0
    seen = 0
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        object_id = str(entry.get("object_id", ""))
        if object_id.startswith(f"obj_{case.label}_"):
            seen += 1
            total_hits += int(entry.get("primary_hit_pixels", 0) or 0)
            total_triangles += int(entry.get("triangle_count", 0) or 0)
    return seen, total_triangles, total_hits


def build_case_report(
    *,
    case: MatrixCase,
    run_root: Path,
    command: list[str],
    elapsed_s: float,
    returncode: int,
) -> dict[str, Any]:
    summary_path = run_root / "render_summary.json"
    frame_path = run_root / "frames/frame_0000.bmp"
    summary = load_json(summary_path) if summary_path.exists() else {}
    bvh = summary.get("bvh_summary", {}) if isinstance(summary, dict) else {}
    accel = summary.get("prepared_acceleration", {}) if isinstance(summary, dict) else {}
    timing = summary.get("timing_breakdown", {}) if isinstance(summary, dict) else {}
    mesh_seen, mesh_triangles, mesh_hits = audit_mesh_hits(summary, case)

    expected_mesh_triangles = case.triangle_count * case.repeat_count
    expected_total_triangles = expected_mesh_triangles + 6
    route_trace_calls = int(accel.get("route_trace_calls", 0) or 0)
    route_tlas_trace_calls = int(accel.get("route_tlas_trace_calls", 0) or 0)
    blas_full_rebuilds = int(accel.get("blas_full_rebuilds", 0) or 0)
    blas_persistent_cache_hits = int(accel.get("blas_persistent_cache_hits", 0) or 0)
    frame_valid = frame_path.exists() and has_bmp_header(frame_path)

    pass_status = bool(
        returncode == 0
        and summary.get("scene_applied") is True
        and summary.get("route_native_3d") is True
        and summary.get("frames_rendered") == 1
        and bvh.get("ready") is True
        and int(bvh.get("triangle_count", 0) or 0) == expected_total_triangles
        and int(bvh.get("trace_overflows", 0) or 0) == 0
        and int(bvh.get("overflow_fallback_calls", 0) or 0) == 0
        and int(bvh.get("flat_fallback_calls", 0) or 0) == 0
        and accel.get("active_trace_route") == "tlas_blas"
        and accel.get("requested_trace_route") == "tlas_blas"
        and route_trace_calls > 0
        and route_tlas_trace_calls == route_trace_calls
        and int(accel.get("route_tlas_trace_hits", 0) or 0) > 0
        and int(accel.get("route_parity_mismatches", 0) or 0) == 0
        and int(accel.get("blas_cached_asset_count", 0) or 0) == 1
        and (blas_full_rebuilds == 1 or blas_persistent_cache_hits >= 1)
        and int(accel.get("tlas_instance_count", 0) or 0) == case.repeat_count + 3
        and mesh_seen == case.repeat_count
        and mesh_triangles == expected_mesh_triangles
        and mesh_hits > 0
        and frame_valid
    )

    return {
        "label": case.label,
        "status": "passed" if pass_status else "failed",
        "pass": pass_status,
        "asset_id": case.asset_id,
        "asset_triangle_count": case.triangle_count,
        "repeat_count": case.repeat_count,
        "expected_mesh_triangles": expected_mesh_triangles,
        "expected_total_triangles": expected_total_triangles,
        "metrics": {
            "bvh_triangle_count": int(bvh.get("triangle_count", 0) or 0),
            "bvh_node_count": int(bvh.get("node_count", 0) or 0),
            "bvh_leaf_count": int(bvh.get("leaf_count", 0) or 0),
            "bvh_max_depth": int(bvh.get("max_depth", 0) or 0),
            "bvh_build_cpu_ms": float(bvh.get("build_cpu_ms", 0) or 0),
            "trace_calls": int(bvh.get("trace_calls", 0) or 0),
            "triangle_tests": int(bvh.get("triangle_tests", 0) or 0),
            "mesh_object_audit_count": mesh_seen,
            "mesh_object_audit_triangles": mesh_triangles,
            "mesh_primary_hit_pixels": mesh_hits,
            "active_trace_route": accel.get("active_trace_route", ""),
            "requested_trace_route": accel.get("requested_trace_route", ""),
            "route_trace_calls": route_trace_calls,
            "route_tlas_trace_calls": route_tlas_trace_calls,
            "route_tlas_trace_hits": int(accel.get("route_tlas_trace_hits", 0) or 0),
            "route_flattened_fallback_calls": int(accel.get("route_flattened_fallback_calls", 0) or 0),
            "route_parity_mismatches": int(accel.get("route_parity_mismatches", 0) or 0),
            "blas_prepare_calls": int(accel.get("blas_prepare_calls", 0) or 0),
            "blas_cache_hits": int(accel.get("blas_cache_hits", 0) or 0),
            "blas_cache_misses": int(accel.get("blas_cache_misses", 0) or 0),
            "blas_full_rebuilds": blas_full_rebuilds,
            "blas_persistent_cache_hits": blas_persistent_cache_hits,
            "blas_cached_asset_count": int(accel.get("blas_cached_asset_count", 0) or 0),
            "tlas_node_count": int(accel.get("tlas_node_count", 0) or 0),
            "tlas_instance_count": int(accel.get("tlas_instance_count", 0) or 0),
            "tlas_rebuilds": int(accel.get("tlas_rebuilds", 0) or 0),
            "native_prepare_frame_ms": float(timing.get("native_prepare_frame_ms", 0) or 0),
            "render_trace_ms": float(timing.get("render_trace_ms", 0) or 0),
            "total_run_ms": float(timing.get("total_run_ms", 0) or 0),
            "elapsed_wall_seconds": round(elapsed_s, 3),
            "frame_valid_bmp": frame_valid,
            "process_returncode": returncode,
        },
        "artifacts": {
            "run_root": str(run_root),
            "scene": str(run_root / "scene_runtime.json"),
            "request": str(run_root / "request.json"),
            "summary": str(summary_path),
            "first_frame": str(frame_path),
            "stdout": str(run_root / "stdout.txt"),
            "stderr": str(run_root / "stderr.txt"),
        },
        "command": command,
    }


def write_markdown_report(path: Path, report: dict[str, Any]) -> None:
    rows = report["cases"]
    lines = [
        f"# TLAS/BLAS Repeated Instance Stress - {report['run_id']}",
        "",
        f"Status: {report['status']}",
        "",
        "| Case | Asset | Repeats | Triangles | Route | BLAS assets/full rebuilds | TLAS instances | Route calls/hits | Build ms | Trace ms |",
        "| --- | --- | ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for row in rows:
        metrics = row["metrics"]
        lines.append(
            "| {label} | {asset} | {repeats} | {triangles} | {route} | {blas_assets}/{blas_rebuilds} | {tlas_instances} | {calls}/{hits} | {build:.3f} | {trace:.3f} |".format(
                label=row["label"],
                asset=row["asset_id"],
                repeats=row["repeat_count"],
                triangles=metrics["bvh_triangle_count"],
                route=metrics["active_trace_route"],
                blas_assets=metrics["blas_cached_asset_count"],
                blas_rebuilds=metrics["blas_full_rebuilds"],
                tlas_instances=metrics["tlas_instance_count"],
                calls=metrics["route_trace_calls"],
                hits=metrics["route_tlas_trace_hits"],
                build=metrics["bvh_build_cpu_ms"],
                trace=metrics["render_trace_ms"],
            )
        )
    lines.extend(
        [
            "",
            f"JSON report: `{report['artifacts']['json_report']}`",
            "",
        ]
    )
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--binary", type=Path, default=default_binary())
    parser.add_argument("--run-id", default=f"tlas_blas_repeated_instance_matrix_{time.strftime('%Y-%m-%d')}")
    parser.add_argument("--width", type=int, default=160)
    parser.add_argument("--height", type=int, default=96)
    parser.add_argument("--temporal-frames", type=int, default=1)
    parser.add_argument("--object-audit-max-dimension", type=int, default=160)
    parser.add_argument("--case", action="append", choices=[case.label for case in DEFAULT_CASES])
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if not args.binary.exists() and not args.dry_run:
        raise FileNotFoundError(f"missing headless renderer binary: {args.binary}")

    selected = DEFAULT_CASES
    if args.case:
        wanted = set(args.case)
        selected = [case for case in DEFAULT_CASES if case.label in wanted]

    base_scene = load_json(FIXTURE_ROOT / "scene_runtime_pressure_mrt10.json")
    run_root = (args.output_root / args.run_id).resolve()
    run_root.mkdir(parents=True, exist_ok=True)

    case_reports: list[dict[str, Any]] = []
    for case in selected:
        case_root = run_root / case.label
        scene_path = case_root / "scene_runtime.json"
        request_path = case_root / "request.json"
        stdout_path = case_root / "stdout.txt"
        stderr_path = case_root / "stderr.txt"
        stage_mesh_assets(case_root)
        write_json(scene_path, generated_scene(base_scene, case))
        write_json(
            request_path,
            make_request(
                case=case,
                scene_path=scene_path,
                run_root=case_root,
                width=args.width,
                height=args.height,
                temporal_frames=args.temporal_frames,
                object_audit_max_dimension=args.object_audit_max_dimension,
            ),
        )
        command = [str(args.binary), "--request", str(request_path), "--render"]
        if args.dry_run:
            case_reports.append(
                {
                    "label": case.label,
                    "status": "dry_run",
                    "pass": True,
                    "asset_id": case.asset_id,
                    "repeat_count": case.repeat_count,
                    "artifacts": {"scene": str(scene_path), "request": str(request_path)},
                    "command": command,
                }
            )
            continue

        start = time.monotonic()
        completed = subprocess.run(
            command,
            cwd=str(REPO_ROOT),
            check=False,
            text=True,
            capture_output=True,
        )
        elapsed_s = time.monotonic() - start
        stdout_path.write_text(completed.stdout, encoding="utf-8")
        stderr_path.write_text(completed.stderr, encoding="utf-8")
        case_reports.append(
            build_case_report(
                case=case,
                run_root=case_root,
                command=command,
                elapsed_s=elapsed_s,
                returncode=completed.returncode,
            )
        )

    passed = all(row.get("pass") is True for row in case_reports)
    report = {
        "schema_version": "ray_tracing_tlas_blas_repeated_instance_stress_report_v1",
        "run_id": args.run_id,
        "status": "passed" if passed else "failed",
        "pass": passed,
        "default_route_contract": "request omits inspection.prepared_acceleration_route and must resolve to tlas_blas",
        "cases": case_reports,
        "artifacts": {
            "run_root": str(run_root),
            "json_report": str(run_root / "matrix_report.json"),
            "markdown_report": str(run_root / "matrix_report.md"),
        },
    }
    write_json(run_root / "matrix_report.json", report)
    write_markdown_report(run_root / "matrix_report.md", report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
