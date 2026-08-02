#!/usr/bin/env python3
"""Build the two-part, submit-review-only aquarium HQ 96-frame batch."""

from __future__ import annotations

import copy
import argparse
import hashlib
import json
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
WORKSPACE_ROOT = REPO_ROOT.parent.parent
sys.path.insert(0, str(WORKSPACE_ROOT / "bin"))

from vps_worker_job_queue_lib.clone_edit import rewrite_payload_and_apply_edits  # noqa: E402
from vps_worker_job_queue_lib.plan import mark_export_item_pending  # noqa: E402
from submit_codework_worker_job_via_vps import build_package_catalog_preflight  # noqa: E402


ACCEPTED_PAYLOAD = (
    WORKSPACE_ROOT
    / "_private_workspace_artifacts/agent_runs/ray_tracing/"
    "aquarium_w2_vf3d_rear_stl_centered_camera_preview_20260719/typed_edit/submit_payload.json"
)
SOURCE_B_PAYLOAD = (
    WORKSPACE_ROOT
    / "_private_workspace_artifacts/agent_runs/physics_trio/aquarium_glass_room_v1/"
    "queue_exports/staged/aquarium-dark-mirror-glazed-brick-48f-b-20260715d/"
    "payload/submit_payload.json"
)
OUTPUT_ROOT = (
    WORKSPACE_ROOT
    / "_private_workspace_artifacts/agent_runs/ray_tracing/"
    "aquarium_centered_unified_water_hq_96f_review_20260719"
)
BATCH_ID = "aquarium-centered-unified-water-hq-96f-20260719b"
SCENE_ID = "aquarium-unified-water-glass-room-v1"
SNAPSHOT_ID = "snap-0001-centered-water-vf3d-rear-stls-good"
RAY_TRACING_WORKER_VERSION = "0.8.1"
QUALITY = {
    "width": 1280,
    "height": 720,
    "temporal_frames": 8,
    "transmission_samples_3d": 8,
    "secondary_diffuse_samples_3d": 6,
    "integrator_3d": "disney_v2",
    "denoise_enabled": False,
    "trace_route": "flattened_bvh",
    "fps": 16,
}
HALVES = (
    {
        "slug": "source_a",
        "item_id": "aquarium-centered-unified-water-hq-48f-a-20260719b",
        "job_id": "ray-tracing--aquarium-centered-unified-water-hq-48f-a--20260720T054500Z--aquchq48a",
        "start_frame": 200,
        "last_frame": 247,
        "worker_id": "linuxpc-fast",
        "worker_label": "linux-pc-fast",
    },
    {
        "slug": "source_b",
        "item_id": "aquarium-centered-unified-water-hq-48f-b-20260719b",
        "job_id": "ray-tracing--aquarium-centered-unified-water-hq-48f-b--20260720T054501Z--aquchq48b",
        "start_frame": 248,
        "last_frame": 295,
        "worker_id": "linuxpc-long",
        "worker_label": "linux-pc-long",
    },
)
DESKTOP_QUEUE_ROOT = Path.home() / "Desktop/VPS_WORKER_JOB_QUEUE"
DESKTOP_STAGING_ROOT = WORKSPACE_ROOT / "_private_workspace_artifacts/vps_worker_job_queue/staged"
DESKTOP_STATE_PATH = WORKSPACE_ROOT / "_private_workspace_artifacts/vps_worker_job_queue/state.json"
DESKTOP_EXPORT_STATE_PATH = WORKSPACE_ROOT / "_private_workspace_artifacts/vps_worker_job_queue/export_dropbox_state.json"


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, doc: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(doc, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--stage-desktop-queue",
        action="store_true",
        help="Wrap the exact reviewed payloads as prepared Desktop queue items without regenerating them.",
    )
    return parser.parse_args()


def entries(payload: dict) -> dict[str, dict]:
    return {str(entry["relpath"]): entry for entry in payload["payload_files"]}


def embedded_json(payload: dict, relpath: str) -> dict:
    return json.loads(entries(payload)[relpath]["content_utf8"])


