#!/usr/bin/env python3
"""Render same-distance closed-lens shape caustic comparisons."""

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
import run_ray_tracing_spatial_caustic_mesh_dielectric_lens_fixture as mesh_fixture  # noqa: E402
import run_ray_tracing_spatial_caustic_plano_convex_lens_distance_matrix as plano_convex  # noqa: E402


LENS_Y = -1.05
CAUSTIC_SURFACE_ENERGY_SCALE = 0.0025
LENS_SHAPES = ("flat_slab", "biconvex", "plano_convex", "biconcave")


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
        / "caustic_lens_shape_comparison"
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


def lens_mesh_spec(shape_id: str) -> dict:
    if shape_id == "flat_slab":
        return {
            "asset_id": "asset_shape_compare_flat_slab_lens_96x12",
            "source_asset_id": "shape_compare_flat_slab",
            "shape": "closed_flat_slab_lens",
            "front_semantic": "incident_flat_surface",
            "back_semantic": "exit_flat_surface",
            "front_profile": "flat",
            "back_profile": "flat",
            "center_half_thickness": 0.08,
            "rim_half_thickness": 0.08,
            "path": "asset_shape_compare_flat_slab_lens_96x12.runtime.json",
        }
    if shape_id == "biconcave":
        return {
            "asset_id": "asset_shape_compare_biconcave_lens_96x12",
            "source_asset_id": "shape_compare_biconcave",
            "shape": "closed_biconcave_lens",
            "front_semantic": "incident_concave_surface",
            "back_semantic": "exit_concave_surface",
            "front_profile": "concave",
            "back_profile": "concave",
            "center_half_thickness": 0.055,
            "rim_half_thickness": 0.28,
            "path": "asset_shape_compare_biconcave_lens_96x12.runtime.json",
        }
    raise ValueError(f"unsupported generated lens shape: {shape_id}")


