#!/usr/bin/env python3
"""Render receiver-distance focal sweep diagnostics for closed lens meshes."""

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
import run_ray_tracing_spatial_caustic_lens_shape_comparison as shape_compare  # noqa: E402
import run_ray_tracing_spatial_caustic_mesh_dielectric_lens_fixture as mesh_fixture  # noqa: E402
import run_ray_tracing_spatial_caustic_plano_convex_heatmap_diagnostic as heatmap_diag  # noqa: E402


LENS_Y = -1.05
RECEIVER_Y_POSITIONS = (-0.25, 0.15, 0.55, 0.95, 1.35, 1.75, 2.15)
SWEEP_SHAPES = ("plano_convex", "biconcave")
CAUSTIC_SURFACE_ENERGY_SCALE = 0.0025
CAUSTIC_SURFACE_FOOTPRINT_SCALE = 5.0


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
        / "caustic_lens_focal_sweep_diagnostic"
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


def wall_plane_at(receiver_y: float) -> dict:
    wall = wall_preview.sphere_mist.plane_object(
        "vivid_receiver_wall",
        {
            "origin": {"x": 0.0, "y": receiver_y, "z": 1.25},
            "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
            "axis_v": {"x": 0.0, "y": 0.0, "z": 1.0},
            "normal": {"x": 0.0, "y": -1.0, "z": 0.0},
        },
        7.5,
        5.2,
        "mat_vivid_wall",
    )
    wall["transform"]["position"] = {"x": 0.0, "y": receiver_y, "z": 1.25}
    return wall


