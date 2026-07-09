#!/usr/bin/env python3
"""Render a low-cost imported closed-lens wall-caustic preview."""

from __future__ import annotations

import argparse
import json
import math
import platform
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import run_ray_tracing_spatial_caustic_mesh_dielectric_lens_fixture as mesh_fixture  # noqa: E402
import run_ray_tracing_spatial_caustic_visual_sphere_mist_matrix as sphere_mist  # noqa: E402


CALIBRATION_ENERGY_SCALES = (0.0005, 0.001, 0.0025, 0.005, 0.010, 0.025, 0.050)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def workspace_root() -> Path:
    return repo_root().parent


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = root / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless"
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def default_review_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "caustic_imported_lens_wall_preview"
    )


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--review-root", type=Path, default=default_review_root())
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--debug-export", action="store_true")
    return parser.parse_args()


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def clean_generated_review_root(review_root: Path) -> None:
    for child_name in (
        "generated_scene",
        "generated_requests",
        "runs",
        "summaries",
        "frames",
        "diffs",
    ):
        child = review_root / child_name
        if child.exists():
            shutil.rmtree(child)
    for file_name in (
        "imported_lens_wall_preview_report.json",
        "imported_lens_wall_preview_index.md",
        "imported_lens_wall_preview_contact_sheet.png",
    ):
        path = review_root / file_name
        if path.exists():
            path.unlink()


