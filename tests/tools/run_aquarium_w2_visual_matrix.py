#!/usr/bin/env python3
"""Prepare and render the non-promoted Source-A frame-200 W2 visual matrix."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
INTEGRATION_DIR = SCRIPT_DIR.parent / "integration"
if str(INTEGRATION_DIR) not in sys.path:
    sys.path.insert(0, str(INTEGRATION_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import run_ray_tracing_aquarium_real_shell_transparent_receiver_diagnostic as diagnostic  # noqa: E402


SOURCE_JOB_ID = "ray-tracing--aquarium-dark-mirror-glazed-brick-48f-a--20260715T183500Z--aqdmgb48a"
SOURCE_ITEM_ID = "aquarium-dark-mirror-glazed-brick-48f-a-20260715d"
FRAME_INDEX = 200
GLASS_ID = "aquarium_glass_shell"
BENCHY_ID = "benchy_floating_inside_aquarium"
LEGACY_WATER_ID = "water_surface_placeholder"
FOCUS_EXCLUDED_IDS = (
    "stanford_bunny_room_plinth",
    "stanford_dragon_room_stress_mesh",
    "waterline_front_preview",
)


VARIANTS = (
    {
        "id": "01_legacy_split_glass_benchy",
        "label": "Legacy split water control with glass and Benchy",
        "unified": False,
        "glass_mode": "accepted",
        "benchy": True,
    },
    {
        "id": "02_unified_optical_no_glass_no_benchy",
        "label": "Unified water, optically neutral glass, no Benchy",
        "unified": True,
        "glass_mode": "optically_neutral",
        "benchy": False,
    },
    {
        "id": "03_unified_glass_no_benchy",
        "label": "Unified water with accepted glass, no Benchy",
        "unified": True,
        "glass_mode": "accepted",
        "benchy": False,
    },
    {
        "id": "04_unified_glass_benchy",
        "label": "Unified water with accepted glass and Benchy",
        "unified": True,
        "glass_mode": "accepted",
        "benchy": True,
    },
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def workspace_root() -> Path:
    return repo_root().parent.parent


def default_submit_payload() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "physics_trio"
        / "aquarium_glass_room_v1"
        / "queue_exports"
        / "staged"
        / SOURCE_ITEM_ID
        / "payload"
        / "submit_payload.json"
    )


def default_output_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "aquarium_w2_visual_matrix_20260718"
    )


def default_cli() -> Path:
    machine = platform.machine()
    return (
        repo_root()
        / "build"
        / "toolchains"
        / "clang"
        / machine
        / "tools"
        / "cli"
        / "ray_tracing_render_headless"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--submit-payload", type=Path, default=default_submit_payload())
    parser.add_argument("--output-root", type=Path, default=default_output_root())
    parser.add_argument("--cli", type=Path, default=default_cli())
    parser.add_argument("--width", type=int, default=480)
    parser.add_argument("--height", type=int, default=270)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--render-existing", action="store_true")
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
        stream.write("\n")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def payload_map(submit: dict) -> dict[str, str]:
    result: dict[str, str] = {}
    for entry in submit.get("payload_files", []):
        relpath = entry.get("relpath")
        content = entry.get("content_utf8")
        if isinstance(relpath, str) and isinstance(content, str):
            result[relpath] = content
    return result


def require_embedded(files: dict[str, str], relpath: str) -> str:
    if relpath not in files:
        raise ValueError(f"accepted payload missing embedded file: {relpath}")
    return files[relpath]


def object_by_id(scene: dict, object_id: str) -> dict:
    for obj in scene.get("objects", []):
        if obj.get("object_id") == object_id:
            return obj
    raise ValueError(f"scene object missing: {object_id}")


def material_override(scene: dict, object_id: str) -> dict:
    authoring = scene.get("extensions", {}).get("ray_tracing", {}).get("authoring", {})
    for material in authoring.get("object_materials", []):
        if material.get("object_id") == object_id:
            return material
    raise ValueError(f"scene material override missing: {object_id}")


def set_visible(scene: dict, object_id: str, visible: bool) -> None:
    object_by_id(scene, object_id).setdefault("flags", {})["visible"] = visible


def remove_focus_excluded_objects(scene: dict) -> None:
    excluded = set(FOCUS_EXCLUDED_IDS)
    scene["objects"] = [
        obj for obj in scene.get("objects", []) if obj.get("object_id") not in excluded
    ]


def set_glass_mode(scene: dict, mode: str) -> None:
    material = material_override(scene, GLASS_ID)
    if mode == "accepted":
        return
    if mode != "optically_neutral":
        raise ValueError(f"unsupported glass mode: {mode}")
    # Keep the container primitive resolvable for the W2 identity check while
    # neutralizing its optical effect for the no-glass connectivity control.
    material.update(
        {
            "alpha": 0.0,
            "glass_absorption_distance": 1000000.0,
            "glass_ior": 1.0,
            "glass_thin_walled": True,
            "glass_transmission": 1.0,
            "glass_transport_override": True,
            "reflectivity": 0.0,
            "roughness": 0.0,
        }
    )


def water_body_contract(manifest: dict) -> dict:
    frames = manifest.get("frames", [])
    frame = next((item for item in frames if item.get("frame_index") == FRAME_INDEX), None)
    if not frame:
        raise ValueError("accepted manifest does not contain frame 200")
    base_height = frame.get("surface_avg_y")
    if not isinstance(base_height, (int, float)):
        raise ValueError("accepted frame 200 has no numeric surface_avg_y")
    return {
        "closure_mode": "heightfield_volume",
        "body_id": "aquarium_unified_water_body",
        "container_id": GLASS_ID,
        "object_id": "aquarium_unified_water_body",
        "material_id": "aquarium_water_material",
        "medium_id": "aquarium_water_medium",
        "legacy_shell_object_id": LEGACY_WATER_ID,
        "container_inner_bounds_m": {
            "min_x": -1.91,
            "max_x": 1.91,
            "min_y": 0.08,
            "max_y": 1.60,
            "min_z": -1.01,
            "max_z": 1.01,
        },
        "boundary_inset_m": 0.0,
        "bottom_height_m": 0.08,
        "base_surface_height_m": base_height,
        "dry_sample_policy": "surface_min_epsilon_to_base",
        "dry_height_epsilon_m": 0.000001,
        "solid_occluder_policy": "ordinary_geometry_occlusion",
        "classification_metadata": "legacy_height_sentinel",
    }


def materialize_source(files: dict[str, str], root: Path) -> None:
    required = (
        "assets/mesh_assets/asset_generated_aquarium_water_side_bottom_shell_amp2_v1.runtime.json",
        "assets/mesh_assets/asset_stl_aquarium_glass_shell_v1.runtime.json",
        "assets/mesh_assets/asset_stl_benchy.runtime.json",
        "assets/mesh_assets/asset_stl_stanford_bunny.runtime.json",
        "assets/mesh_assets/imported_stanford_dragon_vrip_full.runtime.json",
        "assets/physics/water_basin/scene_bundle.json",
        "assets/physics/water_basin/water_surface_000200.json",
    )
    for relpath in required:
        destination = root / relpath
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(require_embedded(files, relpath), encoding="utf-8")


def build_request(base: dict, scene_path: Path, bundle_path: Path, render_root: Path,
                  variant: dict, width: int, height: int) -> dict:
    request = copy.deepcopy(base)
    request["run_id"] = f"aquarium_w2_{variant['id']}_frame_{FRAME_INDEX}"
    request["scene"] = {"runtime_scene_path": str(scene_path)}
    request.setdefault("render", {}).update(
        {
            "width": width,
            "height": height,
            "frame_count": 1,
            "start_frame": FRAME_INDEX,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "transmission_samples_3d": 2,
            "secondary_diffuse_samples_3d": 2,
            "denoise_enabled": False,
        }
    )
    request.setdefault("inspection", {}).update(
        {
            "object_audit_enabled": True,
            "object_audit_max_dimension": 64,
            "transmission_samples_3d": 2,
            "secondary_diffuse_samples_3d": 2,
        }
    )
    request.setdefault("volume", {}).update(
        {
            "enabled": True,
            "source_kind": "scene_bundle",
            "source_path": str(bundle_path),
            "visible": False,
            "affects_lighting": False,
            "debug_overlay": False,
        }
    )
    request["output"] = {"root": str(render_root), "overwrite": True}
    request["progress"] = {
        "summary_path": str(render_root / "render_summary.json"),
        "progress_path": str(render_root / "render_progress.json"),
    }
    return request


def run_cli(cli: Path, request: Path, mode: str, summary: Path) -> float:
    summary.parent.mkdir(parents=True, exist_ok=True)
    stderr_path = summary.with_suffix(".stderr.txt")
    start = time.perf_counter()
    result = subprocess.run(
        [str(cli), "--request", str(request), mode, "--summary", str(summary)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env={**os.environ, "RAY_TRACING_RENDER_TRACE_COST_LEDGER": "1"},
    )
    stderr_path.write_text(result.stderr or "", encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(
            f"{mode} failed for {request} with exit {result.returncode}: {result.stderr.strip()}"
        )
    return time.perf_counter() - start


def assert_summary(summary: dict, unified: bool) -> dict:
    water = summary.get("water_surface", {})
    body = water.get("water_body", {})
    if not water.get("source_found") or not water.get("loaded") or not water.get("mesh_attached"):
        raise ValueError("water surface was not loaded and attached")
    if unified:
        topology = body.get("topology", {})
        expected = {
            "boundary_contract_present": True,
            "active": True,
            "legacy_shell_suppressed": True,
            "material_parity_valid": True,
        }
        for key, value in expected.items():
            if body.get(key) != value:
                raise ValueError(f"unified water summary {key} != {value}")
        if not topology.get("valid") or topology.get("components") != 1:
            raise ValueError("unified water topology is not one valid component")
        if topology.get("boundary_edges") != 0 or topology.get("nonmanifold_edges") != 0:
            raise ValueError("unified water topology is open or nonmanifold")
        if topology.get("max_seam_error_m", 1.0) > 1e-6:
            raise ValueError("unified water seam exceeds 1e-6 m")
    elif body.get("boundary_contract_present") or body.get("active"):
        raise ValueError("legacy control unexpectedly activated unified water")
    return body


def convert_first_frame(summary: dict, output_path: Path) -> Path:
    frame = diagnostic.first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame)
    review_artifacts.write_png_rgb(output_path, width, height, pixels)
    return frame


def render_existing_matrix(args: argparse.Namespace, output_root: Path, cli: Path) -> int:
    report_path = output_root / "aquarium_w2_visual_matrix_report.json"
    if not report_path.is_file():
        raise FileNotFoundError(f"prepared matrix report missing: {report_path}")
    report = load_json(report_path)
    for entry in report.get("variants", []):
        variant_id = entry["id"]
        variant_root = output_root / variant_id
        request_path = Path(entry["request_path"])
        render_summary_path = variant_root / "render" / "render_summary.json"
        entry["render_seconds"] = run_cli(
            cli, request_path, "--render", render_summary_path
        )
        render_summary = load_json(render_summary_path)
        entry["water_body"] = assert_summary(render_summary, bool(entry["unified"]))
        png_path = variant_root / f"{variant_id}.png"
        entry["source_frame_path"] = str(convert_first_frame(render_summary, png_path))
        entry["png_path"] = str(png_path)
        entry["render_summary_path"] = str(render_summary_path)
    report["render_completed_at_utc"] = datetime.now(timezone.utc).isoformat()
    report["render_complete"] = True
    write_json(report_path, report)
    print(report_path)
    return 0


def main() -> int:
    args = parse_args()
    submit_path = args.submit_payload.resolve()
    output_root = args.output_root.resolve()
    cli = args.cli.resolve()
    if not submit_path.is_file():
        raise FileNotFoundError(f"accepted Source-A payload missing: {submit_path}")
    if not cli.is_file():
        raise FileNotFoundError(f"headless CLI missing: {cli}")
    if args.width <= 0 or args.height <= 0:
        raise ValueError("render dimensions must be positive")
    if args.render_existing:
        return render_existing_matrix(args, output_root, cli)

    submit = load_json(submit_path)
    if submit.get("job_id") != SOURCE_JOB_ID:
        raise ValueError("Source-A job ID drifted")
    files = payload_map(submit)
    base_scene = json.loads(require_embedded(files, "scene_runtime.json"))
    base_manifest = json.loads(
        require_embedded(files, "assets/physics/water_basin/water_manifest_v1.json")
    )
    base_request = json.loads(require_embedded(files, "ray_tracing_request.json"))

    if output_root.exists():
        shutil.rmtree(output_root)
    source_root = output_root / "source_a_frame_200_candidate"
    source_root.mkdir(parents=True)
    materialize_source(files, source_root)
    contract = water_body_contract(base_manifest)

    legacy_manifest = copy.deepcopy(base_manifest)
    legacy_manifest["frames"] = [
        frame for frame in legacy_manifest.get("frames", []) if frame.get("frame_index") == FRAME_INDEX
    ]
    legacy_manifest["frame_count"] = 1
    unified_manifest = copy.deepcopy(legacy_manifest)
    unified_manifest["water_body_boundary_v1"] = contract

    bundle_base = json.loads(
        require_embedded(files, "assets/physics/water_basin/scene_bundle.json")
    )
    report = {
        "schema": "ray_tracing_aquarium_w2_visual_matrix_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "promotion_state": "non_promoted_local_candidate",
        "source": {
            "item_id": SOURCE_ITEM_ID,
            "job_id": SOURCE_JOB_ID,
            "frame_index": FRAME_INDEX,
            "submit_payload": str(submit_path),
            "submit_payload_sha256": sha256(submit_path),
        },
        "candidate_contract": contract,
        "render": {"width": args.width, "height": args.height},
        "variants": [],
    }

    for variant in VARIANTS:
        variant_root = output_root / variant["id"]
        render_root = variant_root / "render"
        scene = copy.deepcopy(base_scene)
        scene["scene_id"] = f"aquarium_w2_{variant['id']}_frame_{FRAME_INDEX}"
        remove_focus_excluded_objects(scene)
        if variant["benchy"]:
            set_visible(scene, BENCHY_ID, True)
        else:
            scene["objects"] = [
                obj for obj in scene.get("objects", []) if obj.get("object_id") != BENCHY_ID
            ]
        set_glass_mode(scene, variant["glass_mode"])
        scene_path = source_root / f"scene_{variant['id']}.json"
        write_json(scene_path, scene)

        water_root = variant_root / "water_source"
        water_root.mkdir(parents=True, exist_ok=True)
        manifest_path = water_root / "water_manifest_v1.json"
        write_json(manifest_path, unified_manifest if variant["unified"] else legacy_manifest)
        shutil.copy2(
            source_root / "assets/physics/water_basin/water_surface_000200.json",
            water_root / "water_surface_000200.json",
        )
        bundle = copy.deepcopy(bundle_base)
        # The bundle contract owns a manifest path relative to its own directory.
        water_source = bundle.get("water_source")
        if not isinstance(water_source, dict) or water_source.get("kind") != "water_manifest":
            raise ValueError("accepted scene bundle has no water_manifest source")
        water_source["path"] = manifest_path.name
        bundle_path = water_root / "scene_bundle.json"
        write_json(bundle_path, bundle)

        request = build_request(
            base_request,
            scene_path,
            bundle_path,
            render_root,
            variant,
            args.width,
            args.height,
        )
        request_path = variant_root / "request.json"
        write_json(request_path, request)
        preflight_path = variant_root / "preflight_summary.json"
        preflight_seconds = run_cli(cli, request_path, "--preflight", preflight_path)
        preflight = load_json(preflight_path)
        body = assert_summary(preflight, variant["unified"])
        entry = {
            **variant,
            "request_path": str(request_path),
            "scene_path": str(scene_path),
            "manifest_path": str(manifest_path),
            "preflight_summary_path": str(preflight_path),
            "preflight_seconds": preflight_seconds,
            "water_body": body,
        }
        if not args.prepare_only:
            render_summary_path = render_root / "render_summary.json"
            entry["render_seconds"] = run_cli(cli, request_path, "--render", render_summary_path)
            render_summary = load_json(render_summary_path)
            entry["water_body"] = assert_summary(render_summary, variant["unified"])
            png_path = variant_root / f"{variant['id']}.png"
            entry["source_frame_path"] = str(convert_first_frame(render_summary, png_path))
            entry["png_path"] = str(png_path)
            entry["render_summary_path"] = str(render_summary_path)
        report["variants"].append(entry)

    report["passed"] = True
    report_path = output_root / "aquarium_w2_visual_matrix_report.json"
    write_json(report_path, report)
    print(report_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
