#!/usr/bin/env python3
"""Generate the deterministic W0 aquarium replay-lineage record."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


SCENE_ROOT = Path(
    "_private_workspace_artifacts/agent_runs/physics_trio/"
    "aquarium_glass_room_v1"
)
ITEMS = (
    "aquarium-dark-mirror-glazed-brick-48f-a-20260715d",
    "aquarium-dark-mirror-glazed-brick-48f-b-20260715d",
)
SUMMARY_RELPATHS = (
    SCENE_ROOT / "material_96f_assembly_20260717/source_a/output/render_summary.json",
    SCENE_ROOT / "material_96f_assembly_20260717/source_b/output/render_summary.json",
)
SCREENSHOT_RELPATH = (
    SCENE_ROOT
    / "water_volume_surface_audit_20260718/operator_screenshot_20260718.png"
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_record(workspace_root: Path, relpath: Path) -> dict[str, object]:
    data = (workspace_root / relpath).read_bytes()
    return {
        "relpath": relpath.as_posix(),
        "size_bytes": len(data),
        "sha256": sha256_bytes(data),
    }


def payload_record(workspace_root: Path, item: str) -> dict[str, object]:
    payload_relpath = (
        SCENE_ROOT
        / "queue_exports/staged"
        / item
        / "payload/submit_payload.json"
    )
    payload_path = workspace_root / payload_relpath
    payload_bytes = payload_path.read_bytes()
    payload = json.loads(payload_bytes)
    embedded = []
    for entry in sorted(payload["payload_files"], key=lambda value: value["relpath"]):
        content = entry["content_utf8"].encode("utf-8")
        embedded.append(
            {
                "relpath": entry["relpath"],
                "size_bytes": len(content),
                "sha256": sha256_bytes(content),
            }
        )
    sidecars = [
        entry
        for entry in embedded
        if entry["relpath"].startswith("assets/physics/water_basin/water_surface_")
    ]
    split = payload["codex_split_48f"]
    return {
        "item_id": item,
        "job_id": payload["job_id"],
        "submit_payload": {
            "relpath": payload_relpath.as_posix(),
            "size_bytes": len(payload_bytes),
            "sha256": sha256_bytes(payload_bytes),
        },
        "immutable_parent": {
            "source_item": split["source_item"],
            "source_job": split["source_job"],
            "split_index": split["split_index"],
        },
        "frame_range": {
            "first": split["first_frame_index"],
            "last": split["last_frame_index"],
            "count": split["frame_count"],
        },
        "worker_package_set": payload["worker_package_set"],
        "payload_file_count": len(embedded),
        "water_sidecar_count": len(sidecars),
        "embedded_files": embedded,
    }


def summary_record(workspace_root: Path, relpath: Path) -> dict[str, object]:
    record = file_record(workspace_root, relpath)
    summary = json.loads((workspace_root / relpath).read_bytes())
    water = summary.get("water_surface", {})
    record.update(
        {
            "run_id": summary.get("run_id"),
            "frames_rendered": summary.get("frames_rendered"),
            "water_grid_w": water.get("grid_w"),
            "water_grid_d": water.get("grid_d"),
            "water_dry_columns_last_frame": water.get("dry_columns"),
        }
    )
    return record


def git_output(repo_root: Path, *args: str) -> str:
    return subprocess.check_output(
        ["git", *args], cwd=repo_root, text=True
    ).strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace-root", type=Path, required=True)
    parser.add_argument("--ray-tracing-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    workspace_root = args.workspace_root.resolve()
    ray_tracing_root = args.ray_tracing_root.resolve()

    payloads = [payload_record(workspace_root, item) for item in ITEMS]
    all_sidecars = sum(item["water_sidecar_count"] for item in payloads)
    record = {
        "schema": "ray_tracing_aquarium_replay_lineage_w0_v1",
        "record_kind": "explicit_immutable_parent_local_lineage",
        "proof_intent": "reject disconnected water topology while preserving accepted room and material composition",
        "promotion_eligible": False,
        "latest_good_mutated": False,
        "published_visualizer_run": {
            "run_id": "ray-tracing--aquarium-dark-mirror-glazed-brick-96f--20260717T174824Z--aqdmgb96",
            "frame_count": 96,
            "fps": 16,
            "duration_seconds": 6,
            "role": "visual rejection evidence for water connectivity",
        },
        "operator_screenshot": file_record(workspace_root, SCREENSHOT_RELPATH),
        "payloads": payloads,
        "render_summaries": [
            summary_record(workspace_root, relpath) for relpath in SUMMARY_RELPATHS
        ],
        "lineage_totals": {
            "payload_count": len(payloads),
            "ordinal_sidecar_count": all_sidecars,
            "ordinal_range": [200, 295],
        },
        "source_facts": {
            "historical_replay_ray_tracing_package": "0.6.2",
            "historical_replay_physics_sim_package": "0.3.1",
            "implementation_base_commit": git_output(ray_tracing_root, "rev-parse", "HEAD"),
            "implementation_source_version": (
                ray_tracing_root / "VERSION"
            ).read_text(encoding="utf-8").strip(),
            "historical_package_truth_rewritten": False,
        },
        "hard_boundaries": {
            "scene_pointer_change": False,
            "remote_runtime_change": False,
            "sidecar_regeneration": False,
            "render_submission": False,
            "publication": False,
        },
    }
    if all_sidecars != 96:
        raise SystemExit(f"expected 96 ordinal sidecars, found {all_sidecars}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