def replace_embedded_json(payload: dict, relpath: str, doc: dict) -> None:
    entry = entries(payload)[relpath]
    entry["content_utf8"] = json.dumps(doc, indent=2, sort_keys=True) + "\n"
    entry.pop("content_base64", None)


def compose_source_b(accepted: dict, source_b: dict) -> dict:
    payload = copy.deepcopy(source_b)
    target = entries(payload)
    accepted_entries = entries(accepted)
    for relpath in (
        "scene_runtime.json",
        "assets/mesh_assets/asset_generated_aquarium_water_side_bottom_shell_amp2_v1.runtime.json",
        "assets/mesh_assets/asset_stl_aquarium_glass_shell_v1.runtime.json",
        "assets/mesh_assets/asset_stl_benchy.runtime.json",
        "assets/mesh_assets/asset_stl_stanford_bunny.runtime.json",
        "assets/mesh_assets/imported_stanford_dragon_vrip_full.runtime.json",
        "ray_tracing_request.json",
    ):
        target[relpath].clear()
        target[relpath].update(copy.deepcopy(accepted_entries[relpath]))
    for relpath, entry in accepted_entries.items():
        if relpath.startswith("assets/vf3d/"):
            payload["payload_files"].append(copy.deepcopy(entry))
    accepted_manifest = embedded_json(accepted, "assets/physics/water_basin/water_manifest_v1.json")
    manifest = embedded_json(payload, "assets/physics/water_basin/water_manifest_v1.json")
    manifest["water_body_boundary_v1"] = copy.deepcopy(accepted_manifest["water_body_boundary_v1"])
    replace_embedded_json(payload, "assets/physics/water_basin/water_manifest_v1.json", manifest)
    return payload


