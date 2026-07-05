#!/usr/bin/env python3
"""Private high-triangle sidecar stress runner for RayTracing.

This helper is intentionally workspace-private: it writes requests and reports
under ray_tracing/_private_workspace_artifacts/high_triangle_stress and expects
pre-existing external sidecars outside the package tree.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import subprocess
import sys
import time
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SIDECAR_LIBRARY = Path(
    "/Users/calebsv/Desktop/stls/Curated_STL_Test_Library/runtime_mesh_sidecars"
)
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "_private_workspace_artifacts/high_triangle_stress"
DEFAULT_BINARY_CANDIDATES = [
    REPO_ROOT / "build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless",
    REPO_ROOT / "build/arm64/tools/cli/ray_tracing_render_headless",
    REPO_ROOT / "build/tools/cli/ray_tracing_render_headless",
]


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_vec3(raw: str, label: str) -> dict[str, float]:
    parts = raw.split(",")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError(f"{label} must be x,y,z")
    try:
        values = [float(part.strip()) for part in parts]
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"{label} must contain numeric values") from exc
    return {"x": values[0], "y": values[1], "z": values[2]}


def sidecar_root_from_arg(sidecar: str, library_root: Path) -> Path:
    candidate = Path(sidecar).expanduser()
    if candidate.exists():
        return candidate.resolve()
    return (library_root / sidecar).resolve()


def default_binary() -> Path:
    for candidate in DEFAULT_BINARY_CANDIDATES:
        if candidate.exists():
            return candidate
    return DEFAULT_BINARY_CANDIDATES[0]


def find_runtime_asset(sidecar_root: Path, import_summary: dict[str, Any]) -> Path:
    runtime_hint = import_summary.get("runtime")
    candidates: list[Path] = []
    if isinstance(runtime_hint, str) and runtime_hint:
        candidates.append(Path(runtime_hint).expanduser())
    asset_id = import_summary.get("asset_id")
    if isinstance(asset_id, str) and asset_id:
        candidates.append(sidecar_root / "assets" / "mesh_assets" / f"{asset_id}.runtime.json")
    candidates.extend(sorted((sidecar_root / "assets" / "mesh_assets").glob("*.runtime.json")))
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    raise FileNotFoundError(f"no runtime mesh asset JSON found under {sidecar_root}")


def bounds_camera(runtime_asset: dict[str, Any], distance_scale: float) -> tuple[dict[str, float], dict[str, float], dict[str, Any]]:
    bounds = runtime_asset.get("local_bounds", {})
    bmin = bounds.get("min", {})
    bmax = bounds.get("max", {})
    axes = ("x", "y", "z")
    try:
        min_v = {axis: float(bmin[axis]) for axis in axes}
        max_v = {axis: float(bmax[axis]) for axis in axes}
    except (KeyError, TypeError, ValueError) as exc:
        raise ValueError("runtime asset is missing numeric local_bounds") from exc

    center = {axis: (min_v[axis] + max_v[axis]) * 0.5 for axis in axes}
    extent = {axis: max_v[axis] - min_v[axis] for axis in axes}
    max_extent = max(extent.values())
    diagonal = math.sqrt(sum(value * value for value in extent.values()))
    distance = max(max_extent, diagonal * 0.65, 0.25) * distance_scale

    camera_position = {
        "x": center["x"],
        "y": center["y"] - distance,
        "z": center["z"] + max(max_extent * 0.28, 0.08),
    }
    camera_look_at = center
    camera_info = {
        "bounds_min": min_v,
        "bounds_max": max_v,
        "center": center,
        "extent": extent,
        "diagonal": diagonal,
        "distance": distance,
        "distance_scale": distance_scale,
    }
    return camera_position, camera_look_at, camera_info


def has_bmp_header(path: Path) -> bool:
    try:
        with path.open("rb") as handle:
            return handle.read(2) == b"BM"
    except OSError:
        return False


def first_object_hit(summary: dict[str, Any], asset_id: str) -> tuple[int, int, str]:
    object_audit = summary.get("object_audit")
    if not isinstance(object_audit, list):
        return 0, 0, ""
    best_hits = 0
    best_triangles = 0
    best_object = ""
    for entry in object_audit:
        if not isinstance(entry, dict):
            continue
        object_id = str(entry.get("object_id", ""))
        triangles = int(entry.get("triangle_count", 0) or 0)
        hits = int(entry.get("primary_hit_pixels", 0) or 0)
        if asset_id in object_id or hits > best_hits:
            best_hits = hits
            best_triangles = triangles
            best_object = object_id
    return best_hits, best_triangles, best_object


def build_report(
    *,
    args: argparse.Namespace,
    sidecar_root: Path,
    run_root: Path,
    request_path: Path,
    import_summary: dict[str, Any],
    runtime_asset_path: Path,
    runtime_asset: dict[str, Any],
    scene_runtime_path: Path,
    camera_info: dict[str, Any],
    command: list[str],
    elapsed_s: float,
    returncode: int,
    stdout_path: Path,
    stderr_path: Path,
) -> dict[str, Any]:
    summary_path = run_root / "render_summary.json"
    frame_path = run_root / "frames/frame_0000.bmp"
    summary = load_json(summary_path) if summary_path.exists() else {}
    bvh = summary.get("bvh_summary", {}) if isinstance(summary, dict) else {}
    accel = summary.get("prepared_acceleration", {}) if isinstance(summary, dict) else {}
    asset_id = str(import_summary.get("asset_id", runtime_asset.get("asset_id", "")))
    primary_hits, audit_triangles, audit_object_id = first_object_hit(summary, asset_id)

    expected_triangles = int(import_summary.get("triangles", runtime_asset.get("mesh", {}).get("triangle_count", 0)) or 0)
    bvh_triangles = int(bvh.get("triangle_count", 0) or 0)
    trace_overflows = int(bvh.get("trace_overflows", 0) or 0)
    overflow_fallbacks = int(bvh.get("overflow_fallback_calls", 0) or 0)
    flat_fallbacks = int(bvh.get("flat_fallback_calls", 0) or 0)
    active_trace_route = str(accel.get("active_trace_route", ""))
    requested_trace_route = str(accel.get("requested_trace_route", ""))
    route_trace_calls = int(accel.get("route_trace_calls", 0) or 0)
    route_tlas_trace_calls = int(accel.get("route_tlas_trace_calls", 0) or 0)
    route_tlas_trace_hits = int(accel.get("route_tlas_trace_hits", 0) or 0)
    route_flattened_fallback_calls = int(accel.get("route_flattened_fallback_calls", 0) or 0)
    route_parity_mismatches = int(accel.get("route_parity_mismatches", 0) or 0)
    frame_valid = frame_path.exists() and has_bmp_header(frame_path)
    pass_status = bool(
        returncode == 0
        and summary.get("scene_applied") is True
        and summary.get("route_native_3d") is True
        and summary.get("frames_rendered") == 1
        and bvh.get("ready") is True
        and bvh_triangles >= expected_triangles
        and trace_overflows == 0
        and overflow_fallbacks == 0
        and flat_fallbacks == 0
        and active_trace_route == "tlas_blas"
        and requested_trace_route == "tlas_blas"
        and route_trace_calls > 0
        and route_tlas_trace_calls == route_trace_calls
        and route_tlas_trace_hits > 0
        and route_parity_mismatches == 0
        and primary_hits > 0
        and frame_valid
    )

    source_stl = import_summary.get("source_stl")
    source_stl_path = Path(source_stl).expanduser() if isinstance(source_stl, str) and source_stl else None
    source_hash = sha256_file(source_stl_path) if source_stl_path and source_stl_path.exists() else ""

    report = {
        "schema_version": "ray_tracing_private_high_triangle_stress_report_v1",
        "run_id": args.run_id,
        "status": "passed" if pass_status else "failed",
        "pass": pass_status,
        "sidecar": {
            "id": sidecar_root.name,
            "root": str(sidecar_root),
            "scene_runtime": str(scene_runtime_path),
            "runtime_asset": str(runtime_asset_path),
            "import_summary": str(sidecar_root / "import_summary.json"),
            "source_stl": str(source_stl_path) if source_stl_path else "",
            "source_stl_sha256": source_hash,
            "asset_id": asset_id,
            "vertices": import_summary.get("vertices", runtime_asset.get("mesh", {}).get("vertex_count", 0)),
            "triangles": expected_triangles,
            "surface_groups": import_summary.get("surface_groups", 0),
        },
        "render": {
            "integrator_3d": args.integrator,
            "width": args.width,
            "height": args.height,
            "temporal_frames": args.temporal_frames,
            "camera_position": summary.get("inspection", {}).get("camera_position", {}),
            "camera_look_at": summary.get("inspection", {}).get("camera_look_at", {}),
            "camera_zoom": summary.get("inspection", {}).get("camera_zoom", args.camera_zoom),
            "derived_camera": camera_info,
        },
        "metrics": {
            "bvh_ready": bvh.get("ready", False),
            "bvh_triangle_count": bvh_triangles,
            "bvh_node_count": bvh.get("node_count", 0),
            "bvh_leaf_count": bvh.get("leaf_count", 0),
            "bvh_max_depth": bvh.get("max_depth", 0),
            "bvh_build_cpu_ms": bvh.get("build_cpu_ms", 0),
            "bvh_total_bytes": bvh.get("total_bytes", 0),
            "bvh_build_scratch_bytes": bvh.get("build_scratch_bytes", 0),
            "trace_calls": bvh.get("trace_calls", 0),
            "trace_overflows": trace_overflows,
            "flat_fallback_calls": flat_fallbacks,
            "overflow_fallback_calls": overflow_fallbacks,
            "triangle_tests": bvh.get("triangle_tests", 0),
            "prepared_acceleration_enabled": accel.get("enabled", False),
            "prepared_accel_reuse_status": accel.get("prepared_accel_reuse_status", ""),
            "blas_prepare_calls": accel.get("blas_prepare_calls", 0),
            "blas_cache_hits": accel.get("blas_cache_hits", 0),
            "blas_cache_misses": accel.get("blas_cache_misses", 0),
            "blas_full_rebuilds": accel.get("blas_full_rebuilds", 0),
            "blas_cached_asset_count": accel.get("blas_cached_asset_count", 0),
            "blas_build_ms": accel.get("blas_build_ms", 0),
            "blas_persistent_cache_hits": accel.get("blas_persistent_cache_hits", 0),
            "blas_persistent_cache_misses": accel.get("blas_persistent_cache_misses", 0),
            "blas_persistent_cache_writes": accel.get("blas_persistent_cache_writes", 0),
            "blas_persistent_cache_invalidations": accel.get("blas_persistent_cache_invalidations", 0),
            "blas_persistent_cache_refreshes": accel.get("blas_persistent_cache_refreshes", 0),
            "blas_persistent_cache_read_ms": accel.get("blas_persistent_cache_read_ms", 0),
            "blas_persistent_cache_write_ms": accel.get("blas_persistent_cache_write_ms", 0),
            "tlas_node_count": accel.get("tlas_node_count", 0),
            "tlas_instance_count": accel.get("tlas_instance_count", 0),
            "tlas_rebuilds": accel.get("tlas_rebuilds", 0),
            "active_trace_route": active_trace_route,
            "requested_trace_route": requested_trace_route,
            "route_trace_calls": route_trace_calls,
            "route_tlas_trace_calls": route_tlas_trace_calls,
            "route_tlas_trace_hits": route_tlas_trace_hits,
            "route_flattened_fallback_calls": route_flattened_fallback_calls,
            "route_parity_mismatches": route_parity_mismatches,
            "primary_hit_pixels": primary_hits,
            "object_audit_triangle_count": audit_triangles,
            "object_audit_object_id": audit_object_id,
            "frame_valid_bmp": frame_valid,
            "elapsed_wall_seconds": round(elapsed_s, 3),
            "process_returncode": returncode,
        },
        "artifacts": {
            "run_root": str(run_root),
            "request": str(request_path),
            "summary": str(summary_path),
            "first_frame": str(frame_path),
            "report": str(run_root / "stress_report.json"),
            "stdout": str(stdout_path),
            "stderr": str(stderr_path),
        },
        "command": command,
    }
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sidecar", help="sidecar id under the library root, or an explicit sidecar root")
    parser.add_argument("--sidecar-library-root", type=Path, default=DEFAULT_SIDECAR_LIBRARY)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--binary", type=Path, default=default_binary())
    parser.add_argument("--run-id", default="")
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=192)
    parser.add_argument("--temporal-frames", type=int, default=1)
    parser.add_argument("--integrator", default="direct_light", choices=["direct_light", "diffuse_bounce", "material", "emission_transparency", "disney", "disney_v2"])
    parser.add_argument("--camera-position", type=lambda value: parse_vec3(value, "camera-position"))
    parser.add_argument("--camera-look-at", type=lambda value: parse_vec3(value, "camera-look-at"))
    parser.add_argument("--camera-zoom", type=float, default=1.05)
    parser.add_argument("--camera-distance-scale", type=float, default=1.6)
    parser.add_argument("--ambient-strength", type=float, default=0.45)
    parser.add_argument("--top-fill-strength", type=float, default=1.6)
    parser.add_argument("--light-intensity", type=float, default=4.0)
    parser.add_argument("--light-radius", type=float, default=0.05)
    parser.add_argument("--object-audit-max-dimension", type=int, default=160)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if args.integrator != "direct_light":
        print("warning: H3 stress proof should use direct_light first; non-direct run is diagnostic only", file=sys.stderr)

    sidecar_root = sidecar_root_from_arg(args.sidecar, args.sidecar_library_root)
    import_summary_path = sidecar_root / "import_summary.json"
    scene_runtime_path = sidecar_root / "scene_runtime.json"
    if not import_summary_path.exists():
        raise FileNotFoundError(f"missing import summary: {import_summary_path}")
    if not scene_runtime_path.exists():
        raise FileNotFoundError(f"missing runtime scene: {scene_runtime_path}")

    import_summary = load_json(import_summary_path)
    runtime_asset_path = find_runtime_asset(sidecar_root, import_summary)
    runtime_asset = load_json(runtime_asset_path)

    default_run_id = f"h3_{sidecar_root.name}_{time.strftime('%Y-%m-%d')}"
    args.run_id = args.run_id or default_run_id
    run_root = (args.output_root / args.run_id).resolve()
    request_path = run_root / "request.json"
    summary_path = run_root / "render_summary.json"
    progress_path = run_root / "render_progress.json"
    stdout_path = run_root / "stdout.txt"
    stderr_path = run_root / "stderr.txt"

    derived_camera_position, derived_camera_look_at, camera_info = bounds_camera(
        runtime_asset, args.camera_distance_scale
    )
    camera_position = args.camera_position or derived_camera_position
    camera_look_at = args.camera_look_at or derived_camera_look_at

    request = {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": args.run_id.replace("-", "_"),
        "scene": {"runtime_scene_path": str(scene_runtime_path.resolve())},
        "volume": {"enabled": False},
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": args.width,
            "height": args.height,
            "normalized_t": 0.0,
            "temporal_frames": args.temporal_frames,
            "integrator_3d": args.integrator,
        },
        "inspection": {
            "camera_position": camera_position,
            "camera_look_at": camera_look_at,
            "camera_zoom": args.camera_zoom,
            "environment_light_mode": "ambient",
            "ambient_strength": args.ambient_strength,
            "top_fill_strength": args.top_fill_strength,
            "light_intensity": args.light_intensity,
            "light_radius": args.light_radius,
            "object_audit_enabled": True,
            "object_audit_max_dimension": args.object_audit_max_dimension,
        },
        "output": {"root": str(run_root), "overwrite": True},
        "progress": {"summary_path": str(summary_path), "progress_path": str(progress_path)},
    }
    write_json(request_path, request)

    command = [str(args.binary), "--request", str(request_path), "--render"]
    if args.dry_run:
        write_json(
            run_root / "stress_report.json",
            {
                "schema_version": "ray_tracing_private_high_triangle_stress_report_v1",
                "run_id": args.run_id,
                "status": "dry_run",
                "sidecar_root": str(sidecar_root),
                "request": str(request_path),
                "command": command,
                "derived_camera": camera_info,
            },
        )
        print(str(request_path))
        return 0

    if not args.binary.exists():
        raise FileNotFoundError(f"missing headless renderer binary: {args.binary}")

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

    report = build_report(
        args=args,
        sidecar_root=sidecar_root,
        run_root=run_root,
        request_path=request_path,
        import_summary=import_summary,
        runtime_asset_path=runtime_asset_path,
        runtime_asset=runtime_asset,
        scene_runtime_path=scene_runtime_path.resolve(),
        camera_info=camera_info,
        command=command,
        elapsed_s=elapsed_s,
        returncode=completed.returncode,
        stdout_path=stdout_path,
        stderr_path=stderr_path,
    )
    write_json(run_root / "stress_report.json", report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