def write_radial_lens_asset(mesh_dir: Path, shape_id: str) -> Path:
    mesh_dir.mkdir(parents=True, exist_ok=True)
    spec = lens_mesh_spec(shape_id)
    segments = 96
    radial_rings = 12
    aperture_radius = 1.0
    center_half = float(spec["center_half_thickness"])
    rim_half = float(spec["rim_half_thickness"])
    vertices: list[dict] = []

    def half_thickness(radial_t: float) -> float:
        return center_half + (rim_half - center_half) * radial_t * radial_t

    def add_surface(sign: float) -> tuple[int, list[list[int]]]:
        center_index = len(vertices)
        vertices.append({"x": 0.0, "y": sign * center_half, "z": 0.0})
        rings: list[list[int]] = []
        for ring in range(1, radial_rings + 1):
            radial_t = float(ring) / float(radial_rings)
            y = sign * half_thickness(radial_t)
            ring_indices = []
            for i in range(segments):
                theta = (2.0 * math.pi * float(i)) / float(segments)
                ring_indices.append(len(vertices))
                vertices.append({
                    "x": aperture_radius * radial_t * math.cos(theta),
                    "y": y,
                    "z": aperture_radius * radial_t * math.sin(theta),
                })
            rings.append(ring_indices)
        return center_index, rings

    front_center, front_rings = add_surface(-1.0)
    back_center, back_rings = add_surface(1.0)
    triangles: list[dict] = []
    for i in range(segments):
        n = (i + 1) % segments
        triangles.append({
            "a": front_center,
            "b": front_rings[0][i],
            "c": front_rings[0][n],
            "surface_group_id": "lens_front",
        })
        triangles.append({
            "a": back_center,
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
            triangles.append({"a": front_inner[i], "b": front_outer[i], "c": front_outer[n], "surface_group_id": "lens_front"})
            triangles.append({"a": front_inner[i], "b": front_outer[n], "c": front_inner[n], "surface_group_id": "lens_front"})
            triangles.append({"a": back_inner[i], "b": back_outer[n], "c": back_outer[i], "surface_group_id": "lens_back"})
            triangles.append({"a": back_inner[i], "b": back_inner[n], "c": back_outer[n], "surface_group_id": "lens_back"})
    front_outer = front_rings[-1]
    back_outer = back_rings[-1]
    for i in range(segments):
        n = (i + 1) % segments
        triangles.append({"a": front_outer[i], "b": back_outer[n], "c": front_outer[n], "surface_group_id": "lens_rim"})
        triangles.append({"a": front_outer[i], "b": back_outer[i], "c": back_outer[n], "surface_group_id": "lens_rim"})

    surface_tri_count = segments + (radial_rings - 1) * segments * 2
    rim_start = surface_tri_count * 2
    asset = {
        "schema_family": "codework_geometry",
        "schema_variant": "mesh_asset_runtime_v1",
        "schema_version": 1,
        "asset_id": spec["asset_id"],
        "source_asset_id": spec["source_asset_id"],
        "asset_type": "solid_mesh",
        "compile_meta": {
            "profile": "runtime_default",
            "generator": Path(__file__).name,
            "shape": spec["shape"],
            "axis": "y",
            "segments": segments,
            "radial_rings": radial_rings,
            "aperture_radius": aperture_radius,
            "center_half_thickness": center_half,
            "rim_half_thickness": rim_half,
            "fidelity": "shape_comparison_preview",
        },
        "local_bounds": {
            "min": {"x": -aperture_radius, "y": -rim_half, "z": -aperture_radius},
            "max": {"x": aperture_radius, "y": rim_half, "z": aperture_radius},
        },
        "mesh": {
            "vertex_count": len(vertices),
            "triangle_count": len(triangles),
            "vertices": vertices,
            "triangles": triangles,
        },
        "surface_groups": [
            {"group_id": "lens_front", "semantic": spec["front_semantic"], "triangle_span": {"start": 0, "count": surface_tri_count}},
            {"group_id": "lens_back", "semantic": spec["back_semantic"], "triangle_span": {"start": surface_tri_count, "count": surface_tri_count}},
            {"group_id": "lens_rim", "semantic": "closed_lens_rim", "triangle_span": {"start": rim_start, "count": segments * 2}},
        ],
        "topology_flags": {
            "closed_volume": True,
            "manifold_expected": True,
        },
        "extensions": {},
    }
    mesh_path = mesh_dir / str(spec["path"])
    write_json(mesh_path, asset)
    return mesh_path


def write_shape_lens_asset(mesh_dir: Path, shape_id: str) -> Path:
    if shape_id == "biconvex":
        return wall_preview.write_biconvex_lens_asset(mesh_dir)
    if shape_id == "plano_convex":
        return plano_convex.write_plano_convex_lens_asset(mesh_dir)
    return write_radial_lens_asset(mesh_dir, shape_id)


def asset_id_for_shape(shape_id: str) -> str:
    if shape_id == "biconvex":
        return "asset_imported_biconvex_lens_96x16"
    if shape_id == "plano_convex":
        return "asset_imported_plano_convex_lens_128x20"
    return str(lens_mesh_spec(shape_id)["asset_id"])


def runtime_mesh_filename_for_shape(shape_id: str) -> str:
    if shape_id == "biconvex":
        return "asset_imported_biconvex_lens_96x16.runtime.json"
    if shape_id == "plano_convex":
        return "asset_imported_plano_convex_lens_128x20.runtime.json"
    return str(lens_mesh_spec(shape_id)["path"])


def lens_object_id(shape_id: str) -> str:
    return f"shape_compare_{shape_id}_lens"


def write_shape_scene(review_root: Path, shape_id: str) -> tuple[Path, Path]:
    scene_dir = review_root / "generated_scenes" / shape_id
    mesh_dir = scene_dir / "assets" / "mesh_assets"
    mesh_dir.mkdir(parents=True, exist_ok=True)
    lens_path = write_shape_lens_asset(mesh_dir, shape_id)
    object_id = lens_object_id(shape_id)
    scene = {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"caustic_lens_shape_compare_{shape_id}",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            wall_preview.wall_plane(),
            {
                "object_id": object_id,
                "object_type": "mesh_asset_instance",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": {"x": 0.0, "y": LENS_Y, "z": 1.25},
                    "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "scale": {"x": 0.54, "y": 0.54, "z": 0.54},
                },
                "geometry_ref": {
                    "kind": "mesh_asset",
                    "id": asset_id_for_shape(shape_id),
                    "variant": "runtime_default",
                },
                "extensions": {
                    "line_drawing": {
                        "runtime_mesh_path": f"assets/mesh_assets/{runtime_mesh_filename_for_shape(shape_id)}",
                    }
                },
                "material_id": "mat_lens_dense_glass",
                "flags": {"visible": True, "locked": False, "selectable": True},
            },
        ],
        "materials": [
            {"material_id": "mat_vivid_wall", "kind": "lambert", "albedo": [0.0, 0.30, 0.85]},
            {"material_id": "mat_lens_dense_glass", "kind": "dielectric", "albedo": [0.96, 0.98, 1.0]},
        ],
        "lights": [
            {
                "light_id": "shape_compare_light",
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
                "camera_id": "shape_compare_camera",
                "kind": "perspective",
                "position": {"x": 1.95, "y": -3.25, "z": 1.52},
                "target": {"x": 0.0, "y": LENS_Y, "z": 1.24},
                "yaw": 0.0,
                "look_pitch": 0.0,
            }
        ],
        "constraints": [],
        "hierarchy": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": {"x": 0.0, "y": LENS_Y, "z": 1.24},
                    "environment": {
                        "light_mode": 1,
                        "ambient_strength": 0.18,
                        "top_fill_strength": 0.08,
                    },
                    "object_materials": [
                        {
                            "object_id": "vivid_receiver_wall",
                            "material_id": 0,
                            "object_color": wall_preview.sphere_mist.rgb_u24(0, 78, 220),
                            "roughness": 0.78,
                            "reflectivity": 0.01,
                            "alpha": 1.0,
                        },
                        {
                            "object_id": object_id,
                            "material_id": 5,
                            "object_color": wall_preview.sphere_mist.rgb_u24(158, 236, 255),
                            "alpha": 0.16,
                            "glass_transport_override": True,
                            "glass_transmission": 0.82,
                            "glass_ior": 1.52,
                            "glass_absorption_distance": 4.0,
                            "glass_thin_walled": True,
                            "roughness": 0.004,
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


def request_for_cell(review_root: Path, scene_path: Path, shape_id: str, caustics_enabled: bool, debug_export: bool) -> dict:
    suffix = "caustic" if caustics_enabled else "off"
    cell_id = f"{shape_id}_{suffix}"
    output_root = review_root / "runs" / cell_id
    summary_path = output_root / "render_summary.json"
    request = wall_preview.base_request(
        f"caustic_lens_shape_comparison_{cell_id}",
        scene_path,
        output_root,
        summary_path,
    )
    request["inspection"]["camera_look_at"] = {"x": 0.0, "y": LENS_Y, "z": 1.24}
    request["inspection"]["camera_position"] = {"x": 1.95, "y": -3.25, "z": 1.52}
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
            "caustic_sample_budget": 2048,
            "caustic_max_path_depth": 2,
            "caustic_surface_energy_scale": CAUSTIC_SURFACE_ENERGY_SCALE,
            "caustic_surface_footprint_scale": 5.0,
            "caustic_surface_receiver_fallback_enabled": False,
            "caustic_transport_emission_policy": "mesh_dielectric_lens",
            "caustic_lens_traversal_preset": "dense_glass",
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


def copy_frame_png(summary: dict, review_root: Path, cell_id: str) -> tuple[Path, Path]:
    frame_path = wall_preview.first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{cell_id}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return frame_path, png_path


def footprint_radius(metrics: dict) -> float:
    area = float(metrics.get("positive_pixel_count", 0))
    return (area / math.pi) ** 0.5 if area > 0.0 else 0.0


def centroid_distance(a: dict, b: dict) -> float:
    ac = a.get("positive_delta_centroid", {})
    bc = b.get("positive_delta_centroid", {})
    ax, ay = ac.get("x"), ac.get("y")
    bx, by = bc.get("x"), bc.get("y")
    if ax is None or ay is None or bx is None or by is None:
        return 0.0
    return math.hypot(float(ax) - float(bx), float(ay) - float(by))


def signed_heatmap_pixels(baseline_path: Path, caustic_path: Path) -> tuple[int, int, list[list[tuple[int, int, int]]]]:
    before_w, before_h, before = review_artifacts.read_bmp_rgb(baseline_path)
    after_w, after_h, after = review_artifacts.read_bmp_rgb(caustic_path)
    if (before_w, before_h) != (after_w, after_h):
        raise ValueError("heatmap frames must have matching dimensions")
    rows: list[list[tuple[int, int, int]]] = []
    for y in range(before_h):
        row = []
        for x in range(before_w):
            delta = wall_preview.luma(after[y][x]) - wall_preview.luma(before[y][x])
            if delta > 0.25:
                t = min(1.0, delta / 24.0)
                row.append((int(255.0 * t), int(210.0 * t), int(48.0 * t)))
            elif delta < -0.25:
                t = min(1.0, -delta / 24.0)
                row.append((int(32.0 * t), int(96.0 * t), int(255.0 * t)))
            else:
                row.append((0, 0, 0))
        rows.append(row)
    return before_w, before_h, rows


def heatmap_difference(a: list[list[tuple[int, int, int]]], b: list[list[tuple[int, int, int]]]) -> float:
    total = 0.0
    count = 0
    for row_a, row_b in zip(a, b):
        for pa, pb in zip(row_a, row_b):
            total += abs(pa[0] - pb[0]) + abs(pa[1] - pb[1]) + abs(pa[2] - pb[2])
            count += 3
    return total / float(max(1, count))


def write_sheet(path: Path, image_rows: list[list[tuple[int, int, list[list[tuple[int, int, int]]]]]]) -> None:
    if not image_rows or not image_rows[0]:
        return
    cell_width, cell_height = image_rows[0][0][0], image_rows[0][0][1]
    separator = 4
    sheet_width = cell_width * len(image_rows[0]) + separator * (len(image_rows[0]) - 1)
    sheet_height = cell_height * len(image_rows) + separator * (len(image_rows) - 1)
    rows = [[(20, 20, 22)] * sheet_width for _ in range(sheet_height)]
    for row_i, image_row in enumerate(image_rows):
        for col_i, (_width, _height, pixels) in enumerate(image_row):
            offset_x = col_i * (cell_width + separator)
            offset_y = row_i * (cell_height + separator)
            for y in range(cell_height):
                rows[offset_y + y][offset_x:offset_x + cell_width] = pixels[y]
    review_artifacts.write_png_rgb(path, sheet_width, sheet_height, rows)


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Lens Shape Caustic Comparison",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- comparison sheet: `{report['comparison_sheet_path']}`",
        f"- fixed lens y: `{report['lens_y']}`",
        f"- fixed caustic scale: `{report['caustic_surface_energy_scale']}`",
        f"- distribution spread detected: `{report['distribution_review']['spread_detected']}`",
        "",
        "## Shape Metrics",
        "",
    ]
    for row in report.get("shape_rows", []):
        metrics = row["metrics"]
        caustic = row["caustic"]
        lines.append(
            f"- `{row['shape_id']}`: emitted "
            f"`{caustic.get('transport_mesh_dielectric_lens_emitted_path_count', 0)}`, "
            f"deposits `{caustic.get('surface_cache_record_count', 0)}`, "
            f"positive `{metrics.get('positive_pixel_count', 0)}`, "
            f"p95+ `{metrics.get('positive_delta_p95', 0.0):.4f}`, "
            f"max `{metrics.get('max_luma_delta', 0.0):.4f}`, "
            f"radius `{row.get('footprint_radius', 0.0):.4f}`"
        )
    if report.get("failures"):
        lines.extend(["", "## Failures", ""])
        lines.extend([f"- {failure}" for failure in report["failures"]])
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
    shape_rows: list[dict] = []
    render_images = []
    heatmap_images = []
    heatmap_pixels_by_shape: dict[str, list[list[tuple[int, int, int]]]] = {}

    for shape_id in LENS_SHAPES:
        scene_path, lens_path = write_shape_scene(review_root, shape_id)
        topology = mesh_fixture.audit_runtime_mesh_topology(lens_path)
        failures.extend([
            f"{shape_id}: lens_topology: {failure}"
            for failure in mesh_fixture.validate_mesh_topology_audit(topology)
        ])

        off_request = request_for_cell(review_root, scene_path, shape_id, False, args.debug_export)
        elapsed = render_request(cli, off_request["request_path"], off_request["summary_path"], args.skip_render)
        off_summary = load_json(off_request["summary_path"])
        off_frame, off_png = copy_frame_png(off_summary, review_root, off_request["cell_id"])

        caustic_request = request_for_cell(review_root, scene_path, shape_id, True, args.debug_export)
        elapsed = render_request(cli, caustic_request["request_path"], caustic_request["summary_path"], args.skip_render)
        caustic_summary = load_json(caustic_request["summary_path"])
        caustic_frame, caustic_png = copy_frame_png(caustic_summary, review_root, caustic_request["cell_id"])
        caustic = wall_preview.caustic_digest(caustic_summary)
        metrics = wall_preview.wall_delta_metrics(off_frame, caustic_frame)
        heat_w, heat_h, heat_pixels = signed_heatmap_pixels(off_frame, caustic_frame)
        heatmap_path = review_root / "diffs" / f"{shape_id}_lens_no_caustic_vs_caustic_signed_heatmap.png"
        heatmap_path.parent.mkdir(parents=True, exist_ok=True)
        review_artifacts.write_png_rgb(heatmap_path, heat_w, heat_h, heat_pixels)
        heatmap_pixels_by_shape[shape_id] = heat_pixels

        if caustic.get("transport_mesh_dielectric_lens_emitted_path_count", 0) <= 0:
            failures.append(f"{shape_id}: emitted zero mesh-dielectric paths")
        if caustic.get("surface_cache_record_count", 0) <= 0:
            failures.append(f"{shape_id}: recorded zero surface-cache deposits")
        if metrics.get("positive_pixel_count", 0) <= 0:
            failures.append(f"{shape_id}: brightened zero receiver pixels")

        render_images.append(review_artifacts.read_bmp_rgb(caustic_frame))
        heatmap_images.append((heat_w, heat_h, heat_pixels))
        shape_rows.append({
            "shape_id": shape_id,
            "scene_path": str(scene_path),
            "lens_mesh_path": str(lens_path),
            "lens_topology_audit": topology,
            "baseline": {
                "cell_id": off_request["cell_id"],
                "request_path": str(off_request["request_path"]),
                "summary_path": str(off_request["summary_path"]),
                "frame_path": str(off_frame),
                "png_path": str(off_png),
            },
            "caustic_frame_path": str(caustic_frame),
            "caustic_png_path": str(caustic_png),
            "heatmap_path": str(heatmap_path),
            "elapsed_seconds": elapsed,
            "caustic": caustic,
            "metrics": metrics,
            "footprint_radius": footprint_radius(metrics),
        })

    radii = [float(row["footprint_radius"]) for row in shape_rows]
    p95s = [float(row["metrics"].get("positive_delta_p95", 0.0)) for row in shape_rows]
    max_centroid_distance = 0.0
    max_heatmap_difference = 0.0
    for i, row_a in enumerate(shape_rows):
        for row_b in shape_rows[i + 1:]:
            max_centroid_distance = max(
                max_centroid_distance,
                centroid_distance(row_a["metrics"], row_b["metrics"]),
            )
            max_heatmap_difference = max(
                max_heatmap_difference,
                heatmap_difference(
                    heatmap_pixels_by_shape[row_a["shape_id"]],
                    heatmap_pixels_by_shape[row_b["shape_id"]],
                ),
            )
    distribution_review = {
        "radius_spread": max(radii) - min(radii) if radii else 0.0,
        "positive_delta_p95_spread": max(p95s) - min(p95s) if p95s else 0.0,
        "max_centroid_distance": max_centroid_distance,
        "max_heatmap_difference": max_heatmap_difference,
        "spread_detected": (
            (max(radii) - min(radii) if radii else 0.0) >= 1.0
            or (max(p95s) - min(p95s) if p95s else 0.0) >= 1.0
            or max_centroid_distance >= 1.0
            or max_heatmap_difference >= 0.25
        ),
    }
    if not distribution_review["spread_detected"]:
        failures.append("lens shape comparison did not detect any distribution spread")

    comparison_sheet_path = review_root / "lens_shape_comparison_sheet.png"
    write_sheet(comparison_sheet_path, [render_images, heatmap_images])
    report = {
        "schema_version": "ray_tracing_lens_shape_comparison_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "lens_y": LENS_Y,
        "lens_shapes": list(LENS_SHAPES),
        "caustic_surface_energy_scale": CAUSTIC_SURFACE_ENERGY_SCALE,
        "comparison_sheet_path": str(comparison_sheet_path),
        "shape_rows": shape_rows,
        "distribution_review": distribution_review,
        "failures": failures,
        "passed": len(failures) == 0,
    }
    report_path = review_root / "lens_shape_comparison_report.json"
    write_json(report_path, report)
    write_index(review_root / "lens_shape_comparison_index.md", report)
    print(report_path)
    print(comparison_sheet_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