def write_biconvex_lens_asset(mesh_dir: Path) -> Path:
    mesh_dir.mkdir(parents=True, exist_ok=True)
    segments = 96
    radial_rings = 16
    radius = 1.0
    rim_half_thickness = 0.08
    center_bulge = 0.24
    front_center_y = -rim_half_thickness - center_bulge
    back_center_y = rim_half_thickness + center_bulge
    vertices: list[dict] = []

    def surface_y(sign: float, radial_t: float) -> float:
        return sign * (rim_half_thickness + center_bulge * (1.0 - radial_t * radial_t))

    front_center_index = len(vertices)
    vertices.append({"x": 0.0, "y": front_center_y, "z": 0.0})
    front_rings: list[list[int]] = []
    for ring in range(1, radial_rings + 1):
        radial_t = float(ring) / float(radial_rings)
        ring_indices = []
        for i in range(segments):
            theta = (2.0 * math.pi * float(i)) / float(segments)
            ring_indices.append(len(vertices))
            vertices.append({
                "x": radius * radial_t * math.cos(theta),
                "y": surface_y(-1.0, radial_t),
                "z": radius * radial_t * math.sin(theta),
            })
        front_rings.append(ring_indices)

    back_center_index = len(vertices)
    vertices.append({"x": 0.0, "y": back_center_y, "z": 0.0})
    back_rings: list[list[int]] = []
    for ring in range(1, radial_rings + 1):
        radial_t = float(ring) / float(radial_rings)
        ring_indices = []
        for i in range(segments):
            theta = (2.0 * math.pi * float(i)) / float(segments)
            ring_indices.append(len(vertices))
            vertices.append({
                "x": radius * radial_t * math.cos(theta),
                "y": surface_y(1.0, radial_t),
                "z": radius * radial_t * math.sin(theta),
            })
        back_rings.append(ring_indices)

    triangles: list[dict] = []
    for i in range(segments):
        n = (i + 1) % segments
        triangles.append({
            "a": front_center_index,
            "b": front_rings[0][i],
            "c": front_rings[0][n],
            "surface_group_id": "lens_front",
        })
        triangles.append({
            "a": back_center_index,
            "b": back_rings[0][n],
            "c": back_rings[0][i],
            "surface_group_id": "lens_back",
        })
    for ring in range(1, radial_rings):
        front_inner = front_rings[ring - 1]
        front_outer = front_rings[ring]
        back_inner = back_rings[ring - 1]
        back_outer = back_rings[ring]
        for i in range(segments):
            n = (i + 1) % segments
            triangles.append({
                "a": front_inner[i],
                "b": front_outer[i],
                "c": front_outer[n],
                "surface_group_id": "lens_front",
            })
            triangles.append({
                "a": front_inner[i],
                "b": front_outer[n],
                "c": front_inner[n],
                "surface_group_id": "lens_front",
            })
            triangles.append({
                "a": back_inner[i],
                "b": back_outer[n],
                "c": back_outer[i],
                "surface_group_id": "lens_back",
            })
            triangles.append({
                "a": back_inner[i],
                "b": back_inner[n],
                "c": back_outer[n],
                "surface_group_id": "lens_back",
            })
    front_outer = front_rings[-1]
    back_outer = back_rings[-1]
    for i in range(segments):
        n = (i + 1) % segments
        triangles.append({
            "a": front_outer[i],
            "b": back_outer[n],
            "c": front_outer[n],
            "surface_group_id": "lens_rim",
        })
        triangles.append({
            "a": front_outer[i],
            "b": back_outer[i],
            "c": back_outer[n],
            "surface_group_id": "lens_rim",
        })

    asset = {
        "schema_family": "codework_geometry",
        "schema_variant": "mesh_asset_runtime_v1",
        "schema_version": 1,
        "asset_id": "asset_imported_biconvex_lens_96x16",
        "source_asset_id": "imported_closed_biconvex_lens_preview",
        "asset_type": "solid_mesh",
        "compile_meta": {
            "profile": "runtime_default",
            "generator": Path(__file__).name,
            "shape": "closed_biconvex_lens",
            "axis": "y",
            "segments": segments,
            "radial_rings": radial_rings,
            "fidelity": "high_resolution_preview",
        },
        "local_bounds": {
            "min": {"x": -radius, "y": front_center_y, "z": -radius},
            "max": {"x": radius, "y": back_center_y, "z": radius},
        },
        "mesh": {
            "vertex_count": len(vertices),
            "triangle_count": len(triangles),
            "vertices": vertices,
            "triangles": triangles,
        },
        "surface_groups": [
            {
                "group_id": "lens_front",
                "semantic": "incident_convex_surface",
                "triangle_span": {"start": 0, "count": segments + (radial_rings - 1) * segments * 2},
            },
            {
                "group_id": "lens_back",
                "semantic": "exit_convex_surface",
                "triangle_span": {"start": segments, "count": segments + (radial_rings - 1) * segments * 2},
            },
            {
                "group_id": "lens_rim",
                "semantic": "closed_lens_rim",
                "triangle_span": {"start": segments * radial_rings * 4 - segments * 2, "count": segments * 2},
            },
        ],
        "topology_flags": {
            "closed_volume": True,
            "manifold_expected": True,
        },
        "extensions": {},
    }
    mesh_path = mesh_dir / "asset_imported_biconvex_lens_96x16.runtime.json"
    write_json(mesh_path, asset)
    return mesh_path


def wall_plane() -> dict:
    return sphere_mist.plane_object(
        "vivid_receiver_wall",
        {
            "origin": {"x": 0.0, "y": 0.85, "z": 1.25},
            "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
            "axis_v": {"x": 0.0, "y": 0.0, "z": 1.0},
            "normal": {"x": 0.0, "y": -1.0, "z": 0.0},
        },
        7.0,
        5.0,
        "mat_vivid_wall",
    )


def floor_plane() -> dict:
    return sphere_mist.plane_object(
        "matte_floor",
        {
            "origin": {"x": 0.0, "y": -0.95, "z": -0.02},
            "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
            "axis_v": {"x": 0.0, "y": 1.0, "z": 0.0},
            "normal": {"x": 0.0, "y": 0.0, "z": 1.0},
        },
        4.2,
        5.6,
        "mat_dark_floor",
    )