def build_half(accepted: dict, source_b: dict, spec: dict) -> dict:
    payload = copy.deepcopy(accepted) if spec["slug"] == "source_a" else compose_source_b(accepted, source_b)
    old_job_id = str(payload.get("job_id") or "")
    old_request_run_id = str(embedded_json(payload, "ray_tracing_request.json").get("run_id") or "")
    out_dir = OUTPUT_ROOT / spec["slug"]
    payload_path = out_dir / "payload" / "submit_payload.json"
    write_json(payload_path, payload)
    rewrite_payload_and_apply_edits(
        payload_path,
        replacements={
            old_job_id: spec["job_id"],
            old_request_run_id: spec["job_id"],
        },
        object_material_edits=[],
        inspection_edits=[
            f"trace_route={QUALITY['trace_route']}",
            f"transmission_samples_3d={QUALITY['transmission_samples_3d']}",
            f"secondary_diffuse_samples_3d={QUALITY['secondary_diffuse_samples_3d']}",
        ],
        render_edits=[
            f"width={QUALITY['width']}",
            f"height={QUALITY['height']}",
            "frame_count=48",
            f"start_frame={spec['start_frame']}",
            f"temporal_frames={QUALITY['temporal_frames']}",
            f"transmission_samples_3d={QUALITY['transmission_samples_3d']}",
            f"secondary_diffuse_samples_3d={QUALITY['secondary_diffuse_samples_3d']}",
            "normalized_t=0",
        ],
    )
    payload = load_json(payload_path)
    payload["item_id"] = spec["item_id"]
    payload["job_id"] = spec["job_id"]
    payload["requested_job_id"] = spec["job_id"]
    payload.pop("latest_hot_cache_inventory_digest", None)
    payload.setdefault("worker_package_set", {}).setdefault("ray_tracing", {})["version"] = RAY_TRACING_WORKER_VERSION
    payload["worker_package_set"]["ray_tracing"]["worker_slug"] = "ray_tracing_headless_worker"
    payload["worker_routing"] = {
        "allow_fallback": False,
        "explain_selection": True,
        "fallback_worker_kinds": ["remote_worker"],
        "preferred_hostnames": ["localhost.localdomain"],
        "preferred_labels": ["linux-pc", spec["worker_label"]],
        "preferred_remote_host_pool": ["localhost.localdomain"],
        "preferred_worker_ids": [spec["worker_id"]],
        "preferred_worker_kinds": ["remote_worker"],
        "selection_policy": "preferred_remote_pool_v1",
    }
    payload["codex_split_48f"] = {
        "batch_id": BATCH_ID,
        "split_index": 0 if spec["slug"] == "source_a" else 1,
        "frame_count": 48,
        "first_frame_index": spec["start_frame"],
        "last_frame_index": spec["last_frame"],
        "payload_file_limit_strategy": "two ordered 48-frame payload-local halves; each remains <=64 payload_files",
    }
    payload["publication"] = {
        "job_type": "trio-headless-worker",
        "program": "ray-tracing",
        "primary_output_source_relpath": f"stages/ray_tracing/output/frames/frame_{spec['last_frame']:04d}.bmp",
        "preview_source_relpath": f"stages/ray_tracing/output/frames/frame_{spec['last_frame']:04d}.bmp",
        "outputs": [{
            "kind": "preview_frame",
            "media_type": "image/bmp",
            "relpath": f"outputs/frame_{spec['last_frame']:04d}.bmp",
            "source_relpath": f"stages/ray_tracing/output/frames/frame_{spec['last_frame']:04d}.bmp",
        }],
    }
    replace_embedded_json(payload, "ray_tracing_request.json", embedded_json(payload, "ray_tracing_request.json"))
    write_json(payload_path, payload)

    request = embedded_json(payload, "ray_tracing_request.json")
    scene = embedded_json(payload, "scene_runtime.json")
    manifest = embedded_json(payload, "assets/physics/water_basin/water_manifest_v1.json")
    water_sidecars = sorted(
        relpath for relpath in entries(payload)
        if relpath.startswith("assets/physics/water_basin/water_surface_")
    )
    expected_sidecars = [
        f"assets/physics/water_basin/water_surface_{frame:06d}.json"
        for frame in range(spec["start_frame"], spec["last_frame"] + 1)
    ]
    if water_sidecars != expected_sidecars:
        raise ValueError(f"water sidecar range mismatch for {spec['slug']}")
    if len(payload["payload_files"]) > 64:
        raise ValueError(f"payload_files limit exceeded for {spec['slug']}")
    render = request["render"]
    inspection = request["inspection"]
    expected_camera = {
        "camera_position": {"x": 0.0, "y": -4.15, "z": 1.78},
        "camera_look_at": {"x": 0.0, "y": 0.18, "z": 1.08},
        "camera_zoom": 0.96,
    }
    for field, expected in expected_camera.items():
        if inspection.get(field) != expected:
            raise ValueError(f"camera mismatch for {spec['slug']}: {field}")
    if (
        render.get("width") != QUALITY["width"]
        or render.get("height") != QUALITY["height"]
        or render.get("frame_count") != 48
        or render.get("start_frame") != spec["start_frame"]
        or render.get("temporal_frames") != QUALITY["temporal_frames"]
        or render.get("transmission_samples_3d") != QUALITY["transmission_samples_3d"]
        or render.get("secondary_diffuse_samples_3d") != QUALITY["secondary_diffuse_samples_3d"]
        or render.get("integrator_3d") != QUALITY["integrator_3d"]
        or render.get("denoise_enabled") is not QUALITY["denoise_enabled"]
        or inspection.get("trace_route") != QUALITY["trace_route"]
        or inspection.get("transmission_samples_3d") != QUALITY["transmission_samples_3d"]
        or inspection.get("secondary_diffuse_samples_3d") != QUALITY["secondary_diffuse_samples_3d"]
    ):
        raise ValueError(f"quality profile mismatch for {spec['slug']}")
    routing = payload.get("worker_routing") or {}
    if (
        routing.get("preferred_worker_ids") != [spec["worker_id"]]
        or spec["worker_label"] not in routing.get("preferred_labels", [])
        or routing.get("allow_fallback") is not False
    ):
        raise ValueError(f"worker routing mismatch for {spec['slug']}")
    boundary = manifest.get("water_body_boundary_v1")
    if not isinstance(boundary, dict) or boundary.get("closure_mode") != "heightfield_volume":
        raise ValueError(f"unified water boundary missing for {spec['slug']}")
    object_ids = {obj.get("object_id") for obj in scene.get("objects", [])}
    if {"stanford_bunny_room_plinth", "stanford_dragon_room_stress_mesh"} - object_ids:
        raise ValueError(f"rear STL objects missing for {spec['slug']}")

    result = {
        **spec,
        "payload_path": str(payload_path),
        "payload_sha256": sha256(payload_path),
        "payload_bytes": payload_path.stat().st_size,
        "payload_file_count": len(payload["payload_files"]),
        "water_sidecar_count": len(water_sidecars),
        "volume_source_path": request["volume"]["source_path"],
        "ray_tracing_package_requirement": payload["worker_package_set"]["ray_tracing"],
        "quality": copy.deepcopy(QUALITY),
        "camera": expected_camera,
        "unified_water_boundary": boundary,
        "worker_routing": payload.get("worker_routing"),
        "validation": "local_payload_contract_ok",
    }
    write_json(out_dir / "review_manifest.json", result)
    return result


