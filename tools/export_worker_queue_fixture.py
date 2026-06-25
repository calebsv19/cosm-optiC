#!/usr/bin/env python3
"""Export a deterministic RayTracing worker-queue item.

The first supported package mode is scene-only: runtime scene plus mesh sidecars,
with no VF3D or PhysicsSim volume attachment. The output is a local queue root
with one inbox item so it can be validated by bin/vps_worker_job_queue.py.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path
from typing import Any


SCRIPT_PATH = Path(__file__).resolve()
RAY_ROOT = SCRIPT_PATH.parents[1]
WORKSPACE_ROOT = RAY_ROOT.parent
DEFAULT_ITEM_NAME = "ray-tracing-portable-fixture-20260624a"
DEFAULT_JOB_ID = "ray-tracing--portable-fixture--20260624T000000Z--rtbundle01"
DEFAULT_RAY_VERSION = "0.4.12"
DEFAULT_PHYSICS_VERSION = "0.1.0"


class ExportError(RuntimeError):
    pass


def read_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as handle:
            data = json.load(handle)
    except OSError as exc:
        raise ExportError(f"failed to read JSON: {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise ExportError(f"failed to parse JSON: {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise ExportError(f"JSON root must be an object: {path}")
    return data


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2)
        handle.write("\n")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def copy_file(src: Path, dst: Path) -> None:
    if not src.is_file():
        raise ExportError(f"required source file missing: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def relpath(path: Path, root: Path) -> str:
    return path.resolve().relative_to(root.resolve()).as_posix()


def fixture_scene_runtime() -> Path:
    return RAY_ROOT / "tests" / "fixtures" / "mesh_asset_runtime_spheres" / "scene_runtime.json"


def fixture_render_request() -> Path:
    return RAY_ROOT / "tests" / "fixtures" / "agent_render_mesh_asset_spheres_request.json"


def default_output_root() -> Path:
    return RAY_ROOT / "visual_artifacts" / "worker_queue_exports"


def mesh_asset_ids(scene: dict[str, Any]) -> list[str]:
    ids: list[str] = []
    seen: set[str] = set()
    for obj in scene.get("objects", []):
        if not isinstance(obj, dict):
            continue
        if obj.get("object_type") != "mesh_asset_instance":
            continue
        geometry_ref = obj.get("geometry_ref")
        if not isinstance(geometry_ref, dict):
            continue
        if geometry_ref.get("kind") != "mesh_asset":
            continue
        asset_id = str(geometry_ref.get("id") or "").strip()
        if asset_id and asset_id not in seen:
            seen.add(asset_id)
            ids.append(asset_id)
    return ids


def validate_scene_runtime(scene: dict[str, Any], path: Path) -> None:
    if scene.get("schema_family") != "codework_scene":
        raise ExportError(f"scene runtime schema_family must be codework_scene: {path}")
    if scene.get("schema_variant") != "scene_runtime_v1":
        raise ExportError(f"scene runtime schema_variant must be scene_runtime_v1: {path}")
    if not isinstance(scene.get("objects"), list):
        raise ExportError(f"scene runtime objects must be an array: {path}")


def validate_render_request(request: dict[str, Any], path: Path) -> None:
    if request.get("schema_version") != "ray_tracing_agent_render_request_v1":
        raise ExportError(f"render request schema_version is unsupported: {path}")
    scene = request.get("scene")
    if not isinstance(scene, dict) or not isinstance(scene.get("runtime_scene_path"), str):
        raise ExportError(f"render request scene.runtime_scene_path is required: {path}")
    render = request.get("render")
    if not isinstance(render, dict):
        raise ExportError(f"render request render object is required: {path}")


def source_mesh_asset_root(scene_runtime_path: Path, explicit_root: Path | None) -> Path:
    if explicit_root:
        return explicit_root
    candidate = scene_runtime_path.parent / "assets" / "mesh_assets"
    if candidate.is_dir():
        return candidate
    return scene_runtime_path.parent / "mesh_assets"


def rewrite_render_request(
    source_request: dict[str, Any],
    *,
    item_name: str,
    job_id: str,
) -> dict[str, Any]:
    request = json.loads(json.dumps(source_request))
    request["run_id"] = job_id
    request.setdefault("scene", {})
    request["scene"]["runtime_scene_path"] = "scene_runtime.json"
    request.setdefault("volume", {})
    request["volume"]["enabled"] = False
    request.setdefault("output", {})
    request["output"]["root"] = "output"
    request["output"]["overwrite"] = True
    request.setdefault("progress", {})
    request["progress"]["summary_path"] = "output/render_summary.json"
    request["progress"]["progress_path"] = "output/render_progress.json"
    request.setdefault("metadata", {})
    request["metadata"]["portable_export_item"] = item_name
    request["metadata"]["portable_export_mode"] = "scene-only"
    return request


def inspection_settings_from_request(request: dict[str, Any]) -> dict[str, Any]:
    inspection = request.get("inspection")
    if isinstance(inspection, dict):
        return json.loads(json.dumps(inspection))
    return {}


def render_value(request: dict[str, Any], key: str, default: Any) -> Any:
    render = request.get("render")
    if isinstance(render, dict) and key in render:
        return render[key]
    return default


def int_render_value(request: dict[str, Any], key: str, default: int, *, minimum: int = 0) -> int:
    try:
        value = int(render_value(request, key, default))
    except (TypeError, ValueError) as exc:
        raise ExportError(f"render.{key} must be an integer") from exc
    if value < minimum:
        raise ExportError(f"render.{key} must be >= {minimum}")
    return value


def float_render_value(request: dict[str, Any], key: str, default: float) -> float:
    try:
        return float(render_value(request, key, default))
    except (TypeError, ValueError) as exc:
        raise ExportError(f"render.{key} must be numeric") from exc


def sampling_window(request: dict[str, Any], start_frame: int, frame_count: int) -> tuple[int, int]:
    sampling = request.get("sampling")
    if isinstance(sampling, dict):
        try:
            offset = int(sampling.get("frame_offset"))
            count = int(sampling.get("frame_count"))
        except (TypeError, ValueError) as exc:
            raise ExportError("sampling.frame_offset and sampling.frame_count must be integers") from exc
        if offset < 0 or count <= 0 or offset + frame_count > count:
            raise ExportError("sampling window is invalid for requested frame count")
        return offset, count
    return start_frame, max(start_frame + frame_count, frame_count)


def resource_budget(request: dict[str, Any]) -> dict[str, int]:
    resources = request.get("resources")
    defaults = {"cpu_percent": 25, "max_workers": 1, "reserve_cpu_count": 1}
    if not isinstance(resources, dict):
        return defaults
    out = dict(defaults)
    for key in out:
        if key in resources:
            try:
                out[key] = int(resources[key])
            except (TypeError, ValueError) as exc:
                raise ExportError(f"resources.{key} must be an integer") from exc
    if not 1 <= out["cpu_percent"] <= 100:
        raise ExportError("resources.cpu_percent must be 1..100")
    if out["max_workers"] < 0 or out["reserve_cpu_count"] < 0:
        raise ExportError("resources.max_workers and reserve_cpu_count must be >= 0")
    return out


def video_fps(request: dict[str, Any], default: int = 30) -> int:
    output = request.get("output")
    if isinstance(output, dict):
        video = output.get("video")
        if isinstance(video, dict):
            try:
                fps = int(video.get("fps"))
                return fps if fps > 0 else default
            except (TypeError, ValueError):
                return default
    return default


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
    *,
    item_name: str,
    job_id: str,
    ray_version: str,
    render_request: dict[str, Any],
    inspection_path: Path,
) -> dict[str, Any]:
    frame_count = int_render_value(render_request, "frame_count", 1, minimum=1)
    start_frame = int_render_value(render_request, "start_frame", 0, minimum=0)
    width = int_render_value(render_request, "width", 160, minimum=1)
    height = int_render_value(render_request, "height", 96, minimum=1)
    temporal_frames = int_render_value(render_request, "temporal_frames", 1, minimum=1)
    normalized_t = float_render_value(render_request, "normalized_t", 0.0)
    integrator = str(render_value(render_request, "integrator_3d", "direct_light"))
    sampling_frame_offset, sampling_frame_count = sampling_window(render_request, start_frame, frame_count)
    resources = resource_budget(render_request)
    return {
        "schema": "codework-worker-desktop-queue-item/v1",
        "item_id": item_name,
        "bundle_dir": "bundle",
        "overwrite_staging": True,
        "submit_args": [
            "--job-id",
            job_id,
            "--job-id-policy",
            "validate",
            "--profile",
            "preview",
            "--ray-version",
            ray_version,
            "--start-stage",
            "ray_tracing",
            "--ray-disable-volume",
            "--ray-frame-count",
            str(frame_count),
            "--ray-start-frame",
            str(start_frame),
            "--ray-sampling-frame-offset",
            str(sampling_frame_offset),
            "--ray-sampling-frame-count",
            str(sampling_frame_count),
            "--ray-width",
            str(width),
            "--ray-height",
            str(height),
            "--ray-temporal-frames",
            str(temporal_frames),
            "--ray-normalized-t",
            str(normalized_t),
            "--ray-integrator",
            integrator,
            "--ray-video-fps",
            str(video_fps(render_request)),
            "--ray-resource-cpu-percent",
            str(resources["cpu_percent"]),
            "--ray-resource-max-workers",
            str(resources["max_workers"]),
            "--ray-resource-reserve-cpu-count",
            str(resources["reserve_cpu_count"]),
            "--ray-inspection-file",
            str(inspection_path.resolve()),
        ],
    }


def add_manifest_file(files: list[dict[str, Any]], item_root: Path, path: Path, role: str, source: Path | None = None) -> None:
    files.append(
        {
            "relpath": relpath(path, item_root),
            "role": role,
            "bytes": path.stat().st_size,
            "sha256": sha256_file(path),
            "source": str(source.resolve()) if source else None,
        }
    )


def export_scene_only(args: argparse.Namespace) -> dict[str, Any]:
    scene_runtime_path = (args.scene_runtime or fixture_scene_runtime()).resolve()
    render_request_path = (args.render_request or fixture_render_request()).resolve()
    scene_authoring_path = args.scene_authoring.resolve() if args.scene_authoring else None
    output_root = (args.output_root or default_output_root()).resolve()
    item_name = args.item_name
    job_id = args.job_id
    queue_inbox = output_root / "inbox"
    item_root = queue_inbox / item_name
    bundle_root = item_root / "bundle"
    mesh_root = source_mesh_asset_root(scene_runtime_path, args.mesh_asset_root.resolve() if args.mesh_asset_root else None)

    if item_root.exists():
        if not args.force:
            raise ExportError(f"output item already exists: {item_root} (use --force)")
        shutil.rmtree(item_root)

    scene = read_json(scene_runtime_path)
    validate_scene_runtime(scene, scene_runtime_path)
    source_request = read_json(render_request_path)
    validate_render_request(source_request, render_request_path)
    render_request = rewrite_render_request(source_request, item_name=item_name, job_id=job_id)
    inspection_settings = inspection_settings_from_request(render_request)
    asset_ids = mesh_asset_ids(scene)
    support_relpaths = [
        f"line_drawing/assets/mesh_assets/{asset_id}.runtime.json"
        for asset_id in asset_ids
    ]

    source_map: dict[str, Path] = {}
    copy_file(scene_runtime_path, bundle_root / "scene_runtime.json")
    source_map[(bundle_root / "scene_runtime.json").resolve().as_posix()] = scene_runtime_path
    copy_file(scene_runtime_path, bundle_root / "request" / "payload" / "line_drawing" / "scene_runtime.json")
    source_map[(bundle_root / "request" / "payload" / "line_drawing" / "scene_runtime.json").resolve().as_posix()] = scene_runtime_path
    if scene_authoring_path:
        copy_file(scene_authoring_path, bundle_root / "scene_authoring.json")
        source_map[(bundle_root / "scene_authoring.json").resolve().as_posix()] = scene_authoring_path
    for asset_id in asset_ids:
        src = mesh_root / f"{asset_id}.runtime.json"
        copy_file(src, bundle_root / "assets" / "mesh_assets" / src.name)
        source_map[(bundle_root / "assets" / "mesh_assets" / src.name).resolve().as_posix()] = src
        copy_file(src, bundle_root / "request" / "payload" / "line_drawing" / "assets" / "mesh_assets" / src.name)
        source_map[(bundle_root / "request" / "payload" / "line_drawing" / "assets" / "mesh_assets" / src.name).resolve().as_posix()] = src

    write_json(bundle_root / "render_request.json", render_request)
    source_map[(bundle_root / "render_request.json").resolve().as_posix()] = render_request_path
    write_json(bundle_root / "presets" / "inspection_settings.json", inspection_settings)
    write_json(
        bundle_root / "presets" / "renderer_preset.json",
        {
            "schema": "ray_tracing_renderer_preset/v1",
            "mode": "scene-only",
            "integrator_3d": render_value(render_request, "integrator_3d", "direct_light"),
            "temporal_frames": render_value(render_request, "temporal_frames", 1),
            "deferred": ["ui_state_capture", "vf3d_attachment", "physics_bundle_attachment"],
        },
    )
    write_json(
        bundle_root / "presets" / "material_presets.json",
        {
            "schema": "ray_tracing_material_presets/v1",
            "mode": "scene-only",
            "source": "runtime_scene_materials",
            "deferred": ["editable_material_stack_export"],
        },
    )
    write_text(
        bundle_root / "review" / "notes.md",
        "\n".join(
            [
                "# Portable Worker Queue Export Fixture",
                "",
                f"- mode: `scene-only`",
                f"- item: `{item_name}`",
                f"- job: `{job_id}`",
                "- VF3D/PhysicsSim sidecars: not attached in this fixture",
                "- next modes should attach sidecar roots without changing this base scene package contract",
                "",
            ]
        ),
    )
    write_json(
        bundle_root / "request" / "job.json",
        build_job_json(job_id, args.ray_version, args.physics_version, support_relpaths),
    )

    queue_config = build_queue_config(
        item_name=item_name,
        job_id=job_id,
        ray_version=args.ray_version,
        render_request=render_request,
        inspection_path=bundle_root / "presets" / "inspection_settings.json",
    )
    write_json(item_root / "worker_job_queue.json", queue_config)
    (bundle_root / "assets" / "vf3d").mkdir(parents=True, exist_ok=True)
    (bundle_root / "assets" / "physics").mkdir(parents=True, exist_ok=True)

    manifest_files: list[dict[str, Any]] = []
    for path in sorted(item_root.rglob("*")):
        if path.is_file() and path.name != "manifest.json":
            add_manifest_file(
                manifest_files,
                item_root,
                path,
                role_for_path(path, bundle_root),
                source_map.get(path.resolve().as_posix()),
            )
    manifest = {
        "schema": "ray_tracing_portable_worker_export_manifest/v1",
        "mode": "scene-only",
        "item_name": item_name,
        "job_id": job_id,
        "queue_root": str(output_root),
        "item_root": str(item_root),
        "source_scene_runtime": str(scene_runtime_path),
        "source_scene_authoring": str(scene_authoring_path) if scene_authoring_path else None,
        "source_render_request": str(render_request_path),
        "mesh_asset_ids": asset_ids,
        "attachment_slots": {
            "vf3d": "assets/vf3d/",
            "physics": "assets/physics/",
        },
        "files": manifest_files,
    }
    write_json(bundle_root / "manifest.json", manifest)

    return {
        "status": "ok",
        "mode": "scene-only",
        "queue_root": str(output_root),
        "item_name": item_name,
        "item_root": str(item_root),
        "bundle_root": str(bundle_root),
        "manifest_path": str(bundle_root / "manifest.json"),
        "worker_job_queue_path": str(item_root / "worker_job_queue.json"),
        "mesh_asset_count": len(asset_ids),
    }


def role_for_path(path: Path, bundle_root: Path) -> str:
    if path.name == "worker_job_queue.json":
        return "queue_config"
    try:
        bundle_rel = path.resolve().relative_to(bundle_root.resolve()).as_posix()
    except ValueError:
        return "queue_item"
    if bundle_rel == "scene_runtime.json":
        return "portable_scene_runtime"
    if bundle_rel == "render_request.json":
        return "portable_render_request"
    if bundle_rel == "request/job.json":
        return "queue_worker_job"
    if bundle_rel.startswith("request/payload/"):
        return "queue_payload"
    if bundle_rel.startswith("assets/mesh_assets/"):
        return "portable_mesh_asset"
    if bundle_rel.startswith("presets/"):
        return "preset"
    if bundle_rel.startswith("review/"):
        return "review_note"
    return "portable_bundle_file"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", action="store_true", help="Use the built-in mesh-asset sphere fixture paths.")
    parser.add_argument("--mode", choices=["scene-only"], default="scene-only")
    parser.add_argument("--scene-runtime", type=Path)
    parser.add_argument("--scene-authoring", type=Path)
    parser.add_argument("--render-request", type=Path)
    parser.add_argument("--mesh-asset-root", type=Path)
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--item-name", default=DEFAULT_ITEM_NAME)
    parser.add_argument("--job-id", default=DEFAULT_JOB_ID)
    parser.add_argument("--ray-version", default=DEFAULT_RAY_VERSION)
    parser.add_argument("--physics-version", default=DEFAULT_PHYSICS_VERSION)
    parser.add_argument("--force", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.fixture and (not args.scene_runtime or not args.render_request):
        raise SystemExit("error: use --fixture or provide --scene-runtime and --render-request")
    try:
        result = export_scene_only(args)
    except ExportError as exc:
        raise SystemExit(f"error: {exc}") from exc
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
