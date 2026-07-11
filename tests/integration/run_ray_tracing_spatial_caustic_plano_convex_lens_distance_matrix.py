#!/usr/bin/env python3
"""Render a fixed-scale plano-convex closed-lens distance matrix."""

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


LENS_Y_POSITIONS = (-2.05, -1.85, -1.65, -1.45, -1.25, -1.05, -0.85, -0.65, -0.50, -0.35)
CAUSTIC_SURFACE_ENERGY_SCALE = 0.0025


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
        / "caustic_plano_convex_lens_distance_matrix"
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


def lens_y_token(lens_y: float) -> str:
    sign = "m" if lens_y < 0.0 else "p"
    return f"lens_y_{sign}{abs(lens_y):.2f}".replace(".", "p")


def write_plano_convex_lens_asset(mesh_dir: Path) -> Path:
    mesh_dir.mkdir(parents=True, exist_ok=True)
    segments = 128
    radial_rings = 20
    aperture_radius = 1.0
    rim_thickness = 0.08
    center_thickness = 0.58
    flat_y = 0.24
    curved_center_y = flat_y - center_thickness
    curved_rim_y = flat_y - rim_thickness
    sag = curved_rim_y - curved_center_y
    sphere_radius = (aperture_radius * aperture_radius + sag * sag) / (2.0 * sag)

    vertices: list[dict] = []

    def curved_y(radial_t: float) -> float:
        radial = aperture_radius * radial_t
        return curved_center_y + sphere_radius - math.sqrt(
            max(0.0, sphere_radius * sphere_radius - radial * radial)
        )

    curved_center_index = len(vertices)
    vertices.append({"x": 0.0, "y": curved_center_y, "z": 0.0})
    curved_rings: list[list[int]] = []
    for ring in range(1, radial_rings + 1):
        radial_t = float(ring) / float(radial_rings)
        ring_indices = []
        for i in range(segments):
            theta = (2.0 * math.pi * float(i)) / float(segments)
            ring_indices.append(len(vertices))
            vertices.append({
                "x": aperture_radius * radial_t * math.cos(theta),
                "y": curved_y(radial_t),
                "z": aperture_radius * radial_t * math.sin(theta),
            })
        curved_rings.append(ring_indices)

    flat_center_index = len(vertices)
    vertices.append({"x": 0.0, "y": flat_y, "z": 0.0})
    flat_rings: list[list[int]] = []
    for ring in range(1, radial_rings + 1):
        radial_t = float(ring) / float(radial_rings)
        ring_indices = []
        for i in range(segments):
            theta = (2.0 * math.pi * float(i)) / float(segments)
            ring_indices.append(len(vertices))
            vertices.append({
                "x": aperture_radius * radial_t * math.cos(theta),
                "y": flat_y,
                "z": aperture_radius * radial_t * math.sin(theta),
            })
        flat_rings.append(ring_indices)

    triangles: list[dict] = []
    for i in range(segments):
        n = (i + 1) % segments
        triangles.append({
            "a": curved_center_index,
            "b": curved_rings[0][i],
            "c": curved_rings[0][n],
            "surface_group_id": "lens_curved_front",
        })
        triangles.append({
            "a": flat_center_index,
            "b": flat_rings[0][n],
            "c": flat_rings[0][i],
            "surface_group_id": "lens_flat_back",
        })

    for ring in range(1, radial_rings):
        curved_inner = curved_rings[ring - 1]
        curved_outer = curved_rings[ring]
        flat_inner = flat_rings[ring - 1]
        flat_outer = flat_rings[ring]
        for i in range(segments):
            n = (i + 1) % segments
            triangles.append({
                "a": curved_inner[i],
                "b": curved_outer[i],
                "c": curved_outer[n],
                "surface_group_id": "lens_curved_front",
            })
            triangles.append({
                "a": curved_inner[i],
                "b": curved_outer[n],
                "c": curved_inner[n],
                "surface_group_id": "lens_curved_front",
            })
            triangles.append({
                "a": flat_inner[i],
                "b": flat_outer[n],
                "c": flat_outer[i],
                "surface_group_id": "lens_flat_back",
            })
            triangles.append({
                "a": flat_inner[i],
                "b": flat_inner[n],
                "c": flat_outer[n],
                "surface_group_id": "lens_flat_back",
            })

    curved_outer = curved_rings[-1]
    flat_outer = flat_rings[-1]
    for i in range(segments):
        n = (i + 1) % segments
        triangles.append({
            "a": curved_outer[i],
            "b": flat_outer[n],
            "c": curved_outer[n],
            "surface_group_id": "lens_rim",
        })
        triangles.append({
            "a": curved_outer[i],
            "b": flat_outer[i],
            "c": flat_outer[n],
            "surface_group_id": "lens_rim",
        })

    curved_tri_count = segments + (radial_rings - 1) * segments * 2
    flat_tri_count = curved_tri_count
    rim_tri_start = curved_tri_count + flat_tri_count
    asset = {
        "schema_family": "codework_geometry",
        "schema_variant": "mesh_asset_runtime_v1",
        "schema_version": 1,
        "asset_id": "asset_imported_plano_convex_lens_128x20",
        "source_asset_id": "generated_plano_convex_distance_matrix",
        "asset_type": "solid_mesh",
        "compile_meta": {
            "profile": "runtime_default",
            "generator": Path(__file__).name,
            "shape": "closed_plano_convex_lens",
            "axis": "y",
            "segments": segments,
            "radial_rings": radial_rings,
            "aperture_radius": aperture_radius,
            "rim_thickness": rim_thickness,
            "center_thickness": center_thickness,
            "fidelity": "high_resolution_preview",
        },
        "local_bounds": {
            "min": {"x": -aperture_radius, "y": curved_center_y, "z": -aperture_radius},
            "max": {"x": aperture_radius, "y": flat_y, "z": aperture_radius},
        },
        "mesh": {
            "vertex_count": len(vertices),
            "triangle_count": len(triangles),
            "vertices": vertices,
            "triangles": triangles,
        },
        "surface_groups": [
            {
                "group_id": "lens_curved_front",
                "semantic": "incident_spherical_cap_surface",
                "triangle_span": {"start": 0, "count": curved_tri_count},
            },
            {
                "group_id": "lens_flat_back",
                "semantic": "exit_flat_plane_surface",
                "triangle_span": {"start": curved_tri_count, "count": flat_tri_count},
            },
            {
                "group_id": "lens_rim",
                "semantic": "closed_lens_rim",
                "triangle_span": {"start": rim_tri_start, "count": segments * 2},
            },
        ],
        "topology_flags": {
            "closed_volume": True,
            "manifold_expected": True,
        },
        "extensions": {},
    }
    mesh_path = mesh_dir / "asset_imported_plano_convex_lens_128x20.runtime.json"
    write_json(mesh_path, asset)
    return mesh_path