def write_preview_scene(review_root: Path) -> tuple[Path, Path]:
    scene_dir = review_root / "generated_scene"
    mesh_dir = scene_dir / "assets" / "mesh_assets"
    mesh_dir.mkdir(parents=True, exist_ok=True)
    lens_path = write_biconvex_lens_asset(mesh_dir)
    scene = {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "caustic_imported_lens_wall_preview_v1",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            wall_plane(),
            {
                "object_id": "imported_biconvex_lens",
                "object_type": "mesh_asset_instance",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": {"x": 0.0, "y": -1.05, "z": 1.25},
                    "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "scale": {"x": 0.52, "y": 0.52, "z": 0.52},
                },
                "geometry_ref": {
                    "kind": "mesh_asset",
                    "id": "asset_imported_biconvex_lens_96x16",
                    "variant": "runtime_default",
                },
                "extensions": {
                    "line_drawing": {
                        "runtime_mesh_path": "assets/mesh_assets/asset_imported_biconvex_lens_96x16.runtime.json",
                    }
                },
                "material_id": "mat_lens_glass",
                "flags": {"visible": True, "locked": False, "selectable": True},
            },
        ],
        "materials": [
            {"material_id": "mat_dark_floor", "kind": "lambert", "albedo": [0.06, 0.06, 0.06]},
            {"material_id": "mat_vivid_wall", "kind": "lambert", "albedo": [0.0, 0.30, 0.85]},
            {"material_id": "mat_lens_glass", "kind": "dielectric", "albedo": [0.88, 0.97, 1.0]},
        ],
        "lights": [
            {
                "light_id": "lens_preview_light",
                "kind": "sphere",
                "position": {"x": 0.0, "y": -2.85, "z": 1.25},
                "radius": 0.090,
                "intensity": 24.0,
                "falloff_distance": 5.0,
                "color": {"r": 1.0, "g": 0.96, "b": 0.88},
                "enabled": True,
                "moving": False,
            }
        ],
        "cameras": [
            {
                "camera_id": "wall_preview_camera",
                "kind": "perspective",
                "position": {"x": 1.95, "y": -3.25, "z": 1.52},
                "target": {"x": 0.0, "y": -1.05, "z": 1.24},
                "yaw": 0.0,
                "look_pitch": 0.0,
            }
        ],
        "constraints": [],
        "hierarchy": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": {"x": 0.0, "y": -1.05, "z": 1.24},
                    "environment": {
                        "light_mode": 1,
                        "ambient_strength": 0.18,
                        "top_fill_strength": 0.08,
                    },
                    "object_materials": [
                        {
                            "object_id": "matte_floor",
                            "material_id": 0,
                            "object_color": sphere_mist.rgb_u24(18, 18, 18),
                            "roughness": 0.9,
                            "reflectivity": 0.0,
                            "alpha": 1.0,
                        },
                        {
                            "object_id": "vivid_receiver_wall",
                            "material_id": 0,
                            "object_color": sphere_mist.rgb_u24(0, 78, 220),
                            "roughness": 0.78,
                            "reflectivity": 0.01,
                            "alpha": 1.0,
                        },
                        {
                            "object_id": "imported_biconvex_lens",
                            "material_id": 5,
                            "object_color": sphere_mist.rgb_u24(126, 228, 255),
                            "alpha": 0.18,
                            "glass_transport_override": True,
                            "glass_transmission": 0.82,
                            "glass_ior": 1.52,
                            "glass_absorption_distance": 4.0,
                            "glass_thin_walled": True,
                            "roughness": 0.006,
                            "reflectivity": 0.01,
                        },
                    ],
                }
            }
        },
    }
    scene_path = scene_dir / "scene_runtime.json"
    write_json(scene_path, scene)
    return scene_path, lens_path