def stage_desktop_queue(halves: list[dict]) -> list[dict]:
    state = load_json(DESKTOP_STATE_PATH) if DESKTOP_STATE_PATH.is_file() else {
        "schema": "codework-worker-desktop-queue/v1",
        "items": [],
    }
    known_ids = {str(item.get("item_id") or "") for item in state.get("items", [])}
    for half in halves:
        item_id = half["item_id"]
        targets = [
            DESKTOP_QUEUE_ROOT / lane / item_id
            for lane in ("inbox", "prepared", "submitted", "failed", "archived")
        ] + [DESKTOP_STAGING_ROOT / item_id]
        if item_id in known_ids or any(path.exists() for path in targets):
            raise ValueError(f"desktop queue item already exists: {item_id}")

    prepared_at = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    staged_records = []
    for half in halves:
        item_id = half["item_id"]
        job_id = half["job_id"]
        reviewed_payload = Path(half["payload_path"])
        if sha256(reviewed_payload) != half["payload_sha256"]:
            raise ValueError(f"reviewed payload digest drifted before queue staging: {item_id}")
        submit_doc = load_json(reviewed_payload)
        if submit_doc.get("job_id") != job_id:
            raise ValueError(f"reviewed payload job id mismatch: {item_id}")

        source_dir = DESKTOP_QUEUE_ROOT / "prepared" / item_id
        staged_dir = DESKTOP_STAGING_ROOT / item_id
        source_dir.mkdir(parents=True)
        (staged_dir / "payload").mkdir(parents=True)
        staged_payload = staged_dir / "payload/submit_payload.json"
        shutil.copy2(reviewed_payload, staged_payload)
        if sha256(staged_payload) != half["payload_sha256"]:
            raise ValueError(f"staged payload digest mismatch: {item_id}")

        package_preflight = build_package_catalog_preflight(
            item_id=item_id,
            submit_doc=submit_doc,
            prepared_at=prepared_at,
        )
        write_json(staged_dir / "payload/package_catalog_preflight.json", package_preflight)
        write_json(
            staged_dir / "payload/vps_submit_request.json",
            {
                "schema": "codework-worker-vps-submit-request/v1",
                "item_id": item_id,
                "job_id": job_id,
                "requested_job_id": job_id,
                "job_id_policy": "validate",
                "job_id_resolution": "preserved_v1",
                "prepared_at": prepared_at,
                "submit_payload_relpath": "payload/submit_payload.json",
                "package_catalog_preflight_relpath": "payload/package_catalog_preflight.json",
                "package_catalog_policy": package_preflight["policy"],
                "vps_api_base": "http://10.0.10.1:9102",
                "vps_api_endpoint": "/api/codework-worker/jobs",
                "coordinator_policy": {
                    "vps_must_not_claim_as_worker": True,
                    "allow_vps_local_diagnostic": False,
                },
            },
        )
        write_json(
            staged_dir / "manifest.json",
            {
                "schema": "codework-worker-export-dropbox-item/v1",
                "item_id": item_id,
                "source_name": job_id,
                "source_kind": "reviewed_worker_submit_payload",
                "source_path": str(reviewed_payload),
                "prepared_at": prepared_at,
                "payload_root": "payload",
                "normalized_root": "",
                "artifacts": {
                    "vps_submit_request": "payload/vps_submit_request.json",
                    "submit_payload": "payload/submit_payload.json",
                    "package_catalog_preflight": "payload/package_catalog_preflight.json",
                },
            },
        )
        write_json(
            staged_dir / "summary.json",
            {
                "schema": "codework-worker-vps-submit-item-summary/v1",
                "item_id": item_id,
                "job_id": job_id,
                "requested_job_id": job_id,
                "job_id_policy": "validate",
                "job_id_resolution": "preserved_v1",
                "prepared_at": prepared_at,
                "start_stage": submit_doc.get("start_stage"),
                "worker_package_set": submit_doc.get("worker_package_set", {}),
                "worker_routing": submit_doc.get("worker_routing", {}),
                "reviewed_payload_sha256": half["payload_sha256"],
            },
        )
        write_json(
            source_dir / "worker_job_queue.json",
            {
                "schema": "codework-worker-desktop-queue-item/v1",
                "item_id": item_id,
                "source_kind": "reviewed_worker_submit_payload",
                "reviewed_payload_path": str(reviewed_payload),
                "reviewed_payload_sha256": half["payload_sha256"],
            },
        )
        record = {
            "item_id": item_id,
            "status": "prepared",
            "source_key": f"reviewed-submit-payload::{half['payload_sha256']}",
            "source_path": str(source_dir),
            "prepared_at": prepared_at,
            "staging_dir": str(staged_dir),
            "remote_item_path": f".codework-worker/export-dropbox/{item_id}",
            "job_id": job_id,
            "requested_job_id": job_id,
            "submit_result_thread_id": None,
            "last_submit_output": None,
            "edit_kind": "reviewed_submit_payload_import",
            "reviewed_payload_sha256": half["payload_sha256"],
            "status_watch_policy": "deferred",
        }
        state.setdefault("items", []).append(record)
        mark_export_item_pending(DESKTOP_EXPORT_STATE_PATH, record)
        staged_records.append(record)

    state["queue_root"] = str(DESKTOP_QUEUE_ROOT)
    state["staging_root"] = str(DESKTOP_STAGING_ROOT)
    state["export_state_path"] = str(DESKTOP_EXPORT_STATE_PATH)
    state["updated_at"] = prepared_at
    write_json(DESKTOP_STATE_PATH, state)
    return staged_records