def write_distance_scene(review_root: Path, lens_y: float) -> tuple[Path, Path]:
    scene_dir = review_root / "generated_scenes" / lens_y_token(lens_y)
    mesh_dir = scene_dir / "assets" / "mesh_assets"
    mesh_dir.mkdir(parents=True, exist_ok=True)
    lens_path = write_plano_convex_lens_asset(mesh_dir)
    scene = {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"caustic_plano_convex_lens_distance_{lens_y_token(lens_y)}",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            wall_preview.wall_plane(),
            {
                "object_id": "imported_plano_convex_lens",
                "object_type": "mesh_asset_instance",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": {"x": 0.0, "y": lens_y, "z": 1.25},
                    "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "scale": {"x": 0.56, "y": 0.56, "z": 0.56},
                },
                "geometry_ref": {
                    "kind": "mesh_asset",
                    "id": "asset_imported_plano_convex_lens_128x20",
                    "variant": "runtime_default",
                },
                "extensions": {
                    "line_drawing": {
                        "runtime_mesh_path": "assets/mesh_assets/asset_imported_plano_convex_lens_128x20.runtime.json",
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
                "light_id": "plano_convex_distance_light",
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
                "camera_id": "plano_convex_distance_camera",
                "kind": "perspective",
                "position": {"x": 1.95, "y": -3.25, "z": 1.52},
                "target": {"x": 0.0, "y": lens_y, "z": 1.24},
                "yaw": 0.0,
                "look_pitch": 0.0,
            }
        ],
        "constraints": [],
        "hierarchy": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": {"x": 0.0, "y": lens_y, "z": 1.24},
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
                            "object_id": "imported_plano_convex_lens",
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


def base_request(run_id: str, scene_path: Path, output_root: Path, summary_path: Path, lens_y: float) -> dict:
    request = wall_preview.base_request(run_id, scene_path, output_root, summary_path)
    request["inspection"]["camera_look_at"] = {"x": 0.0, "y": lens_y, "z": 1.24}
    request["inspection"]["camera_position"] = {"x": 1.95, "y": -3.25, "z": 1.52}
    request["inspection"]["camera_zoom"] = 0.52
    return request


def request_for_cell(review_root: Path,
                     scene_path: Path,
                     lens_y: float,
                     caustics_enabled: bool,
                     debug_export: bool) -> dict:
    cell_id = lens_y_token(lens_y) if caustics_enabled else f"{lens_y_token(lens_y)}_off"
    output_root = review_root / "runs" / cell_id
    summary_path = output_root / "render_summary.json"
    request = base_request(
        f"caustic_plano_convex_lens_distance_matrix_{cell_id}",
        scene_path,
        output_root,
        summary_path,
        lens_y,
    )
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
    return {
        "cell_id": cell_id,
        "request_path": request_path,
        "summary_path": summary_path,
    }


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
    if area <= 0.0:
        return 0.0
    return (area / math.pi) ** 0.5


def write_matrix_sheet(path: Path, matrix_rows: list[dict]) -> None:
    cells = []
    for row in matrix_rows:
        width, height, pixels = review_artifacts.read_bmp_rgb(Path(row["caustic"]["frame_path"]))
        cells.append((width, height, pixels))
    if not cells:
        return
    cell_width, cell_height = cells[0][0], cells[0][1]
    separator = 4
    sheet_width = cell_width
    sheet_height = cell_height * len(cells) + separator * (len(cells) - 1)
    rows = [[(20, 20, 22)] * sheet_width for _ in range(sheet_height)]
    for row_i, (_width, _height, pixels) in enumerate(cells):
        offset_y = row_i * (cell_height + separator)
        for y in range(cell_height):
            rows[offset_y + y][0:cell_width] = pixels[y]
    review_artifacts.write_png_rgb(path, sheet_width, sheet_height, rows)


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Plano-Convex Lens Distance Matrix",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- matrix sheet: `{report['matrix_sheet_path']}`",
        f"- fixed caustic surface energy scale: `{report['caustic_surface_energy_scale']}`",
        f"- lens y positions: `{report['lens_y_positions']}`",
        "",
        "## Results",
        "",
    ]
    for row in report.get("matrix_rows", []):
        metrics = row["caustic"]["metrics"]
        caustic = row["caustic"]["caustic"]
        lines.append(
            f"- lens y `{row['lens_y']}`: emitted "
            f"`{caustic.get('transport_mesh_dielectric_lens_emitted_path_count', 0)}`, "
            f"positive `{metrics.get('positive_pixel_count', 0)}`, "
            f"saturated `{metrics.get('saturated_pixel_count', 0)}`, "
            f"p95 `{metrics.get('positive_delta_p95', 0.0):.4f}`, "
            f"max `{metrics.get('max_luma_delta', 0.0):.4f}`, "
            f"radius `{row['caustic'].get('footprint_radius', 0.0):.4f}`"
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

    failures = []
    matrix_rows = []
    first_lens_path: Path | None = None
    topology = {}
    for lens_y in LENS_Y_POSITIONS:
        scene_path, lens_path = write_distance_scene(review_root, lens_y)
        if first_lens_path is None:
            first_lens_path = lens_path
            topology = mesh_fixture.audit_runtime_mesh_topology(lens_path)
            failures.extend([
                f"lens_topology: {failure}"
                for failure in mesh_fixture.validate_mesh_topology_audit(topology)
            ])

        baseline_request = request_for_cell(review_root, scene_path, lens_y, False, args.debug_export)
        elapsed = render_request(cli, baseline_request["request_path"], baseline_request["summary_path"], args.skip_render)
        baseline_summary = load_json(baseline_request["summary_path"])
        baseline_frame, baseline_png = copy_frame_png(baseline_summary, review_root, baseline_request["cell_id"])
        baseline_run = {
            "cell_id": baseline_request["cell_id"],
            "request_path": str(baseline_request["request_path"]),
            "summary_path": str(baseline_request["summary_path"]),
            "frame_path": str(baseline_frame),
            "png_path": str(baseline_png),
            "elapsed_seconds": elapsed,
        }

        caustic_request = request_for_cell(review_root, scene_path, lens_y, True, args.debug_export)
        elapsed = render_request(cli, caustic_request["request_path"], caustic_request["summary_path"], args.skip_render)
        caustic_summary = load_json(caustic_request["summary_path"])
        caustic_frame, caustic_png = copy_frame_png(caustic_summary, review_root, caustic_request["cell_id"])
        caustic = wall_preview.caustic_digest(caustic_summary)
        metrics = wall_preview.wall_delta_metrics(Path(baseline_frame), caustic_frame)
        caustic_run = {
            "cell_id": caustic_request["cell_id"],
            "request_path": str(caustic_request["request_path"]),
            "summary_path": str(caustic_request["summary_path"]),
            "frame_path": str(caustic_frame),
            "png_path": str(caustic_png),
            "elapsed_seconds": elapsed,
            "caustic": caustic,
            "metrics": metrics,
            "footprint_radius": footprint_radius(metrics),
        }
        if caustic.get("transport_mesh_dielectric_lens_emitted_path_count", 0) <= 0:
            failures.append(f"{caustic_request['cell_id']} emitted zero mesh-dielectric paths")
        if caustic.get("surface_cache_record_count", 0) <= 0:
            failures.append(f"{caustic_request['cell_id']} recorded zero surface-cache deposits")
        if metrics.get("positive_pixel_count", 0) <= 0:
            failures.append(f"{caustic_request['cell_id']} brightened zero receiver pixels")

        matrix_rows.append({
            "lens_y": lens_y,
            "scene_path": str(scene_path),
            "baseline": baseline_run,
            "caustic": caustic_run,
        })

    matrix_sheet_path = review_root / "plano_convex_lens_distance_matrix_sheet.png"
    write_matrix_sheet(matrix_sheet_path, matrix_rows)
    report = {
        "schema_version": "ray_tracing_plano_convex_lens_distance_matrix_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "lens_mesh_path": str(first_lens_path) if first_lens_path else None,
        "lens_topology_audit": topology,
        "lens_y_positions": list(LENS_Y_POSITIONS),
        "caustic_surface_energy_scale": CAUSTIC_SURFACE_ENERGY_SCALE,
        "matrix_sheet_path": str(matrix_sheet_path),
        "matrix_rows": matrix_rows,
        "failures": failures,
        "passed": len(failures) == 0,
    }
    report_path = review_root / "plano_convex_lens_distance_matrix_report.json"
    write_json(report_path, report)
    write_index(review_root / "plano_convex_lens_distance_matrix_index.md", report)
    print(report_path)
    print(matrix_sheet_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
