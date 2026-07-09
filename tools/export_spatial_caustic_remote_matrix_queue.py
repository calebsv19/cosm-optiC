#!/usr/bin/env python3
"""Export queue items for remote spatial-caustic feature matrix renders."""

from __future__ import annotations

import argparse
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

import sys

SCRIPT_DIR = Path(__file__).resolve().parent
RAY_ROOT = SCRIPT_DIR.parent
WORKSPACE_ROOT = RAY_ROOT.parent
INTEGRATION_DIR = RAY_ROOT / "tests" / "integration"
if str(INTEGRATION_DIR) not in sys.path:
    sys.path.insert(0, str(INTEGRATION_DIR))

import run_ray_tracing_spatial_caustic_visual_sphere_mist_matrix as sphere_mist  # noqa: E402


RAY_WORKER_VERSION = "0.5.0"
PHYSICS_WORKER_VERSION = "0.3.0"
VF3D_PAYLOAD_RELPATH = "assets/vf3d/spatial_caustic_soft_mist/soft_mist.vf3d"

CELLS = [
    {
        "cell_id": "vf3d_off_caustic_off_surface_off",
        "volume_enabled": False,
        "caustic_volume": False,
        "caustic_surface": False,
    },
    {
        "cell_id": "vf3d_off_volume_caustic_on_surface_off",
        "volume_enabled": False,
        "caustic_volume": True,
        "caustic_surface": False,
    },
    {
        "cell_id": "vf3d_off_volume_caustic_off_surface_on",
        "volume_enabled": False,
        "caustic_volume": False,
        "caustic_surface": True,
    },
    {
        "cell_id": "vf3d_off_volume_caustic_on_surface_on",
        "volume_enabled": False,
        "caustic_volume": True,
        "caustic_surface": True,
    },
    {
        "cell_id": "vf3d_on_caustic_off_surface_off",
        "volume_enabled": True,
        "caustic_volume": False,
        "caustic_surface": False,
    },
    {
        "cell_id": "vf3d_on_volume_caustic_on_surface_off",
        "volume_enabled": True,
        "caustic_volume": True,
        "caustic_surface": False,
    },
    {
        "cell_id": "vf3d_on_volume_caustic_off_surface_on",
        "volume_enabled": True,
        "caustic_volume": False,
        "caustic_surface": True,
    },
    {
        "cell_id": "vf3d_on_volume_caustic_on_surface_on",
        "volume_enabled": True,
        "caustic_volume": True,
        "caustic_surface": True,
    },
]


def utc_now_compact() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def copy_support_file(src: Path, dst: Path) -> None:
    if not src.is_file():
        raise RuntimeError(f"missing support file: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def write_scene(scene_path: Path) -> None:
    review_root = scene_path.parent / "_scene_source"
    generated = sphere_mist.write_visual_scene(review_root)
    scene = json.loads(generated.read_text(encoding="utf-8"))
    scene["scene_id"] = "caustic_remote_matrix_sphere_mist_v1"
    write_json(scene_path, scene)


def write_job_json(bundle_root: Path, job_id: str, support_relpaths: list[str]) -> None:
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
                "scene_runtime_relpath": "scene_runtime.json",
                "support_files_relpaths": support_relpaths,
            },
            "output_contract": {
                "schema_version": "visualizer-run/v1",
                "publish_mode": "stage_visualizer_drop",
            },
        },
    )


def inspection_for_cell(cell: dict) -> dict:
    inspection = dict(sphere_mist.base_request(
        "inspection_seed",
        Path("scene_runtime.json"),
        Path("output"),
        Path("output/render_summary.json"),
        Path(VF3D_PAYLOAD_RELPATH),
    )["inspection"])
    caustic_enabled = bool(cell["caustic_volume"] or cell["caustic_surface"])
    inspection.update({
        "caustic_mode": "transport" if caustic_enabled else "off",
        "caustic_volume_enabled": bool(cell["caustic_volume"]),
        "caustic_surface_enabled": bool(cell["caustic_surface"]),
        "caustic_sidecar_enabled": False,
        "caustic_sample_budget": 3072 if caustic_enabled else 0,
        "caustic_max_path_depth": 2,
        "caustic_surface_energy_scale": sphere_mist.SURFACE_REVIEW_RADIANCE_SCALE,
        "caustic_surface_footprint_scale": sphere_mist.SURFACE_REVIEW_FOOTPRINT_SCALE,
        "caustic_surface_receiver_fallback_enabled": False,
        "caustic_debug_summary": True,
        "object_audit_enabled": True,
    })
    return inspection


