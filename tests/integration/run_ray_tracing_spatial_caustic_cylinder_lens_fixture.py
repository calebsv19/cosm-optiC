#!/usr/bin/env python3
"""Run a physical mist visual for the analytic cylinder-lens transport path."""

from __future__ import annotations

import argparse
import json
import math
import platform
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import run_ray_tracing_spatial_caustic_visual_sphere_mist_matrix as sphere_mist  # noqa: E402


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
        / "caustic_cylinder_lens_fixture"
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


def write_cylinder_runtime_mesh_asset(mesh_dir: Path, segments: int = 32) -> Path:
    mesh_dir.mkdir(parents=True, exist_ok=True)
    vertices = []
    triangles = []
    for x in (-1.0, 1.0):
        for i in range(segments):
            theta = (2.0 * math.pi * float(i)) / float(segments)
            vertices.append({
                "x": x,
                "y": round(math.cos(theta), 9),
                "z": round(math.sin(theta), 9),
            })
    left_center_index = len(vertices)
    vertices.append({"x": -1.0, "y": 0.0, "z": 0.0})
    right_center_index = len(vertices)
    vertices.append({"x": 1.0, "y": 0.0, "z": 0.0})
    for i in range(segments):
        j = (i + 1) % segments
        left_i = i
        left_j = j
        right_i = segments + i
        right_j = segments + j
        triangles.append({
            "a": left_i,
            "b": right_i,
            "c": right_j,
            "surface_group_id": "cylinder_side",
        })
        triangles.append({
            "a": left_i,
            "b": right_j,
            "c": left_j,
            "surface_group_id": "cylinder_side",
        })
        triangles.append({
            "a": left_center_index,
            "b": left_j,
            "c": left_i,
            "surface_group_id": "cylinder_cap",
        })
        triangles.append({
            "a": right_center_index,
            "b": right_i,
            "c": right_j,
            "surface_group_id": "cylinder_cap",
        })
    asset = {
        "schema_family": "codework_geometry",
        "schema_variant": "mesh_asset_runtime_v1",
        "schema_version": 1,
        "asset_id": "asset_caustic_cylinder_32",
        "source_asset_id": "generated_analytic_cylinder_fixture",
        "asset_type": "solid_mesh",
        "compile_meta": {
            "profile": "runtime_default",
            "generator": Path(__file__).name,
            "shape": "cylinder",
            "segments": segments,
            "axis": "x",
            "fidelity": "fixture_review",
        },
        "local_bounds": {
            "min": {"x": -1.0, "y": -1.0, "z": -1.0},
            "max": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "mesh": {
            "vertex_count": len(vertices),
            "triangle_count": len(triangles),
            "vertices": vertices,
            "triangles": triangles,
        },
        "surface_groups": [
            {
                "group_id": "cylinder_side",
                "semantic": "cylinder_side_wall",
                "triangle_span": {
                    "start": 0,
                    "count": segments * 2,
                },
            },
            {
                "group_id": "cylinder_cap",
                "semantic": "cylinder_end_caps",
                "triangle_span": {
                    "start": segments * 2,
                    "count": segments * 2,
                },
            },
        ],
        "topology_flags": {
            "closed_volume": True,
            "manifold_expected": True,
        },
        "extensions": {},
    }
    mesh_path = mesh_dir / "asset_caustic_cylinder_32.runtime.json"
    write_json(mesh_path, asset)
    return mesh_path


def write_cylinder_scene(review_root: Path) -> Path:
    scene_path = sphere_mist.write_visual_scene(review_root)
    scene = load_json(scene_path)
    scene["scene_id"] = "caustic_cylinder_lens_fixture_v1"
    mesh_dir = scene_path.parent / "assets" / "mesh_assets"
    write_cylinder_runtime_mesh_asset(mesh_dir)
    for obj in scene.get("objects", []):
        if obj.get("object_id") == "high_quality_glass_sphere":
            obj["object_id"] = "authored_glass_cylinder_lens"
            transform = obj.setdefault("transform", {})
            transform["position"] = {"x": 0.0, "y": 0.0, "z": 1.38}
            transform["scale"] = {"x": 1.18, "y": 0.42, "z": 0.42}
            obj["geometry_ref"] = {
                "kind": "mesh_asset",
                "id": "asset_caustic_cylinder_32",
                "variant": "runtime_default",
            }
            obj["extensions"] = {
                "line_drawing": {
                    "runtime_mesh_path": "assets/mesh_assets/asset_caustic_cylinder_32.runtime.json",
                }
            }
    for light in scene.get("lights", []):
        if light.get("light_id") == "overhead_focus_light":
            light["position"] = {"x": -0.26, "y": -0.18, "z": 3.70}
            light["radius"] = 0.07
            light["intensity"] = 9.6
    for camera in scene.get("cameras", []):
        if camera.get("camera_id") == "caustic_visual_camera":
            camera["position"] = {"x": 0.78, "y": -5.55, "z": 0.88}
            camera["target"] = {"x": 0.0, "y": 0.0, "z": 1.02}
    authoring = scene.get("extensions", {}).get("ray_tracing", {}).get("authoring", {})
    authoring["camera_focus_target"] = {"x": 0.0, "y": 0.0, "z": 1.02}
    for material in authoring.get("object_materials", []):
        if material.get("object_id") == "high_quality_glass_sphere":
            material["object_id"] = "authored_glass_cylinder_lens"
            material["object_color"] = sphere_mist.rgb_u24(224, 246, 255)
            material["alpha"] = 0.38
            material["roughness"] = 0.01
            material["reflectivity"] = 0.035
    write_json(scene_path, scene)
    return scene_path


def request_for_cell(review_root: Path,
                     scene_path: Path,
                     volume_path: Path,
                     cell_id: str,
                     debug_export: bool) -> dict:
    output_root = review_root / "runs" / cell_id
    summary_path = output_root / "render_summary.json"
    request = sphere_mist.base_request(
        f"caustic_cylinder_lens_{cell_id}",
        scene_path,
        output_root,
        summary_path,
        volume_path,
    )
    request["inspection"].update({
        "camera_position": {"x": 0.78, "y": -5.55, "z": 0.88},
        "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.02},
        "camera_zoom": 0.92,
        "light_intensity": 9.6,
        "light_radius": 0.07,
    })
    if cell_id == "mist_no_caustic":
        request["inspection"].update({
            "caustic_mode": "off",
            "caustic_volume_enabled": False,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 0,
        })
    elif cell_id == "volume_triangle_targets":
        request["inspection"].update({
            "caustic_mode": "transport",
            "caustic_volume_enabled": True,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 3072,
            "caustic_max_path_depth": 2,
            "caustic_surface_receiver_fallback_enabled": False,
        })
    elif cell_id == "volume_analytic_cylinder_lens":
        request["inspection"].update({
            "caustic_mode": "transport",
            "caustic_volume_enabled": True,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 3072,
            "caustic_max_path_depth": 2,
            "caustic_surface_receiver_fallback_enabled": False,
            "caustic_transport_emission_policy": "analytic_cylinder_lens",
        })
    elif cell_id == "volume_analytic_cylinder_lens_focused":
        request["inspection"].update({
            "caustic_mode": "transport",
            "caustic_volume_enabled": True,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 3072,
            "caustic_max_path_depth": 2,
            "caustic_surface_receiver_fallback_enabled": False,
            "caustic_transport_emission_policy": "analytic_cylinder_lens_focused",
        })
    else:
        raise ValueError(f"unknown cylinder fixture cell: {cell_id}")
    if debug_export and cell_id != "mist_no_caustic":
        request["inspection"]["caustic_transport_debug_export_enabled"] = True
    request_path = review_root / "generated_requests" / f"request_{cell_id}.json"
    write_json(request_path, request)
    return {
        "cell_id": cell_id,
        "request_path": request_path,
        "summary_path": summary_path,
    }


def cylinder_digest(summary: dict) -> dict:
    digest = sphere_mist.caustic_digest(summary)
    state = summary.get("inspection", {}).get("caustic_state", {})
    digest.update({
        "transport_analytic_cylinder_lens_resolved_count": int(
            state.get("transport_analytic_cylinder_lens_resolved_count", 0)
        ),
        "transport_analytic_cylinder_lens_rejected_count": int(
            state.get("transport_analytic_cylinder_lens_rejected_count", 0)
        ),
        "transport_analytic_cylinder_lens_evaluated_paths": int(
            state.get("transport_analytic_cylinder_lens_evaluated_path_count", 0)
        ),
        "transport_analytic_cylinder_lens_emitted_paths": int(
            state.get("transport_analytic_cylinder_lens_emitted_path_count", 0)
        ),
        "transport_analytic_cylinder_lens_sample_weight": float(
            state.get("transport_analytic_cylinder_lens_sample_weight", 0.0)
        ),
        "transport_analytic_cylinder_lens_total_sample_weight": float(
            state.get("transport_analytic_cylinder_lens_total_sample_weight", 0.0)
        ),
    })
    return digest


def validate_cell(cell_id: str, digest: dict) -> list[str]:
    if cell_id == "mist_no_caustic":
        return sphere_mist.validate_cell(cell_id, digest)
    failures = sphere_mist.validate_cell("volume_caustic_only", digest)
    if cell_id in ("volume_analytic_cylinder_lens", "volume_analytic_cylinder_lens_focused"):
        if digest.get("transport_analytic_cylinder_lens_resolved_count", 0) <= 0:
            failures.append("analytic cylinder-lens cell did not resolve a cylinder descriptor")
        if digest.get("transport_analytic_cylinder_lens_rejected_count", 0) != 0:
            failures.append("analytic cylinder-lens cell reported descriptor rejection")
        if digest.get("transport_analytic_cylinder_lens_evaluated_paths", 0) <= 0:
            failures.append("analytic cylinder-lens cell did not evaluate analytic paths")
        if digest.get("transport_analytic_cylinder_lens_emitted_paths", 0) <= 0:
            failures.append("analytic cylinder-lens cell did not emit analytic paths")
        if digest.get("transport_analytic_sphere_lens_resolved_count", 0) != 0:
            failures.append("analytic cylinder-lens cell unexpectedly resolved the sphere policy")
    return failures


def write_delta_artifacts(review_root: Path, runs_by_cell: dict[str, dict]) -> dict:
    baseline = runs_by_cell.get("mist_no_caustic")
    if not baseline:
        return {}
    before_w, before_h, before = review_artifacts.read_bmp_rgb(Path(baseline["frame_path"]))
    out_dir = review_root / "diffs"
    out_dir.mkdir(parents=True, exist_ok=True)
    result = {}
    for cell_id in (
        "volume_triangle_targets",
        "volume_analytic_cylinder_lens",
        "volume_analytic_cylinder_lens_focused",
    ):
        run = runs_by_cell.get(cell_id)
        if not run:
            continue
        after_w, after_h, after = review_artifacts.read_bmp_rgb(Path(run["frame_path"]))
        if (before_w, before_h) != (after_w, after_h):
            raise ValueError("delta frames must have matching dimensions")
        diff16_path = out_dir / f"{cell_id}_vs_mist_diff16x.png"
        side_path = out_dir / f"side_by_side_mist_{cell_id}_diff16x.png"
        diff16 = review_artifacts.abs_diff_pixels(before, after, 16)
        side_w, side_h, side = review_artifacts.side_by_side(before, after, diff16)
        review_artifacts.write_png_rgb(diff16_path, before_w, before_h, diff16)
        review_artifacts.write_png_rgb(side_path, side_w, side_h, side)
        result[cell_id] = {
            "diff16_path": str(diff16_path),
            "side_by_side_diff16_path": str(side_path),
        }
    return result


def vector3(record: dict, key: str) -> tuple[float, float, float] | None:
    value = record.get(key)
    if not isinstance(value, dict):
        return None
    return (
        float(value.get("x", 0.0)),
        float(value.get("y", 0.0)),
        float(value.get("z", 0.0)),
    )


def bounds3(values: list[tuple[float, float, float]]) -> dict:
    if not values:
        return {
            "min": {"x": 0.0, "y": 0.0, "z": 0.0},
            "max": {"x": 0.0, "y": 0.0, "z": 0.0},
            "span": {"x": 0.0, "y": 0.0, "z": 0.0},
        }
    mins = [min(v[i] for v in values) for i in range(3)]
    maxs = [max(v[i] for v in values) for i in range(3)]
    return {
        "min": {"x": mins[0], "y": mins[1], "z": mins[2]},
        "max": {"x": maxs[0], "y": maxs[1], "z": maxs[2]},
        "span": {"x": maxs[0] - mins[0], "y": maxs[1] - mins[1], "z": maxs[2] - mins[2]},
    }


def average(values: list[float]) -> float:
    return sum(values) / float(len(values)) if values else 0.0


def direction_spread_degrees(directions: list[tuple[float, float, float]]) -> dict:
    if not directions:
        return {"max_from_mean_degrees": 0.0, "avg_from_mean_degrees": 0.0}
    sx = sum(v[0] for v in directions)
    sy = sum(v[1] for v in directions)
    sz = sum(v[2] for v in directions)
    length = math.sqrt(sx * sx + sy * sy + sz * sz)
    if length <= 1.0e-9:
        return {"max_from_mean_degrees": 0.0, "avg_from_mean_degrees": 0.0}
    mean = (sx / length, sy / length, sz / length)
    angles = []
    for direction in directions:
        dot = max(-1.0, min(1.0, direction[0] * mean[0] + direction[1] * mean[1] + direction[2] * mean[2]))
        angles.append(math.degrees(math.acos(dot)))
    return {
        "max_from_mean_degrees": max(angles) if angles else 0.0,
        "avg_from_mean_degrees": average(angles),
    }


def read_debug_focus_diagnostics(review_root: Path) -> dict:
    result: dict[str, dict] = {}
    for cell_id in (
        "volume_triangle_targets",
        "volume_analytic_cylinder_lens",
        "volume_analytic_cylinder_lens_focused",
    ):
        debug_path = review_root / "runs" / cell_id / "caustic_transport_debug_paths.jsonl"
        lens_shape_counts: dict[str, int] = {}
        receiver_crossings: list[tuple[float, float, float]] = []
        exit_positions: list[tuple[float, float, float]] = []
        exit_directions: list[tuple[float, float, float]] = []
        inside_distances: list[float] = []
        footprint_radius_min: list[float] = []
        footprint_radius_max: list[float] = []
        volume_deposits: list[float] = []
        event_counts: list[float] = []
        if debug_path.exists():
            with debug_path.open("r", encoding="utf-8") as f:
                for line in f:
                    if not line.strip():
                        continue
                    record = json.loads(line)
                    shape = record.get("lens_shape_kind", "")
                    lens_shape_counts[shape] = lens_shape_counts.get(shape, 0) + 1
                    crossing = vector3(record, "lens_receiver_crossing")
                    if crossing:
                        receiver_crossings.append(crossing)
                    exit_position = vector3(record, "lens_exit_position")
                    if exit_position:
                        exit_positions.append(exit_position)
                    exit_direction = vector3(record, "lens_exit_outgoing_direction")
                    if exit_direction:
                        exit_directions.append(exit_direction)
                    if "lens_inside_distance" in record:
                        inside_distances.append(float(record.get("lens_inside_distance", 0.0)))
                    if "footprint_radius_min" in record:
                        footprint_radius_min.append(float(record.get("footprint_radius_min", 0.0)))
                    if "footprint_radius_max" in record:
                        footprint_radius_max.append(float(record.get("footprint_radius_max", 0.0)))
                    if "volume_deposit_accepted_count" in record:
                        volume_deposits.append(float(record.get("volume_deposit_accepted_count", 0.0)))
                    if "lens_interface_event_count" in record:
                        event_counts.append(float(record.get("lens_interface_event_count", 0.0)))
        crossing_bounds = bounds3(receiver_crossings)
        crossing_span = crossing_bounds["span"]
        result[cell_id] = {
            "path": str(debug_path),
            "path_count": sum(lens_shape_counts.values()),
            "lens_shape_kind_counts": lens_shape_counts,
            "receiver_crossing_bounds": crossing_bounds,
            "receiver_crossing_lateral_span": math.sqrt(
                crossing_span["x"] * crossing_span["x"] + crossing_span["y"] * crossing_span["y"]
            ),
            "exit_position_bounds": bounds3(exit_positions),
            "exit_direction_spread_degrees": direction_spread_degrees(exit_directions),
            "inside_distance_avg": average(inside_distances),
            "inside_distance_min": min(inside_distances) if inside_distances else 0.0,
            "inside_distance_max": max(inside_distances) if inside_distances else 0.0,
            "footprint_radius_min_avg": average(footprint_radius_min),
            "footprint_radius_max_avg": average(footprint_radius_max),
            "volume_deposit_accepted_avg": average(volume_deposits),
            "lens_interface_event_count_avg": average(event_counts),
        }
    return result


def read_debug_lens_shapes(review_root: Path) -> dict:
    result: dict[str, dict] = {}
    for cell_id in (
        "volume_triangle_targets",
        "volume_analytic_cylinder_lens",
        "volume_analytic_cylinder_lens_focused",
    ):
        debug_path = review_root / "runs" / cell_id / "caustic_transport_debug_paths.jsonl"
        counts: dict[str, int] = {}
        if debug_path.exists():
            with debug_path.open("r", encoding="utf-8") as f:
                for line in f:
                    if not line.strip():
                        continue
                    record = json.loads(line)
                    shape = record.get("lens_shape_kind", "")
                    counts[shape] = counts.get(shape, 0) + 1
        result[cell_id] = {
            "path": str(debug_path),
            "lens_shape_kind_counts": counts,
        }
    return result


def focused_line_review(report: dict) -> dict:
    broad_id = "volume_analytic_cylinder_lens"
    focused_id = "volume_analytic_cylinder_lens_focused"
    diagnostics = report.get("focus_diagnostics", {})
    deltas = report.get("frame_deltas_vs_off", {})
    broad_focus = diagnostics.get(broad_id, {})
    focused_focus = diagnostics.get(focused_id, {})
    broad_delta = deltas.get(broad_id, {})
    focused_delta = deltas.get(focused_id, {})

    broad_span = broad_focus.get("receiver_crossing_bounds", {}).get("span", {})
    focused_span = focused_focus.get("receiver_crossing_bounds", {}).get("span", {})
    broad_spread = broad_focus.get("exit_direction_spread_degrees", {})
    focused_spread = focused_focus.get("exit_direction_spread_degrees", {})
    broad_positive = float(broad_delta.get("positive_pixel_count", 0.0))
    focused_positive = float(focused_delta.get("positive_pixel_count", 0.0))

    def ratio(num: float, den: float) -> float:
        return num / den if den > 1.0e-9 else 0.0

    review = {
        "broad_cell_id": broad_id,
        "focused_cell_id": focused_id,
        "receiver_span_y_ratio": ratio(float(focused_span.get("y", 0.0)), float(broad_span.get("y", 0.0))),
        "receiver_span_z_ratio": ratio(float(focused_span.get("z", 0.0)), float(broad_span.get("z", 0.0))),
        "exit_direction_avg_spread_ratio": ratio(
            float(focused_spread.get("avg_from_mean_degrees", 0.0)),
            float(broad_spread.get("avg_from_mean_degrees", 0.0)),
        ),
        "exit_direction_max_spread_ratio": ratio(
            float(focused_spread.get("max_from_mean_degrees", 0.0)),
            float(broad_spread.get("max_from_mean_degrees", 0.0)),
        ),
        "positive_pixel_ratio": ratio(focused_positive, broad_positive),
        "focused_line_positive": False,
    }
    review["focused_line_positive"] = (
        focused_focus.get("path_count", 0) > 0
        and review["receiver_span_y_ratio"] <= 0.15
        and review["receiver_span_z_ratio"] <= 0.35
        and review["exit_direction_avg_spread_ratio"] <= 0.65
        and focused_positive >= broad_positive
    )
    return review


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Spatial Caustic Cylinder Lens Fixture",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- scene: `{report['scene_path']}`",
        f"- vf3d: `{report['vf3d_path']}`",
        f"- contact sheet: `{Path(report['contact_sheet_path']).name}`",
        "",
        "## Focus Diagnostics",
        "",
    ]
    for cell_id, focus in report.get("focus_diagnostics", {}).items():
        receiver_span = focus.get("receiver_crossing_bounds", {}).get("span", {})
        direction_spread = focus.get("exit_direction_spread_degrees", {})
        lines.append(
            f"- `{cell_id}`: paths `{focus.get('path_count', 0)}`, "
            f"lens shapes `{focus.get('lens_shape_kind_counts', {})}`, "
            f"receiver span x/y/z `{receiver_span.get('x', 0.0):.4f}`/"
            f"`{receiver_span.get('y', 0.0):.4f}`/"
            f"`{receiver_span.get('z', 0.0):.4f}`, "
            f"exit dir max/avg spread `{direction_spread.get('max_from_mean_degrees', 0.0):.3f}`/"
            f"`{direction_spread.get('avg_from_mean_degrees', 0.0):.3f}` deg, "
            f"inside avg `{focus.get('inside_distance_avg', 0.0):.4f}`, "
            f"footprint avg `{focus.get('footprint_radius_min_avg', 0.0):.4f}`-"
            f"`{focus.get('footprint_radius_max_avg', 0.0):.4f}`"
        )
    lines.extend([
        "",
        "## Focused Line Review",
        "",
    ])
    review = report.get("focused_line_review", {})
    if review:
        lines.append(
            f"- focused line positive: `{review.get('focused_line_positive', False)}`; "
            f"receiver y/z ratios `{review.get('receiver_span_y_ratio', 0.0):.4f}`/"
            f"`{review.get('receiver_span_z_ratio', 0.0):.4f}`; "
            f"exit avg/max spread ratios `{review.get('exit_direction_avg_spread_ratio', 0.0):.4f}`/"
            f"`{review.get('exit_direction_max_spread_ratio', 0.0):.4f}`; "
            f"positive pixel ratio `{review.get('positive_pixel_ratio', 0.0):.4f}`"
        )
    lines.extend([
        "",
        "## Runs",
        "",
    ])
    for run in report["runs"]:
        digest = run["caustic"]
        lines.append(
            f"- `{run['cell_id']}`: transport `{digest['transport_active']}`, "
            f"cylinder resolved `{digest.get('transport_analytic_cylinder_lens_resolved_count', 0)}`, "
            f"cylinder emitted `{digest.get('transport_analytic_cylinder_lens_emitted_paths', 0)}`, "
            f"volume cells `{digest['volume_cache_nonzero_cells']}`, "
            f"scatter samples `{digest['volume_scatter_contributing_samples']}`, "
            f"scatter pixels `{digest['volume_scatter_contributing_pixel_count']}`, "
            f"volume radiance `{digest['volume_scatter_radiance_sum']:.4f}`, "
            f"PNG `{Path(run['png_path']).name}`"
        )
    lines.extend(["", "## Frame Deltas Vs Mist No Caustic", ""])
    for cell_id, deltas in report["frame_deltas_vs_off"].items():
        lines.append(
            f"- `{cell_id}`: positive `{deltas['positive_pixel_count']}`, "
            f"high+ `{deltas['high_positive_pixel_count']}`, "
            f"mean `{deltas['mean_luma_delta']:.4f}`, max `{deltas['max_luma_delta']:.4f}`, "
            f"p95+ `{deltas['positive_luma_delta_p95']:.4f}`"
        )
    if report["failures"]:
        lines.extend(["", "## Failures", ""])
        for failure in report["failures"]:
            lines.append(f"- {failure}")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    cli = args.cli.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2
    review_root.mkdir(parents=True, exist_ok=True)

    scene_path = write_cylinder_scene(review_root)
    volume_path = review_root / "generated_volume" / "dark_grazing_mist.vf3d"
    sphere_mist.write_soft_mist_vf3d(volume_path)
    cell_ids = [
        "mist_no_caustic",
        "volume_triangle_targets",
        "volume_analytic_cylinder_lens",
        "volume_analytic_cylinder_lens_focused",
    ]
    runs = []
    runs_by_cell = {}
    failures = []
    for cell_id in cell_ids:
        item = request_for_cell(review_root, scene_path, volume_path, cell_id, args.debug_export)
        elapsed = sphere_mist.render_request(cli, item["request_path"], item["summary_path"], args.skip_render)
        summary = load_json(item["summary_path"])
        frame_path, png_path = sphere_mist.copy_frame_png(summary, review_root, cell_id)
        summary_copy = review_root / "summaries" / f"summary_{cell_id}.json"
        summary_copy.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(item["summary_path"], summary_copy)
        digest = cylinder_digest(summary)
        cell_failures = validate_cell(cell_id, digest)
        failures.extend([f"{cell_id}: {failure}" for failure in cell_failures])
        run = {
            "cell_id": cell_id,
            "request_path": str(item["request_path"]),
            "summary_path": str(item["summary_path"]),
            "summary_copy": str(summary_copy),
            "frame_path": str(frame_path),
            "png_path": str(png_path),
            "elapsed_seconds": elapsed,
            "caustic": digest,
            "failures": cell_failures,
        }
        runs.append(run)
        runs_by_cell[cell_id] = run

    frame_deltas = sphere_mist.frame_delta_metrics(runs_by_cell)
    contact_sheet_path = review_root / "cylinder_lens_fixture_contact_sheet.png"
    sphere_mist.write_contact_sheet(contact_sheet_path, runs)
    delta_artifacts = write_delta_artifacts(review_root, runs_by_cell)
    report = {
        "schema_version": "ray_tracing_spatial_caustic_cylinder_lens_fixture_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "scene_path": str(scene_path),
        "vf3d_path": str(volume_path),
        "contact_sheet_path": str(contact_sheet_path),
        "delta_artifacts": delta_artifacts,
        "debug_readback": read_debug_lens_shapes(review_root),
        "focus_diagnostics": read_debug_focus_diagnostics(review_root),
        "runs": runs,
        "frame_deltas_vs_off": frame_deltas,
        "failures": failures,
        "passed": len(failures) == 0,
    }
    report["focused_line_review"] = focused_line_review(report)
    report_path = review_root / "cylinder_lens_fixture_report.json"
    write_json(report_path, report)
    write_index(review_root / "cylinder_lens_fixture_index.md", report)
    print(report_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
