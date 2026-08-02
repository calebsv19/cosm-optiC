#!/usr/bin/env python3
"""Build and render a local unified-water plus VF3D aquarium preview."""

from __future__ import annotations

import argparse
import base64
import copy
import hashlib
import json
import platform
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
WORKSPACE_ROOT = REPO_ROOT.parent.parent
for module_root in (
    WORKSPACE_ROOT / "bin",
    REPO_ROOT / "tools",
    REPO_ROOT / "tests" / "integration",
    SCRIPT_DIR,
):
    if str(module_root) not in sys.path:
        sys.path.insert(0, str(module_root))

from vps_worker_job_queue_lib.clone_edit import rewrite_payload_and_apply_edits  # noqa: E402
from scene_iteration_layout import (  # noqa: E402
    attachment_layout_warnings,
    resolve_layout_edit,
    scene_layout_bounds,
)
from vf3d_initial_state_preset_tool import generate_preset  # noqa: E402
from vf3d_scene_atmosphere_resolver import resolve_transform_spec  # noqa: E402
import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import run_aquarium_w2_visual_matrix as w2  # noqa: E402
import run_ray_tracing_aquarium_real_shell_transparent_receiver_diagnostic as diagnostic  # noqa: E402


DEFAULT_OUTPUT_ROOT = (
    WORKSPACE_ROOT
    / "_private_workspace_artifacts"
    / "agent_runs"
    / "ray_tracing"
    / "aquarium_w2_vf3d_rear_stl_preview_20260719"
)
DEFAULT_CLI = (
    REPO_ROOT
    / "build"
    / "toolchains"
    / "clang"
    / platform.machine()
    / "tools"
    / "cli"
    / "ray_tracing_render_headless"
)
ATMOSPHERE_PROFILE = {
    "volume_density_scale": 0.78,
    "volume_density_gamma": 0.58,
    "volume_scatter_gain": 3.8,
    "volume_absorption_gain": 0.0,
    "volume_opacity_clamp": 1.6,
    "volume_step_scale": 0.68,
    "volume_tint.r": 0.92,
    "volume_tint.g": 0.96,
    "volume_tint.b": 1.0,
}
CENTERED_CAMERA_PROFILE = {
    "camera_position": [0.0, -4.15, 1.78],
    "camera_look_at": [0.0, 0.18, 1.08],
    "camera_zoom": 0.96,
}
REAR_STL_PLACEMENTS = {
    "stanford_dragon_room_stress_mesh": {
        "position": [0.0, 0.0, 0.0],
        "desired_center": [-0.92, 1.72],
        "rotation": [90.0, 0.0, 18.0],
        "scale": [7.5, 7.5, 7.5],
        "color": 0x22D3EE,
    },
    "stanford_bunny_room_plinth": {
        "position": [0.0, 0.0, 0.0],
        "desired_center": [0.92, 1.72],
        "rotation": [90.0, 0.0, -12.0],
        "scale": [8.0, 8.0, 8.0],
        "color": 0xFF3DAE,
    },
}
SOLID_COLOR_STACK = {
    "layers": [{
        "blend": "replace",
        "enabled": True,
        "id": "agent_base",
        "kind": "solid",
        "name": "Rear Refraction Color",
        "opacity": 1.0,
        "parameters": {
            "color_depth": 0.82,
            "contrast": 0.72,
            "coverage": 1.0,
            "edge_softness": 0.4,
            "flow": 0.0,
            "grain": 0.12,
            "pattern_mode": 0,
            "seed": 217,
            "surface_damage": 0.04,
        },
        "placement": {"scale": 1.0, "strength": 1.0},
    }],
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--submit-payload", type=Path, default=w2.default_submit_payload())
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--cli", type=Path, default=DEFAULT_CLI)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument("--transmission-samples", type=int, default=3)
    parser.add_argument("--secondary-samples", type=int, default=3)
    parser.add_argument("--temporal-frames", type=int, default=1)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--render-existing", action="store_true")
    parser.add_argument("--skip-preflight", action="store_true")
    return parser.parse_args()


def payload_entries(payload: dict) -> dict[str, dict]:
    return {
        str(entry.get("relpath") or ""): entry
        for entry in payload.get("payload_files", [])
        if isinstance(entry, dict) and entry.get("relpath")
    }


def embedded_json(payload: dict, relpath: str) -> dict:
    entry = payload_entries(payload).get(relpath)
    if entry is None or not isinstance(entry.get("content_utf8"), str):
        raise ValueError(f"payload JSON file missing: {relpath}")
    return json.loads(entry["content_utf8"])


def replace_embedded_json(payload: dict, relpath: str, doc: dict) -> None:
    entry = payload_entries(payload).get(relpath)
    if entry is None:
        raise ValueError(f"payload file missing: {relpath}")
    entry["content_utf8"] = json.dumps(doc, indent=2, sort_keys=True) + "\n"
    entry.pop("content_base64", None)


def materialize_payload(payload: dict, destination: Path) -> None:
    for relpath, entry in payload_entries(payload).items():
        target = destination / relpath
        target.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(entry.get("content_utf8"), str):
            target.write_text(entry["content_utf8"], encoding="utf-8")
        elif isinstance(entry.get("content_base64"), str):
            target.write_bytes(base64.b64decode(entry["content_base64"]))
        else:
            raise ValueError(f"payload file has no supported content: {relpath}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_cli(cli: Path, request_path: Path, mode: str, summary_path: Path) -> None:
    result = subprocess.run(
        [str(cli), "--request", str(request_path), mode, "--summary", str(summary_path)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    summary_path.with_suffix(".stderr.txt").write_text(result.stderr or "", encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(f"{mode} failed with exit {result.returncode}: {result.stderr.strip()}")


def assert_composite_summary(summary: dict) -> None:
    body = summary.get("water_surface", {}).get("water_body", {})
    topology = body.get("topology", {})
    if not body.get("active") or not topology.get("valid"):
        raise ValueError("unified water body is not active and valid")
    if topology.get("components") != 1:
        raise ValueError("unified water body is not one component")
    if topology.get("boundary_edges") != 0 or topology.get("nonmanifold_edges") != 0:
        raise ValueError("unified water body is open or nonmanifold")
    if not summary.get("volume_attached") or not summary.get("volume_visible"):
        raise ValueError("VF3D volume did not attach visibly")
    if not summary.get("volume_summary_built"):
        raise ValueError("VF3D volume summary was not built")


def placement_proof(scene: dict, resolved: dict, generated: dict) -> dict:
    domain = scene.get("extensions", {}).get("physics_sim", {}).get("scene_domain", {})
    domain_min = domain.get("min", {})
    domain_max = domain.get("max", {})
    if not domain.get("active"):
        raise ValueError("aquarium PhysicsSim scene domain is not active")
    tank_bounds = {
        "min": [float(domain_min[axis]) for axis in ("x", "y", "z")],
        "max": [float(domain_max[axis]) for axis in ("x", "y", "z")],
    }
    atmosphere_bounds = resolved["resolved_bounds"]
    clearance = float(atmosphere_bounds["min"][2]) - tank_bounds["max"][2]
    spans_tank_xy = (
        float(atmosphere_bounds["min"][0]) <= tank_bounds["min"][0]
        and float(atmosphere_bounds["max"][0]) >= tank_bounds["max"][0]
        and float(atmosphere_bounds["min"][1]) <= tank_bounds["min"][1]
        and float(atmosphere_bounds["max"][1]) >= tank_bounds["max"][1]
    )
    if clearance < 0.01:
        raise ValueError(f"VF3D bounds overlap aquarium cavity: clearance={clearance}")
    if not spans_tank_xy:
        raise ValueError("VF3D bounds do not span the aquarium footprint")
    distribution = generated["warmup_preview"]["output_density_distribution"]
    return {
        "verdict": "cloud_support_above_and_spanning_aquarium",
        "tank_cavity_bounds": tank_bounds,
        "atmosphere_bounds": atmosphere_bounds,
        "vertical_clearance_m": clearance,
        "spans_tank_xy": spans_tank_xy,
        "active_density_cells": generated["active_density_cells"],
        "active_density_fraction": generated["active_density_fraction"],
        "density_stats": generated["density_stats"],
        "density_center_of_mass_world": distribution["center_of_mass_world"],
        "max_density_world": distribution["max_density_world"],
        "density_projection_paths": generated["projection_paths"],
    }


def object_by_id(scene: dict, object_id: str) -> dict:
    return next(obj for obj in scene["objects"] if obj.get("object_id") == object_id)


def rear_stl_placement_proof(payload: dict, layout_audits: list[dict]) -> dict:
    scene = embedded_json(payload, "scene_runtime.json")
    objects = {obj["object_id"]: obj for obj in scene["objects"] if obj.get("object_id")}
    bounds_by_id = scene_layout_bounds(scene, payload)
    floor_bounds = bounds_by_id["room_floor"]
    proofs = {}
    for object_id in REAR_STL_PLACEMENTS:
        obj = objects[object_id]
        bounds = bounds_by_id[object_id]
        bounds_min = [float(bounds["min"][axis]) for axis in ("x", "y", "z")]
        bounds_max = [float(bounds["max"][axis]) for axis in ("x", "y", "z")]
        warnings = attachment_layout_warnings(
            objects=objects,
            bounds_by_id=bounds_by_id,
            object_id=object_id,
            target_id="room_floor",
            axis="z",
            clearance=0.0,
            source_final_bounds=bounds,
            target_bounds=floor_bounds,
        )
        behind_aquarium = bounds_min[1] >= 1.06
        in_front_of_back_wall = bounds_max[1] <= 2.85
        within_projected_tank_x = bounds_min[0] >= -1.91 and bounds_max[0] <= 1.91
        floor_supported = abs(bounds_min[2] - float(floor_bounds["max"]["z"])) <= 1e-6
        upright = (
            abs(float(obj["transform"]["rotation"]["x"]) - 90.0) <= 1e-9
            and abs(float(obj["transform"]["rotation"]["y"])) <= 1e-9
            and bounds_max[2] - bounds_min[2] >= 1.0
        )
        pivot_aware = obj["transform"].get("pivot_policy") == "bounds_center"
        if not all((behind_aquarium, in_front_of_back_wall, within_projected_tank_x, floor_supported)):
            raise ValueError(f"invalid rear STL placement for {object_id}: {bounds_min} .. {bounds_max}")
        if not upright or not pivot_aware or warnings:
            raise ValueError(
                f"rear STL orientation/collision failure for {object_id}: "
                f"upright={upright} pivot_aware={pivot_aware} warnings={warnings}"
            )
        proofs[object_id] = {
            "asset_id": obj["geometry_ref"]["id"],
            "world_bounds": {"min": bounds_min, "max": bounds_max},
            "world_center": [float(bounds["center"][axis]) for axis in ("x", "y", "z")],
            "behind_aquarium": behind_aquarium,
            "in_front_of_back_wall": in_front_of_back_wall,
            "within_projected_tank_x": within_projected_tank_x,
            "floor_supported": floor_supported,
            "upright": upright,
            "pivot_policy": bounds["pivot_policy"],
            "collision_warnings": warnings,
            "color_rgb24": REAR_STL_PLACEMENTS[object_id]["color"],
        }
    return {
        "verdict": "rear_stls_upright_supported_clear_and_in_refraction_band",
        "layout_audits": layout_audits,
        "objects": proofs,
    }


def prepare(args: argparse.Namespace) -> tuple[Path, Path]:
    submit_path = args.submit_payload.resolve()
    output_root = args.output_root.resolve()
    if output_root.exists():
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True)
    typed_payload_path = output_root / "typed_edit" / "submit_payload.json"
    typed_payload_path.parent.mkdir(parents=True)
    shutil.copy2(submit_path, typed_payload_path)

    payload = json.loads(typed_payload_path.read_text(encoding="utf-8"))
    scene = embedded_json(payload, "scene_runtime.json")
    scene["scene_id"] = "aquarium_unified_water_vf3d_rear_stl_preview"
    scene["objects"] = [
        obj for obj in scene.get("objects", []) if obj.get("object_id") != "waterline_front_preview"
    ]
    manifest_relpath = "assets/physics/water_basin/water_manifest_v1.json"
    manifest = embedded_json(payload, manifest_relpath)
    manifest["water_body_boundary_v1"] = w2.water_body_contract(manifest)
    replace_embedded_json(payload, "scene_runtime.json", scene)
    replace_embedded_json(payload, manifest_relpath, manifest)
    typed_payload_path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    resolver_scene_path = output_root / "resolver_scene_runtime.json"
    w2.write_json(resolver_scene_path, scene)
    resolved = resolve_transform_spec(
        resolver_scene_path,
        output_root=output_root / "atmosphere_generation",
        seed=12,
        preset="mist_patch_v1",
        base_mode="small_patch",
        size_scale=[0.32, 0.31, 0.50],
        center_offset=[0.0, -0.08, 0.81],
        rotation_degrees=[0.0, 0.0, 8.0],
        density_scale=1.25,
        run_suffix="aquarium_visible_cloud_v2",
    )
    generated = generate_preset(w2.load_json(Path(resolved["spec_path"])), render=False)
    placement = placement_proof(scene, resolved, generated)
    atmosphere_bundle = Path(generated["scene_bundle_path"])
    edit_audit = rewrite_payload_and_apply_edits(
        typed_payload_path,
        replacements={},
        camera_position=",".join(str(value) for value in CENTERED_CAMERA_PROFILE["camera_position"]),
        camera_look_at=",".join(str(value) for value in CENTERED_CAMERA_PROFILE["camera_look_at"]),
        camera_zoom=str(CENTERED_CAMERA_PROFILE["camera_zoom"]),
        object_material_edits=[
            f"{object_id}:object_color={placement['color']}"
            for object_id, placement in REAR_STL_PLACEMENTS.items()
        ] + [
            f"{object_id}:roughness=0.2" for object_id in REAR_STL_PLACEMENTS
        ] + [
            f"{object_id}:reflectivity=0.24" for object_id in REAR_STL_PLACEMENTS
        ] + [
            f"{object_id}:emissive_strength=0.12" for object_id in REAR_STL_PLACEMENTS
        ],
        object_material_stacks={
            object_id: copy.deepcopy(SOLID_COLOR_STACK) for object_id in REAR_STL_PLACEMENTS
        },
        object_transform_edits=[
            f"{object_id}:{group}.{axis}={values[index]}"
            for object_id, placement in REAR_STL_PLACEMENTS.items()
            for group in ("position", "rotation", "scale")
            for index, axis in enumerate(("x", "y", "z"))
            for values in (placement[group],)
        ],
        atmosphere_edits=[
            (
                "preset=mist_patch_v1,bounds_mode=small_patch,seed=12,"
                f"scene_bundle={atmosphere_bundle},render_profile=aquarium_elevated_ambient_cloud"
            )
        ],
        render_edits=[
            f"width={args.width}",
            f"height={args.height}",
            "frame_count=1",
            "start_frame=200",
            f"temporal_frames={args.temporal_frames}",
            "normalized_t=0",
        ],
    )

    payload = json.loads(typed_payload_path.read_text(encoding="utf-8"))
    scene = embedded_json(payload, "scene_runtime.json")
    bounds_by_id = scene_layout_bounds(scene, payload)
    layout_transform_flags = []
    layout_audits = []
    for object_id, placement_spec in REAR_STL_PLACEMENTS.items():
        bounds = bounds_by_id[object_id]
        for axis, desired_center in zip(("x", "y"), placement_spec["desired_center"]):
            new_position = (
                float(bounds["position"][axis])
                + float(desired_center)
                - float(bounds["center"][axis])
            )
            layout_transform_flags.append(f"{object_id}:position.{axis}={new_position:.12g}")
        floor_flags, floor_audit = resolve_layout_edit(
            {
                "kind": "layout_place_on_plane",
                "object_id": object_id,
                "target_object_id": "room_floor",
                "axis": "z",
                "anchor": "min",
                "target_anchor": "max",
                "offset": 0.0,
                "source_clause": f"place-on-plane {object_id} plane=room_floor clearance=0",
            },
            scene,
            payload,
        )
        layout_transform_flags.extend(floor_flags)
        layout_audits.append(floor_audit)
    layout_edit_audit = rewrite_payload_and_apply_edits(
        typed_payload_path,
        replacements={},
        object_material_edits=[],
        object_transform_edits=layout_transform_flags,
    )

    payload = json.loads(typed_payload_path.read_text(encoding="utf-8"))
    object_placement = rear_stl_placement_proof(payload, layout_audits)
    extracted_root = output_root / "payload"
    materialize_payload(payload, extracted_root)
    request_path = extracted_root / "ray_tracing_request.json"
    request = w2.load_json(request_path)
    volume_relpath = str(request["volume"]["source_path"])
    render_root = output_root / "render"
    request["run_id"] = "aquarium_unified_water_vf3d_rear_stls_frame_200_preview"
    request["scene"] = {"runtime_scene_path": str(extracted_root / "scene_runtime.json")}
    request["volume"]["source_path"] = str(extracted_root / volume_relpath)
    request.setdefault("inspection", {}).update(ATMOSPHERE_PROFILE)
    request["inspection"].update({
        "trace_route": "flattened_bvh",
        "transmission_samples_3d": args.transmission_samples,
        "secondary_diffuse_samples_3d": args.secondary_samples,
        "object_audit_enabled": True,
        "object_audit_max_dimension": 64,
    })
    request["output"] = {"root": str(render_root), "overwrite": True}
    request["progress"] = {
        "summary_path": str(render_root / "render_summary.json"),
        "progress_path": str(render_root / "render_progress.json"),
    }
    w2.write_json(request_path, request)
    composite_bundle = embedded_json(payload, volume_relpath)
    evidence = {
        "schema": "aquarium_unified_water_vf3d_composite_preview_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "promotion_eligible": False,
        "remote_submit": False,
        "latest_good_changed": False,
        "source_submit_payload": str(submit_path),
        "source_submit_payload_sha256": sha256(submit_path),
        "typed_edit_payload": str(typed_payload_path),
        "typed_edit_audit": edit_audit,
        "layout_transform_flags": layout_transform_flags,
        "layout_edit_audit": layout_edit_audit,
        "composite_bundle_relpath": volume_relpath,
        "composite_bundle": composite_bundle,
        "water_body_boundary_v1": manifest["water_body_boundary_v1"],
        "atmosphere_transform": resolved["transform"],
        "atmosphere_bounds": resolved["resolved_bounds"],
        "placement_proof": placement,
        "rear_stl_placement_proof": object_placement,
        "camera_profile": CENTERED_CAMERA_PROFILE,
        "atmosphere_profile": ATMOSPHERE_PROFILE,
        "request_path": str(request_path),
        "render_root": str(render_root),
    }
    w2.write_json(output_root / "composite_preview_evidence.json", evidence)
    return request_path, output_root


def render(args: argparse.Namespace, request_path: Path, output_root: Path) -> None:
    if args.prepare_only:
        print(output_root / "composite_preview_evidence.json")
        return
    cli = args.cli.resolve()
    preflight_path = output_root / "preflight_summary.json"
    if not args.skip_preflight and not (args.render_existing and preflight_path.is_file()):
        run_cli(cli, request_path, "--preflight", preflight_path)
    if not args.skip_preflight:
        assert_composite_summary(w2.load_json(preflight_path))
    summary_path = output_root / "render" / "render_summary.json"
    run_cli(cli, request_path, "--render", summary_path)
    summary = w2.load_json(summary_path)
    assert_composite_summary(summary)
    frame = diagnostic.first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame)
    png_path = output_root / "aquarium_unified_water_vf3d_rear_stls_preview.png"
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    evidence_path = output_root / "composite_preview_evidence.json"
    evidence = w2.load_json(evidence_path)
    evidence.update({
        "completed_at_utc": datetime.now(timezone.utc).isoformat(),
        "render_summary_path": str(summary_path),
        "render_progress_path": str(output_root / "render" / "render_progress.json"),
        "preview_png_path": str(png_path),
        "preview_png_sha256": sha256(png_path),
        "water_body_readback": summary["water_surface"]["water_body"],
        "volume_summary": summary.get("volume_summary"),
        "render_complete": True,
    })
    w2.write_json(evidence_path, evidence)
    print(png_path)


def main() -> int:
    args = parse_args()
    if args.render_existing:
        output_root = args.output_root.resolve()
        request_path = output_root / "payload" / "ray_tracing_request.json"
    else:
        request_path, output_root = prepare(args)
    render(args, request_path, output_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