def base_request(run_id: str,
                 scene_path: Path,
                 output_root: Path,
                 summary_path: Path) -> dict:
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": run_id,
        "scene": {
            "runtime_scene_path": str(scene_path),
        },
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": 256,
            "height": 160,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "integrator_3d": "disney_v2",
            "denoise_enabled": False,
        },
        "inspection": {
            "preset": "glass_review",
            "camera_position": {"x": 1.95, "y": -3.25, "z": 1.52},
            "camera_look_at": {"x": 0.0, "y": -1.05, "z": 1.24},
            "camera_zoom": 0.52,
            "environment_light_mode": "ambient",
            "ambient_strength": 0.18,
            "top_fill_strength": 0.08,
            "background_brightness": 0.012,
            "background_color": {"r": 0.01, "g": 0.012, "b": 0.016},
            "light_position": {"x": 0.0, "y": -2.85, "z": 1.25},
            "light_intensity": 24.0,
            "light_radius": 0.090,
            "secondary_diffuse_samples_3d": 6,
            "transmission_samples_3d": 8,
            "caustic_debug_summary": True,
            "object_audit_enabled": True,
        },
        "output": {
            "root": str(output_root),
            "overwrite": True,
        },
        "progress": {
            "summary_path": str(summary_path),
            "progress_path": str(output_root / "render_progress.json"),
        },
    }


def request_for_cell(review_root: Path,
                     scene_path: Path,
                     cell_id: str,
                     debug_export: bool) -> dict:
    output_root = review_root / "runs" / cell_id
    summary_path = output_root / "render_summary.json"
    request = base_request(
        f"caustic_imported_lens_wall_preview_{cell_id}",
        scene_path,
        output_root,
        summary_path,
    )
    if cell_id == "wall_no_caustic":
        request["inspection"].update({
            "caustic_mode": "off",
            "caustic_volume_enabled": False,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 0,
        })
    elif cell_id.startswith("wall_caustic_scale_"):
        energy_scale = energy_scale_for_cell(cell_id)
        request["inspection"].update({
            "caustic_mode": "transport",
            "caustic_volume_enabled": False,
            "caustic_surface_enabled": True,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 2048,
            "caustic_max_path_depth": 2,
            "caustic_surface_energy_scale": energy_scale,
            "caustic_surface_footprint_scale": 5.0,
            "caustic_surface_receiver_fallback_enabled": False,
            "caustic_transport_emission_policy": "mesh_dielectric_lens",
            "caustic_lens_traversal_preset": "dense_glass",
        })
        if debug_export:
            request["inspection"]["caustic_transport_debug_export_enabled"] = True
    else:
        raise ValueError(f"unknown imported lens wall preview cell: {cell_id}")
    request_path = review_root / "generated_requests" / f"request_{cell_id}.json"
    write_json(request_path, request)
    return {
        "cell_id": cell_id,
        "request_path": request_path,
        "summary_path": summary_path,
    }


def cell_id_for_energy_scale(scale: float) -> str:
    token = f"{scale:.4f}".rstrip("0").rstrip(".").replace(".", "p")
    return f"wall_caustic_scale_{token}"


def energy_scale_for_cell(cell_id: str) -> float:
    suffix = cell_id.removeprefix("wall_caustic_scale_")
    return float(suffix.replace("p", "."))


