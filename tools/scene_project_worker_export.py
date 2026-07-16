#!/usr/bin/env python3
"""Build deterministic RayTracing scene-project worker snapshots.

This module owns the ``scene-plus-physics-cache`` package policy.  The CLI
router remains in ``export_worker_queue_fixture.py`` so the original
``scene-only`` path can stay stable.
"""

from __future__ import annotations

import hashlib
import json
import shutil
from pathlib import Path
from typing import Any

from scene_project_contract import SceneProjectValidationError, validate_project


SCENE_PROJECT_SCHEMA = "codework_scene_project_v1"
RENDER_REQUEST_SCHEMA = "ray_tracing_agent_render_request_v1"
CACHE_MANIFEST_SCHEMA = "physics_sim_active_cache_manifest_v1"
PACKAGE_MODE = "scene-plus-physics-cache"
DEFAULT_SUBMISSION_PROFILE = "trio-headless-v1-mesh-sidecar"
PORTABLE_ONLY_PROJECT_RELPATHS = {
    "scene_project.json",
    "scene_authoring.json",
    "object_manifest.json",
    "physics_sim/active_cache_manifest.json",
    "ray_tracing/render_request.json",
}


class SceneProjectExportError(RuntimeError):
    pass


def read_json(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise SceneProjectExportError(f"failed to read JSON: {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise SceneProjectExportError(f"failed to parse JSON: {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise SceneProjectExportError(f"JSON root must be an object: {path}")
    return data


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def portable_relpath(value: Any, *, field: str) -> Path:
    if not isinstance(value, str) or not value.strip():
        raise SceneProjectExportError(f"{field} must be a non-empty project-relative path")
    path = Path(value)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise SceneProjectExportError(f"{field} must stay inside the scene project: {value}")
    return path


def resolve_project_file(project_root: Path, value: Any, *, field: str) -> Path:
    relpath = portable_relpath(value, field=field)
    candidate = (project_root / relpath).resolve()
    try:
        candidate.relative_to(project_root)
    except ValueError as exc:
        raise SceneProjectExportError(f"{field} escapes the scene project: {value}") from exc
    if not candidate.is_file():
        raise SceneProjectExportError(f"{field} file is missing: {candidate}")
    return candidate


def resolve_child_file(root: Path, value: Any, *, project_root: Path, field: str) -> Path:
    relpath = portable_relpath(value, field=field)
    candidate = (root / relpath).resolve()
    try:
        candidate.relative_to(project_root)
    except ValueError as exc:
        raise SceneProjectExportError(f"{field} escapes the scene project: {value}") from exc
    if not candidate.is_file():
        raise SceneProjectExportError(f"{field} file is missing: {candidate}")
    return candidate


def optional_active_path(manifest: dict[str, Any], key: str, fallback: str | None) -> str | None:
    active = manifest.get("active")
    value = active.get(key) if isinstance(active, dict) else None
    if value is None:
        value = manifest.get(key)
    if value is None:
        value = fallback
    return value if isinstance(value, str) and value.strip() else None


def active_render_request_path(manifest: dict[str, Any]) -> str:
    active = manifest.get("active")
    ray = manifest.get("ray_tracing")
    value = manifest.get("active_render_request")
    if value is None and isinstance(active, dict):
        value = active.get("render_request")
    if value is None and isinstance(ray, dict):
        value = ray.get("active_render_request")
    return value if isinstance(value, str) and value.strip() else "ray_tracing/render_request.json"


def active_cache_path(manifest: dict[str, Any]) -> str:
    active = manifest.get("active")
    physics = manifest.get("physics")
    value = manifest.get("active_cache")
    if value is None and isinstance(active, dict):
        value = active.get("physics_cache")
    if value is None and isinstance(physics, dict):
        value = physics.get("active_cache")
    return value if isinstance(value, str) and value.strip() else "physics_sim/active_cache_manifest.json"


def validate_scene_runtime(scene: dict[str, Any], path: Path) -> None:
    if scene.get("schema_family") != "codework_scene":
        raise SceneProjectExportError(f"scene runtime schema_family must be codework_scene: {path}")
    if scene.get("schema_variant") != "scene_runtime_v1":
        raise SceneProjectExportError(f"scene runtime schema_variant must be scene_runtime_v1: {path}")
    if not isinstance(scene.get("objects"), list):
        raise SceneProjectExportError(f"scene runtime objects must be an array: {path}")


def validate_render_request(request: dict[str, Any], path: Path) -> None:
    if request.get("schema_version") != RENDER_REQUEST_SCHEMA:
        raise SceneProjectExportError(f"render request schema_version is unsupported: {path}")
    if not isinstance(request.get("render"), dict):
        raise SceneProjectExportError(f"render request render object is required: {path}")


def mesh_asset_ids(scene: dict[str, Any]) -> list[str]:
    ids: list[str] = []
    seen: set[str] = set()
    for obj in scene.get("objects", []):
        if not isinstance(obj, dict) or obj.get("object_type") != "mesh_asset_instance":
            continue
        geometry_ref = obj.get("geometry_ref")
        if not isinstance(geometry_ref, dict) or geometry_ref.get("kind") != "mesh_asset":
            continue
        asset_id = str(geometry_ref.get("id") or "").strip()
        if asset_id and asset_id not in seen:
            ids.append(asset_id)
            seen.add(asset_id)
    return ids


def validate_mesh_asset_runtime(asset: dict[str, Any], path: Path) -> None:
    if asset.get("schema_family") != "codework_geometry":
        raise SceneProjectExportError(f"mesh asset schema_family must be codework_geometry: {path}")
    if asset.get("schema_variant") != "mesh_asset_runtime_v1":
        raise SceneProjectExportError(f"mesh asset schema_variant must be mesh_asset_runtime_v1: {path}")
    mesh = asset.get("mesh")
    if not isinstance(mesh, dict):
        raise SceneProjectExportError(f"mesh asset mesh object is required: {path}")
    vertices = mesh.get("vertices")
    triangles = mesh.get("triangles")
    if not isinstance(vertices, list) or not vertices:
        raise SceneProjectExportError(f"mesh asset vertices must be a non-empty array: {path}")
    if not isinstance(triangles, list) or not triangles:
        raise SceneProjectExportError(f"mesh asset triangles must be a non-empty array: {path}")
    if mesh.get("vertex_count") != len(vertices) or mesh.get("triangle_count") != len(triangles):
        raise SceneProjectExportError(f"mesh asset counts must match geometry arrays: {path}")


def rewrite_scene_runtime_mesh_paths(scene: dict[str, Any]) -> dict[str, Any]:
    rewritten = json.loads(json.dumps(scene))
    for obj in rewritten.get("objects", []):
        if not isinstance(obj, dict) or obj.get("object_type") != "mesh_asset_instance":
            continue
        geometry_ref = obj.get("geometry_ref")
        if not isinstance(geometry_ref, dict) or geometry_ref.get("kind") != "mesh_asset":
            continue
        asset_id = str(geometry_ref.get("id") or "").strip()
        if not asset_id:
            continue
        extensions = obj.setdefault("extensions", {})
        if not isinstance(extensions, dict):
            extensions = {}
            obj["extensions"] = extensions
        line_drawing = extensions.setdefault("line_drawing", {})
        if not isinstance(line_drawing, dict):
            line_drawing = {}
            extensions["line_drawing"] = line_drawing
        line_drawing["runtime_mesh_path"] = f"assets/mesh_assets/{asset_id}.runtime.json"
    return rewritten


def simulation_window(request: dict[str, Any]) -> tuple[int, int, int, list[int]]:
    simulation = request.get("simulation_frames")
    render = request.get("render")
    if not isinstance(simulation, dict):
        simulation = {}
    if not isinstance(render, dict):
        render = {}
    try:
        start = int(simulation.get("start", render.get("start_frame", 0)))
        count = int(simulation.get("count", render.get("frame_count", 1)))
        stride = int(simulation.get("stride", 1))
    except (TypeError, ValueError) as exc:
        raise SceneProjectExportError("simulation frame start/count/stride must be integers") from exc
    if start < 0 or count <= 0 or stride <= 0:
        raise SceneProjectExportError("simulation frame window requires start>=0, count>0, stride>0")
    indices = [start + offset * stride for offset in range(count)]
    return start, count, stride, indices


def selected_frame_manifest(
    source: dict[str, Any],
    indices: list[int],
    *,
    label: str,
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    frames = source.get("frames")
    if not isinstance(frames, list):
        raise SceneProjectExportError(f"{label}.frames must be an array")
    by_index: dict[int, dict[str, Any]] = {}
    for frame in frames:
        if not isinstance(frame, dict):
            continue
        try:
            index = int(frame.get("frame_index"))
        except (TypeError, ValueError):
            continue
        by_index[index] = frame
    missing = [index for index in indices if index not in by_index]
    if missing:
        raise SceneProjectExportError(f"{label} is missing requested frame indices: {missing}")
    selected = [json.loads(json.dumps(by_index[index])) for index in indices]
    rewritten = json.loads(json.dumps(source))
    rewritten["frames"] = selected
    if "run_name" in rewritten:
        rewritten["run_name"] = "."
    return rewritten, selected


def render_value(request: dict[str, Any], key: str, default: Any) -> Any:
    render = request.get("render")
    return render.get(key, default) if isinstance(render, dict) else default


def int_render_value(request: dict[str, Any], key: str, default: int, *, minimum: int) -> int:
    try:
        value = int(render_value(request, key, default))
    except (TypeError, ValueError) as exc:
        raise SceneProjectExportError(f"render.{key} must be an integer") from exc
    if value < minimum:
        raise SceneProjectExportError(f"render.{key} must be >= {minimum}")
    return value


def float_render_value(request: dict[str, Any], key: str, default: float) -> float:
    try:
        return float(render_value(request, key, default))
    except (TypeError, ValueError) as exc:
        raise SceneProjectExportError(f"render.{key} must be numeric") from exc


def resource_budget(request: dict[str, Any]) -> dict[str, int]:
    defaults = {"cpu_percent": 25, "max_workers": 1, "reserve_cpu_count": 1}
    resources = request.get("resources")
    if not isinstance(resources, dict):
        return defaults
    result = dict(defaults)
    for key in result:
        try:
            result[key] = int(resources.get(key, result[key]))
        except (TypeError, ValueError) as exc:
            raise SceneProjectExportError(f"resources.{key} must be an integer") from exc
    if not 1 <= result["cpu_percent"] <= 100:
        raise SceneProjectExportError("resources.cpu_percent must be 1..100")
    if result["max_workers"] < 0 or result["reserve_cpu_count"] < 0:
        raise SceneProjectExportError("resource worker counts must be >= 0")
    return result


def video_fps(request: dict[str, Any]) -> int:
    output = request.get("output")
    video = output.get("video") if isinstance(output, dict) else None
    try:
        value = int(video.get("fps", 30)) if isinstance(video, dict) else 30
    except (TypeError, ValueError):
        value = 30
    return value if value > 0 else 30


def rewrite_project_manifest(source: dict[str, Any], *, has_authoring: bool, has_object_manifest: bool) -> dict[str, Any]:
    manifest = json.loads(json.dumps(source))
    active = manifest.setdefault("active", {})
    if not isinstance(active, dict):
        active = {}
        manifest["active"] = active
    active["scene_runtime"] = "scene_runtime.json"
    if has_authoring:
        active["scene_authoring"] = "scene_authoring.json"
    if has_object_manifest:
        active["object_manifest"] = "object_manifest.json"
    active["physics_cache"] = "physics_sim/active_cache_manifest.json"
    active["render_request"] = "ray_tracing/render_request.json"
    return manifest


def rewrite_cache_manifest(source: dict[str, Any], indices: list[int], start: int, count: int, stride: int) -> dict[str, Any]:
    manifest = json.loads(json.dumps(source))
    manifest["project_root"] = "."
    manifest["runtime_scene"] = "scene_runtime.json"
    manifest["vf3d_active_dir"] = "assets/vf3d/active"
    manifest["physics_active_dir"] = "assets/physics/active"
    manifest["scene_bundle"] = "assets/physics/active/scene_bundle.json"
    manifest["frame_count"] = count
    manifest["retained_frame_indices"] = indices
    manifest["export_start_frame"] = start
    manifest["export_stride"] = stride
    manifest["export_max_frames"] = count
    return manifest


def rewrite_project_render_request(source: dict[str, Any], item_name: str, job_id: str) -> dict[str, Any]:
    request = json.loads(json.dumps(source))
    request["run_id"] = job_id
    request.setdefault("scene", {})["runtime_scene_path"] = "../scene_runtime.json"
    request["volume"] = {
        "enabled": True,
        "source_kind": "scene_bundle",
        "source_path": "../assets/physics/active/scene_bundle.json",
        "visible": True,
        "affects_lighting": True,
        "debug_overlay": False,
    }
    request.setdefault("metadata", {})["portable_export_item"] = item_name
    request["metadata"]["portable_export_mode"] = PACKAGE_MODE
    return request


def rewrite_worker_render_request(source: dict[str, Any], item_name: str, job_id: str) -> dict[str, Any]:
    request = rewrite_project_render_request(source, item_name, job_id)
    request["scene"]["runtime_scene_path"] = "scene_runtime.json"
    request["volume"]["source_path"] = "assets/physics/active/scene_bundle.json"
    request.setdefault("output", {})["root"] = "output"
    request["output"]["overwrite"] = True
    request.setdefault("progress", {})["summary_path"] = "output/render_summary.json"
    request["progress"]["progress_path"] = "output/render_progress.json"
    return request


def build_job_json(job_id: str, ray_version: str, physics_version: str, support_relpaths: list[str]) -> dict[str, Any]:
    return {
        "schema_version": "codework_worker_job_v1",
        "job_id": job_id,
        "workflow": "line_author->physics_sim->ray_tracing",
        "submission_mode": "private-api",
        "start_stage": "ray_tracing",
        "worker_package_set": {
            "physics_sim": {
                "worker_slug": "physics_sim_headless_worker",
                "version": physics_version,
                "platform": "linux-x86_64",
            },
            "ray_tracing": {
                "worker_slug": "ray_tracing_headless_worker",
                "version": ray_version,
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
    }


def build_queue_config(
    item_name: str,
    job_id: str,
    ray_version: str,
    request: dict[str, Any],
    inspection_path: Path,
    force_worker_id: str | None = None,
    execution_profile: str = DEFAULT_SUBMISSION_PROFILE,
) -> dict[str, Any]:
    frame_count = int_render_value(request, "frame_count", 1, minimum=1)
    start_frame = int_render_value(request, "start_frame", 0, minimum=0)
    width = int_render_value(request, "width", 160, minimum=1)
    height = int_render_value(request, "height", 96, minimum=1)
    temporal_frames = int_render_value(request, "temporal_frames", 1, minimum=1)
    normalized_t = float_render_value(request, "normalized_t", 0.0)
    resources = resource_budget(request)
    submit_args = [
        "--job-id", job_id,
        "--job-id-policy", "validate",
        "--execution-profile", execution_profile,
        "--profile", "preview",
        "--ray-version", ray_version,
        "--start-stage", "ray_tracing",
        "--ray-frame-count", str(frame_count),
        "--ray-start-frame", str(start_frame),
        "--ray-sampling-frame-offset", str(start_frame),
        "--ray-sampling-frame-count", str(max(start_frame + frame_count, frame_count)),
        "--ray-width", str(width),
        "--ray-height", str(height),
        "--ray-temporal-frames", str(temporal_frames),
        "--ray-normalized-t", str(normalized_t),
        "--ray-integrator", str(render_value(request, "integrator_3d", "direct_light")),
        "--ray-video-fps", str(video_fps(request)),
        "--ray-resource-cpu-percent", str(resources["cpu_percent"]),
        "--ray-resource-max-workers", str(resources["max_workers"]),
        "--ray-resource-reserve-cpu-count", str(resources["reserve_cpu_count"]),
        "--ray-inspection-file", str(inspection_path.resolve()),
        "--ray-volume-source-kind", "scene_bundle",
        "--ray-volume-source-path", "assets/physics/active/scene_bundle.json",
        "--ray-volume-visible", "true",
        "--ray-volume-affects-lighting", "true",
    ]
    if force_worker_id:
        submit_args.extend(["--force-worker-id", force_worker_id])
    return {
        "schema": "codework-worker-desktop-queue-item/v1",
        "item_id": item_name,
        "bundle_dir": "bundle",
        "overwrite_staging": True,
        "submit_args": submit_args,
    }


def copy_file(src: Path, dst: Path, source_map: dict[str, Path]) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    source_map[dst.resolve().as_posix()] = src


def write_derived_json(path: Path, data: dict[str, Any], source: Path, source_map: dict[str, Path]) -> None:
    write_json(path, data)
    source_map[path.resolve().as_posix()] = source


def copy_payload_file(bundle_root: Path, project_relpath: Path, source_path: Path, source_map: dict[str, Path]) -> str:
    payload_relpath = project_relpath.as_posix()
    dst = bundle_root / "request" / "payload" / payload_relpath
    copy_file(source_path, dst, source_map)
    return payload_relpath


def include_in_worker_runtime_payload(project_relpath: Path) -> bool:
    """Keep portable control documents local while sending render inputs inline."""
    return project_relpath.as_posix() not in PORTABLE_ONLY_PROJECT_RELPATHS


def role_for_path(path: Path, bundle_root: Path) -> str:
    if path.name == "worker_job_queue.json":
        return "queue_config"
    try:
        relpath = path.resolve().relative_to(bundle_root.resolve()).as_posix()
    except ValueError:
        return "queue_item"
    if relpath == "scene_project.json":
        return "portable_scene_project"
    if relpath == "scene_runtime.json":
        return "portable_scene_runtime"
    if relpath == "scene_authoring.json":
        return "portable_scene_authoring"
    if relpath == "object_manifest.json":
        return "portable_object_manifest"
    if relpath in {"render_request.json", "ray_tracing/render_request.json"}:
        return "portable_render_request"
    if relpath == "physics_sim/active_cache_manifest.json":
        return "physics_cache_manifest"
    if relpath.startswith("assets/mesh_assets/"):
        return "portable_mesh_asset"
    if relpath.startswith("assets/vf3d/"):
        return "physics_vf3d_attachment"
    if relpath.startswith("assets/physics/"):
        return "physics_sidecar_attachment"
    if relpath.startswith("lineage/"):
        return "source_lineage"
    if relpath.startswith("request/payload/"):
        return "queue_payload"
    if relpath == "request/job.json":
        return "queue_worker_job"
    if relpath.startswith("presets/"):
        return "preset"
    if relpath.startswith("review/"):
        return "review_note"
    return "portable_bundle_file"


def add_manifest_file(files: list[dict[str, Any]], item_root: Path, path: Path, role: str, source: Path | None) -> None:
    files.append(
        {
            "relpath": path.resolve().relative_to(item_root.resolve()).as_posix(),
            "role": role,
            "bytes": path.stat().st_size,
            "sha256": sha256_file(path),
            "source": str(source.resolve()) if source else None,
        }
    )


def export_scene_plus_physics_cache(
    *,
    project_root: Path,
    output_root: Path,
    item_name: str,
    job_id: str,
    ray_version: str,
    physics_version: str,
    force_worker_id: str | None = None,
    execution_profile: str = DEFAULT_SUBMISSION_PROFILE,
    force: bool = False,
) -> dict[str, Any]:
    project_root = project_root.resolve()
    output_root = output_root.resolve()
    if not isinstance(execution_profile, str) or not execution_profile.strip():
        raise SceneProjectExportError("execution profile must be a non-empty string")
    try:
        project_validation = validate_project(project_root)
    except SceneProjectValidationError as exc:
        raise SceneProjectExportError(str(exc)) from exc
    if not project_root.is_dir():
        raise SceneProjectExportError(f"scene project root is not a directory: {project_root}")
    project_manifest_path = project_root / "scene_project.json"
    project_manifest = read_json(project_manifest_path)
    if project_manifest.get("schema") != SCENE_PROJECT_SCHEMA:
        raise SceneProjectExportError(f"unsupported scene project schema: {project_manifest_path}")

    runtime_relpath = optional_active_path(project_manifest, "scene_runtime", "scene_runtime.json")
    authoring_relpath = optional_active_path(project_manifest, "scene_authoring", None)
    object_manifest_relpath = optional_active_path(project_manifest, "object_manifest", None)
    render_request_relpath = active_render_request_path(project_manifest)
    cache_manifest_relpath = active_cache_path(project_manifest)
    runtime_path = resolve_project_file(project_root, runtime_relpath, field="active.scene_runtime")
    authoring_path = resolve_project_file(project_root, authoring_relpath, field="active.scene_authoring") if authoring_relpath else None
    object_manifest_path = resolve_project_file(project_root, object_manifest_relpath, field="active.object_manifest") if object_manifest_relpath else None
    render_request_path = resolve_project_file(project_root, render_request_relpath, field="active.render_request")
    cache_manifest_path = resolve_project_file(project_root, cache_manifest_relpath, field="active.physics_cache")

    scene = read_json(runtime_path)
    validate_scene_runtime(scene, runtime_path)
    source_request = read_json(render_request_path)
    validate_render_request(source_request, render_request_path)
    cache_manifest = read_json(cache_manifest_path)
    if cache_manifest.get("schema") != CACHE_MANIFEST_SCHEMA:
        raise SceneProjectExportError(f"unsupported active cache manifest schema: {cache_manifest_path}")
    start, count, stride, selected_indices = simulation_window(source_request)

    vf3d_dir_relpath = portable_relpath(cache_manifest.get("vf3d_active_dir"), field="cache.vf3d_active_dir")
    physics_dir_relpath = portable_relpath(cache_manifest.get("physics_active_dir"), field="cache.physics_active_dir")
    vf3d_dir = (project_root / vf3d_dir_relpath).resolve()
    physics_dir = (project_root / physics_dir_relpath).resolve()
    for label, path in (("vf3d active directory", vf3d_dir), ("physics active directory", physics_dir)):
        try:
            path.relative_to(project_root)
        except ValueError as exc:
            raise SceneProjectExportError(f"{label} escapes the scene project: {path}") from exc
        if not path.is_dir():
            raise SceneProjectExportError(f"{label} is missing: {path}")

    vf3d_manifest_path = resolve_child_file(vf3d_dir, "manifest.json", project_root=project_root, field="vf3d manifest")
    vf3d_manifest, vf3d_frames = selected_frame_manifest(
        read_json(vf3d_manifest_path), selected_indices, label="VF3D manifest"
    )
    scene_bundle_path = resolve_project_file(project_root, cache_manifest.get("scene_bundle"), field="cache.scene_bundle")
    scene_bundle = read_json(scene_bundle_path)
    fluid_source = scene_bundle.get("fluid_source")
    if not isinstance(fluid_source, dict):
        raise SceneProjectExportError(f"scene bundle fluid_source is required: {scene_bundle_path}")
    scene_bundle = json.loads(json.dumps(scene_bundle))
    scene_bundle["fluid_source"]["kind"] = "manifest"
    scene_bundle["fluid_source"]["path"] = "../../vf3d/active/manifest.json"

    water_manifest_path: Path | None = None
    water_manifest: dict[str, Any] | None = None
    water_frames: list[dict[str, Any]] = []
    water_source = scene_bundle.get("water_source")
    if isinstance(water_source, dict) and water_source.get("path"):
        water_manifest_path = resolve_child_file(
            scene_bundle_path.parent,
            water_source.get("path"),
            project_root=project_root,
            field="scene_bundle.water_source.path",
        )
        water_manifest, water_frames = selected_frame_manifest(
            read_json(water_manifest_path), selected_indices, label="water manifest"
        )
        scene_bundle["water_source"]["path"] = "water_manifest_v1.json"

    asset_sources: list[tuple[str, Path]] = []
    asset_ids = mesh_asset_ids(scene)
    for asset_id in asset_ids:
        asset_path = resolve_project_file(
            project_root,
            f"assets/mesh_assets/{asset_id}.runtime.json",
            field=f"mesh asset {asset_id}",
        )
        validate_mesh_asset_runtime(read_json(asset_path), asset_path)
        asset_sources.append((asset_id, asset_path))
    vf3d_sources: list[tuple[Path, Path, Path | None]] = []
    for frame in vf3d_frames:
        source = resolve_child_file(
            vf3d_dir,
            frame.get("path"),
            project_root=project_root,
            field="VF3D frame path",
        )
        frame_relpath = Path("assets/vf3d/active") / portable_relpath(
            frame.get("path"), field="VF3D frame path"
        )
        pack_source = source.with_suffix(".pack")
        vf3d_sources.append((frame_relpath, source, pack_source if pack_source.is_file() else None))
    water_sources: list[tuple[Path, Path]] = []
    if water_manifest_path:
        for frame in water_frames:
            source = resolve_child_file(
                water_manifest_path.parent,
                frame.get("path"),
                project_root=project_root,
                field="water frame path",
            )
            frame_relpath = Path("assets/physics/active") / portable_relpath(
                frame.get("path"), field="water frame path"
            )
            water_sources.append((frame_relpath, source))

    item_root = output_root / "inbox" / item_name
    bundle_root = item_root / "bundle"
    if item_root.exists():
        if not force:
            raise SceneProjectExportError(f"output item already exists: {item_root} (use --force)")
        shutil.rmtree(item_root)
    source_map: dict[str, Path] = {}

    snapshot_project = rewrite_project_manifest(
        project_manifest,
        has_authoring=authoring_path is not None,
        has_object_manifest=object_manifest_path is not None,
    )
    snapshot_cache = rewrite_cache_manifest(cache_manifest, selected_indices, start, count, stride)
    snapshot_scene = rewrite_scene_runtime_mesh_paths(scene)
    project_request = rewrite_project_render_request(source_request, item_name, job_id)
    worker_request = rewrite_worker_render_request(source_request, item_name, job_id)
    write_derived_json(bundle_root / "scene_project.json", snapshot_project, project_manifest_path, source_map)
    write_derived_json(bundle_root / "scene_runtime.json", snapshot_scene, runtime_path, source_map)
    if authoring_path:
        copy_file(authoring_path, bundle_root / "scene_authoring.json", source_map)
    if object_manifest_path:
        copy_file(object_manifest_path, bundle_root / "object_manifest.json", source_map)
    write_derived_json(bundle_root / "ray_tracing" / "render_request.json", project_request, render_request_path, source_map)
    write_derived_json(bundle_root / "render_request.json", worker_request, render_request_path, source_map)
    write_derived_json(bundle_root / "physics_sim" / "active_cache_manifest.json", snapshot_cache, cache_manifest_path, source_map)
    write_derived_json(bundle_root / "assets" / "vf3d" / "active" / "manifest.json", vf3d_manifest, vf3d_manifest_path, source_map)
    write_derived_json(bundle_root / "assets" / "physics" / "active" / "scene_bundle.json", scene_bundle, scene_bundle_path, source_map)

    copied_project_relpaths: list[Path] = [
        Path("scene_project.json"),
        Path("scene_runtime.json"),
        Path("ray_tracing/render_request.json"),
        Path("physics_sim/active_cache_manifest.json"),
        Path("assets/vf3d/active/manifest.json"),
        Path("assets/physics/active/scene_bundle.json"),
    ]
    if authoring_path:
        copied_project_relpaths.append(Path("scene_authoring.json"))
    if object_manifest_path:
        copied_project_relpaths.append(Path("object_manifest.json"))

    for asset_id, source in asset_sources:
        destination_relpath = Path("assets/mesh_assets") / source.name
        copy_file(source, bundle_root / destination_relpath, source_map)
        copied_project_relpaths.append(destination_relpath)

    for frame_relpath, source, pack_source in vf3d_sources:
        copy_file(source, bundle_root / frame_relpath, source_map)
        copied_project_relpaths.append(frame_relpath)
        if pack_source:
            pack_relpath = frame_relpath.with_suffix(".pack")
            copy_file(pack_source, bundle_root / pack_relpath, source_map)
            copied_project_relpaths.append(pack_relpath)

    if water_manifest_path and water_manifest is not None:
        water_manifest_dest = Path("assets/physics/active/water_manifest_v1.json")
        write_derived_json(bundle_root / water_manifest_dest, water_manifest, water_manifest_path, source_map)
        copied_project_relpaths.append(water_manifest_dest)
        for frame_relpath, source in water_sources:
            copy_file(source, bundle_root / frame_relpath, source_map)
            copied_project_relpaths.append(frame_relpath)

    support_relpaths: list[str] = []
    payload_prefixes = {
        "scene_project.json": Path("line_drawing/scene_project.json"),
        "scene_runtime.json": Path("line_drawing/scene_runtime.json"),
        "scene_authoring.json": Path("line_drawing/scene_authoring.json"),
        "object_manifest.json": Path("line_drawing/object_manifest.json"),
    }
    for project_relpath in copied_project_relpaths:
        payload_relpath = payload_prefixes.get(project_relpath.as_posix())
        if payload_relpath is None:
            if project_relpath.parts[:2] == ("assets", "mesh_assets"):
                payload_relpath = Path("line_drawing") / project_relpath
            elif project_relpath.parts[0] == "assets":
                payload_relpath = project_relpath
            elif project_relpath.parts[0] == "physics_sim":
                payload_relpath = project_relpath
            elif project_relpath.parts[0] == "ray_tracing":
                payload_relpath = project_relpath
            else:
                payload_relpath = Path("line_drawing") / project_relpath
        source_path = bundle_root / project_relpath
        payload_path = bundle_root / "request" / "payload" / payload_relpath
        if project_relpath.as_posix() == "physics_sim/active_cache_manifest.json":
            worker_cache = json.loads(json.dumps(snapshot_cache))
            worker_cache["vf3d_active_dir"] = "assets/vf3d/active"
            worker_cache["physics_active_dir"] = "assets/physics/active"
            worker_cache["scene_bundle"] = "assets/physics/active/scene_bundle.json"
            write_derived_json(payload_path, worker_cache, cache_manifest_path, source_map)
        else:
            copy_file(source_path, payload_path, source_map)
        if (
            payload_relpath.as_posix() != "line_drawing/scene_runtime.json"
            and include_in_worker_runtime_payload(project_relpath)
        ):
            support_relpaths.append(payload_relpath.as_posix())

    inspection = source_request.get("inspection")
    inspection_settings = json.loads(json.dumps(inspection)) if isinstance(inspection, dict) else {}
    write_json(bundle_root / "presets" / "inspection_settings.json", inspection_settings)
    write_json(
        bundle_root / "presets" / "renderer_preset.json",
        {
            "schema": "ray_tracing_renderer_preset/v1",
            "mode": PACKAGE_MODE,
            "integrator_3d": render_value(worker_request, "integrator_3d", "direct_light"),
            "temporal_frames": render_value(worker_request, "temporal_frames", 1),
            "volume_source": "assets/physics/active/scene_bundle.json",
        },
    )
    write_text(
        bundle_root / "review" / "notes.md",
        "\n".join(
            [
                "# Scene Project Worker Snapshot",
                "",
                f"- mode: `{PACKAGE_MODE}`",
                f"- item: `{item_name}`",
                f"- job: `{job_id}`",
                f"- selected simulation frames: `{selected_indices}`",
                "- source cache was snapshotted; PhysicsSim is not re-run by this package",
                "",
            ]
        ),
    )
    lineage = {
        "schema": "ray_tracing_scene_project_lineage/v1",
        "mode": PACKAGE_MODE,
        "source_project_root": str(project_root),
        "source_scene_project": str(project_manifest_path),
        "source_render_request": str(render_request_path),
        "source_cache_manifest": str(cache_manifest_path),
        "source_active_run_id": cache_manifest.get("active_run_id"),
        "selected_frame_indices": selected_indices,
        "selection": {"start": start, "count": count, "stride": stride},
    }
    write_json(bundle_root / "lineage" / "source_lineage.json", lineage)
    write_json(bundle_root / "request" / "job.json", build_job_json(job_id, ray_version, physics_version, sorted(support_relpaths)))
    queue_config = build_queue_config(
        item_name,
        job_id,
        ray_version,
        worker_request,
        bundle_root / "presets" / "inspection_settings.json",
        force_worker_id,
        execution_profile,
    )
    write_json(item_root / "worker_job_queue.json", queue_config)

    manifest_files: list[dict[str, Any]] = []
    for path in sorted(item_root.rglob("*")):
        if path.is_file() and path.resolve() != (bundle_root / "manifest.json").resolve():
            add_manifest_file(
                manifest_files,
                item_root,
                path,
                role_for_path(path, bundle_root),
                source_map.get(path.resolve().as_posix()),
            )
    manifest = {
        "schema": "ray_tracing_portable_worker_export_manifest/v1",
        "mode": PACKAGE_MODE,
        "item_name": item_name,
        "job_id": job_id,
        "queue_root": str(output_root),
        "item_root": str(item_root),
        "source_project_root": str(project_root),
        "source_scene_runtime": str(runtime_path),
        "source_scene_authoring": str(authoring_path) if authoring_path else None,
        "source_object_manifest": str(object_manifest_path) if object_manifest_path else None,
        "source_render_request": str(render_request_path),
        "source_cache_manifest": str(cache_manifest_path),
        "source_project_content_sha256": project_validation["content"]["sha256"],
        "source_project_content_file_count": project_validation["content"]["file_count"],
        "mesh_asset_ids": asset_ids,
        "selected_frame_indices": selected_indices,
        "files": manifest_files,
    }
    write_json(bundle_root / "manifest.json", manifest)
    return {
        "status": "ok",
        "mode": PACKAGE_MODE,
        "queue_root": str(output_root),
        "item_name": item_name,
        "item_root": str(item_root),
        "bundle_root": str(bundle_root),
        "manifest_path": str(bundle_root / "manifest.json"),
        "worker_job_queue_path": str(item_root / "worker_job_queue.json"),
        "mesh_asset_count": len(asset_ids),
        "selected_frame_indices": selected_indices,
        "physics_attachment_count": sum(
            1 for path in (bundle_root / "assets").rglob("*") if path.is_file()
        ),
    }
