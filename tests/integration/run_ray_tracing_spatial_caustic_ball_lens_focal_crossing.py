#!/usr/bin/env python3
"""Render a ball-lens receiver sweep that should show focus crossing."""

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
import run_ray_tracing_spatial_caustic_imported_lens_wall_preview as wall_preview  # noqa: E402
import run_ray_tracing_spatial_caustic_lens_focal_sweep_diagnostic as focal_sweep  # noqa: E402
import run_ray_tracing_spatial_caustic_mesh_dielectric_lens_fixture as mesh_fixture  # noqa: E402
import run_ray_tracing_spatial_caustic_plano_convex_heatmap_diagnostic as heatmap_diag  # noqa: E402


LENS_Y = -1.05
LIGHT_Y = -8.25
LIGHT_INTENSITY = 24.0
RECEIVER_Y_POSITIONS = (-0.55, -0.30, -0.05, 0.20, 0.45, 0.70, 0.95, 1.25, 1.60, 2.00, 2.45, 2.95)
CAUSTIC_SURFACE_ENERGY_SCALE = 0.0025
CAUSTIC_SURFACE_FOOTPRINT_SCALE = 1.5
BALL_LENS_IOR = 1.20
BALL_LENS_RADIUS = 0.48
RECEIVER_MAP_X_RANGE = (-2.4, 2.4)
RECEIVER_MAP_Z_RANGE = (-1.3, 3.8)
REFERENCE_FOCUS_DISTANCE_TOLERANCE = 0.35


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
        / "caustic_ball_lens_focal_crossing"
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


def clean_review_root(review_root: Path) -> None:
    for child in review_root.iterdir() if review_root.exists() else []:
        if child.is_dir():
            shutil.rmtree(child)
        else:
            child.unlink()


def receiver_y_token(receiver_y: float) -> str:
    sign = "m" if receiver_y < 0.0 else "p"
    return f"wall_y_{sign}{abs(receiver_y):.2f}".replace(".", "p")


def receiver_distance(receiver_y: float) -> float:
    return receiver_y - LENS_Y


def write_ball_lens_asset(mesh_dir: Path) -> Path:
    mesh_dir.mkdir(parents=True, exist_ok=True)
    segments = 96
    lat_steps = 32
    radius = 1.0
    vertices: list[dict] = []
    rings: list[list[int]] = []

    top_index = len(vertices)
    vertices.append({"x": 0.0, "y": radius, "z": 0.0})
    for lat in range(1, lat_steps):
        phi = math.pi * float(lat) / float(lat_steps)
        y = radius * math.cos(phi)
        radial = radius * math.sin(phi)
        ring_indices = []
        for i in range(segments):
            theta = 2.0 * math.pi * float(i) / float(segments)
            ring_indices.append(len(vertices))
            vertices.append({
                "x": radial * math.cos(theta),
                "y": y,
                "z": radial * math.sin(theta),
            })
        rings.append(ring_indices)
    bottom_index = len(vertices)
    vertices.append({"x": 0.0, "y": -radius, "z": 0.0})

    front_triangles: list[dict] = []
    back_triangles: list[dict] = []

    def append_triangle(a: int, b: int, c: int) -> None:
        avg_y = (float(vertices[a]["y"]) + float(vertices[b]["y"]) + float(vertices[c]["y"])) / 3.0
        group_id = "lens_front" if avg_y < 0.0 else "lens_back"
        target = front_triangles if group_id == "lens_front" else back_triangles
        target.append({"a": a, "b": b, "c": c, "surface_group_id": group_id})

    first_ring = rings[0]
    for i in range(segments):
        n = (i + 1) % segments
        append_triangle(top_index, first_ring[n], first_ring[i])

    for lat in range(len(rings) - 1):
        upper = rings[lat]
        lower = rings[lat + 1]
        for i in range(segments):
            n = (i + 1) % segments
            append_triangle(upper[i], upper[n], lower[n])
            append_triangle(upper[i], lower[n], lower[i])

    last_ring = rings[-1]
    for i in range(segments):
        n = (i + 1) % segments
        append_triangle(last_ring[i], last_ring[n], bottom_index)

    triangles = front_triangles + back_triangles
    asset = {
        "schema_family": "codework_geometry",
        "schema_variant": "mesh_asset_runtime_v1",
        "schema_version": 1,
        "asset_id": "asset_ball_lens_focal_crossing_96x32",
        "source_asset_id": "generated_ball_lens_focal_crossing",
        "asset_type": "solid_mesh",
        "compile_meta": {
            "profile": "runtime_default",
            "generator": Path(__file__).name,
            "shape": "closed_ball_lens",
            "axis": "y",
            "segments": segments,
            "lat_steps": lat_steps,
            "fidelity": "focal_crossing_diagnostic",
        },
        "local_bounds": {
            "min": {"x": -radius, "y": -radius, "z": -radius},
            "max": {"x": radius, "y": radius, "z": radius},
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
                "semantic": "incident_spherical_surface",
                "triangle_span": {"start": 0, "count": len(front_triangles)},
            },
            {
                "group_id": "lens_back",
                "semantic": "exit_spherical_surface",
                "triangle_span": {"start": len(front_triangles), "count": len(back_triangles)},
            },
        ],
        "topology_flags": {
            "closed_volume": True,
            "manifold_expected": True,
        },
        "extensions": {},
    }
    mesh_path = mesh_dir / "asset_ball_lens_focal_crossing_96x32.runtime.json"
    write_json(mesh_path, asset)
    return mesh_path