def main() -> int:
    args = parse_args()
    accepted = load_json(ACCEPTED_PAYLOAD)
    source_b = load_json(SOURCE_B_PAYLOAD)
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    halves = [build_half(accepted, source_b, spec) for spec in HALVES]
    batch = {
        "schema": "aquarium-centered-unified-water-hq-96f-review/v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "batch_id": BATCH_ID,
        "scene_id": SCENE_ID,
        "latest_good_snapshot_id": SNAPSHOT_ID,
        "operator_review_required_before_submit": True,
        "remote_plan_ran": False,
        "remote_submit_ran": False,
        "worker_package_built_or_installed": False,
        "publication_ran": False,
        "frame_count": 96,
        "frame_range": [200, 295],
        "fps": QUALITY["fps"],
        "duration_seconds": 96 / QUALITY["fps"],
        "quality": QUALITY,
        "split_reason": "60 payload files per half after VF3D composition; one merged 96-frame payload would exceed the 64-file queue contract",
        "package_readiness": {
            "required_ray_tracing_version": RAY_TRACING_WORKER_VERSION,
            "historical_source_payload_version": "0.6.2",
            "status": "planner_and_package_inventory_check_required_after_operator_approval",
        },
        "halves": halves,
        "next_action": "After operator approval, stage both items into the Desktop queue, run local validate/prepare, then remote plan; submit only if both planner replies are submit_ready.",
    }
    write_json(OUTPUT_ROOT / "batch_review_manifest.json", batch)
    if args.stage_desktop_queue:
        batch["desktop_queue_records"] = stage_desktop_queue(halves)
        batch["desktop_queue_staged_at_utc"] = datetime.now(timezone.utc).isoformat()
        write_json(OUTPUT_ROOT / "batch_review_manifest.json", batch)
    print(json.dumps({"status": "ok", "manifest": str(OUTPUT_ROOT / "batch_review_manifest.json")}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