def submit_args(job_id: str, cell: dict, inspection_path: Path) -> list[str]:
    args = [
        "--job-id", job_id,
        "--job-id-policy", "validate",
        "--profile", "preview",
        "--physics-version", PHYSICS_WORKER_VERSION,
        "--ray-version", RAY_WORKER_VERSION,
        "--start-stage", "ray_tracing",
        "--ray-frame-count", "1",
        "--ray-start-frame", "0",
        "--ray-width", "384",
        "--ray-height", "240",
        "--ray-temporal-frames", "2",
        "--ray-normalized-t", "0.0",
        "--ray-integrator", "disney_v2",
        "--ray-resource-cpu-percent", "35",
        "--ray-resource-max-workers", "1",
        "--ray-resource-reserve-cpu-count", "1",
        "--ray-inspection-file", str(inspection_path.resolve()),
        "--force-worker-id", "linuxpc",
        "--force-hostname", "localhost.localdomain",
        "--force-worker-kind", "remote_worker",
        "--force-label", "linux-pc",
        "--no-worker-fallback",
        "--publication-output-kind", "review_matrix_cell",
        "--publication-preview-relpath", "stages/ray_tracing/output/frames/frame_0000.bmp",
        "--publication-primary-relpath", "stages/ray_tracing/output/frames/frame_0000.bmp",
    ]
    if cell["volume_enabled"]:
        args.extend([
            "--ray-volume-source-kind", "raw_vf3d",
            "--ray-volume-source-path", VF3D_PAYLOAD_RELPATH,
            "--ray-volume-visible", "true",
            "--ray-volume-affects-lighting", "true",
        ])
    else:
        args.append("--ray-disable-volume")
    return args


def export_item(queue_root: Path, batch_id: str, cell: dict, timestamp: str, force: bool) -> dict:
    item_name = f"{batch_id}-{cell['cell_id']}"
    item_root = queue_root / "inbox" / item_name
    if item_root.exists():
        if not force:
            raise RuntimeError(f"queue item already exists: {item_root}")
        shutil.rmtree(item_root)
    bundle_root = item_root / "bundle"
    payload_root = bundle_root / "request" / "payload"
    scene_path = payload_root / "scene_runtime.json"
    write_scene(scene_path)
    support_relpaths = [
        "assets/mesh_assets/asset_sphere_256x128.runtime.json",
        "line_drawing/assets/mesh_assets/asset_sphere_256x128.runtime.json",
        VF3D_PAYLOAD_RELPATH,
    ]
    source_mesh = (
        RAY_ROOT / "tests" / "fixtures" / "mesh_asset_runtime_spheres" / "assets" / "mesh_assets" /
        "asset_sphere_256x128.runtime.json"
    )
    copy_support_file(source_mesh, payload_root / support_relpaths[0])
    copy_support_file(source_mesh, payload_root / support_relpaths[1])
    sphere_mist.write_soft_mist_vf3d(payload_root / support_relpaths[2])
    job_id = f"ray-tracing--caustic-remote-matrix-{cell['cell_id'].replace('_', '-')}--{timestamp}--cst{len(cell['cell_id']) % 10:01d}"
    write_job_json(bundle_root, job_id, support_relpaths)
    inspection_path = bundle_root / "presets" / "inspection_settings.json"
    write_json(inspection_path, inspection_for_cell(cell))
    write_json(
        item_root / "worker_job_queue.json",
        {
            "schema": "codework-worker-desktop-queue-item/v1",
            "item_id": item_name,
            "bundle_dir": "bundle",
            "overwrite_staging": True,
            "submit_args": submit_args(job_id, cell, inspection_path),
        },
    )
    write_json(
        bundle_root / "manifest.json",
        {
            "schema": "ray_tracing_spatial_caustic_remote_matrix_queue_item/v1",
            "batch_id": batch_id,
            "item_name": item_name,
            "job_id": job_id,
            "cell": cell,
            "ray_worker_version": RAY_WORKER_VERSION,
            "volume_source_path": VF3D_PAYLOAD_RELPATH if cell["volume_enabled"] else "",
        },
    )
    return {"item_name": item_name, "job_id": job_id, "cell_id": cell["cell_id"]}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--queue-root",
        type=Path,
        default=WORKSPACE_ROOT / "_private_workspace_artifacts" / "ray_tracing" /
        "caustic_remote_matrix_queue",
    )
    parser.add_argument("--batch-id", default="rt-caustic-remote-matrix-20260704a")
    parser.add_argument("--force", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    queue_root = args.queue_root.resolve()
    for name in ("inbox", "prepared", "submitted", "failed", "archived"):
        (queue_root / name).mkdir(parents=True, exist_ok=True)
    timestamp = utc_now_compact()
    items = [export_item(queue_root, args.batch_id, cell, timestamp, args.force) for cell in CELLS]
    write_json(
        queue_root / f"{args.batch_id}_manifest.json",
        {
            "schema": "ray_tracing_spatial_caustic_remote_matrix_queue/v1",
            "batch_id": args.batch_id,
            "queue_root": str(queue_root),
            "ray_worker_version": RAY_WORKER_VERSION,
            "items": items,
        },
    )
    print(queue_root)
    for item in items:
        print(item["item_name"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