def wall_plane_at(receiver_y: float) -> dict:
    wall = wall_preview.sphere_mist.plane_object(
        "dark_receiver_wall",
        {
            "origin": {"x": 0.0, "y": receiver_y, "z": 1.25},
            "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
            "axis_v": {"x": 0.0, "y": 0.0, "z": 1.0},
            "normal": {"x": 0.0, "y": -1.0, "z": 0.0},
        },
        8.5,
        5.8,
        "mat_dark_receiver_wall",
    )
    wall["transform"]["position"] = {"x": 0.0, "y": receiver_y, "z": 1.25}
    return wall


def write_scene(review_root: Path, receiver_y: float) -> tuple[Path, Path]:
    scene_dir = review_root / "generated_scenes" / receiver_y_token(receiver_y)
    mesh_dir = scene_dir / "assets" / "mesh_assets"
    lens_path = write_ball_lens_asset(mesh_dir)
    scene = {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"caustic_ball_lens_focal_crossing_{receiver_y_token(receiver_y)}",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            wall_plane_at(receiver_y),
            {
                "object_id": "ball_lens_focal_crossing",
                "object_type": "mesh_asset_instance",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": {"x": 0.0, "y": LENS_Y, "z": 1.25},
                    "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "scale": {"x": 0.48, "y": 0.48, "z": 0.48},
                },
                "geometry_ref": {
                    "kind": "mesh_asset",
                    "id": "asset_ball_lens_focal_crossing_96x32",
                    "variant": "runtime_default",
                },
                "extensions": {
                    "line_drawing": {
                        "runtime_mesh_path": "assets/mesh_assets/asset_ball_lens_focal_crossing_96x32.runtime.json",
                    }
                },
                "material_id": "mat_dense_ball_glass",
                "flags": {"visible": True, "locked": False, "selectable": True},
            },
        ],
        "materials": [
            {"material_id": "mat_dark_receiver_wall", "kind": "lambert", "albedo": [0.0, 0.035, 0.16]},
            {"material_id": "mat_dense_ball_glass", "kind": "dielectric", "albedo": [0.97, 0.99, 1.0]},
        ],
        "lights": [
            {
                "light_id": "distant_axis_light",
                "kind": "sphere",
                "position": {"x": 0.0, "y": LIGHT_Y, "z": 1.25},
                "radius": 0.035,
                "intensity": LIGHT_INTENSITY,
                "falloff_distance": 12.0,
                "color": {"r": 1.0, "g": 0.96, "b": 0.88},
                "enabled": True,
                "moving": False,
            }
        ],
        "cameras": [
            {
                "camera_id": "ball_lens_focal_crossing_camera",
                "kind": "perspective",
                "position": {"x": 2.35, "y": -4.15, "z": 1.62},
                "target": {"x": 0.0, "y": 0.80, "z": 1.24},
                "yaw": 0.0,
                "look_pitch": 0.0,
            }
        ],
        "constraints": [],
        "hierarchy": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": {"x": 0.0, "y": 0.80, "z": 1.24},
                    "environment": {
                        "light_mode": 1,
                        "ambient_strength": 0.06,
                        "top_fill_strength": 0.02,
                    },
                    "object_materials": [
                        {
                            "object_id": "dark_receiver_wall",
                            "material_id": 0,
                            "object_color": wall_preview.sphere_mist.rgb_u24(0, 18, 74),
                            "roughness": 0.82,
                            "reflectivity": 0.0,
                            "alpha": 1.0,
                        },
                        {
                            "object_id": "ball_lens_focal_crossing",
                            "material_id": 5,
                            "object_color": wall_preview.sphere_mist.rgb_u24(158, 236, 255),
                            "alpha": 0.15,
                            "glass_transport_override": True,
                            "glass_transmission": 0.88,
                            "glass_ior": BALL_LENS_IOR,
                            "glass_absorption_distance": 5.0,
                            "glass_thin_walled": False,
                            "roughness": 0.002,
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


def request_for_cell(review_root: Path,
                     scene_path: Path,
                     receiver_y: float,
                     caustics_enabled: bool,
                     debug_export: bool) -> dict:
    suffix = "caustic" if caustics_enabled else "off"
    cell_id = f"ball_lens_{receiver_y_token(receiver_y)}_{suffix}"
    output_root = review_root / "runs" / cell_id
    summary_path = output_root / "render_summary.json"
    request = wall_preview.base_request(
        f"caustic_ball_lens_focal_crossing_{cell_id}",
        scene_path,
        output_root,
        summary_path,
    )
    request["render"]["width"] = 256
    request["render"]["height"] = 160
    request["inspection"].update({
        "camera_position": {"x": 2.35, "y": -4.15, "z": 1.62},
        "camera_look_at": {"x": 0.0, "y": 0.80, "z": 1.24},
        "camera_zoom": 0.50,
        "ambient_strength": 0.06,
        "top_fill_strength": 0.02,
        "background_brightness": 0.006,
        "background_color": {"r": 0.004, "g": 0.006, "b": 0.010},
        "light_position": {"x": 0.0, "y": LIGHT_Y, "z": 1.25},
        "light_intensity": LIGHT_INTENSITY,
        "light_radius": 0.035,
    })
    if not caustics_enabled:
        request["inspection"].update({
            "caustic_mode": "off",
            "caustic_volume_enabled": False,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 0,
        })
    else:
        request["inspection"].update({
            "caustic_mode": "transport",
            "caustic_volume_enabled": False,
            "caustic_surface_enabled": True,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 4096,
            "caustic_max_path_depth": 2,
            "caustic_surface_energy_scale": CAUSTIC_SURFACE_ENERGY_SCALE,
            "caustic_surface_footprint_scale": CAUSTIC_SURFACE_FOOTPRINT_SCALE,
            "caustic_surface_receiver_fallback_enabled": False,
            "caustic_transport_emission_policy": "mesh_dielectric_lens",
            "caustic_lens_traversal_profile": {
                "outside_ior": 1.0,
                "material_ior": BALL_LENS_IOR,
                "fresnel_scale": 1.0,
                "transmission_scale": 1.0,
                "tint": {"r": 1.0, "g": 0.99, "b": 0.96},
                "absorption_distance": 12.0,
                "aperture_radius_scale": 1.0,
            },
        })
        if debug_export:
            request["inspection"]["caustic_transport_debug_export_enabled"] = True
    request_path = review_root / "generated_requests" / f"request_{cell_id}.json"
    write_json(request_path, request)
    return {"cell_id": cell_id, "request_path": request_path, "summary_path": summary_path}


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


def image_tuple(path: Path) -> tuple[int, int, list[list[tuple[int, int, int]]]]:
    return review_artifacts.read_bmp_rgb(path)


def vec_from_json(value: dict | None) -> tuple[float, float, float]:
    value = value or {}
    return (
        float(value.get("x", 0.0)),
        float(value.get("y", 0.0)),
        float(value.get("z", 0.0)),
    )


def v_add(a: tuple[float, float, float],
          b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def v_sub(a: tuple[float, float, float],
          b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def v_scale(a: tuple[float, float, float], s: float) -> tuple[float, float, float]:
    return (a[0] * s, a[1] * s, a[2] * s)


def v_dot(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_len(a: tuple[float, float, float]) -> float:
    return math.sqrt(max(0.0, v_dot(a, a)))


def v_norm(a: tuple[float, float, float]) -> tuple[float, float, float] | None:
    length = v_len(a)
    if not length > 1.0e-12:
        return None
    return v_scale(a, 1.0 / length)


def intersect_sphere(origin: tuple[float, float, float],
                     direction: tuple[float, float, float],
                     center: tuple[float, float, float],
                     radius: float,
                     min_t: float) -> tuple[float, tuple[float, float, float]] | None:
    oc = v_sub(origin, center)
    b = 2.0 * v_dot(oc, direction)
    c = v_dot(oc, oc) - radius * radius
    disc = b * b - 4.0 * c
    if disc < 0.0:
        return None
    root = math.sqrt(disc)
    candidates = [(-b - root) * 0.5, (-b + root) * 0.5]
    for t in sorted(candidates):
        if t > min_t:
            return t, v_add(origin, v_scale(direction, t))
    return None


def refract_vector(incident: tuple[float, float, float],
                   normal: tuple[float, float, float],
                   eta_from: float,
                   eta_to: float) -> tuple[float, float, float] | None:
    i = v_norm(incident)
    n = v_norm(normal)
    if i is None or n is None or not eta_from > 0.0 or not eta_to > 0.0:
        return None
    cosi = max(-1.0, min(1.0, v_dot(i, n)))
    oriented_n = n
    if cosi < 0.0:
        cosi = -cosi
    else:
        oriented_n = v_scale(n, -1.0)
    eta = eta_from / eta_to
    k = 1.0 - eta * eta * (1.0 - cosi * cosi)
    if k < 0.0:
        return None
    refracted = v_add(v_scale(i, eta), v_scale(oriented_n, eta * cosi - math.sqrt(k)))
    return v_norm(refracted)


def intersect_y_plane(origin: tuple[float, float, float],
                      direction: tuple[float, float, float],
                      receiver_y: float) -> tuple[float, tuple[float, float, float]] | None:
    if abs(direction[1]) <= 1.0e-12:
        return None
    t = (receiver_y - origin[1]) / direction[1]
    if not t > 1.0e-9:
        return None
    return t, v_add(origin, v_scale(direction, t))


def reference_ball_lens_hit(record: dict, receiver_y: float) -> dict | None:
    center = (0.0, LENS_Y, 1.25)
    light = vec_from_json(record.get("light_position"))
    target = vec_from_json(record.get("target_position"))
    entry_dir = v_norm(v_sub(target, light))
    if entry_dir is None:
        return None
    entry = intersect_sphere(light, entry_dir, center, BALL_LENS_RADIUS, 1.0e-6)
    if entry is None:
        return None
    entry_t, entry_pos = entry
    entry_normal = v_norm(v_sub(entry_pos, center))
    if entry_normal is None:
        return None
    inside_dir = refract_vector(entry_dir, entry_normal, 1.0, BALL_LENS_IOR)
    if inside_dir is None:
        return None
    exit_origin = v_add(entry_pos, v_scale(inside_dir, 1.0e-5))
    exit_hit = intersect_sphere(exit_origin, inside_dir, center, BALL_LENS_RADIUS, 1.0e-5)
    if exit_hit is None:
        return None
    _exit_t, exit_pos = exit_hit
    exit_normal = v_norm(v_sub(exit_pos, center))
    if exit_normal is None:
        return None
    exit_dir = refract_vector(inside_dir, exit_normal, BALL_LENS_IOR, 1.0)
    if exit_dir is None:
        return None
    receiver_hit = intersect_y_plane(exit_pos, exit_dir, receiver_y)
    if receiver_hit is None:
        return None
    receiver_t, receiver_pos = receiver_hit
    weight = max(1.0e-6, heatmap_diag.vec_luma(record.get("surface_receiver_deposited_radiance") or record.get("throughput") or {}))
    return {
        "x": receiver_pos[0],
        "y": receiver_pos[1],
        "z": receiver_pos[2],
        "weight": weight,
        "entry_t": entry_t,
        "receiver_t": receiver_t,
    }


def reference_stats(hits: list[dict]) -> dict:
    if not hits:
        return {"count": 0, "hit_radius": 0.0}
    weight_sum = sum(float(hit.get("weight", 0.0)) for hit in hits)
    centroid_x = (
        sum(float(hit["x"]) * float(hit.get("weight", 0.0)) for hit in hits) / weight_sum
        if weight_sum > 0.0 else 0.0
    )
    centroid_z = (
        sum(float(hit["z"]) * float(hit.get("weight", 0.0)) for hit in hits) / weight_sum
        if weight_sum > 0.0 else 0.0
    )
    x_min = min(float(hit["x"]) for hit in hits)
    x_max = max(float(hit["x"]) for hit in hits)
    z_min = min(float(hit["z"]) for hit in hits)
    z_max = max(float(hit["z"]) for hit in hits)
    return {
        "count": len(hits),
        "x_min": x_min,
        "x_max": x_max,
        "z_min": z_min,
        "z_max": z_max,
        "hit_radius": 0.5 * math.hypot(x_max - x_min, z_max - z_min),
        "weighted_centroid": {"x": centroid_x, "z": centroid_z},
    }


def read_reference_hits(paths_path: Path, receiver_y: float) -> tuple[list[dict], dict]:
    hits = []
    if not paths_path.exists():
        return hits, reference_stats(hits)
    for line in paths_path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        hit = reference_ball_lens_hit(json.loads(line), receiver_y)
        if hit is not None and all(math.isfinite(float(hit[key])) for key in ("x", "y", "z")):
            hits.append(hit)
    return hits, reference_stats(hits)


def centroid_metrics(baseline_path: Path, caustic_path: Path) -> dict:
    width, height, baseline = review_artifacts.read_bmp_rgb(baseline_path)
    _cw, _ch, caustic = review_artifacts.read_bmp_rgb(caustic_path)
    weight_sum = 0.0
    x_sum = 0.0
    y_sum = 0.0
    for y in range(height):
        for x in range(width):
            before = sum(baseline[y][x]) / 3.0
            after = sum(caustic[y][x]) / 3.0
            weight = max(0.0, after - before)
            if weight <= 0.0:
                continue
            weight_sum += weight
            x_sum += weight * float(x)
            y_sum += weight * float(y)
    if weight_sum <= 0.0:
        return {"x": 0.0, "y": 0.0, "weight_sum": 0.0}
    return {"x": x_sum / weight_sum, "y": y_sum / weight_sum, "weight_sum": weight_sum}


def write_receiver_surface_map_png(path: Path,
                                   width: int,
                                   height: int,
                                   hits: list[dict]) -> None:
    accum = [[0.0 for _x in range(width)] for _y in range(height)]
    x0, x1 = RECEIVER_MAP_X_RANGE
    z0, z1 = RECEIVER_MAP_Z_RANGE
    for hit in hits:
        u = (float(hit["x"]) - x0) / (x1 - x0)
        v = (float(hit["z"]) - z0) / (z1 - z0)
        px = int(round(u * float(width - 1)))
        py = int(round((1.0 - v) * float(height - 1)))
        if px < 0 or py < 0 or px >= width or py >= height:
            continue
        for oy in (-2, -1, 0, 1, 2):
            for ox in (-2, -1, 0, 1, 2):
                qx = px + ox
                qy = py + oy
                if 0 <= qx < width and 0 <= qy < height:
                    distance = math.hypot(float(ox), float(oy))
                    falloff = max(0.0, 1.0 - distance / 3.0)
                    accum[qy][qx] += max(1.0e-6, float(hit.get("weight", 0.0))) * falloff
    max_value = max(max(row) for row in accum) if hits else 0.0
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            row.append(heatmap_diag.heat_color(accum[y][x] / max_value if max_value > 0.0 else 0.0))
        rows.append(row)
    path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(path, width, height, rows)


def curve_summary(rows: list[dict]) -> dict:
    radii = [float(row["footprint_radius"]) for row in rows]
    p95s = [float(row["metrics"].get("positive_delta_p95", 0.0)) for row in rows]
    distances = [float(row["receiver_distance"]) for row in rows]
    surface_radii = [float(row["debug_hit_stats"].get("hit_radius", 0.0)) for row in rows]
    reference_radii = [float(row["reference_hit_stats"].get("hit_radius", 0.0)) for row in rows]
    positive_counts = [int(row["metrics"].get("positive_pixel_count", 0)) for row in rows]
    if not radii:
        return {}
    min_index = min(range(len(radii)), key=lambda i: radii[i])
    surface_min_index = (
        min(range(len(surface_radii)), key=lambda i: surface_radii[i])
        if surface_radii
        else min_index
    )
    reference_min_index = (
        min(range(len(reference_radii)), key=lambda i: reference_radii[i])
        if reference_radii
        else min_index
    )
    left_radius = radii[0]
    right_radius = radii[-1]
    min_radius = radii[min_index]
    u_shape_score = min(left_radius, right_radius) - min_radius
    surface_u_shape_score = (
        min(surface_radii[0], surface_radii[-1]) - surface_radii[surface_min_index]
        if surface_radii
        else 0.0
    )
    reference_u_shape_score = (
        min(reference_radii[0], reference_radii[-1]) - reference_radii[reference_min_index]
        if reference_radii
        else 0.0
    )
    p95_peak_index = max(range(len(p95s)), key=lambda i: p95s[i]) if p95s else min_index
    return {
        "radii": radii,
        "positive_delta_p95": p95s,
        "receiver_distances": distances,
        "min_radius": min_radius,
        "min_radius_index": min_index,
        "min_radius_receiver_y": rows[min_index]["receiver_y"],
        "min_radius_distance": rows[min_index]["receiver_distance"],
        "radius_spread": max(radii) - min(radii),
        "surface_hit_radius_spread": max(surface_radii) - min(surface_radii) if surface_radii else 0.0,
        "surface_hit_radii": surface_radii,
        "surface_min_radius": surface_radii[surface_min_index] if surface_radii else 0.0,
        "surface_min_radius_index": surface_min_index,
        "surface_min_radius_receiver_y": rows[surface_min_index]["receiver_y"],
        "surface_min_radius_distance": rows[surface_min_index]["receiver_distance"],
        "reference_hit_radius_spread": max(reference_radii) - min(reference_radii) if reference_radii else 0.0,
        "reference_hit_radii": reference_radii,
        "reference_min_radius": reference_radii[reference_min_index] if reference_radii else 0.0,
        "reference_min_radius_index": reference_min_index,
        "reference_min_radius_receiver_y": rows[reference_min_index]["receiver_y"],
        "reference_min_radius_distance": rows[reference_min_index]["receiver_distance"],
        "u_shape_score": u_shape_score,
        "surface_u_shape_score": surface_u_shape_score,
        "reference_u_shape_score": reference_u_shape_score,
        "surface_reference_focus_distance_error": abs(
            float(rows[surface_min_index]["receiver_distance"]) -
            float(rows[reference_min_index]["receiver_distance"])
        ),
        "p95_peak_index": p95_peak_index,
        "p95_peak_receiver_y": rows[p95_peak_index]["receiver_y"],
        "p95_peak_distance": rows[p95_peak_index]["receiver_distance"],
        "p95_peak_value": p95s[p95_peak_index] if p95s else 0.0,
        "interior_minimum": 0 < min_index < (len(radii) - 1),
        "surface_interior_minimum": 0 < surface_min_index < (len(surface_radii) - 1),
        "reference_interior_minimum": 0 < reference_min_index < (len(reference_radii) - 1),
        "positive_pixel_counts": positive_counts,
        "positive_receiver_count": sum(1 for count in positive_counts if count > 0),
    }


def write_index(path: Path, report: dict) -> None:
    summary = report.get("curve_summary", {})
    lines = [
        "# Ball Lens Focal Crossing Diagnostic",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- comparison sheet: `{report['comparison_sheet_path']}`",
        f"- lens y: `{report['lens_y']}`",
        f"- light y: `{report['light_y']}`",
        f"- light intensity: `{report['light_intensity']}`",
        f"- receiver y positions: `{report['receiver_y_positions']}`",
        f"- caustic scale: `{report['caustic_surface_energy_scale']}`",
        f"- footprint scale: `{report['caustic_surface_footprint_scale']}`",
        f"- ball lens IOR: `{report['ball_lens_ior']}`",
        "",
        "## Curve Summary",
        "",
        f"- min footprint radius: `{summary.get('min_radius', 0.0):.4f}`",
        f"- min receiver distance: `{summary.get('min_radius_distance')}`",
        f"- radius spread: `{summary.get('radius_spread', 0.0):.4f}`",
        f"- surface hit radius spread: `{summary.get('surface_hit_radius_spread', 0.0):.4f}`",
        f"- surface min radius: `{summary.get('surface_min_radius', 0.0):.4f}`",
        f"- surface min receiver distance: `{summary.get('surface_min_radius_distance')}`",
        f"- reference hit radius spread: `{summary.get('reference_hit_radius_spread', 0.0):.4f}`",
        f"- reference min radius: `{summary.get('reference_min_radius', 0.0):.4f}`",
        f"- reference min receiver distance: `{summary.get('reference_min_radius_distance')}`",
        f"- surface/reference focus distance error: `{summary.get('surface_reference_focus_distance_error', 0.0):.4f}`",
        f"- u-shape score: `{summary.get('u_shape_score', 0.0):.4f}`",
        f"- surface u-shape score: `{summary.get('surface_u_shape_score', 0.0):.4f}`",
        f"- reference u-shape score: `{summary.get('reference_u_shape_score', 0.0):.4f}`",
        f"- p95 peak distance: `{summary.get('p95_peak_distance')}`",
        f"- interior minimum: `{summary.get('interior_minimum')}`",
        f"- surface interior minimum: `{summary.get('surface_interior_minimum')}`",
        f"- reference interior minimum: `{summary.get('reference_interior_minimum')}`",
        "",
        "## Rows",
        "",
    ]
    for row in report.get("sweep_rows", []):
        lines.append(
            f"- receiver y `{row['receiver_y']}` distance `{row['receiver_distance']:.3f}`: "
            f"positive `{row['metrics'].get('positive_pixel_count', 0)}`, radius "
            f"`{row.get('footprint_radius', 0.0):.4f}`, p95 "
            f"`{row['metrics'].get('positive_delta_p95', 0.0):.4f}`, emitted "
            f"`{row['caustic'].get('transport_mesh_dielectric_lens_emitted_path_count', 0)}`, "
            f"surface hits `{row['debug_hit_stats'].get('surface_receiver_count', 0)}`, "
            f"surface radius `{row['debug_hit_stats'].get('hit_radius', 0.0):.4f}`, "
            f"reference radius `{row['reference_hit_stats'].get('hit_radius', 0.0):.4f}`"
        )
    if report.get("failures"):
        lines.extend(["", "## Failures", ""])
        lines.extend([f"- {failure}" for failure in report["failures"]])
    if report.get("warnings"):
        lines.extend(["", "## Warnings", ""])
        lines.extend([f"- {warning}" for warning in report["warnings"]])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    cli = args.cli.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2
    review_root.mkdir(parents=True, exist_ok=True)
    clean_review_root(review_root)

    failures: list[str] = []
    warnings: list[str] = []
    sweep_rows: list[dict] = []
    render_images = []
    heatmap_images = []
    receiver_surface_images = []
    reference_surface_images = []
    topology = None

    for receiver_y in RECEIVER_Y_POSITIONS:
        scene_path, lens_path = write_scene(review_root, receiver_y)
        if topology is None:
            topology = mesh_fixture.audit_runtime_mesh_topology(lens_path)
            failures.extend([
                f"lens_topology: {failure}"
                for failure in mesh_fixture.validate_mesh_topology_audit(topology)
            ])

        off_request = request_for_cell(review_root, scene_path, receiver_y, False, args.debug_export)
        off_elapsed = render_request(cli, off_request["request_path"], off_request["summary_path"], args.skip_render)
        off_summary = load_json(off_request["summary_path"])
        off_frame, off_png = focal_sweep.copy_frame_png(off_summary, review_root, off_request["cell_id"])

        caustic_request = request_for_cell(review_root, scene_path, receiver_y, True, args.debug_export)
        caustic_elapsed = render_request(
            cli,
            caustic_request["request_path"],
            caustic_request["summary_path"],
            args.skip_render,
        )
        caustic_summary = load_json(caustic_request["summary_path"])
        caustic_frame, caustic_png = focal_sweep.copy_frame_png(caustic_summary, review_root, caustic_request["cell_id"])
        caustic = wall_preview.caustic_digest(caustic_summary)
        metrics = wall_preview.wall_delta_metrics(off_frame, caustic_frame)
        heat_w, heat_h, heat_pixels = focal_sweep.shape_compare.signed_heatmap_pixels(off_frame, caustic_frame)
        heatmap_path = review_root / "heatmaps" / f"{caustic_request['cell_id']}_signed_heatmap.png"
        heatmap_path.parent.mkdir(parents=True, exist_ok=True)
        review_artifacts.write_png_rgb(heatmap_path, heat_w, heat_h, heat_pixels)

        debug_path = Path(caustic_request["summary_path"]).parent / "caustic_transport_debug_paths.jsonl"
        debug_hits, debug_hit_stats = heatmap_diag.read_debug_hits(debug_path)
        reference_hits, reference_hit_stats = read_reference_hits(debug_path, receiver_y)
        receiver_surface_map_path = (
            review_root / "surface_receiver_maps" / f"{caustic_request['cell_id']}_surface_receiver_map.png"
        )
        reference_surface_map_path = (
            review_root / "reference_receiver_maps" / f"{caustic_request['cell_id']}_reference_receiver_map.png"
        )
        write_receiver_surface_map_png(receiver_surface_map_path, heat_w, heat_h, debug_hits)
        write_receiver_surface_map_png(reference_surface_map_path, heat_w, heat_h, reference_hits)
        hit_span_x = float(debug_hit_stats.get("x_max", 0.0)) - float(debug_hit_stats.get("x_min", 0.0))
        hit_span_z = float(debug_hit_stats.get("z_max", 0.0)) - float(debug_hit_stats.get("z_min", 0.0))
        debug_hit_stats["hit_radius"] = 0.5 * math.hypot(hit_span_x, hit_span_z)
        centroid = centroid_metrics(off_frame, caustic_frame)

        if caustic.get("transport_mesh_dielectric_lens_emitted_path_count", 0) <= 0:
            failures.append(f"{caustic_request['cell_id']} emitted zero mesh-dielectric paths")
        if caustic.get("surface_cache_record_count", 0) <= 0:
            failures.append(f"{caustic_request['cell_id']} recorded zero surface-cache deposits")
        if debug_hit_stats.get("surface_receiver_count", 0) <= 0:
            failures.append(f"{caustic_request['cell_id']} exported zero actual surface receiver hits")
        if reference_hit_stats.get("count", 0) <= 0:
            failures.append(f"{caustic_request['cell_id']} computed zero analytic reference receiver hits")
        if metrics.get("saturated_area_ratio", 0.0) > 0.02:
            warnings.append(
                f"{caustic_request['cell_id']} has high saturated area "
                f"{metrics.get('saturated_area_ratio', 0.0):.4f}"
            )

        render_images.append(image_tuple(caustic_frame))
        heatmap_images.append((heat_w, heat_h, heat_pixels))
        receiver_surface_images.append(heatmap_diag.read_rgb_image(receiver_surface_map_path))
        reference_surface_images.append(heatmap_diag.read_rgb_image(reference_surface_map_path))
        sweep_rows.append({
            "receiver_y": receiver_y,
            "receiver_distance": receiver_distance(receiver_y),
            "scene_path": str(scene_path),
            "lens_mesh_path": str(lens_path),
            "baseline": {
                "cell_id": off_request["cell_id"],
                "request_path": str(off_request["request_path"]),
                "summary_path": str(off_request["summary_path"]),
                "frame_path": str(off_frame),
                "png_path": str(off_png),
                "elapsed_seconds": off_elapsed,
            },
            "caustic_frame_path": str(caustic_frame),
            "caustic_png_path": str(caustic_png),
            "heatmap_path": str(heatmap_path),
            "surface_receiver_map_path": str(receiver_surface_map_path),
            "reference_receiver_map_path": str(reference_surface_map_path),
            "elapsed_seconds": caustic_elapsed,
            "caustic": caustic,
            "metrics": metrics,
            "centroid": centroid,
            "debug_hit_stats": debug_hit_stats,
            "reference_hit_stats": reference_hit_stats,
            "footprint_radius": focal_sweep.footprint_radius(metrics),
        })

    curve = curve_summary(sweep_rows)
    if curve.get("positive_receiver_count", 0) <= 0:
        failures.append("ball lens focal crossing brightened zero receiver positions")
    if curve.get("radius_spread", 0.0) <= 1.0:
        warnings.append("ball lens focal crossing did not produce a clear footprint-radius spread")
    if curve.get("surface_hit_radius_spread", 0.0) <= 1.0:
        warnings.append("ball lens focal crossing did not produce a clear actual surface-hit spread")
    if curve.get("reference_hit_radius_spread", 0.0) <= 1.0:
        failures.append("ball lens reference bundle did not produce a clear focal spread")
    if not curve.get("surface_interior_minimum", False):
        warnings.append("ball lens focal crossing did not place the actual surface-hit minimum inside the receiver sweep")
    if not curve.get("reference_interior_minimum", False):
        failures.append("ball lens reference bundle did not place the focal minimum inside the receiver sweep")
    if curve.get("surface_u_shape_score", 0.0) <= 0.05:
        warnings.append("ball lens focal crossing actual surface-hit U-shape score is weak")
    if curve.get("reference_u_shape_score", 0.0) <= 0.05:
        failures.append("ball lens reference bundle U-shape score is weak")
    if curve.get("surface_reference_focus_distance_error", 999.0) > REFERENCE_FOCUS_DISTANCE_TOLERANCE:
        failures.append(
            "ball lens measured focus distance differs from analytic reference "
            f"by {curve.get('surface_reference_focus_distance_error', 999.0):.4f}"
        )

    comparison_sheet_path = review_root / "ball_lens_focal_crossing_sheet.png"
    focal_sweep.write_sheet(comparison_sheet_path, [receiver_surface_images, reference_surface_images, heatmap_images])
    report = {
        "schema_version": "ray_tracing_ball_lens_focal_crossing_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "lens_y": LENS_Y,
        "light_y": LIGHT_Y,
        "light_intensity": LIGHT_INTENSITY,
        "ball_lens_ior": BALL_LENS_IOR,
        "receiver_y_positions": list(RECEIVER_Y_POSITIONS),
        "caustic_surface_energy_scale": CAUSTIC_SURFACE_ENERGY_SCALE,
        "caustic_surface_footprint_scale": CAUSTIC_SURFACE_FOOTPRINT_SCALE,
        "receiver_map_x_range": list(RECEIVER_MAP_X_RANGE),
        "receiver_map_z_range": list(RECEIVER_MAP_Z_RANGE),
        "reference_focus_distance_tolerance": REFERENCE_FOCUS_DISTANCE_TOLERANCE,
        "comparison_sheet_path": str(comparison_sheet_path),
        "topology": topology,
        "curve_summary": curve,
        "sweep_rows": sweep_rows,
        "failures": failures,
        "warnings": warnings,
        "passed": len(failures) == 0,
    }
    report_path = review_root / "ball_lens_focal_crossing_report.json"
    write_json(report_path, report)
    write_index(review_root / "ball_lens_focal_crossing_index.md", report)
    print(report_path)
    print(comparison_sheet_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
