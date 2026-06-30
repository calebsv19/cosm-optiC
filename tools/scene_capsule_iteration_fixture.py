#!/usr/bin/env python3
"""Build and validate the local scene-capsule iteration fixture.

This is a water-free proving lane for stable cache + scene + patch iteration.
It intentionally keeps generated requests and render outputs under
_private_workspace_artifacts so the source tree only owns the reusable driver.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


CAPSULE_ID = "scene_capsule_iteration_fixture_v1"
SCHEMA = "codework_scene_capsule_fixture_v1"
PHYSICS_WORKER_VERSION = "0.3.0"
RAY_WORKER_VERSION = "0.4.16"


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def default_capsule_root(root: Path) -> Path:
    return root / "_private_workspace_artifacts" / "scene_capsules" / CAPSULE_ID


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_hash(value: Any) -> str:
    data = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return sha256_bytes(data)


def file_hash(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def point(x: float, y: float, z: float) -> dict[str, float]:
    return {"x": x, "y": y, "z": z}


def base_runtime_scene() -> dict[str, Any]:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": CAPSULE_ID,
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            {
                "object_id": "capsule_floor",
                "object_type": "plane_primitive",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {"position": point(0.0, 0.0, -0.65), "scale": point(1.0, 1.0, 1.0)},
                "primitive": {
                    "kind": "plane_primitive",
                    "width": 5.2,
                    "height": 4.2,
                    "frame": {
                        "origin": point(0.0, 0.0, -0.65),
                        "axis_u": point(1.0, 0.0, 0.0),
                        "axis_v": point(0.0, 1.0, 0.0),
                        "normal": point(0.0, 0.0, 1.0),
                    },
                },
            },
            {
                "object_id": "capsule_back_wall",
                "object_type": "plane_primitive",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {"position": point(0.0, 1.65, 0.35), "scale": point(1.0, 1.0, 1.0)},
                "primitive": {
                    "kind": "plane_primitive",
                    "width": 5.2,
                    "height": 2.2,
                    "frame": {
                        "origin": point(0.0, 1.65, 0.35),
                        "axis_u": point(1.0, 0.0, 0.0),
                        "axis_v": point(0.0, 0.0, 1.0),
                        "normal": point(0.0, -1.0, 0.0),
                    },
                },
            },
            {
                "object_id": "capsule_reference_block",
                "object_type": "rect_prism_primitive",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {"position": point(0.85, 0.15, -0.15), "scale": point(1.0, 1.0, 1.0)},
                "primitive": {"kind": "rect_prism_primitive", "width": 0.62, "height": 0.72, "depth": 0.95},
            },
            {
                "object_id": "capsule_emitter_marker",
                "object_type": "rect_prism_primitive",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {"position": point(-0.95, -0.45, -0.22), "scale": point(1.0, 1.0, 1.0)},
                "primitive": {"kind": "rect_prism_primitive", "width": 0.42, "height": 0.42, "depth": 0.72},
            },
        ],
        "materials": [],
        "lights": [],
        "cameras": [],
        "constraints": [],
        "hierarchy": [],
        "extensions": {
            "physics_sim": {
                "scene_domain": {
                    "active": True,
                    "shape": "box",
                    "min": {"x": -2.6, "y": -2.1, "z": -0.65},
                    "max": {"x": 2.6, "y": 2.1, "z": 1.65},
                },
                "wind_tunnel": {
                    "active": True,
                    "inlet_face": "left",
                    "outlet_face": "right",
                    "inflow_speed": 9.0,
                    "inflow_density": 0.85,
                    "inlet_slab_cells": 2,
                    "outlet_policy": "receive",
                    "wall_policy": "no_slip",
                },
            },
            "ray_tracing": {
                "authoring": {
                    "light_settings": {"intensity": 4.2, "radius": 0.12},
                    "environment": {"light_mode": 2, "ambient_strength": 0.22, "top_fill_strength": 0.9},
                    "object_materials": [
                        {"object_id": "capsule_floor", "material_id": 0, "object_color": 10592673, "roughness": 0.74, "reflectivity": 0.05},
                        {"object_id": "capsule_back_wall", "material_id": 0, "object_color": 8751493, "roughness": 0.70, "reflectivity": 0.04},
                        {"object_id": "capsule_reference_block", "material_id": 0, "object_color": 4093374, "roughness": 0.58, "reflectivity": 0.12},
                        {"object_id": "capsule_emitter_marker", "material_id": 4, "object_color": 16758085, "roughness": 1.0, "reflectivity": 0.0, "emissive_strength": 0.8},
                    ],
                }
            },
        },
    }


def base_request(capsule_root: Path, scene_path: Path, scene_bundle: Path, render_slug: str) -> dict[str, Any]:
    output_root = capsule_root / "renders" / render_slug
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": f"{CAPSULE_ID}_{render_slug}",
        "scene": {"runtime_scene_path": str(scene_path)},
        "volume": {
            "enabled": True,
            "source_kind": "scene_bundle",
            "source_path": str(scene_bundle),
            "visible": True,
            "affects_lighting": True,
            "debug_overlay": False,
        },
        "render": {
            "start_frame": 0,
            "frame_count": 3,
            "width": 240,
            "height": 160,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "integrator_3d": "direct_light",
        },
        "inspection": {
            "camera_zoom": 0.95,
            "camera_position": {"x": 0.0, "y": -4.6, "z": 1.95},
            "camera_look_at": {"x": 0.0, "y": 0.05, "z": 0.2},
            "environment_light_mode": "ambient",
            "ambient_strength": 0.22,
            "top_fill_strength": 0.9,
            "background_brightness": 0.035,
            "background_color": {"r": 0.08, "g": 0.09, "b": 0.10},
            "light_intensity": 4.2,
            "light_radius": 0.12,
            "forward_decay": 180.0,
            "volume_scatter_gain": 1.4,
            "volume_step_scale": 1.2,
            "volume_tint": {"r": 0.38, "g": 0.72, "b": 1.35},
            "object_audit_enabled": True,
            "object_audit_max_dimension": 96,
        },
        "output": {"root": str(output_root), "overwrite": True},
        "progress": {
            "summary_path": str(output_root / "render_summary.json"),
            "progress_path": str(output_root / "render_progress.json"),
        },
    }


def direct_draft_profile() -> dict[str, Any]:
    return {
        "schema": f"{SCHEMA}.render_profile",
        "profile_id": "direct_draft",
        "request_overrides": {},
        "output_contract": {"kind": "png_sequence", "visualizer": False},
    }


def direct_quality_profile() -> dict[str, Any]:
    return {
        "schema": f"{SCHEMA}.render_profile",
        "profile_id": "direct_quality",
        "request_overrides": {
            "/render/width": 320,
            "/render/height": 200,
            "/render/temporal_frames": 2,
            "/inspection/volume_step_scale": 0.95,
        },
        "output_contract": {"kind": "png_sequence", "visualizer": False},
    }


def floor_patch() -> dict[str, Any]:
    return {
        "schema": f"{SCHEMA}.patch",
        "patch_id": "floor_material_wood_v1",
        "allowed_scene_paths": [
            "/extensions/ray_tracing/authoring/object_materials/0/object_color",
            "/extensions/ray_tracing/authoring/object_materials/0/roughness",
            "/extensions/ray_tracing/authoring/object_materials/0/reflectivity",
        ],
        "allowed_request_paths": [],
        "scene_overrides": {
            "/extensions/ray_tracing/authoring/object_materials/0/object_color": 12754043,
            "/extensions/ray_tracing/authoring/object_materials/0/roughness": 0.46,
            "/extensions/ray_tracing/authoring/object_materials/0/reflectivity": 0.14,
        },
        "request_overrides": {},
    }


def camera_patch() -> dict[str, Any]:
    return {
        "schema": f"{SCHEMA}.patch",
        "patch_id": "camera_close_v1",
        "allowed_scene_paths": [],
        "allowed_request_paths": [
            "/inspection/camera_zoom",
            "/inspection/camera_position/y",
            "/inspection/camera_position/z",
            "/inspection/camera_look_at/z",
        ],
        "scene_overrides": {},
        "request_overrides": {
            "/inspection/camera_zoom": 1.16,
            "/inspection/camera_position/y": -3.55,
            "/inspection/camera_position/z": 1.55,
            "/inspection/camera_look_at/z": 0.05,
        },
    }


def set_json_pointer(value: Any, pointer: str, replacement: Any) -> None:
    if not pointer.startswith("/"):
        raise ValueError(f"expected JSON pointer path, got {pointer!r}")
    parts = [part.replace("~1", "/").replace("~0", "~") for part in pointer.split("/")[1:]]
    cursor = value
    for part in parts[:-1]:
        cursor = cursor[int(part)] if isinstance(cursor, list) else cursor[part]
    leaf = parts[-1]
    if isinstance(cursor, list):
        cursor[int(leaf)] = replacement
    else:
        cursor[leaf] = replacement


def diff_paths(a: Any, b: Any, prefix: str = "") -> list[str]:
    if type(a) is not type(b):
        return [prefix or "/"]
    if isinstance(a, dict):
        paths: list[str] = []
        for key in sorted(set(a) | set(b)):
            escaped = str(key).replace("~", "~0").replace("/", "~1")
            child = f"{prefix}/{escaped}"
            if key not in a or key not in b:
                paths.append(child)
            else:
                paths.extend(diff_paths(a[key], b[key], child))
        return paths
    if isinstance(a, list):
        paths = []
        max_len = max(len(a), len(b))
        for index in range(max_len):
            child = f"{prefix}/{index}"
            if index >= len(a) or index >= len(b):
                paths.append(child)
            else:
                paths.extend(diff_paths(a[index], b[index], child))
        return paths
    return [] if a == b else [prefix or "/"]


def ensure_allowed(changed: list[str], allowed: list[str], label: str) -> None:
    unexpected = [path for path in changed if path not in allowed]
    if unexpected:
        raise RuntimeError(f"{label} changed outside patch allowlist: {unexpected}")


def init_capsule(capsule_root: Path) -> None:
    scene = base_runtime_scene()
    scene_hash = canonical_hash(scene)
    scene_path = capsule_root / "base_scene" / "scene_runtime.json"
    write_json(scene_path, scene)
    (capsule_root / "base_scene" / "scene_hash.txt").write_text(scene_hash + "\n", encoding="utf-8")
    write_json(
        capsule_root / "base_scene" / "scene_baseline.json",
        {
            "schema": f"{SCHEMA}.baseline",
            "capsule_id": CAPSULE_ID,
            "scene_id": scene["scene_id"],
            "scene_hash": scene_hash,
            "locked_fields": [
                "scene.objects",
                "scene.extensions.ray_tracing.authoring",
                "scene.extensions.physics_sim",
                "request.inspection",
                "request.render",
                "request.volume",
            ],
        },
    )
    write_json(capsule_root / "render_profiles" / "direct_draft.json", direct_draft_profile())
    write_json(capsule_root / "render_profiles" / "direct_quality.json", direct_quality_profile())
    write_json(capsule_root / "patches" / "floor_material_wood_v1.json", floor_patch())
    write_json(capsule_root / "patches" / "camera_close_v1.json", camera_patch())
    write_json(
        capsule_root / "capsule_manifest.json",
        {
            "schema": SCHEMA,
            "capsule_id": CAPSULE_ID,
            "base_scene": "base_scene/scene_runtime.json",
            "base_scene_hash": scene_hash,
            "cache_manifest": "cache/volume_manifest.json",
            "first_slice_variants": [
                {"slug": "a_base_direct", "profile": "direct_draft", "patch": None},
                {"slug": "b_floor_wood", "profile": "direct_draft", "patch": "floor_material_wood_v1"},
                {"slug": "c_camera_close", "profile": "direct_draft", "patch": "camera_close_v1"},
            ],
        },
    )


def run_command(command: list[str], cwd: Path | None = None) -> None:
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, cwd=str(cwd) if cwd else None, check=True)


def make_tool(root: Path, target_dir: str, target: str, build_toolchain: str | None = None) -> None:
    command = ["make", "-C", str(root / target_dir)]
    if build_toolchain:
        command.append(f"BUILD_TOOLCHAIN={build_toolchain}")
    command.append(target)
    run_command(command)


def find_scene_bundle(cache_run_root: Path) -> Path:
    candidates = sorted(cache_run_root.glob("volume_frames/**/scene_bundle.json"))
    if not candidates:
        raise RuntimeError(f"no scene_bundle.json under {cache_run_root}")
    return candidates[0]


def generate_cache(root: Path, capsule_root: Path, build_tools: bool) -> None:
    if build_tools:
        make_tool(root, "physics_sim", "physics_sim_headless")
    physics_tool = root / "physics_sim" / "physics_sim_headless"
    if not physics_tool.exists():
        raise RuntimeError(f"missing PhysicsSim tool: {physics_tool}")
    cache_run_root = capsule_root / "cache" / "physics_sim"
    run_command(
        [
            str(physics_tool),
            "--runtime-scene",
            str(capsule_root / "base_scene" / "scene_runtime.json"),
            "--frames",
            "4",
            "--sim-steps-per-frame",
            "2",
            "--grid",
            "24x12x12",
            "--output-root",
            str(cache_run_root),
            "--summary",
            str(cache_run_root / "run_summary.json"),
            "--progress",
            str(cache_run_root / "run_progress.json"),
            "--progress-interval",
            "0",
            "--overwrite",
            "--save-volume-frames",
        ]
    )
    scene_bundle = find_scene_bundle(cache_run_root)
    frames = sorted(scene_bundle.parent.glob("frame_*.vf3d"))
    if not frames:
        raise RuntimeError(f"no VF3D frames beside {scene_bundle}")
    write_json(
        capsule_root / "cache" / "volume_manifest.json",
        {
            "schema": f"{SCHEMA}.cache_manifest",
            "cache_id": f"{CAPSULE_ID}_tiny_volume_cache_v1",
            "physics_summary_path": str(cache_run_root / "run_summary.json"),
            "scene_bundle_path": str(scene_bundle),
            "scene_bundle_sha256": file_hash(scene_bundle),
            "frame_count": len(frames),
            "frames": [
                {
                    "ordinal_index": index,
                    "physical_frame_index": index,
                    "path": str(frame),
                    "sha256": file_hash(frame),
                }
                for index, frame in enumerate(frames)
            ],
        },
    )


def apply_overrides(value: Any, overrides: dict[str, Any]) -> Any:
    result = copy.deepcopy(value)
    for pointer, replacement in overrides.items():
        set_json_pointer(result, pointer, replacement)
    return result


def load_profile(capsule_root: Path, profile_id: str) -> dict[str, Any]:
    path = capsule_root / "render_profiles" / f"{profile_id}.json"
    if not path.exists():
        raise RuntimeError(f"missing render profile: {path}")
    return read_json(path)


def load_patch(capsule_root: Path, patch_id: str | None) -> dict[str, Any]:
    if not patch_id:
        return {
            "patch_id": None,
            "allowed_scene_paths": [],
            "allowed_request_paths": [],
            "scene_overrides": {},
            "request_overrides": {},
        }
    path = capsule_root / "patches" / f"{patch_id}.json"
    if not path.exists():
        raise RuntimeError(f"missing patch: {path}")
    return read_json(path)


def build_variant(capsule_root: Path, slug: str, profile_id: str, patch_id: str | None) -> Path:
    baseline = read_json(capsule_root / "base_scene" / "scene_baseline.json")
    base_scene = read_json(capsule_root / "base_scene" / "scene_runtime.json")
    actual_scene_hash = canonical_hash(base_scene)
    if actual_scene_hash != baseline["scene_hash"]:
        raise RuntimeError(f"base scene hash mismatch: expected {baseline['scene_hash']} got {actual_scene_hash}")
    cache_manifest = read_json(capsule_root / "cache" / "volume_manifest.json")
    scene_bundle = Path(cache_manifest["scene_bundle_path"])
    profile = load_profile(capsule_root, profile_id)
    patch = load_patch(capsule_root, patch_id)
    base_scene_out = capsule_root / "generated_requests" / slug / "scene_runtime.json"
    base_request_value = base_request(capsule_root, base_scene_out, scene_bundle, slug)
    profiled_request = apply_overrides(base_request_value, profile.get("request_overrides", {}))
    patched_scene = apply_overrides(base_scene, patch.get("scene_overrides", {}))
    patched_request = apply_overrides(profiled_request, patch.get("request_overrides", {}))
    scene_changes = diff_paths(base_scene, patched_scene)
    request_changes = diff_paths(profiled_request, patched_request)
    ensure_allowed(scene_changes, patch.get("allowed_scene_paths", []), f"{slug} scene")
    ensure_allowed(request_changes, patch.get("allowed_request_paths", []), f"{slug} request")
    request_path = capsule_root / "generated_requests" / slug / "ray_tracing_request.json"
    audit_path = capsule_root / "audits" / f"{slug}.audit.json"
    write_json(base_scene_out, patched_scene)
    write_json(request_path, patched_request)
    missing_references: list[str] = []
    for reference in [
        Path(patched_request["scene"]["runtime_scene_path"]),
        Path(patched_request.get("volume", {}).get("source_path", "")),
    ]:
        if not reference.exists():
            missing_references.append(str(reference))
    if missing_references:
        raise RuntimeError(f"{slug} generated request has missing referenced files: {missing_references}")
    write_json(
        audit_path,
        {
            "schema": f"{SCHEMA}.audit",
            "capsule_id": CAPSULE_ID,
            "slug": slug,
            "profile_id": profile_id,
            "patch_id": patch.get("patch_id"),
            "base_scene_hash": baseline["scene_hash"],
            "generated_scene_hash": canonical_hash(patched_scene),
            "cache_id": cache_manifest["cache_id"],
            "scene_changes": scene_changes,
            "request_changes": request_changes,
            "allowed_scene_paths": patch.get("allowed_scene_paths", []),
            "allowed_request_paths": patch.get("allowed_request_paths", []),
            "referenced_files": {
                "runtime_scene_path": patched_request["scene"]["runtime_scene_path"],
                "volume_source_path": patched_request["volume"]["source_path"],
            },
        },
    )
    return request_path


def ray_binary(root: Path) -> Path:
    machine = os.uname().machine
    candidates = [
        root / "ray_tracing" / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless",
        root / "ray_tracing" / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless",
        root / "ray_tracing" / "build" / "tools" / "cli" / "ray_tracing_render_headless",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise RuntimeError("missing ray_tracing_render_headless binary")


def render_variant(root: Path, request_path: Path) -> None:
    request = read_json(request_path)
    output_root = Path(request["output"]["root"])
    summary_path = Path(request["progress"]["summary_path"])
    stdout_summary = output_root / "stdout_summary.json"
    output_root.mkdir(parents=True, exist_ok=True)
    run_command([str(ray_binary(root)), "--request", str(request_path), "--render", "--summary", str(summary_path)])
    if not summary_path.exists():
        raise RuntimeError(f"missing render summary: {summary_path}")
    summary = read_json(summary_path)
    if summary.get("schema_version") != "ray_tracing_headless_summary_v1":
        raise RuntimeError(f"bad render summary schema in {summary_path}")
    if int(summary.get("frames_rendered", 0)) < 1:
        raise RuntimeError(f"no rendered frames in {summary_path}")
    frames = sorted((output_root / "frames").glob("frame_*.bmp"))
    if not frames:
        raise RuntimeError(f"missing BMP frames under {output_root / 'frames'}")
    for frame in frames[:3]:
        if frame.read_bytes()[:2] != b"BM":
            raise RuntimeError(f"rendered frame is not BMP: {frame}")
    write_json(stdout_summary, {"summary_path": str(summary_path), "frames_checked": [str(frame) for frame in frames[:3]]})


def copy_file(src: Path, dst: Path) -> None:
    if not src.is_file():
        raise RuntimeError(f"missing source file: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def safe_item_component(value: str) -> str:
    out: list[str] = []
    last_dash = False
    for ch in str(value).lower():
        if ch.isalnum():
            out.append(ch)
            last_dash = False
        elif not last_dash:
            out.append("-")
            last_dash = True
    return "".join(out).strip("-") or "scene-capsule-fixture"


def queue_output_root(capsule_root: Path) -> Path:
    return capsule_root / "queue_exports"


def fixture_job_id(slug: str) -> str:
    return f"ray-tracing--scene-capsule-fixture-{safe_item_component(slug)}--20260626T000000Z--scapsule04"


def render_submit_args(job_id: str, request: dict[str, Any], inspection_file: Path, volume_source_path: str) -> list[str]:
    render = request["render"]
    inspection = request["inspection"]
    output = request.get("output", {})
    resources = request.get("resources", {})
    args = [
        "--job-id",
        job_id,
        "--job-id-policy",
        "validate",
        "--profile",
        "preview",
        "--physics-version",
        PHYSICS_WORKER_VERSION,
        "--ray-version",
        RAY_WORKER_VERSION,
        "--start-stage",
        "ray_tracing",
        "--physics-frames",
        "4",
        "--physics-sim-steps",
        "2",
        "--physics-progress-interval",
        "1",
        "--physics-grid",
        "24x12x12",
        "--ray-volume-source-kind",
        str(request.get("volume", {}).get("source_kind") or "scene_bundle"),
        "--ray-volume-source-path",
        volume_source_path,
        "--ray-volume-visible",
        "true" if request.get("volume", {}).get("visible", True) else "false",
        "--ray-volume-affects-lighting",
        "true" if request.get("volume", {}).get("affects_lighting", True) else "false",
        "--ray-frame-count",
        str(render["frame_count"]),
        "--ray-start-frame",
        str(render["start_frame"]),
        "--ray-width",
        str(render["width"]),
        "--ray-height",
        str(render["height"]),
        "--ray-temporal-frames",
        str(render["temporal_frames"]),
        "--ray-normalized-t",
        str(render["normalized_t"]),
        "--ray-integrator",
        str(render["integrator_3d"]),
        "--ray-inspection-file",
        str(inspection_file.resolve()),
        "--force-worker-id",
        "linuxpc",
        "--force-hostname",
        "localhost.localdomain",
        "--force-worker-kind",
        "remote_worker",
        "--force-label",
        "linux-pc",
        "--no-worker-fallback",
    ]
    sampling = request.get("sampling")
    if isinstance(sampling, dict):
        args.extend(["--ray-sampling-frame-offset", str(sampling["frame_offset"])])
        args.extend(["--ray-sampling-frame-count", str(sampling["frame_count"])])
    if resources:
        if "cpu_percent" in resources:
            args.extend(["--ray-resource-cpu-percent", str(resources["cpu_percent"])])
        if "max_workers" in resources:
            args.extend(["--ray-resource-max-workers", str(resources["max_workers"])])
        if "reserve_cpu_count" in resources:
            args.extend(["--ray-resource-reserve-cpu-count", str(resources["reserve_cpu_count"])])
    video = output.get("video") if isinstance(output, dict) else None
    if isinstance(video, dict) and video.get("enabled"):
        video_path = str(video.get("path") or "")
        args.extend(["--ray-video-output-relpath", Path(video_path).name])
        if video.get("fps"):
            args.extend(["--ray-video-fps", str(video["fps"])])
    if not isinstance(inspection, dict):
        raise RuntimeError("render request inspection must be an object for queue export")
    return args


def export_queue_item(capsule_root: Path, slug: str, item_name: str | None, force: bool) -> Path:
    request_path = capsule_root / "generated_requests" / slug / "ray_tracing_request.json"
    scene_path = capsule_root / "generated_requests" / slug / "scene_runtime.json"
    if not request_path.is_file() or not scene_path.is_file():
        raise RuntimeError(f"missing generated request for {slug}; run build-requests first")
    request = read_json(request_path)
    cache_manifest = read_json(capsule_root / "cache" / "volume_manifest.json")
    source_bundle = Path(cache_manifest["scene_bundle_path"])
    if not source_bundle.is_file():
        raise RuntimeError(f"missing source scene_bundle: {source_bundle}")
    source_volume_dir = source_bundle.parent
    item = safe_item_component(item_name or f"desktop-scene-capsule-fixture-{slug}-20260626a")
    job_id = fixture_job_id(slug)
    root = queue_output_root(capsule_root)
    item_root = root / "inbox" / item
    bundle_root = item_root / "bundle"
    if item_root.exists():
        if not force:
            raise RuntimeError(f"queue item already exists: {item_root} (use --force)")
        shutil.rmtree(item_root)

    payload_scene = bundle_root / "request" / "payload" / "line_drawing" / "scene_runtime.json"
    copy_file(scene_path, bundle_root / "scene_runtime.json")
    copy_file(scene_path, payload_scene)

    volume_payload_relroot = Path("line_drawing/assets/vf3d/capsule_cache")
    live_volume_relroot = Path("assets/vf3d/capsule_cache")
    support_relpaths: list[str] = []
    copied_volume_files: list[dict[str, Any]] = []
    for src in sorted(source_volume_dir.iterdir()):
        if not src.is_file() or src.suffix.lower() not in {".json", ".vf3d", ".vf3h"}:
            continue
        rel = volume_payload_relroot / src.name
        dst = bundle_root / "request" / "payload" / rel
        copy_file(src, dst)
        support_relpaths.append(rel.as_posix())
        copied_volume_files.append(
            {
                "source": str(src),
                "payload_relpath": rel.as_posix(),
                "live_relpath": (live_volume_relroot / src.name).as_posix(),
                "sha256": file_hash(src),
                "bytes": src.stat().st_size,
            }
        )

    live_scene_bundle_relpath = (live_volume_relroot / "scene_bundle.json").as_posix()
    if not any(item["live_relpath"] == live_scene_bundle_relpath for item in copied_volume_files):
        raise RuntimeError("copied volume payload did not include scene_bundle.json")

    portable_request = copy.deepcopy(request)
    portable_request["run_id"] = job_id
    portable_request["scene"]["runtime_scene_path"] = "scene_runtime.json"
    portable_request["volume"]["source_path"] = live_scene_bundle_relpath
    portable_request["output"] = {"root": "../../stages/ray_tracing/output", "overwrite": True}
    portable_request["progress"] = {
        "summary_path": "../../stages/ray_tracing/output/render_summary.json",
        "progress_path": "../../stages/ray_tracing/output/render_progress.json",
    }
    write_json(bundle_root / "render_request.json", portable_request)
    write_json(bundle_root / "presets" / "inspection_settings.json", portable_request["inspection"])
    write_json(
        bundle_root / "request" / "job.json",
        {
            "schema_version": "codework_worker_job_v1",
            "job_id": job_id,
            "workflow": "line_author->physics_sim->ray_tracing",
            "submission_mode": "private-api",
            "start_stage": "ray_tracing",
            "worker_package_set": {
                "physics_sim": {
                    "worker_slug": "physics_sim_headless_worker",
                    "version": PHYSICS_WORKER_VERSION,
                    "platform": "linux-x86_64",
                },
                "ray_tracing": {
                    "worker_slug": "ray_tracing_headless_worker",
                    "version": RAY_WORKER_VERSION,
                    "platform": "linux-x86_64",
                },
            },
            "input_contract": {
                "schema_version": "codework_worker_input_v1",
                "line_drawing_origin": "mac-authored",
                "scene_runtime_relpath": "line_drawing/scene_runtime.json",
                "support_files_relpaths": support_relpaths,
            },
            "output_contract": {
                "schema_version": "visualizer-run/v1",
                "publish_mode": "stage_visualizer_drop",
            },
        },
    )
    submit_args = render_submit_args(
        job_id,
        portable_request,
        bundle_root / "presets" / "inspection_settings.json",
        live_scene_bundle_relpath,
    )
    write_json(
        item_root / "worker_job_queue.json",
        {
            "schema": "codework-worker-desktop-queue-item/v1",
            "item_id": item,
            "bundle_dir": "bundle",
            "overwrite_staging": True,
            "submit_args": submit_args,
        },
    )
    write_json(
        bundle_root / "manifest.json",
        {
            "schema": f"{SCHEMA}.queue_export_manifest",
            "capsule_id": CAPSULE_ID,
            "slug": slug,
            "item_name": item,
            "job_id": job_id,
            "queue_root": str(root),
            "volume_source_path": live_scene_bundle_relpath,
            "copied_volume_files": copied_volume_files,
            "support_file_count": len(support_relpaths),
            "referenced_file_audit": {
                "request_relpath": "render_request.json",
                "volume_source_path": live_scene_bundle_relpath,
                "volume_source_in_payload": True,
            },
        },
    )
    return item_root


def audit_staged_submit_payload(payload_path: Path) -> dict[str, Any]:
    payload = read_json(payload_path)
    payload_files = payload.get("payload_files")
    if not isinstance(payload_files, list):
        raise RuntimeError(f"submit payload has no payload_files array: {payload_path}")
    relpaths = {
        str(entry.get("relpath") or "")
        for entry in payload_files
        if isinstance(entry, dict)
    }
    request_doc = None
    for entry in payload_files:
        if not isinstance(entry, dict) or entry.get("relpath") != "ray_tracing_request.json":
            continue
        content = entry.get("content_utf8")
        if not isinstance(content, str):
            raise RuntimeError("ray_tracing_request.json payload is not UTF-8 JSON")
        request_doc = json.loads(content)
        break
    if request_doc is None:
        raise RuntimeError("submit payload is missing ray_tracing_request.json")
    volume = request_doc.get("volume")
    if not isinstance(volume, dict) or volume.get("enabled") is not True:
        raise RuntimeError("ray_tracing_request.json does not enable volume")
    source_path = str(volume.get("source_path") or "").strip()
    if not source_path:
        raise RuntimeError("enabled volume has empty source_path")
    if Path(source_path).is_absolute() or ".." in Path(source_path).parts:
        raise RuntimeError(f"volume source path is not payload-local: {source_path}")
    if source_path not in relpaths:
        raise RuntimeError(f"volume source path missing from payload_files: {source_path}")
    return {
        "schema": f"{SCHEMA}.staged_payload_audit",
        "payload_path": str(payload_path),
        "payload_file_count": len(relpaths),
        "ray_tracing_request_present": True,
        "runtime_scene_present": "scene_runtime.json" in relpaths,
        "volume_source_path": source_path,
        "volume_source_present": True,
        "volume_payload_files": sorted(path for path in relpaths if path.startswith("assets/vf3d/")),
    }


def variants(capsule_root: Path) -> list[dict[str, Any]]:
    manifest = read_json(capsule_root / "capsule_manifest.json")
    return manifest["first_slice_variants"]


def command_init(args: argparse.Namespace) -> int:
    init_capsule(args.capsule_root)
    print(f"initialized capsule: {args.capsule_root}")
    return 0


def command_generate_cache(args: argparse.Namespace) -> int:
    generate_cache(args.root, args.capsule_root, args.build_tools)
    print(f"generated cache: {args.capsule_root / 'cache' / 'volume_manifest.json'}")
    return 0


def command_build_requests(args: argparse.Namespace) -> int:
    for variant in variants(args.capsule_root):
        request_path = build_variant(args.capsule_root, variant["slug"], variant["profile"], variant.get("patch"))
        print(f"built request: {request_path}")
    return 0


def command_run_local_suite(args: argparse.Namespace) -> int:
    init_capsule(args.capsule_root)
    generate_cache(args.root, args.capsule_root, args.build_tools)
    if args.build_tools:
        make_tool(args.root, "ray_tracing", "ray-tracing-render-headless", "clang")
    for variant in variants(args.capsule_root):
        request_path = build_variant(args.capsule_root, variant["slug"], variant["profile"], variant.get("patch"))
        render_variant(args.root, request_path)
        print(f"rendered variant: {variant['slug']}")
    print(f"local scene capsule suite passed: {args.capsule_root}")
    return 0


def command_export_queue_item(args: argparse.Namespace) -> int:
    item_root = export_queue_item(args.capsule_root, args.slug, args.item_name, args.force)
    print(f"exported queue item: {item_root}")
    print(f"validate: python3 bin/vps_worker_job_queue.py --queue-root {queue_output_root(args.capsule_root)} validate --item-name {item_root.name}")
    return 0


def command_audit_staged_payload(args: argparse.Namespace) -> int:
    audit = audit_staged_submit_payload(args.payload)
    print(json.dumps(audit, indent=2, sort_keys=True))
    return 0


def build_parser() -> argparse.ArgumentParser:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=root)
    parser.add_argument("--capsule-root", type=Path, default=default_capsule_root(root))
    parser.add_argument("--build-tools", action="store_true", help="build local PhysicsSim/RayTracing tools before use")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("init").set_defaults(func=command_init)
    sub.add_parser("generate-cache").set_defaults(func=command_generate_cache)
    sub.add_parser("build-requests").set_defaults(func=command_build_requests)
    sub.add_parser("run-local-suite").set_defaults(func=command_run_local_suite)
    export = sub.add_parser("export-queue-item")
    export.add_argument("--slug", default="a_base_direct")
    export.add_argument("--item-name")
    export.add_argument("--force", action="store_true")
    export.set_defaults(func=command_export_queue_item)
    audit = sub.add_parser("audit-staged-payload")
    audit.add_argument("--payload", type=Path, required=True)
    audit.set_defaults(func=command_audit_staged_payload)
    return parser


def main(argv: list[str]) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    args.root = args.root.resolve()
    args.capsule_root = args.capsule_root.resolve()
    try:
        return int(args.func(args))
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
