#!/usr/bin/env python3
"""Run a physical mist visual for the mesh dielectric-lens transport path."""

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
        / "caustic_mesh_dielectric_lens_fixture"
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


def write_mesh_dielectric_runtime_mesh_asset(mesh_dir: Path) -> Path:
    mesh_dir.mkdir(parents=True, exist_ok=True)
    segments = 32
    vertices = [{"x": 0.0, "y": -0.16, "z": 0.0}, {"x": 0.0, "y": 0.16, "z": 0.0}]
    for ring_y in (-0.16, 0.16):
        for i in range(segments):
            theta = (2.0 * math.pi * i) / float(segments)
            vertices.append({"x": math.cos(theta), "y": ring_y, "z": math.sin(theta)})
    triangles = []
    front_start = 2
    back_start = front_start + segments
    for i in range(segments):
        n = (i + 1) % segments
        triangles.append({"a": 0, "b": front_start + n, "c": front_start + i, "surface_group_id": "mesh_dielectric_face"})
        triangles.append({"a": 1, "b": back_start + i, "c": back_start + n, "surface_group_id": "mesh_dielectric_face"})
        triangles.append({"a": front_start + i, "b": front_start + n, "c": back_start + n, "surface_group_id": "mesh_dielectric_rim"})
        triangles.append({"a": front_start + i, "b": back_start + n, "c": back_start + i, "surface_group_id": "mesh_dielectric_rim"})
    asset = {
        "schema_family": "codework_geometry",
        "schema_variant": "mesh_asset_runtime_v1",
        "schema_version": 1,
        "asset_id": "asset_caustic_mesh_dielectric_lens",
        "source_asset_id": "generated_mesh_dielectric_fixture",
        "asset_type": "solid_mesh",
        "compile_meta": {
            "profile": "runtime_default",
            "generator": Path(__file__).name,
            "shape": "mesh_dielectric_lens",
            "axis": "y",
            "fidelity": "fixture_review",
        },
        "local_bounds": {
            "min": {"x": -1.0, "y": -0.16, "z": -1.0},
            "max": {"x": 1.0, "y": 0.16, "z": 1.0},
        },
        "mesh": {
            "vertex_count": len(vertices),
            "triangle_count": len(triangles),
            "vertices": vertices,
            "triangles": triangles,
        },
        "surface_groups": [
            {
                "group_id": "mesh_dielectric_face",
                "semantic": "water_like_curved_faces",
                "triangle_span": {
                    "start": 0,
                    "count": segments * 2,
                },
            },
            {
                "group_id": "mesh_dielectric_rim",
                "semantic": "thin_lens_rim",
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
    mesh_path = mesh_dir / "asset_caustic_mesh_dielectric_lens.runtime.json"
    write_json(mesh_path, asset)
    return mesh_path


def write_open_mesh_dielectric_negative_runtime_mesh_asset(mesh_dir: Path) -> Path:
    closed_path = write_mesh_dielectric_runtime_mesh_asset(mesh_dir)
    asset = load_json(closed_path)
    mesh = asset.get("mesh", {})
    triangles = list(mesh.get("triangles", []))
    open_triangles = [
        triangle for index, triangle in enumerate(triangles)
        if index % 4 != 0
    ]
    asset["asset_id"] = "asset_caustic_mesh_dielectric_lens_open_negative"
    asset["source_asset_id"] = "generated_mesh_dielectric_open_negative_fixture"
    asset["compile_meta"]["fidelity"] = "fixture_negative_open_topology"
    asset["compile_meta"]["negative_case"] = "front_cap_removed"
    mesh["triangle_count"] = len(open_triangles)
    mesh["triangles"] = open_triangles
    asset["surface_groups"] = [
        {
            "group_id": "mesh_dielectric_face",
            "semantic": "water_like_curved_faces_missing_front_cap",
            "triangle_span": {
                "start": 0,
                "count": 32,
            },
        },
        {
            "group_id": "mesh_dielectric_rim",
            "semantic": "thin_lens_rim",
            "triangle_span": {
                "start": 32,
                "count": 64,
            },
        },
    ]
    asset["topology_flags"] = {
        "closed_volume": True,
        "manifold_expected": True,
    }
    mesh_path = mesh_dir / "asset_caustic_mesh_dielectric_lens_open_negative.runtime.json"
    write_json(mesh_path, asset)
    return mesh_path


def write_mesh_dielectric_scene(review_root: Path) -> Path:
    scene_path = sphere_mist.write_visual_scene(review_root)
    scene = load_json(scene_path)
    scene["scene_id"] = "caustic_mesh_dielectric_lens_fixture_v1"
    mesh_dir = scene_path.parent / "assets" / "mesh_assets"
    write_mesh_dielectric_runtime_mesh_asset(mesh_dir)
    write_open_mesh_dielectric_negative_runtime_mesh_asset(mesh_dir)
    for obj in scene.get("objects", []):
        if obj.get("object_id") == "high_quality_glass_sphere":
            obj["object_id"] = "authored_mesh_dielectric_lens"
            transform = obj.setdefault("transform", {})
            transform["position"] = {"x": 0.0, "y": 0.0, "z": 1.38}
            transform["scale"] = {"x": 0.76, "y": 0.34, "z": 0.70}
            obj["geometry_ref"] = {
                "kind": "mesh_asset",
                "id": "asset_caustic_mesh_dielectric_lens",
                "variant": "runtime_default",
            }
            obj["extensions"] = {
                "line_drawing": {
                    "runtime_mesh_path": "assets/mesh_assets/asset_caustic_mesh_dielectric_lens.runtime.json",
                }
            }
    for light in scene.get("lights", []):
        if light.get("light_id") == "overhead_focus_light":
            light["position"] = {"x": 0.0, "y": -1.55, "z": 3.70}
            light["radius"] = 0.07
            light["intensity"] = 10.2
    for camera in scene.get("cameras", []):
        if camera.get("camera_id") == "caustic_visual_camera":
            camera["position"] = {"x": 0.78, "y": -5.55, "z": 0.88}
            camera["target"] = {"x": 0.0, "y": 0.0, "z": 1.02}
    authoring = scene.get("extensions", {}).get("ray_tracing", {}).get("authoring", {})
    authoring["camera_focus_target"] = {"x": 0.0, "y": 0.0, "z": 1.02}
    for material in authoring.get("object_materials", []):
        if material.get("object_id") == "high_quality_glass_sphere":
            material["object_id"] = "authored_mesh_dielectric_lens"
            material["object_color"] = sphere_mist.rgb_u24(210, 236, 255)
            material["alpha"] = 0.34
            material["roughness"] = 0.01
            material["reflectivity"] = 0.03
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
        f"caustic_mesh_dielectric_lens_{cell_id}",
        scene_path,
        output_root,
        summary_path,
        volume_path,
    )
    request["inspection"].update({
        "camera_position": {"x": 0.78, "y": -5.55, "z": 0.88},
        "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.02},
        "camera_zoom": 0.92,
        "light_intensity": 10.2,
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
    elif cell_id == "volume_mesh_dielectric_lens":
        request["inspection"].update({
            "caustic_mode": "transport",
            "caustic_volume_enabled": True,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 3072,
            "caustic_max_path_depth": 2,
            "caustic_surface_receiver_fallback_enabled": False,
            "caustic_transport_emission_policy": "mesh_dielectric_lens",
            "caustic_lens_traversal_preset": "dense_glass",
        })
    else:
        raise ValueError(f"unknown mesh_dielectric fixture cell: {cell_id}")
    if debug_export and cell_id != "mist_no_caustic":
        request["inspection"]["caustic_transport_debug_export_enabled"] = True
    request_path = review_root / "generated_requests" / f"request_{cell_id}.json"
    write_json(request_path, request)
    return {
        "cell_id": cell_id,
        "request_path": request_path,
        "summary_path": summary_path,
    }


def mesh_dielectric_digest(summary: dict) -> dict:
    digest = sphere_mist.caustic_digest(summary)
    state = summary.get("inspection", {}).get("caustic_state", {})
    digest.update({
        "transport_mesh_dielectric_lens_resolved_count": int(
            state.get("transport_mesh_dielectric_lens_resolved_count", 0)
        ),
        "transport_mesh_dielectric_lens_rejected_count": int(
            state.get("transport_mesh_dielectric_lens_rejected_count", 0)
        ),
        "transport_mesh_dielectric_lens_evaluated_paths": int(
            state.get("transport_mesh_dielectric_lens_evaluated_path_count", 0)
        ),
        "transport_mesh_dielectric_lens_emitted_paths": int(
            state.get("transport_mesh_dielectric_lens_emitted_path_count", 0)
        ),
        "transport_mesh_dielectric_lens_sample_weight": float(
            state.get("transport_mesh_dielectric_lens_sample_weight", 0.0)
        ),
        "transport_mesh_dielectric_lens_total_sample_weight": float(
            state.get("transport_mesh_dielectric_lens_total_sample_weight", 0.0)
        ),
    })
    return digest


def validate_cell(cell_id: str, digest: dict) -> list[str]:
    if cell_id == "mist_no_caustic":
        return sphere_mist.validate_cell(cell_id, digest)
    failures = sphere_mist.validate_cell("volume_caustic_only", digest)
    if cell_id == "volume_mesh_dielectric_lens":
        if digest.get("transport_mesh_dielectric_lens_resolved_count", 0) <= 0:
            failures.append("mesh dielectric-lens cell did not resolve a mesh_dielectric descriptor")
        if digest.get("transport_mesh_dielectric_lens_rejected_count", 0) != 0:
            failures.append("mesh dielectric-lens cell reported descriptor rejection")
        if digest.get("transport_mesh_dielectric_lens_evaluated_paths", 0) <= 0:
            failures.append("mesh dielectric-lens cell did not evaluate mesh paths")
        if digest.get("transport_mesh_dielectric_lens_emitted_paths", 0) <= 0:
            failures.append("mesh dielectric-lens cell did not emit mesh paths")
        if digest.get("transport_analytic_sphere_lens_resolved_count", 0) != 0:
            failures.append("mesh dielectric-lens cell unexpectedly resolved the sphere policy")
        if digest.get("transport_analytic_cylinder_lens_resolved_count", 0) != 0:
            failures.append("mesh dielectric-lens cell unexpectedly resolved the cylinder policy")
        if digest.get("transport_analytic_prism_lens_resolved_count", 0) != 0:
            failures.append("mesh dielectric-lens cell unexpectedly resolved the prism policy")
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
        "volume_mesh_dielectric_lens",
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


def audit_runtime_mesh_topology(mesh_path: Path) -> dict:
    asset = load_json(mesh_path)
    mesh = asset.get("mesh", {})
    vertices_raw = mesh.get("vertices", [])
    triangles_raw = mesh.get("triangles", [])
    vertices = [
        (
            float(vertex.get("x", 0.0)),
            float(vertex.get("y", 0.0)),
            float(vertex.get("z", 0.0)),
        )
        for vertex in vertices_raw
    ]
    edge_uses: dict[tuple[int, int], list[tuple[int, int]]] = {}
    degenerate_triangles = 0
    signed_volume = 0.0
    total_area = 0.0
    for triangle in triangles_raw:
        try:
            indices = [
                int(triangle["a"]),
                int(triangle["b"]),
                int(triangle["c"]),
            ]
        except (KeyError, TypeError, ValueError):
            degenerate_triangles += 1
            continue
        if any(index < 0 or index >= len(vertices) for index in indices):
            degenerate_triangles += 1
            continue
        a = vertices[indices[0]]
        b = vertices[indices[1]]
        c = vertices[indices[2]]
        ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        ac = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        cross = (
            ab[1] * ac[2] - ab[2] * ac[1],
            ab[2] * ac[0] - ab[0] * ac[2],
            ab[0] * ac[1] - ab[1] * ac[0],
        )
        area = math.sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]) * 0.5
        if area <= 1.0e-10:
            degenerate_triangles += 1
        total_area += area
        signed_volume += (
            a[0] * (b[1] * c[2] - b[2] * c[1]) +
            a[1] * (b[2] * c[0] - b[0] * c[2]) +
            a[2] * (b[0] * c[1] - b[1] * c[0])
        ) / 6.0
        directed_edges = [
            (indices[0], indices[1]),
            (indices[1], indices[2]),
            (indices[2], indices[0]),
        ]
        for start, end in directed_edges:
            key = (start, end) if start < end else (end, start)
            edge_uses.setdefault(key, []).append((start, end))

    boundary_edges = 0
    nonmanifold_edges = 0
    orientation_mismatch_edges = 0
    for uses in edge_uses.values():
        if len(uses) == 1:
            boundary_edges += 1
        elif len(uses) != 2:
            nonmanifold_edges += 1
        elif uses[0] == uses[1]:
            orientation_mismatch_edges += 1

    vertex_count = len(vertices)
    edge_count = len(edge_uses)
    triangle_count = len(triangles_raw)
    euler_characteristic = vertex_count - edge_count + triangle_count
    declared_flags = asset.get("topology_flags", {})
    closed = boundary_edges == 0 and nonmanifold_edges == 0
    consistently_oriented = closed and orientation_mismatch_edges == 0
    watertight_volume = consistently_oriented and abs(signed_volume) > 1.0e-9
    return {
        "path": str(mesh_path),
        "asset_id": asset.get("asset_id", ""),
        "source_asset_id": asset.get("source_asset_id", ""),
        "declared_closed_volume": bool(declared_flags.get("closed_volume", False)),
        "declared_manifold_expected": bool(declared_flags.get("manifold_expected", False)),
        "vertex_count": vertex_count,
        "edge_count": edge_count,
        "triangle_count": triangle_count,
        "declared_vertex_count": int(mesh.get("vertex_count", 0)),
        "declared_triangle_count": int(mesh.get("triangle_count", 0)),
        "degenerate_triangle_count": degenerate_triangles,
        "boundary_edge_count": boundary_edges,
        "nonmanifold_edge_count": nonmanifold_edges,
        "orientation_mismatch_edge_count": orientation_mismatch_edges,
        "euler_characteristic": euler_characteristic,
        "signed_volume": signed_volume,
        "absolute_volume": abs(signed_volume),
        "total_area": total_area,
        "closed_manifold": closed,
        "consistently_oriented": consistently_oriented,
        "watertight_volume": watertight_volume,
        "usable_for_closed_traversal": (
            closed and consistently_oriented and watertight_volume and
            degenerate_triangles == 0 and
            vertex_count == int(mesh.get("vertex_count", 0)) and
            triangle_count == int(mesh.get("triangle_count", 0))
        ),
    }


def validate_mesh_topology_audit(audit: dict) -> list[str]:
    failures: list[str] = []
    if not audit.get("declared_closed_volume", False):
        failures.append("runtime mesh sidecar does not declare closed_volume")
    if not audit.get("declared_manifold_expected", False):
        failures.append("runtime mesh sidecar does not declare manifold_expected")
    if audit.get("declared_vertex_count") != audit.get("vertex_count"):
        failures.append("runtime mesh sidecar vertex_count does not match vertices")
    if audit.get("declared_triangle_count") != audit.get("triangle_count"):
        failures.append("runtime mesh sidecar triangle_count does not match triangles")
    if audit.get("degenerate_triangle_count", 0) != 0:
        failures.append("runtime mesh sidecar has degenerate triangles")
    if audit.get("boundary_edge_count", 0) != 0:
        failures.append("runtime mesh sidecar has boundary edges")
    if audit.get("nonmanifold_edge_count", 0) != 0:
        failures.append("runtime mesh sidecar has nonmanifold edges")
    if audit.get("orientation_mismatch_edge_count", 0) != 0:
        failures.append("runtime mesh sidecar has inconsistent shared-edge orientation")
    if not audit.get("watertight_volume", False):
        failures.append("runtime mesh sidecar does not form a nonzero watertight volume")
    if audit.get("euler_characteristic") != 2:
        failures.append("runtime mesh sidecar closed genus-0 Euler characteristic is not 2")
    return failures


def validate_negative_mesh_topology_audit(audit: dict) -> list[str]:
    failures: list[str] = []
    if audit.get("usable_for_closed_traversal", False):
        failures.append("open negative mesh was incorrectly marked usable")
    if audit.get("boundary_edge_count", 0) <= 0:
        failures.append("open negative mesh did not report boundary edges")
    if audit.get("closed_manifold", False):
        failures.append("open negative mesh was incorrectly marked closed manifold")
    if audit.get("watertight_volume", False):
        failures.append("open negative mesh was incorrectly marked watertight")
    if not validate_mesh_topology_audit(audit):
        failures.append("open negative mesh did not trigger topology audit failures")
    return failures


def read_debug_transport_summaries(review_root: Path) -> dict:
    result: dict[str, dict] = {}
    for cell_id in (
        "volume_triangle_targets",
        "volume_mesh_dielectric_lens",
    ):
        debug_path = review_root / "runs" / cell_id / "caustic_transport_debug.json"
        transport = {}
        if debug_path.exists():
            debug = load_json(debug_path)
            transport = debug.get("transport", {})
        result[cell_id] = {
            "path": str(debug_path),
            "mesh_dielectric_lens_evaluated_path_count": int(
                transport.get("mesh_dielectric_lens_evaluated_path_count", 0)
            ),
            "mesh_dielectric_lens_emitted_path_count": int(
                transport.get("mesh_dielectric_lens_emitted_path_count", 0)
            ),
            "mesh_dielectric_lens_traversal_accepted_count": int(
                transport.get("mesh_dielectric_lens_traversal_accepted_count", 0)
            ),
            "mesh_dielectric_lens_reject_invalid_profile_count": int(
                transport.get("mesh_dielectric_lens_reject_invalid_profile_count", 0)
            ),
            "mesh_dielectric_lens_reject_sample_count": int(
                transport.get("mesh_dielectric_lens_reject_sample_count", 0)
            ),
            "mesh_dielectric_lens_reject_entry_miss_count": int(
                transport.get("mesh_dielectric_lens_reject_entry_miss_count", 0)
            ),
            "mesh_dielectric_lens_reject_entry_wrong_object_count": int(
                transport.get("mesh_dielectric_lens_reject_entry_wrong_object_count", 0)
            ),
            "mesh_dielectric_lens_reject_entry_refraction_count": int(
                transport.get("mesh_dielectric_lens_reject_entry_refraction_count", 0)
            ),
            "mesh_dielectric_lens_reject_exit_miss_count": int(
                transport.get("mesh_dielectric_lens_reject_exit_miss_count", 0)
            ),
            "mesh_dielectric_lens_reject_exit_wrong_object_count": int(
                transport.get("mesh_dielectric_lens_reject_exit_wrong_object_count", 0)
            ),
            "mesh_dielectric_lens_reject_exit_refraction_count": int(
                transport.get("mesh_dielectric_lens_reject_exit_refraction_count", 0)
            ),
            "mesh_dielectric_lens_reject_inside_distance_count": int(
                transport.get("mesh_dielectric_lens_reject_inside_distance_count", 0)
            ),
            "mesh_dielectric_lens_reject_throughput_count": int(
                transport.get("mesh_dielectric_lens_reject_throughput_count", 0)
            ),
            "mesh_dielectric_lens_inside_distance_avg": float(
                transport.get("mesh_dielectric_lens_inside_distance_avg", 0.0)
            ),
            "mesh_dielectric_lens_entry_cosine_avg": float(
                transport.get("mesh_dielectric_lens_entry_cosine_avg", 0.0)
            ),
            "mesh_dielectric_lens_exit_cosine_avg": float(
                transport.get("mesh_dielectric_lens_exit_cosine_avg", 0.0)
            ),
        }
    return result


def validate_mesh_traversal_summary(summary: dict) -> list[str]:
    failures: list[str] = []
    accepted = int(summary.get("mesh_dielectric_lens_traversal_accepted_count", 0))
    evaluated = int(summary.get("mesh_dielectric_lens_evaluated_path_count", 0))
    emitted = int(summary.get("mesh_dielectric_lens_emitted_path_count", 0))
    reject_keys = [
        "mesh_dielectric_lens_reject_invalid_profile_count",
        "mesh_dielectric_lens_reject_sample_count",
        "mesh_dielectric_lens_reject_entry_miss_count",
        "mesh_dielectric_lens_reject_entry_wrong_object_count",
        "mesh_dielectric_lens_reject_entry_refraction_count",
        "mesh_dielectric_lens_reject_exit_miss_count",
        "mesh_dielectric_lens_reject_exit_wrong_object_count",
        "mesh_dielectric_lens_reject_exit_refraction_count",
        "mesh_dielectric_lens_reject_inside_distance_count",
        "mesh_dielectric_lens_reject_throughput_count",
    ]
    if evaluated <= 0:
        failures.append("mesh traversal summary did not evaluate paths")
    if accepted != evaluated:
        failures.append("mesh traversal accepted count does not match evaluated count")
    if emitted != accepted:
        failures.append("mesh traversal emitted count does not match accepted count")
    for key in reject_keys:
        if int(summary.get(key, 0)) != 0:
            failures.append(f"mesh traversal summary reported nonzero {key}")
    if float(summary.get("mesh_dielectric_lens_inside_distance_avg", 0.0)) <= 0.0:
        failures.append("mesh traversal summary has no positive inside-distance average")
    if float(summary.get("mesh_dielectric_lens_entry_cosine_avg", 0.0)) <= 0.0:
        failures.append("mesh traversal summary has no positive entry-cosine average")
    if float(summary.get("mesh_dielectric_lens_exit_cosine_avg", 0.0)) <= 0.0:
        failures.append("mesh traversal summary has no positive exit-cosine average")
    return failures


def direction_spread_degrees(directions: list[tuple[float, float, float]]) -> dict:
    if not directions:
        return {
            "max_from_mean_degrees": 0.0,
            "avg_from_mean_degrees": 0.0,
            "mean_direction": {"x": 0.0, "y": 0.0, "z": 0.0},
        }
    sx = sum(v[0] for v in directions)
    sy = sum(v[1] for v in directions)
    sz = sum(v[2] for v in directions)
    length = math.sqrt(sx * sx + sy * sy + sz * sz)
    if length <= 1.0e-9:
        return {
            "max_from_mean_degrees": 0.0,
            "avg_from_mean_degrees": 0.0,
            "mean_direction": {"x": 0.0, "y": 0.0, "z": 0.0},
        }
    mean = (sx / length, sy / length, sz / length)
    angles = []
    for direction in directions:
        dot = max(-1.0, min(1.0, direction[0] * mean[0] + direction[1] * mean[1] + direction[2] * mean[2]))
        angles.append(math.degrees(math.acos(dot)))
    return {
        "max_from_mean_degrees": max(angles) if angles else 0.0,
        "avg_from_mean_degrees": average(angles),
        "mean_direction": {"x": mean[0], "y": mean[1], "z": mean[2]},
    }


def read_debug_focus_diagnostics(review_root: Path) -> dict:
    result: dict[str, dict] = {}
    for cell_id in (
        "volume_triangle_targets",
        "volume_mesh_dielectric_lens",
    ):
        debug_path = review_root / "runs" / cell_id / "caustic_transport_debug_paths.jsonl"
        lens_shape_counts: dict[str, int] = {}
        receiver_crossings: list[tuple[float, float, float]] = []
        exit_positions: list[tuple[float, float, float]] = []
        exit_directions: list[tuple[float, float, float]] = []
        entry_normals: list[tuple[float, float, float]] = []
        exit_normals: list[tuple[float, float, float]] = []
        inside_distances: list[float] = []
        footprint_radius_min: list[float] = []
        footprint_radius_max: list[float] = []
        volume_deposits: list[float] = []
        event_counts: list[float] = []
        material_iors: list[float] = []
        traversal_kinds: list[int] = []
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
                    entry_normal = vector3(record, "lens_entry_normal")
                    if entry_normal:
                        entry_normals.append(entry_normal)
                    exit_normal = vector3(record, "lens_exit_normal")
                    if exit_normal:
                        exit_normals.append(exit_normal)
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
                    if "lens_material_ior" in record:
                        material_iors.append(float(record.get("lens_material_ior", 0.0)))
                    if "lens_traversal_profile_kind" in record:
                        traversal_kinds.append(int(record.get("lens_traversal_profile_kind", 0)))
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
            "entry_normal_bounds": bounds3(entry_normals),
            "exit_normal_bounds": bounds3(exit_normals),
            "inside_distance_avg": average(inside_distances),
            "inside_distance_min": min(inside_distances) if inside_distances else 0.0,
            "inside_distance_max": max(inside_distances) if inside_distances else 0.0,
            "footprint_radius_min_avg": average(footprint_radius_min),
            "footprint_radius_max_avg": average(footprint_radius_max),
            "volume_deposit_accepted_avg": average(volume_deposits),
            "lens_interface_event_count_avg": average(event_counts),
            "lens_material_ior_avg": average(material_iors),
            "lens_traversal_profile_kinds": sorted(set(traversal_kinds)),
        }
    return result


def read_debug_lens_shapes(review_root: Path) -> dict:
    result: dict[str, dict] = {}
    for cell_id in (
        "volume_triangle_targets",
        "volume_mesh_dielectric_lens",
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


def mesh_dielectric_slab_profile_review(report: dict) -> dict:
    cell_id = "volume_mesh_dielectric_lens"
    diagnostics = report.get("focus_diagnostics", {})
    deltas = report.get("frame_deltas_vs_off", {})
    focus = diagnostics.get(cell_id, {})
    delta = deltas.get(cell_id, {})
    shape_counts = focus.get("lens_shape_kind_counts", {})
    positive_pixels = float(delta.get("positive_pixel_count", 0.0))

    review = {
        "cell_id": cell_id,
        "path_count": int(focus.get("path_count", 0)),
        "mesh_dielectric_shape_count": int(shape_counts.get("mesh_dielectric", 0)),
        "lens_interface_event_count_avg": float(focus.get("lens_interface_event_count_avg", 0.0)),
        "inside_distance_avg": float(focus.get("inside_distance_avg", 0.0)),
        "lens_material_ior_avg": float(focus.get("lens_material_ior_avg", 0.0)),
        "lens_traversal_profile_kinds": focus.get("lens_traversal_profile_kinds", []),
        "positive_pixel_count": positive_pixels,
        "slab_profile_positive": False,
    }
    review["slab_profile_positive"] = (
        review["path_count"] > 0
        and review["mesh_dielectric_shape_count"] > 0
        and review["positive_pixel_count"] > 0
        and review["lens_interface_event_count_avg"] >= 1.99
        and review["inside_distance_avg"] > 0.0
        and review["lens_material_ior_avg"] > 1.55
    )
    return review


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Spatial Caustic Mesh Dielectric Lens Fixture",
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
        "## Mesh Dielectric Slab Profile Review",
        "",
    ])
    review = report.get("mesh_dielectric_slab_profile_review", {})
    if review:
        lines.append(
            f"- slab/profile positive: `{review.get('slab_profile_positive', False)}`; "
            f"paths `{review.get('path_count', 0)}`; "
            f"mesh_dielectric shape count `{review.get('mesh_dielectric_shape_count', 0)}`; "
            f"event avg `{review.get('lens_interface_event_count_avg', 0.0):.4f}`; "
            f"inside avg `{review.get('inside_distance_avg', 0.0):.4f}`; "
            f"material IOR avg `{review.get('lens_material_ior_avg', 0.0):.4f}`; "
            f"positive pixels `{review.get('positive_pixel_count', 0.0)}`"
        )
    lines.extend([
        "",
        "## Mesh Topology Audit",
        "",
    ])
    topology = report.get("mesh_topology_audit", {})
    if topology:
        lines.append(
            f"- asset `{topology.get('asset_id', '')}` from "
            f"`{topology.get('source_asset_id', '')}`: vertices "
            f"`{topology.get('vertex_count', 0)}`, edges "
            f"`{topology.get('edge_count', 0)}`, triangles "
            f"`{topology.get('triangle_count', 0)}`, boundary edges "
            f"`{topology.get('boundary_edge_count', 0)}`, nonmanifold edges "
            f"`{topology.get('nonmanifold_edge_count', 0)}`, orientation "
            f"mismatches `{topology.get('orientation_mismatch_edge_count', 0)}`, "
            f"Euler `{topology.get('euler_characteristic', 0)}`, volume "
            f"`{topology.get('absolute_volume', 0.0):.6f}`, usable "
            f"`{topology.get('usable_for_closed_traversal', False)}`"
        )
    negative_topology = report.get("negative_mesh_topology_audit", {})
    if negative_topology:
        lines.append(
            f"- negative asset `{negative_topology.get('asset_id', '')}` from "
            f"`{negative_topology.get('source_asset_id', '')}`: triangles "
            f"`{negative_topology.get('triangle_count', 0)}`, boundary edges "
            f"`{negative_topology.get('boundary_edge_count', 0)}`, nonmanifold "
            f"edges `{negative_topology.get('nonmanifold_edge_count', 0)}`, "
            f"orientation mismatches "
            f"`{negative_topology.get('orientation_mismatch_edge_count', 0)}`, "
            f"Euler `{negative_topology.get('euler_characteristic', 0)}`, "
            f"watertight `{negative_topology.get('watertight_volume', False)}`, "
            f"usable `{negative_topology.get('usable_for_closed_traversal', False)}`"
        )
    traversal = report.get("mesh_traversal_debug_summary", {}).get("volume_mesh_dielectric_lens", {})
    if traversal:
        lines.append(
            f"- traversal debug: evaluated "
            f"`{traversal.get('mesh_dielectric_lens_evaluated_path_count', 0)}`, "
            f"accepted "
            f"`{traversal.get('mesh_dielectric_lens_traversal_accepted_count', 0)}`, "
            f"emitted "
            f"`{traversal.get('mesh_dielectric_lens_emitted_path_count', 0)}`, "
            f"inside avg "
            f"`{traversal.get('mesh_dielectric_lens_inside_distance_avg', 0.0):.4f}`, "
            f"entry/exit cosine avg "
            f"`{traversal.get('mesh_dielectric_lens_entry_cosine_avg', 0.0):.4f}`/"
            f"`{traversal.get('mesh_dielectric_lens_exit_cosine_avg', 0.0):.4f}`"
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
            f"mesh_dielectric resolved `{digest.get('transport_mesh_dielectric_lens_resolved_count', 0)}`, "
            f"mesh_dielectric emitted `{digest.get('transport_mesh_dielectric_lens_emitted_paths', 0)}`, "
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

    failures = []
    scene_path = write_mesh_dielectric_scene(review_root)
    mesh_path = (
        scene_path.parent /
        "assets" /
        "mesh_assets" /
        "asset_caustic_mesh_dielectric_lens.runtime.json"
    )
    negative_mesh_path = (
        scene_path.parent /
        "assets" /
        "mesh_assets" /
        "asset_caustic_mesh_dielectric_lens_open_negative.runtime.json"
    )
    topology_audit = audit_runtime_mesh_topology(mesh_path)
    negative_topology_audit = audit_runtime_mesh_topology(negative_mesh_path)
    failures.extend([
        f"mesh_topology: {failure}"
        for failure in validate_mesh_topology_audit(topology_audit)
    ])
    failures.extend([
        f"negative_mesh_topology: {failure}"
        for failure in validate_negative_mesh_topology_audit(negative_topology_audit)
    ])
    volume_path = review_root / "generated_volume" / "dark_grazing_mist.vf3d"
    sphere_mist.write_soft_mist_vf3d(volume_path)
    cell_ids = [
        "mist_no_caustic",
        "volume_triangle_targets",
        "volume_mesh_dielectric_lens",
    ]
    runs = []
    runs_by_cell = {}
    for cell_id in cell_ids:
        item = request_for_cell(review_root, scene_path, volume_path, cell_id, args.debug_export)
        elapsed = sphere_mist.render_request(cli, item["request_path"], item["summary_path"], args.skip_render)
        summary = load_json(item["summary_path"])
        frame_path, png_path = sphere_mist.copy_frame_png(summary, review_root, cell_id)
        summary_copy = review_root / "summaries" / f"summary_{cell_id}.json"
        summary_copy.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(item["summary_path"], summary_copy)
        digest = mesh_dielectric_digest(summary)
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
    contact_sheet_path = review_root / "mesh_dielectric_lens_fixture_contact_sheet.png"
    sphere_mist.write_contact_sheet(contact_sheet_path, runs)
    delta_artifacts = write_delta_artifacts(review_root, runs_by_cell)
    report = {
        "schema_version": "ray_tracing_spatial_caustic_mesh_dielectric_lens_fixture_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "scene_path": str(scene_path),
        "mesh_path": str(mesh_path),
        "negative_mesh_path": str(negative_mesh_path),
        "vf3d_path": str(volume_path),
        "contact_sheet_path": str(contact_sheet_path),
        "delta_artifacts": delta_artifacts,
        "mesh_topology_audit": topology_audit,
        "negative_mesh_topology_audit": negative_topology_audit,
        "debug_readback": read_debug_lens_shapes(review_root),
        "mesh_traversal_debug_summary": read_debug_transport_summaries(review_root),
        "focus_diagnostics": read_debug_focus_diagnostics(review_root),
        "runs": runs,
        "frame_deltas_vs_off": frame_deltas,
        "failures": failures,
        "passed": len(failures) == 0,
    }
    traversal_summary = report["mesh_traversal_debug_summary"].get("volume_mesh_dielectric_lens", {})
    traversal_failures = validate_mesh_traversal_summary(traversal_summary)
    if traversal_failures:
        report["failures"].extend([
            f"volume_mesh_dielectric_lens: {failure}"
            for failure in traversal_failures
        ])
        report["passed"] = False
    report["mesh_dielectric_slab_profile_review"] = mesh_dielectric_slab_profile_review(report)
    if not report["mesh_dielectric_slab_profile_review"].get("slab_profile_positive", False):
        report["failures"].append("volume_mesh_dielectric_lens: slab/profile review did not pass")
        report["passed"] = False
    report_path = review_root / "mesh_dielectric_lens_fixture_report.json"
    write_json(report_path, report)
    write_index(review_root / "mesh_dielectric_lens_fixture_index.md", report)
    print(report_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
