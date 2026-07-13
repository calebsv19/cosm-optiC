#!/usr/bin/env python3
"""Run a compact PPM-10 off/reference/production caustic product A/B fixture."""

from __future__ import annotations

import argparse
import copy
import json
import platform
import shutil
import struct
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402


VARIANTS = (
    {
        "id": "off",
        "product_mode": "off",
        "render_contribution": False,
        "description": "no caustic product",
    },
    {
        "id": "reference",
        "product_mode": "reference",
        "render_contribution": False,
        "description": "exploratory lens transport reference",
    },
    {
        "id": "production",
        "product_mode": "production",
        "render_contribution": True,
        "populated_callsite": False,
        "description": "opt-in production photon-map route",
    },
    {
        "id": "production_populated",
        "product_mode": "production",
        "render_contribution": True,
        "populated_callsite": True,
        "description": "opt-in production route with populated callsite count readback",
    },
    {
        "id": "production_trace_populated",
        "product_mode": "production",
        "render_contribution": True,
        "populated_callsite": True,
        "trace_populated_callsite": True,
        "description": "opt-in production route with trace-record populated callsite count readback",
    },
    {
        "id": "production_render_prep_populated",
        "product_mode": "production",
        "render_contribution": True,
        "render_prep_population": True,
        "integrator_3d": "disney_v2",
        "description": "opt-in production route populated during native render prep",
    },
    {
        "id": "production_render_prep_wall_populated",
        "product_mode": "production",
        "render_contribution": True,
        "render_prep_population": True,
        "integrator_3d": "disney_v2",
        "scene_variant": "vertical_receiver_wall",
        "description": "opt-in production render-prep population on a vertical receiver wall",
    },
    {
        "id": "production_render_prep_volume_populated",
        "product_mode": "production",
        "render_contribution": True,
        "render_prep_population": True,
        "volume_render_prep": True,
        "integrator_3d": "disney_v2",
        "description": "opt-in render-prep beam-map contribution into a sampleable VF3D volume",
    },
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = (
        root
        / "build"
        / "toolchains"
        / "clang"
        / machine
        / "tools"
        / "cli"
        / "ray_tracing_render_headless"
    )
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def default_review_root(root: Path) -> Path:
    return root / "build" / "agent_runs" / "ray_tracing" / "ppm10_product_ab_fixture"


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--review-root", type=Path, default=default_review_root(root))
    parser.add_argument("--preflight-only", action="store_true")
    return parser.parse_args()


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def reset_review_root(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)


def write_uniform_vf3d(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    w = h = d = 16
    cell_count = w * h * d
    magic = (ord("V") << 24) | (ord("F") << 16) | (ord("3") << 8) | ord("D")
    header = struct.pack(
        "@IIIIIdQdfffffffIIII",
        magic, 1, w, h, d, 0.0, 0, 1.0 / 24.0,
        -10.0, -10.0, -10.0, 20.0 / float(w), 0.0, 0.0, 1.0, 0, 0, 0, 0,
    )
    if struct.calcsize("@IIIIIdQdfffffffIIII") != 92:
        raise RuntimeError("unexpected native vf3d header packing")
    density = [0.18] * cell_count
    zero_float = [0.0] * cell_count
    with path.open("wb") as f:
        f.write(header)
        f.write(b"\0\0\0\0")
        for channel in (density, zero_float, zero_float, zero_float, zero_float):
            f.write(struct.pack(f"@{cell_count}f", *channel))
        f.write(bytes(cell_count))


def resolved_scene_path(base_request_path: Path, base_request: dict) -> str:
    scene_value = base_request["scene"]["runtime_scene_path"]
    scene_path = Path(scene_value)
    if not scene_path.is_absolute():
        scene_path = (base_request_path.parent / scene_path).resolve()
    return str(scene_path)


def scene_path_for_variant(
    review_root: Path,
    base_request_path: Path,
    base_request: dict,
    variant: dict,
) -> str:
    scene_path = Path(resolved_scene_path(base_request_path, base_request))
    if variant.get("scene_variant") != "vertical_receiver_wall":
        return str(scene_path)

    scene = copy.deepcopy(load_json(scene_path))
    scene["scene_id"] = "caustic_probe_glass_sphere_vertical_receiver_wall"
    mesh_path = (
        scene_path.parent
        / "../mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_256x128.runtime.json"
    ).resolve()
    receiver = scene["objects"][0]
    receiver["object_id"] = "receiver_wall"
    receiver["locked_plane"] = "xz"
    receiver["primitive"]["width"] = 6.0
    receiver["primitive"]["height"] = 6.0
    receiver["primitive"]["frame"] = {
        "origin": {"x": 0.0, "y": 1.4, "z": 0.75},
        "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
        "axis_v": {"x": 0.0, "y": 0.0, "z": 1.0},
        "normal": {"x": 0.0, "y": -1.0, "z": 0.0},
    }
    scene["cameras"][0]["position"] = {"x": 0.0, "y": -5.2, "z": 1.9}
    scene["cameras"][0]["target"] = {"x": 0.0, "y": 1.0, "z": 0.75}
    scene["objects"][1]["extensions"]["line_drawing"]["runtime_mesh_path"] = str(
        mesh_path
    )
    authoring = scene["extensions"]["ray_tracing"]["authoring"]
    authoring["camera_focus_target"] = {"x": 0.0, "y": 1.0, "z": 0.75}
    for material in authoring.get("object_materials", []):
        if material.get("object_id") == "receiver_floor":
            material["object_id"] = "receiver_wall"
            material["object_color"] = 7434868
    variant_scene_path = review_root / "scenes" / "scene_vertical_receiver_wall.json"
    write_json(variant_scene_path, scene)
    return str(variant_scene_path)


def request_for_variant(root: Path, review_root: Path, variant: dict) -> dict:
    base_request_path = (
        root
        / "tests"
        / "fixtures"
        / "caustic_probe_glass_sphere"
        / "request_direct_light.json"
    )
    request = load_json(base_request_path)
    run_id = f"ppm10_product_ab_fixture_{variant['id']}"
    output_root = review_root / "runs" / variant["id"]
    request["run_id"] = run_id
    request["scene"]["runtime_scene_path"] = scene_path_for_variant(
        review_root,
        base_request_path,
        request,
        variant,
    )
    request["render"]["width"] = 160
    request["render"]["height"] = 96
    request["render"]["frame_count"] = 1
    request["render"]["temporal_frames"] = 1
    if variant.get("integrator_3d"):
        request["render"]["integrator_3d"] = variant["integrator_3d"]
    if variant.get("volume_render_prep"):
        volume_path = review_root / "generated_volume" / "ppm18_uniform_volume.vf3d"
        write_uniform_vf3d(volume_path)
        request["volume"] = {
            "enabled": True,
            "source_kind": "raw_vf3d",
            "source_path": str(volume_path),
            "affects_lighting": True,
            "debug_overlay": False,
        }
    if variant.get("scene_variant") == "vertical_receiver_wall":
        request.setdefault("inspection", {}).update({
            "camera_position": {"x": 0.0, "y": -5.2, "z": 1.9},
            "camera_look_at": {"x": 0.0, "y": 1.0, "z": 0.75},
        })
    request.setdefault("inspection", {}).update({
        "caustic_product_mode": variant["product_mode"],
        "caustic_photon_render_contribution_enabled": variant["render_contribution"],
        "caustic_surface_query_enabled": True,
        "caustic_volume_query_enabled": bool(
            variant.get("trace_populated_callsite", False)
            or variant.get("volume_render_prep", False)
        ),
        "caustic_photon_sample_budget": 64,
        "caustic_photon_max_path_depth": 3,
        "caustic_photon_surface_query_radius": 0.20,
        "caustic_photon_volume_query_radius": (
            1.50 if variant.get("volume_render_prep", False) else 0.20
        ),
        "caustic_photon_surface_radiance_scale": 1.0,
        "caustic_photon_render_prep_population_enabled": bool(
            variant.get("render_prep_population", False)
        ),
        "trace_route": "flattened_bvh",
        "caustic_photon_populated_callsite_readback_enabled": bool(
            variant.get("populated_callsite", False)
            and not variant.get("trace_populated_callsite", False)
        ),
        "caustic_photon_trace_populated_callsite_readback_enabled": bool(
            variant.get("trace_populated_callsite", False)
        ),
        "caustic_sidecar_enabled": not variant.get("volume_render_prep", False),
    })
    request["output"] = {
        "root": str(output_root),
        "overwrite": True,
    }
    request["progress"] = {
        "summary_path": str(output_root / "render_summary.json"),
        "progress_path": str(output_root / "render_progress.json"),
    }
    request_path = review_root / "requests" / f"{run_id}.json"
    write_json(request_path, request)
    return {
        "id": variant["id"],
        "request_path": request_path,
        "summary_path": output_root / "render_summary.json",
        "stdout_path": output_root / "stdout_summary.json",
        "stderr_path": output_root / "stderr.txt",
    }


def run_request(cli: Path, run: dict, preflight_only: bool) -> float:
    command = [
        str(cli),
        "--request",
        str(run["request_path"]),
        "--summary",
        str(run["summary_path"]),
    ]
    command.append("--preflight" if preflight_only else "--render")
    run["summary_path"].parent.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    with run["stdout_path"].open("w", encoding="utf-8") as stdout:
        result = subprocess.run(
            command,
            stdout=stdout,
            stderr=subprocess.PIPE,
            text=True,
        )
    elapsed = time.perf_counter() - start
    run["stderr_path"].write_text(result.stderr or "", encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(
            f"PPM-10 fixture cell {run['id']} failed with {result.returncode}; "
            f"stderr: {run['stderr_path']}"
        )
    return elapsed


def nested(summary: dict, *keys: str, default=None):
    value = summary
    for key in keys:
        if not isinstance(value, dict):
            return default
        value = value.get(key)
    return default if value is None else value


def first_frame_path(summary: dict) -> Path | None:
    path_value = nested(summary, "outputs", "first_frame_path", default="")
    if not path_value:
        return None
    path = Path(path_value)
    return path if path.exists() else None


def photon_callsite_counts(summary: dict) -> dict:
    caustic_state = nested(summary, "inspection", "caustic_state", default={})
    callsite = caustic_state.get("photon_callsite", {})
    population = callsite.get("map_population", {})
    lifecycle = callsite.get("map_lifecycle", {})
    radiance = callsite.get("radiance", {})
    return {
        "readback_enabled": nested(
            summary,
            "inspection",
            "caustic_photon_populated_callsite_readback_enabled",
            default=False,
        ),
        "trace_readback_enabled": nested(
            summary,
            "inspection",
            "caustic_photon_trace_populated_callsite_readback_enabled",
            default=False,
        ),
        "render_prep_population_enabled": nested(
            summary,
            "inspection",
            "caustic_photon_render_prep_population_enabled",
            default=False,
        ),
        "readback_built": caustic_state.get("photon_callsite_readback_built", False),
        "route": callsite.get("route", ""),
        "map_lifecycle_evaluated": lifecycle.get("evaluated", False),
        "map_lifecycle_rebuilt": lifecycle.get("rebuilt", False),
        "map_lifecycle_reused": lifecycle.get("reused", False),
        "map_lifecycle_persistent": lifecycle.get(
            "persistent_ownership_enabled", False
        ),
        "map_lifecycle_rebuild_reason": lifecycle.get("rebuild_reason", ""),
        "map_lifecycle_budget_tier": lifecycle.get("budget_tier", ""),
        "map_lifecycle_generation": int(lifecycle.get("generation", 0) or 0),
        "map_lifecycle_rebuild_count": int(
            lifecycle.get("rebuild_count", 0) or 0
        ),
        "map_lifecycle_reuse_count": int(lifecycle.get("reuse_count", 0) or 0),
        "map_lifecycle_fingerprint_cpu_ms": float(
            lifecycle.get("fingerprint_cpu_ms", 0.0) or 0.0
        ),
        "map_lifecycle_map_build_cpu_ms": float(
            lifecycle.get("map_build_cpu_ms", 0.0) or 0.0
        ),
        "map_lifecycle_query_and_deposit_cpu_ms": float(
            lifecycle.get("query_and_deposit_cpu_ms", 0.0) or 0.0
        ),
        "query_attempted": callsite.get("query_attempted", False),
        "query_hit": callsite.get("query_hit", False),
        "contribution_eligible": callsite.get("contribution_eligible", False),
        "cache_deposit_attempted": callsite.get("cache_deposit_attempted", False),
        "surface_deposited": callsite.get("surface_deposited", False),
        "surface_candidate_count": int(callsite.get("surface_candidate_count", 0) or 0),
        "surface_contributing_count": int(
            callsite.get("surface_contributing_count", 0) or 0
        ),
        "volume_contributing_count": int(
            callsite.get("volume_contributing_count", 0) or 0
        ),
        "volume_deposited": callsite.get("volume_deposited", False),
        "beam_contribution_attempted": callsite.get("beam_contribution_attempted", False),
        "beam_contribution_volume_sampleable": callsite.get(
            "beam_contribution_volume_sampleable", False
        ),
        "beam_contribution_beam_map_allocated": callsite.get(
            "beam_contribution_beam_map_allocated", False
        ),
        "beam_contribution_query_hit": callsite.get("beam_contribution_query_hit", False),
        "beam_contribution_eligible": callsite.get("beam_contribution_eligible", False),
        "beam_contribution_volume_deposited": callsite.get(
            "beam_contribution_volume_deposited", False
        ),
        "beam_contribution_query_attempt_count": int(
            callsite.get("beam_contribution_query_attempt_count", 0) or 0
        ),
        "beam_contribution_candidate_count": int(
            callsite.get("beam_contribution_candidate_count", 0) or 0
        ),
        "beam_contribution_contributing_count": int(
            callsite.get("beam_contribution_contributing_count", 0) or 0
        ),
        "beam_contribution_volume_deposit_accepted_count": int(
            callsite.get("beam_contribution_volume_deposit_accepted_count", 0) or 0
        ),
        "beam_contribution_density": float(
            callsite.get("beam_contribution_density", 0.0) or 0.0
        ),
        "beam_contribution_transmittance": float(
            callsite.get("beam_contribution_transmittance", 0.0) or 0.0
        ),
        "receiver_contribution_attempted": callsite.get(
            "receiver_contribution_attempted",
            False,
        ),
        "receiver_contribution_eligible": callsite.get(
            "receiver_contribution_eligible",
            False,
        ),
        "receiver_contribution_bucket_count": int(
            callsite.get("receiver_contribution_bucket_count", 0) or 0
        ),
        "receiver_contribution_query_attempt_count": int(
            callsite.get("receiver_contribution_query_attempt_count", 0) or 0
        ),
        "receiver_contribution_query_hit_count": int(
            callsite.get("receiver_contribution_query_hit_count", 0) or 0
        ),
        "receiver_contribution_surface_deposit_accepted_count": int(
            callsite.get(
                "receiver_contribution_surface_deposit_accepted_count",
                0,
            )
            or 0
        ),
        "receiver_contribution_surface_candidate_count": int(
            callsite.get("receiver_contribution_surface_candidate_count", 0) or 0
        ),
        "receiver_contribution_surface_contributing_count": int(
            callsite.get("receiver_contribution_surface_contributing_count", 0)
            or 0
        ),
        "estimated_cost": int(callsite.get("estimated_cost", 0) or 0),
        "radiance_sum": float(radiance.get("r", 0.0) or 0.0)
        + float(radiance.get("g", 0.0) or 0.0)
        + float(radiance.get("b", 0.0) or 0.0),
        "population_attempted": population.get("attempted", False),
        "population_source": population.get("population_source", ""),
        "surface_map_allocated": population.get("surface_map_allocated", False),
        "emission_attempted": population.get("emission_attempted", False),
        "emission_succeeded": population.get("emission_succeeded", False),
        "surface_map_population_attempted": population.get(
            "surface_map_population_attempted",
            False,
        ),
        "surface_map_populated": population.get("surface_map_populated", False),
        "requested_sample_budget": int(
            population.get("requested_sample_budget", 0) or 0
        ),
        "emitted_photon_count": int(population.get("emitted_photon_count", 0) or 0),
        "trace_input_count": int(population.get("trace_input_count", 0) or 0),
        "trace_solved_path_count": int(
            population.get("trace_solved_path_count", 0) or 0
        ),
        "trace_record_count": int(population.get("trace_record_count", 0) or 0),
        "receiver_lookup_attempt_count": int(
            population.get("receiver_lookup_attempt_count", 0) or 0
        ),
        "receiver_direct_hit_count": int(
            population.get("receiver_direct_hit_count", 0) or 0
        ),
        "receiver_crossing_probe_attempt_count": int(
            population.get("receiver_crossing_probe_attempt_count", 0) or 0
        ),
        "receiver_crossing_probe_hit_count": int(
            population.get("receiver_crossing_probe_hit_count", 0) or 0
        ),
        "receiver_candidate_count": int(
            population.get("receiver_candidate_count", 0) or 0
        ),
        "receiver_accepted_hit_count": int(
            population.get("receiver_accepted_hit_count", 0) or 0
        ),
        "receiver_miss_reject_count": int(
            population.get("receiver_miss_reject_count", 0) or 0
        ),
        "receiver_self_hit_reject_count": int(
            population.get("receiver_self_hit_reject_count", 0) or 0
        ),
        "receiver_invalid_reject_count": int(
            population.get("receiver_invalid_reject_count", 0) or 0
        ),
        "receiver_material_filter_reject_count": int(
            population.get("receiver_material_filter_reject_count", 0) or 0
        ),
        "receiver_object_filter_reject_count": int(
            population.get("receiver_object_filter_reject_count", 0) or 0
        ),
        "receiver_competing_reject_count": int(
            population.get("receiver_competing_reject_count", 0) or 0
        ),
        "receiver_selected_hit_count": int(
            population.get("receiver_selected_hit_count", 0) or 0
        ),
        "receiver_selected_bucket_count": int(
            population.get("receiver_selected_bucket_count", 0) or 0
        ),
        "receiver_footprint_radius": float(
            population.get("receiver_footprint_radius", 0.0) or 0.0
        ),
        "receiver_mean_distance": float(
            population.get("receiver_mean_distance", 0.0) or 0.0
        ),
        "receiver_max_distance": float(
            population.get("receiver_max_distance", 0.0) or 0.0
        ),
        "prepared_scene_mesh_dielectric_attempted": population.get(
            "prepared_scene_mesh_dielectric_attempted",
            False,
        ),
        "prepared_scene_mesh_dielectric_succeeded": population.get(
            "prepared_scene_mesh_dielectric_succeeded",
            False,
        ),
        "fixture_mesh_dielectric_fallback_used": population.get(
            "fixture_mesh_dielectric_fallback_used",
            False,
        ),
        "prepared_scene_mesh_dielectric_candidate_count": int(
            population.get("prepared_scene_mesh_dielectric_candidate_count", 0) or 0
        ),
        "prepared_scene_mesh_dielectric_triangle_count": int(
            population.get("prepared_scene_mesh_dielectric_triangle_count", 0) or 0
        ),
        "prepared_scene_mesh_dielectric_scene_object_index": int(
            population.get("prepared_scene_mesh_dielectric_scene_object_index", -1)
        ),
        "prepared_scene_mesh_dielectric_triangle_index": int(
            population.get("prepared_scene_mesh_dielectric_triangle_index", -1)
        ),
        "surface_map_store_accepted_count": int(
            population.get("surface_map_store_accepted_count", 0) or 0
        ),
        "surface_map_record_count": int(
            population.get("surface_map_record_count", 0) or 0
        ),
        "surface_map_acceleration_inserted_count": int(
            population.get("surface_map_acceleration_inserted_count", 0) or 0
        ),
        "volume_beam_store_accepted_count": int(
            population.get("volume_beam_store_accepted_count", 0) or 0
        ),
        "volume_beam_segment_count": int(
            population.get("volume_beam_segment_count", 0) or 0
        ),
        "surface_cache_sample_contributing_count": int(
            caustic_state.get("surface_cache_sample_contributing_count", 0) or 0
        ),
        "volume_cache_sample_contributing_count": int(
            caustic_state.get("volume_cache_sample_contributing_count", 0) or 0
        ),
    }


def frame_metrics(frame_path: Path | None) -> dict:
    if not frame_path:
        return {
            "frame_available": False,
            "average_luma": 0.0,
            "max_luma": 0.0,
        }
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    luma_sum = 0.0
    max_luma = 0.0
    for row in pixels:
        for r, g, b in row:
            luma = (0.2126 * float(r) + 0.7152 * float(g) + 0.0722 * float(b)) / 255.0
            luma_sum += luma
            max_luma = max(max_luma, luma)
    return {
        "frame_available": True,
        "width": width,
        "height": height,
        "average_luma": luma_sum / float(width * height),
        "max_luma": max_luma,
    }


def write_frame_png(summary: dict, review_root: Path, cell_id: str) -> Path | None:
    frame_path = first_frame_path(summary)
    if not frame_path:
        return None
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{cell_id}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return png_path


def write_contact_sheet(review_root: Path, cells: list[dict]) -> Path | None:
    frame_cells = [cell for cell in cells if cell.get("frame_bmp_path")]
    if not frame_cells:
        return None
    frames = []
    for cell in frame_cells:
        width, height, pixels = review_artifacts.read_bmp_rgb(
            Path(cell["frame_bmp_path"])
        )
        frames.append((width, height, pixels))
    width = max(frame[0] for frame in frames)
    height = max(frame[1] for frame in frames)
    sheet_width = width * len(frames)
    sheet = [[(0, 0, 0) for _x in range(sheet_width)] for _y in range(height)]
    for index, (frame_width, frame_height, pixels) in enumerate(frames):
        x_offset = index * width
        for y in range(frame_height):
            sheet[y][x_offset:x_offset + frame_width] = pixels[y]
    sheet_path = review_root / "ppm10_product_ab_contact_sheet.png"
    review_artifacts.write_png_rgb(sheet_path, sheet_width, height, sheet)
    return sheet_path


def validate_cell(cell: dict, summary: dict, preflight_only: bool) -> list[str]:
    failures: list[str] = []
    expected_mode = cell["product_mode"]
    inspection = summary.get("inspection", {})
    caustic_state = inspection.get("caustic_state", {})
    if inspection.get("caustic_product_mode") != expected_mode:
        failures.append(
            f"{cell['id']}: expected caustic_product_mode={expected_mode}, "
            f"got {inspection.get('caustic_product_mode')}"
        )
    if not inspection.get("has_caustic_product_mode_override"):
        failures.append(f"{cell['id']}: product mode override was not reported")
    if bool(inspection.get("caustic_render_contribution_enabled")) != bool(
        cell["render_contribution"]
    ):
        failures.append(f"{cell['id']}: render contribution gate mismatch")
    if cell["product_mode"] == "production" and not caustic_state.get(
        "photon_map_requested"
    ):
        failures.append(f"{cell['id']}: photon-map route was not requested")
    if cell["product_mode"] != "production" and caustic_state.get(
        "photon_map_requested"
    ):
        failures.append(f"{cell['id']}: photon-map route unexpectedly requested")
    counts = photon_callsite_counts(summary)
    expected_render_prep = bool(cell.get("render_prep_population", False))
    expected_volume_render_prep = bool(cell.get("volume_render_prep", False))
    expected_populated = bool(cell.get("populated_callsite", False) or expected_render_prep)
    expected_trace_populated = bool(cell.get("trace_populated_callsite", False))
    expected_proxy_populated = expected_populated and not expected_trace_populated
    if bool(counts["render_prep_population_enabled"]) != expected_render_prep:
        failures.append(f"{cell['id']}: render-prep population flag mismatch")
    if bool(counts["readback_enabled"]) != (
        expected_proxy_populated and not expected_render_prep
    ):
        failures.append(f"{cell['id']}: populated callsite readback flag mismatch")
    if bool(counts["trace_readback_enabled"]) != expected_trace_populated:
        failures.append(f"{cell['id']}: trace populated callsite readback flag mismatch")
    if expected_populated:
        required_true = (
            "readback_built",
            "query_attempted",
            "query_hit",
            "contribution_eligible",
            "cache_deposit_attempted",
            "surface_deposited",
            "population_attempted",
            "surface_map_allocated",
            "emission_attempted",
            "emission_succeeded",
            "surface_map_population_attempted",
            "surface_map_populated",
        )
        required_positive = (
            "requested_sample_budget",
            "emitted_photon_count",
            "surface_map_store_accepted_count",
            "surface_map_record_count",
            "surface_map_acceleration_inserted_count",
            "surface_contributing_count",
            "estimated_cost",
        )
        if expected_trace_populated or expected_render_prep:
            required_true += (
                "prepared_scene_mesh_dielectric_attempted",
                "prepared_scene_mesh_dielectric_succeeded",
            )
            required_positive += (
                "trace_solved_path_count",
                "trace_record_count",
                "trace_input_count",
                "prepared_scene_mesh_dielectric_candidate_count",
                "prepared_scene_mesh_dielectric_triangle_count",
            )
            if expected_trace_populated:
                required_positive += (
                    "volume_beam_store_accepted_count",
                    "volume_beam_segment_count",
                    "volume_contributing_count",
                )
            if expected_render_prep:
                required_true += (
                    "map_lifecycle_evaluated",
                    "map_lifecycle_rebuilt",
                    "map_lifecycle_persistent",
                )
                required_positive += (
                    "map_lifecycle_generation",
                    "map_lifecycle_rebuild_count",
                    "surface_cache_sample_contributing_count",
                    "receiver_lookup_attempt_count",
                    "receiver_candidate_count",
                    "receiver_accepted_hit_count",
                    "receiver_selected_hit_count",
                    "receiver_selected_bucket_count",
                    "receiver_footprint_radius",
                    "receiver_contribution_bucket_count",
                    "receiver_contribution_query_attempt_count",
                    "receiver_contribution_query_hit_count",
                    "receiver_contribution_surface_deposit_accepted_count",
                    "receiver_contribution_surface_contributing_count",
                )
                if counts["map_lifecycle_reused"]:
                    failures.append(f"{cell['id']}: first prepared map unexpectedly reused")
                if counts["map_lifecycle_rebuild_reason"] != "first_build":
                    failures.append(
                        f"{cell['id']}: expected first_build lifecycle reason"
                    )
                if counts["map_lifecycle_budget_tier"] != "preview":
                    failures.append(
                        f"{cell['id']}: expected preview lifecycle budget tier"
                    )
            if expected_volume_render_prep:
                required_true += (
                    "volume_deposited",
                    "beam_contribution_attempted",
                    "beam_contribution_volume_sampleable",
                    "beam_contribution_beam_map_allocated",
                    "beam_contribution_query_hit",
                    "beam_contribution_eligible",
                    "beam_contribution_volume_deposited",
                )
                required_positive += (
                    "volume_beam_store_accepted_count",
                    "volume_beam_segment_count",
                    "volume_contributing_count",
                    "beam_contribution_query_attempt_count",
                    "beam_contribution_candidate_count",
                    "beam_contribution_contributing_count",
                    "beam_contribution_volume_deposit_accepted_count",
                    "beam_contribution_density",
                    "beam_contribution_transmittance",
                    "volume_cache_sample_contributing_count",
                )
            if counts["fixture_mesh_dielectric_fallback_used"]:
                failures.append(
                    f"{cell['id']}: expected prepared-scene mesh dielectric, got fixture fallback"
                )
        for key in required_true:
            if not counts[key]:
                failures.append(f"{cell['id']}: expected {key}=true")
        for key in required_positive:
            if counts[key] <= 0:
                failures.append(f"{cell['id']}: expected {key}>0")
        expected_source = (
            "trace_records"
            if expected_trace_populated or expected_render_prep
            else "surface_proxy"
        )
        if counts["population_source"] != expected_source:
            failures.append(
                f"{cell['id']}: expected population_source={expected_source}, "
                f"got {counts['population_source']!r}"
            )
        if counts["requested_sample_budget"] != 64:
            failures.append(
                f"{cell['id']}: expected requested_sample_budget=64, "
                f"got {counts['requested_sample_budget']}"
            )
        if counts["surface_contributing_count"] > counts["emitted_photon_count"]:
            failures.append(
                f"{cell['id']}: surface contribution count exceeded emission"
            )
        if expected_render_prep and (
            counts["receiver_direct_hit_count"]
            + counts["receiver_crossing_probe_hit_count"]
            <= 0
        ):
            failures.append(
                f"{cell['id']}: expected direct or crossing-probe receiver hits"
            )
        if expected_render_prep and counts["receiver_accepted_hit_count"] != counts[
            "surface_map_store_accepted_count"
        ]:
            failures.append(
                f"{cell['id']}: receiver accepted count did not match stored surface records"
            )
        if expected_render_prep and counts["receiver_candidate_count"] < counts[
            "receiver_accepted_hit_count"
        ]:
            failures.append(
                f"{cell['id']}: receiver candidates below accepted receiver hits"
            )
        if expected_render_prep and counts[
            "receiver_contribution_surface_deposit_accepted_count"
        ] != counts["receiver_contribution_bucket_count"]:
            failures.append(
                f"{cell['id']}: receiver contribution deposits did not match buckets"
            )
    else:
        zero_keys = (
            "emitted_photon_count",
            "surface_map_store_accepted_count",
            "surface_map_record_count",
            "surface_map_acceleration_inserted_count",
            "surface_contributing_count",
            "trace_solved_path_count",
            "trace_record_count",
            "trace_input_count",
            "prepared_scene_mesh_dielectric_candidate_count",
            "prepared_scene_mesh_dielectric_triangle_count",
            "volume_beam_store_accepted_count",
            "volume_beam_segment_count",
            "volume_contributing_count",
        )
        for key in zero_keys:
            if counts[key] != 0:
                failures.append(f"{cell['id']}: expected {key}=0")
    if not preflight_only and not first_frame_path(summary):
        failures.append(f"{cell['id']}: render did not produce a first frame")
    return failures


def build_report(cells: list[dict], review_root: Path, preflight_only: bool) -> dict:
    off_luma = next(
        (
            cell["frame_metrics"]["average_luma"]
            for cell in cells
            if cell["id"] == "off"
        ),
        0.0,
    )
    for cell in cells:
        metrics = cell["frame_metrics"]
        metrics["average_luma_delta_vs_off"] = metrics["average_luma"] - off_luma
    contact_sheet = write_contact_sheet(review_root, cells)
    return {
        "schema": "ray_tracing_ppm10_product_ab_fixture/v1",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "preflight_only": preflight_only,
        "purpose": (
            "Compare off/reference/production/production_populated/"
            "production_trace_populated/production_render_prep_populated "
            "caustic product routing on transparent mesh receiver scenes with "
            "numeric readback before default promotion."
        ),
        "cells": cells,
        "contact_sheet_path": str(contact_sheet) if contact_sheet else "",
        "production_default_note": (
            "Production is exercised only by explicit request override; default "
            "settings remain reference with render contribution disabled."
        ),
        "non_goals": [
            "Do not require production photon mapping to brighten the image yet.",
            "Do not tune display gain as a substitute for transport correctness.",
            "Do not replace the current hash-grid map acceleration with a BVH in this fixture.",
        ],
    }


def write_index(report: dict, review_root: Path) -> Path:
    index_path = review_root / "ppm10_product_ab_index.md"
    lines = [
        "# PPM-10 Product A/B Fixture",
        "",
        report["purpose"],
        "",
        f"- Contact sheet: `{report['contact_sheet_path']}`",
        f"- Preflight only: `{str(report['preflight_only']).lower()}`",
        f"- Default note: {report['production_default_note']}",
        "",
        "| Cell | Product mode | Contribution | Populated | Source | Prepared mesh | Emitted | Stored | Receiver hits | Selected hits | Receiver radius | Trace paths | Trace records | Beam segs | Surface contrib | Volume contrib | Deposit | Avg luma | Delta vs off |",
        "| --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |",
    ]
    for cell in report["cells"]:
        metrics = cell["frame_metrics"]
        counts = cell["photon_callsite_counts"]
        lines.append(
            "| {id} | {mode} | {contribution} | {populated} | {source} | {prepared_mesh} | {emitted} | {stored} | {receiver_hits} | {selected_hits} | {receiver_radius:.6f} | {trace_paths} | {trace_records} | {beam_segments} | {surface_contrib} | {volume_contrib} | {deposit} | {luma:.6f} | {delta:.6f} |".format(
                id=cell["id"],
                mode=cell["product_mode"],
                contribution=str(cell["render_contribution"]).lower(),
                populated=str(
                    cell["populated_callsite"]
                    or cell.get("render_prep_population", False)
                ).lower(),
                source=counts["population_source"],
                prepared_mesh=counts["prepared_scene_mesh_dielectric_candidate_count"],
                emitted=counts["emitted_photon_count"],
                stored=counts["surface_map_store_accepted_count"],
                receiver_hits=counts["receiver_accepted_hit_count"],
                selected_hits=counts["receiver_selected_hit_count"],
                receiver_radius=counts["receiver_footprint_radius"],
                trace_paths=counts["trace_solved_path_count"],
                trace_records=counts["trace_record_count"],
                beam_segments=counts["volume_beam_segment_count"],
                surface_contrib=counts["surface_contributing_count"],
                volume_contrib=counts["volume_contributing_count"],
                deposit=str(counts["surface_deposited"]).lower(),
                luma=metrics["average_luma"],
                delta=metrics["average_luma_delta_vs_off"],
            )
        )
    lines.append("")
    lines.append("Numeric summaries are stored beside each generated request.")
    index_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return index_path


def main() -> int:
    args = parse_args()
    root = repo_root()
    reset_review_root(args.review_root)
    failures: list[str] = []
    cells: list[dict] = []

    for variant in VARIANTS:
        run = request_for_variant(root, args.review_root, variant)
        elapsed = run_request(args.cli, run, args.preflight_only)
        summary = load_json(run["summary_path"])
        failures.extend(validate_cell(variant, summary, args.preflight_only))
        frame_path = first_frame_path(summary)
        frame_png = write_frame_png(summary, args.review_root, variant["id"])
        metrics = frame_metrics(frame_path)
        cells.append({
            "id": variant["id"],
            "description": variant["description"],
            "product_mode": variant["product_mode"],
        "render_contribution": variant["render_contribution"],
        "integrator_3d": variant.get("integrator_3d", "direct_light"),
        "populated_callsite": bool(variant.get("populated_callsite", False)),
            "render_prep_population": bool(variant.get("render_prep_population", False)),
            "elapsed_seconds": elapsed,
            "request_path": str(run["request_path"]),
            "summary_path": str(run["summary_path"]),
            "frame_bmp_path": str(frame_path) if frame_path else "",
            "frame_png_path": str(frame_png) if frame_png else "",
            "frame_metrics": metrics,
            "photon_callsite_counts": photon_callsite_counts(summary),
            "caustic_readback": nested(summary, "inspection", "caustic_state", default={}),
            "reported_product_mode": nested(
                summary,
                "inspection",
                "caustic_product_mode",
                default="",
            ),
            "reported_render_contribution": nested(
                summary,
                "inspection",
                "caustic_render_contribution_enabled",
                default=False,
            ),
        })

    report = build_report(cells, args.review_root, args.preflight_only)
    report_path = args.review_root / "ppm10_product_ab_report.json"
    write_json(report_path, report)
    index_path = write_index(report, args.review_root)

    if failures:
        write_json(args.review_root / "ppm10_product_ab_failures.json", {
            "failures": failures,
            "report_path": str(report_path),
        })
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print(f"PPM-10 product A/B fixture passed: {report_path}")
    print(f"PPM-10 product A/B index: {index_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