def write_sweep_scene(review_root: Path, shape_id: str, receiver_y: float) -> tuple[Path, Path]:
    scene_dir = review_root / "generated_scenes" / shape_id / receiver_y_token(receiver_y)
    mesh_dir = scene_dir / "assets" / "mesh_assets"
    mesh_dir.mkdir(parents=True, exist_ok=True)
    lens_path = shape_compare.write_shape_lens_asset(mesh_dir, shape_id)
    object_id = shape_compare.lens_object_id(shape_id)
    scene = {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"caustic_lens_focal_sweep_{shape_id}_{receiver_y_token(receiver_y)}",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            wall_plane_at(receiver_y),
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
                    "id": shape_compare.asset_id_for_shape(shape_id),
                    "variant": "runtime_default",
                },
                "extensions": {
                    "line_drawing": {
                        "runtime_mesh_path": (
                            f"assets/mesh_assets/{shape_compare.runtime_mesh_filename_for_shape(shape_id)}"
                        ),
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
                "light_id": "focal_sweep_light",
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
                "camera_id": "focal_sweep_camera",
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


def request_for_cell(review_root: Path,
                     scene_path: Path,
                     shape_id: str,
                     receiver_y: float,
                     caustics_enabled: bool,
                     debug_export: bool) -> dict:
    suffix = "caustic" if caustics_enabled else "off"
    cell_id = f"{shape_id}_{receiver_y_token(receiver_y)}_{suffix}"
    output_root = review_root / "runs" / cell_id
    summary_path = output_root / "render_summary.json"
    request = wall_preview.base_request(
        f"caustic_lens_focal_sweep_{cell_id}",
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
            "caustic_surface_footprint_scale": CAUSTIC_SURFACE_FOOTPRINT_SCALE,
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


def receiver_distance(receiver_y: float) -> float:
    return receiver_y - LENS_Y


def image_tuple(path: Path) -> tuple[int, int, list[list[tuple[int, int, int]]]]:
    return review_artifacts.read_bmp_rgb(path)


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


def curve_summary(rows: list[dict]) -> dict:
    radii = [float(row["footprint_radius"]) for row in rows]
    hit_radii = [float(row["debug_hit_stats"].get("hit_radius", 0.0)) for row in rows]
    p95s = [float(row["metrics"].get("positive_delta_p95", 0.0)) for row in rows]
    if not radii:
        return {
            "min_radius": 0.0,
            "min_radius_receiver_y": None,
            "radius_spread": 0.0,
            "hit_radius_spread": 0.0,
            "positive_delta_p95_spread": 0.0,
        }
    min_index = min(range(len(radii)), key=lambda i: radii[i])
    return {
        "min_radius": radii[min_index],
        "min_radius_receiver_y": rows[min_index]["receiver_y"],
        "min_radius_distance": rows[min_index]["receiver_distance"],
        "radius_spread": max(radii) - min(radii),
        "hit_radius_spread": max(hit_radii) - min(hit_radii) if hit_radii else 0.0,
        "positive_delta_p95_spread": max(p95s) - min(p95s) if p95s else 0.0,
        "radii": radii,
        "hit_radii": hit_radii,
    }


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Lens Focal Sweep Diagnostic",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- comparison sheet: `{report['comparison_sheet_path']}`",
        f"- lens y: `{report['lens_y']}`",
        f"- receiver y positions: `{report['receiver_y_positions']}`",
        f"- caustic scale: `{report['caustic_surface_energy_scale']}`",
        "",
        "## Curve Summaries",
        "",
    ]
    for shape_id, summary in report.get("curve_summaries", {}).items():
        lines.append(
            f"- `{shape_id}`: min radius `{summary.get('min_radius', 0.0):.4f}` at "
            f"receiver y `{summary.get('min_radius_receiver_y')}`, radius spread "
            f"`{summary.get('radius_spread', 0.0):.4f}`, hit-radius spread "
            f"`{summary.get('hit_radius_spread', 0.0):.4f}`"
        )
    lines.extend(["", "## Rows", ""])
    for row in report.get("sweep_rows", []):
        lines.append(
            f"- `{row['shape_id']}` receiver y `{row['receiver_y']}`: positive "
            f"`{row['metrics'].get('positive_pixel_count', 0)}`, radius "
            f"`{row.get('footprint_radius', 0.0):.4f}`, hit radius "
            f"`{row['debug_hit_stats'].get('hit_radius', 0.0):.4f}`, emitted "
            f"`{row['caustic'].get('transport_mesh_dielectric_lens_emitted_path_count', 0)}`"
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
    render_sheet_rows = []
    heatmap_sheet_rows = []
    hit_sheet_rows = []
    topology_by_shape = {}

    for shape_id in SWEEP_SHAPES:
        shape_render_images = []
        shape_heatmap_images = []
        shape_hit_images = []
        for receiver_y in RECEIVER_Y_POSITIONS:
            scene_path, lens_path = write_sweep_scene(review_root, shape_id, receiver_y)
            if shape_id not in topology_by_shape:
                topology = mesh_fixture.audit_runtime_mesh_topology(lens_path)
                topology_by_shape[shape_id] = topology
                failures.extend([
                    f"{shape_id}: lens_topology: {failure}"
                    for failure in mesh_fixture.validate_mesh_topology_audit(topology)
                ])

            off_request = request_for_cell(review_root, scene_path, shape_id, receiver_y, False, args.debug_export)
            off_elapsed = render_request(cli, off_request["request_path"], off_request["summary_path"], args.skip_render)
            off_summary = load_json(off_request["summary_path"])
            off_frame, off_png = copy_frame_png(off_summary, review_root, off_request["cell_id"])

            caustic_request = request_for_cell(review_root, scene_path, shape_id, receiver_y, True, args.debug_export)
            caustic_elapsed = render_request(
                cli,
                caustic_request["request_path"],
                caustic_request["summary_path"],
                args.skip_render,
            )
            caustic_summary = load_json(caustic_request["summary_path"])
            caustic_frame, caustic_png = copy_frame_png(caustic_summary, review_root, caustic_request["cell_id"])
            caustic = wall_preview.caustic_digest(caustic_summary)
            metrics = wall_preview.wall_delta_metrics(off_frame, caustic_frame)
            heat_w, heat_h, heat_pixels = shape_compare.signed_heatmap_pixels(off_frame, caustic_frame)
            heatmap_path = review_root / "heatmaps" / f"{caustic_request['cell_id']}_signed_heatmap.png"
            heatmap_path.parent.mkdir(parents=True, exist_ok=True)
            review_artifacts.write_png_rgb(heatmap_path, heat_w, heat_h, heat_pixels)

            debug_path = Path(caustic_request["summary_path"]).parent / "caustic_transport_debug_paths.jsonl"
            debug_hits, debug_hit_stats = heatmap_diag.read_debug_hits(debug_path)
            hit_map_path = review_root / "heatmaps" / f"{caustic_request['cell_id']}_debug_hit_map.png"
            heatmap_diag.write_hit_map_png(hit_map_path, heat_w, heat_h, debug_hits)
            hit_span_x = float(debug_hit_stats.get("x_max", 0.0)) - float(debug_hit_stats.get("x_min", 0.0))
            hit_span_z = float(debug_hit_stats.get("z_max", 0.0)) - float(debug_hit_stats.get("z_min", 0.0))
            debug_hit_stats["hit_radius"] = 0.5 * math.hypot(hit_span_x, hit_span_z)

            if caustic.get("transport_mesh_dielectric_lens_emitted_path_count", 0) <= 0:
                failures.append(f"{caustic_request['cell_id']} emitted zero mesh-dielectric paths")
            if caustic.get("surface_cache_record_count", 0) <= 0:
                failures.append(f"{caustic_request['cell_id']} recorded zero surface-cache deposits")
            if metrics.get("positive_pixel_count", 0) <= 0:
                failures.append(f"{caustic_request['cell_id']} brightened zero receiver pixels")
            if debug_hit_stats.get("count", 0) <= 0:
                failures.append(f"{caustic_request['cell_id']} exported zero receiver debug hits")

            shape_render_images.append(image_tuple(caustic_frame))
            shape_heatmap_images.append((heat_w, heat_h, heat_pixels))
            shape_hit_images.append(heatmap_diag.read_rgb_image(hit_map_path))
            sweep_rows.append({
                "shape_id": shape_id,
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
                "hit_map_path": str(hit_map_path),
                "elapsed_seconds": caustic_elapsed,
                "caustic": caustic,
                "metrics": metrics,
                "debug_hit_stats": debug_hit_stats,
                "footprint_radius": footprint_radius(metrics),
            })
        render_sheet_rows.append(shape_render_images)
        heatmap_sheet_rows.append(shape_heatmap_images)
        hit_sheet_rows.append(shape_hit_images)

    curve_summaries = {
        shape_id: curve_summary([row for row in sweep_rows if row["shape_id"] == shape_id])
        for shape_id in SWEEP_SHAPES
    }
    if curve_summaries.get("plano_convex", {}).get("radius_spread", 0.0) <= 1.0:
        failures.append("plano-convex focal sweep did not change receiver footprint radius")
    if curve_summaries.get("biconcave", {}).get("radius_spread", 0.0) <= 1.0:
        failures.append("biconcave focal sweep did not change receiver footprint radius")
    for shape_id, summary in curve_summaries.items():
        if summary.get("hit_radius_spread", 0.0) <= 1.0e-6:
            warnings.append(
                f"{shape_id}: debug hit map is invariant because exported "
                "lens_receiver_crossing still uses provider nominal receiver distance"
            )

    comparison_sheet_path = review_root / "lens_focal_sweep_diagnostic_sheet.png"
    write_sheet(comparison_sheet_path, render_sheet_rows + heatmap_sheet_rows + hit_sheet_rows)
    report = {
        "schema_version": "ray_tracing_lens_focal_sweep_diagnostic_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "lens_y": LENS_Y,
        "receiver_y_positions": list(RECEIVER_Y_POSITIONS),
        "sweep_shapes": list(SWEEP_SHAPES),
        "caustic_surface_energy_scale": CAUSTIC_SURFACE_ENERGY_SCALE,
        "caustic_surface_footprint_scale": CAUSTIC_SURFACE_FOOTPRINT_SCALE,
        "comparison_sheet_path": str(comparison_sheet_path),
        "topology_by_shape": topology_by_shape,
        "curve_summaries": curve_summaries,
        "sweep_rows": sweep_rows,
        "failures": failures,
        "warnings": warnings,
        "passed": len(failures) == 0,
    }
    report_path = review_root / "lens_focal_sweep_diagnostic_report.json"
    write_json(report_path, report)
    write_index(review_root / "lens_focal_sweep_diagnostic_index.md", report)
    print(report_path)
    print(comparison_sheet_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