def render_request(cli: Path, request_path: Path, summary_path: Path, skip_render: bool) -> float | None:
    if skip_render:
        return None
    stdout_path = summary_path.parent / "stdout_summary.json"
    stderr_path = summary_path.parent / "stderr.txt"
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    with stdout_path.open("w", encoding="utf-8") as stdout:
        result = subprocess.run(
            [str(cli), "--request", str(request_path), "--render", "--summary", str(summary_path)],
            stdout=stdout,
            stderr=subprocess.PIPE,
            text=True,
        )
    elapsed = time.perf_counter() - start
    stderr_path.write_text(result.stderr or "", encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(f"render failed for {request_path} with exit {result.returncode}; stderr: {stderr_path}")
    return elapsed


def first_frame_path(summary: dict) -> Path:
    frame_path = Path(summary.get("outputs", {}).get("first_frame_path", ""))
    if frame_path.exists():
        return frame_path
    return Path(summary.get("output_root", "")) / "frames" / "frame_0000.bmp"


def copy_frame_png(summary: dict, review_root: Path, cell_id: str) -> tuple[Path, Path]:
    frame_path = first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{cell_id}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return frame_path, png_path


def luma(pixel: tuple[int, int, int]) -> float:
    r, g, b = pixel
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def wall_delta_metrics(baseline_path: Path, caustic_path: Path) -> dict:
    width, height, baseline = review_artifacts.read_bmp_rgb(baseline_path)
    caustic_width, caustic_height, caustic = review_artifacts.read_bmp_rgb(caustic_path)
    if (width, height) != (caustic_width, caustic_height):
        raise ValueError("wall preview frames must have matching dimensions")
    min_x = 0
    max_x = width
    min_y = 0
    max_y = height
    positive_pixels = 0
    white_shift_pixels = 0
    saturated_pixels = 0
    delta_samples: list[float] = []
    positive_delta_samples: list[float] = []
    total_delta = 0.0
    max_delta = 0.0
    max_white_shift = 0.0
    weighted_x = 0.0
    weighted_y = 0.0
    weight_sum = 0.0
    for y in range(min_y, max_y):
        for x in range(min_x, max_x):
            before = baseline[y][x]
            after = caustic[y][x]
            delta = luma(after) - luma(before)
            before_white_distance = math.sqrt(
                (255.0 - before[0]) * (255.0 - before[0]) +
                (255.0 - before[1]) * (255.0 - before[1]) +
                (255.0 - before[2]) * (255.0 - before[2])
            )
            after_white_distance = math.sqrt(
                (255.0 - after[0]) * (255.0 - after[0]) +
                (255.0 - after[1]) * (255.0 - after[1]) +
                (255.0 - after[2]) * (255.0 - after[2])
            )
            white_shift = before_white_distance - after_white_distance
            total_delta += delta
            delta_samples.append(delta)
            if delta > 0.5:
                positive_pixels += 1
                positive_delta_samples.append(delta)
                weighted_x += float(x) * delta
                weighted_y += float(y) * delta
                weight_sum += delta
            if white_shift > 1.0:
                white_shift_pixels += 1
            if after[0] >= 250 or after[1] >= 250 or after[2] >= 250:
                saturated_pixels += 1
            max_delta = max(max_delta, delta)
            max_white_shift = max(max_white_shift, white_shift)
    area = float(max(1, (max_x - min_x) * (max_y - min_y)))
    delta_samples.sort()
    positive_delta_samples.sort()
    return {
        "roi": {
            "min_x": min_x,
            "max_x": max_x,
            "min_y": min_y,
            "max_y": max_y,
        },
        "positive_pixel_count": positive_pixels,
        "positive_area_ratio": positive_pixels / area,
        "white_shift_pixel_count": white_shift_pixels,
        "white_shift_area_ratio": white_shift_pixels / area,
        "saturated_pixel_count": saturated_pixels,
        "saturated_area_ratio": saturated_pixels / area,
        "positive_delta_p50": percentile(positive_delta_samples, 0.50),
        "positive_delta_p95": percentile(positive_delta_samples, 0.95),
        "positive_delta_p99": percentile(positive_delta_samples, 0.99),
        "delta_p95": percentile(delta_samples, 0.95),
        "delta_p99": percentile(delta_samples, 0.99),
        "positive_delta_mean": (
            sum(positive_delta_samples) / float(len(positive_delta_samples))
            if positive_delta_samples else 0.0
        ),
        "positive_delta_centroid": {
            "x": weighted_x / weight_sum if weight_sum > 0.0 else None,
            "y": weighted_y / weight_sum if weight_sum > 0.0 else None,
        },
        "mean_luma_delta": total_delta / area,
        "max_luma_delta": max_delta,
        "max_white_shift": max_white_shift,
    }


def percentile(sorted_values: list[float], fraction: float) -> float:
    if not sorted_values:
        return 0.0
    clamped = min(1.0, max(0.0, fraction))
    index = int(round(clamped * float(len(sorted_values) - 1)))
    return float(sorted_values[index])


def write_delta_artifacts(review_root: Path, baseline_path: Path, caustic_path: Path) -> dict:
    before_w, before_h, before = review_artifacts.read_bmp_rgb(baseline_path)
    after_w, after_h, after = review_artifacts.read_bmp_rgb(caustic_path)
    if (before_w, before_h) != (after_w, after_h):
        raise ValueError("delta frames must have matching dimensions")
    diff16 = review_artifacts.abs_diff_pixels(before, after, 16)
    side_w, side_h, side = review_artifacts.side_by_side(before, after, diff16)
    diff_path = review_root / "diffs" / "wall_mesh_dielectric_surface_vs_off_diff16x.png"
    side_path = review_root / "diffs" / "wall_preview_side_by_side_diff16x.png"
    diff_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(diff_path, before_w, before_h, diff16)
    review_artifacts.write_png_rgb(side_path, side_w, side_h, side)
    return {
        "diff16_path": str(diff_path),
        "side_by_side_diff16_path": str(side_path),
    }


def choose_recommended_scale(calibration_metrics: list[dict]) -> dict | None:
    visible = [
        item for item in calibration_metrics
        if item["metrics"].get("positive_pixel_count", 0) > 0
    ]
    if not visible:
        return None
    unsaturated = [
        item for item in visible
        if item["metrics"].get("saturated_area_ratio", 0.0) <= 0.001
    ]
    candidates = unsaturated if unsaturated else visible

    def score(item: dict) -> tuple[float, float, float]:
        metrics = item["metrics"]
        target_area = 0.01
        area_error = abs(metrics.get("positive_area_ratio", 0.0) - target_area)
        p95_error = abs(metrics.get("positive_delta_p95", 0.0) - 18.0) / 255.0
        saturation_penalty = metrics.get("saturated_area_ratio", 0.0) * 10.0
        return (saturation_penalty + area_error + p95_error, item["energy_scale"], -metrics.get("max_luma_delta", 0.0))

    return min(candidates, key=score)


def caustic_digest(summary: dict) -> dict:
    state = summary.get("inspection", {}).get("caustic_state", {})
    return {
        "transport_active": bool(state.get("transport_path_emission_active", False)),
        "transport_mesh_dielectric_lens_resolved_count": int(
            state.get("transport_mesh_dielectric_lens_resolved_count", 0)
        ),
        "transport_mesh_dielectric_lens_evaluated_path_count": int(
            state.get("transport_mesh_dielectric_lens_evaluated_path_count", 0)
        ),
        "transport_mesh_dielectric_lens_emitted_path_count": int(
            state.get("transport_mesh_dielectric_lens_emitted_path_count", 0)
        ),
        "surface_cache_record_count": int(state.get("surface_cache_record_count", 0)),
        "surface_cache_deposit_accepted_count": int(
            state.get("surface_cache_deposit_accepted_count", 0)
        ),
        "surface_cache_sample_contributing_count": int(
            state.get("surface_cache_sample_contributing_count", 0)
        ),
        "surface_cache_total_radiance_sum": sphere_mist.rgb_sum(
            state.get("surface_cache_total_radiance", {})
        ),
        "surface_cache_max_record_radiance": float(
            state.get("surface_cache_max_record_radiance", 0.0)
        ),
    }


def write_contact_sheet(path: Path, runs: list[dict]) -> None:
    images = []
    for run in runs:
        width, height, pixels = review_artifacts.read_bmp_rgb(Path(run["frame_path"]))
        images.append((width, height, pixels))
    if not images:
        return
    cell_width = images[0][0]
    cell_height = images[0][1]
    separator = 4
    sheet_width = cell_width * len(images) + separator * (len(images) - 1)
    rows = [[(20, 20, 22)] * sheet_width for _ in range(cell_height)]
    for image_i, (_width, _height, pixels) in enumerate(images):
        offset_x = image_i * (cell_width + separator)
        for y in range(cell_height):
            rows[y][offset_x:offset_x + cell_width] = pixels[y]
    review_artifacts.write_png_rgb(path, sheet_width, cell_height, rows)


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Imported Closed Lens Wall Preview",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- preview scene ready: `{report['proof_status']['preview_scene_ready']}`",
        f"- mesh traversal emitted: `{report['proof_status']['mesh_traversal_emitted']}`",
        f"- wall caustic visible: `{report['proof_status']['wall_caustic_visible']}`",
        f"- recommended preview scale: `{(report.get('calibration_recommendation') or {}).get('energy_scale')}`",
        f"- scene: `{report['scene_path']}`",
        f"- lens mesh: `{report['lens_mesh_path']}`",
        f"- contact sheet: `{report['contact_sheet_path']}`",
        f"- side-by-side diff: `{report['delta_artifacts']['side_by_side_diff16_path']}`",
        "",
        "## Topology",
        "",
    ]
    topology = report.get("lens_topology_audit", {})
    lines.append(
        f"- vertices `{topology.get('vertex_count', 0)}`, edges "
        f"`{topology.get('edge_count', 0)}`, triangles "
        f"`{topology.get('triangle_count', 0)}`, boundary "
        f"`{topology.get('boundary_edge_count', 0)}`, nonmanifold "
        f"`{topology.get('nonmanifold_edge_count', 0)}`, orientation mismatches "
        f"`{topology.get('orientation_mismatch_edge_count', 0)}`, volume "
        f"`{topology.get('absolute_volume', 0.0):.6f}`, usable "
        f"`{topology.get('usable_for_closed_traversal', False)}`"
    )
    lines.extend(["", "## Calibration Readback", ""])
    for item in report.get("calibration_metrics", []):
        wall = item.get("metrics", {})
        lines.append(
            f"- scale `{item.get('energy_scale', 0.0):.2f}`: "
            f"positive pixels `{wall.get('positive_pixel_count', 0)}`, "
            f"saturated pixels `{wall.get('saturated_pixel_count', 0)}`, "
            f"p95 positive delta `{wall.get('positive_delta_p95', 0.0):.4f}`, "
            f"max luma delta `{wall.get('max_luma_delta', 0.0):.4f}`, "
            f"PNG `{item.get('png_path')}`"
        )
    lines.extend(["", "## Runs", ""])
    for run in report.get("runs", []):
        digest = run["caustic"]
        lines.append(
            f"- `{run['cell_id']}`: mesh resolved "
            f"`{digest.get('transport_mesh_dielectric_lens_resolved_count', 0)}`, "
            f"mesh emitted "
            f"`{digest.get('transport_mesh_dielectric_lens_emitted_path_count', 0)}`, "
            f"surface records `{digest.get('surface_cache_record_count', 0)}`, "
            f"surface samples `{digest.get('surface_cache_sample_contributing_count', 0)}`, "
            f"surface radiance `{digest.get('surface_cache_total_radiance_sum', 0.0):.4f}`, "
            f"PNG `{run['png_path']}`"
        )
    if report.get("failures"):
        lines.extend(["", "## Failures", ""])
        lines.extend([f"- {failure}" for failure in report["failures"]])
    if report.get("preview_warnings"):
        lines.extend(["", "## Preview Warnings", ""])
        lines.extend([f"- {warning}" for warning in report["preview_warnings"]])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    cli = args.cli.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2
    review_root.mkdir(parents=True, exist_ok=True)
    clean_generated_review_root(review_root)

    scene_path, lens_path = write_preview_scene(review_root)
    topology = mesh_fixture.audit_runtime_mesh_topology(lens_path)
    failures = [
        f"lens_topology: {failure}"
        for failure in mesh_fixture.validate_mesh_topology_audit(topology)
    ]
    preview_warnings = []
    runs = []
    runs_by_cell = {}
    cell_ids = ["wall_no_caustic"] + [
        cell_id_for_energy_scale(scale) for scale in CALIBRATION_ENERGY_SCALES
    ]
    for cell_id in cell_ids:
        item = request_for_cell(review_root, scene_path, cell_id, args.debug_export)
        elapsed = render_request(cli, item["request_path"], item["summary_path"], args.skip_render)
        summary = load_json(item["summary_path"])
        frame_path, png_path = copy_frame_png(summary, review_root, cell_id)
        summary_copy = review_root / "summaries" / f"summary_{cell_id}.json"
        summary_copy.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(item["summary_path"], summary_copy)
        run = {
            "cell_id": cell_id,
            "request_path": str(item["request_path"]),
            "summary_path": str(item["summary_path"]),
            "summary_copy": str(summary_copy),
            "frame_path": str(frame_path),
            "png_path": str(png_path),
            "elapsed_seconds": elapsed,
            "caustic": caustic_digest(summary),
        }
        runs.append(run)
        runs_by_cell[cell_id] = run

    baseline = Path(runs_by_cell["wall_no_caustic"]["frame_path"])
    calibration_metrics = []
    for scale in CALIBRATION_ENERGY_SCALES:
        cell_id = cell_id_for_energy_scale(scale)
        run = runs_by_cell[cell_id]
        metrics = wall_delta_metrics(baseline, Path(run["frame_path"]))
        calibration_metrics.append({
            "cell_id": cell_id,
            "energy_scale": scale,
            "frame_path": run["frame_path"],
            "png_path": run["png_path"],
            "caustic": run["caustic"],
            "metrics": metrics,
        })

    recommended = choose_recommended_scale(calibration_metrics)
    selected_cell_id = recommended["cell_id"] if recommended else cell_id_for_energy_scale(CALIBRATION_ENERGY_SCALES[-1])
    caustic = Path(runs_by_cell[selected_cell_id]["frame_path"])
    wall_metrics = wall_delta_metrics(baseline, caustic)
    delta_artifacts = write_delta_artifacts(review_root, baseline, caustic)
    contact_sheet_path = review_root / "imported_lens_wall_preview_contact_sheet.png"
    write_contact_sheet(contact_sheet_path, runs)

    visible_items = [
        item for item in calibration_metrics
        if item["metrics"].get("positive_pixel_count", 0) > 0
    ]
    if not visible_items:
        failures.append("no calibration scale brightened any receiver pixels")
    emitted_items = [
        item for item in calibration_metrics
        if item["caustic"].get("transport_mesh_dielectric_lens_emitted_path_count", 0) > 0
    ]
    if not emitted_items:
        failures.append("no calibration scale emitted mesh dielectric paths")
    deposited_items = [
        item for item in calibration_metrics
        if item["caustic"].get("surface_cache_record_count", 0) > 0
    ]
    if not deposited_items:
        failures.append("no calibration scale recorded surface-cache deposits")
    if recommended and recommended["metrics"].get("saturated_area_ratio", 0.0) > 0.001:
        preview_warnings.append("recommended scale still has measurable saturation")

    caustic_digest_data = recommended["caustic"] if recommended else calibration_metrics[-1]["caustic"]

    proof_status = {
        "preview_scene_ready": (
            len(failures) == 0
            and bool(contact_sheet_path.exists())
            and bool(delta_artifacts.get("side_by_side_diff16_path"))
        ),
        "mesh_traversal_emitted": (
            caustic_digest_data["transport_mesh_dielectric_lens_emitted_path_count"] > 0
        ),
        "surface_cache_deposited": caustic_digest_data["surface_cache_record_count"] > 0,
        "wall_caustic_visible": wall_metrics["positive_pixel_count"] > 0,
    }

    report = {
        "schema_version": "ray_tracing_imported_lens_wall_preview_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "scene_path": str(scene_path),
        "lens_mesh_path": str(lens_path),
        "lens_topology_audit": topology,
        "contact_sheet_path": str(contact_sheet_path),
        "delta_artifacts": delta_artifacts,
        "wall_delta_metrics": wall_metrics,
        "calibration_energy_scales": list(CALIBRATION_ENERGY_SCALES),
        "calibration_metrics": calibration_metrics,
        "calibration_recommendation": recommended,
        "runs": runs,
        "proof_status": proof_status,
        "preview_warnings": preview_warnings,
        "failures": failures,
        "passed": len(failures) == 0,
    }
    report_path = review_root / "imported_lens_wall_preview_report.json"
    write_json(report_path, report)
    write_index(review_root / "imported_lens_wall_preview_index.md", report)
    print(report_path)
    print(contact_sheet_path)
    print(delta_artifacts["side_by_side_diff16_path"])
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
