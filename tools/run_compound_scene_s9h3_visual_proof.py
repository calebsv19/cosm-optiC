#!/usr/bin/env python3
"""Render the local-only S9-H3 exact six-plane room proof at four ticks."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import platform
import subprocess
from pathlib import Path

import run_compound_scene_s9e_visual_proof as s9e


TICKS = (0, 240, 480, 720)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def rebuild_normals(asset: dict) -> None:
    vertices = asset["mesh"]["vertices"]
    sums = [[0.0, 0.0, 0.0] for _ in vertices]
    for triangle in asset["mesh"]["triangles"]:
        ids = (triangle["a"], triangle["b"], triangle["c"])
        a, b, c = ([vertices[index][axis] for axis in ("x", "y", "z")]
                   for index in ids)
        ab = [b[i] - a[i] for i in range(3)]
        ac = [c[i] - a[i] for i in range(3)]
        normal = [ab[1] * ac[2] - ab[2] * ac[1],
                  ab[2] * ac[0] - ab[0] * ac[2],
                  ab[0] * ac[1] - ab[1] * ac[0]]
        for index in ids:
            for axis in range(3):
                sums[index][axis] += normal[axis]
    for target, normal in zip(asset["mesh"]["normals"], sums):
        length = math.sqrt(sum(value * value for value in normal))
        if length <= 0.0:
            raise RuntimeError("degenerate transformed source-mesh normal")
        target.update(dict(zip(("x", "y", "z"),
                               (value / length for value in normal))))


def mesh_object(template: dict, object_id: str, asset_id: str,
                material_id: str) -> dict:
    result = copy.deepcopy(template)
    result["object_id"] = object_id
    result["geometry_ref"]["id"] = asset_id
    result["material_ref"]["id"] = material_id
    return result


def plane_object(plane: dict) -> dict:
    return s9e.primitive(
        plane["object_id"], plane["material_id"], plane["width"],
        plane["height"], plane["origin"], plane["axis_u"],
        plane["axis_v"], plane["normal"])


def plane_clearance(vertices: list[list[float]], plane: dict) -> float:
    origin = plane["origin"]
    normal = plane["normal"]
    return min(sum((vertex[i] - origin[i]) * normal[i] for i in range(3))
               for vertex in vertices)


def make_contact_sheet(frames: list[Path], output: Path) -> None:
    cells = []
    colors = ((220, 105, 45), (220, 155, 45),
              (65, 145, 215), (120, 85, 205))
    for path, color in zip(frames, colors):
        width, height, pixels = s9e.review_artifacts.read_bmp_rgb(path)
        cells.append([[color] * width for _ in range(8)] + pixels)
    separator = 8
    width = len(cells[0][0])
    height = len(cells[0])
    rows = []
    for row_index in range(2):
        left, right = cells[row_index * 2:row_index * 2 + 2]
        for y in range(height):
            rows.append(left[y] + [(25, 28, 34)] * separator + right[y])
        if row_index == 0:
            rows.extend([[(25, 28, 34)] * (width * 2 + separator)] *
                        separator)
    output.parent.mkdir(parents=True, exist_ok=True)
    s9e.review_artifacts.write_png_rgb(output, len(rows[0]), len(rows), rows)


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    fixture = root / "tests" / "fixtures" / "compound_scene_handoff"
    machine = platform.machine()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emitter", type=Path, default=root / "build" /
                        "toolchains" / "clang" / machine / "tools" /
                        "compound_scene_room_visual_proof_emit")
    parser.add_argument("--renderer", type=Path, default=root / "build" /
                        "toolchains" / "clang" / machine / "tools" / "cli" /
                        "ray_tracing_render_headless")
    parser.add_argument("--output-root", type=Path,
                        default=s9e.codework_root(root) /
                        "_private_workspace_artifacts" / "agent_runs" /
                        "ray_tracing" / "compound_scene_s9h3_room")
    args = parser.parse_args()

    packet = fixture / "compound_scene_renderer_handoff_v1.txt"
    room = fixture / "compound_scene_static_room_v1.txt"
    mesh_paths = [fixture / "assets" / "mesh_assets" /
                  "mesh_c2_u_channel.runtime.json",
                  fixture / "assets" / "mesh_assets" /
                  "mesh_c1_l_bracket.runtime.json"]
    source_scene = json.loads((fixture / "source_scene_runtime.json").read_text())
    source_assets = [json.loads(path.read_text()) for path in mesh_paths]
    template = source_scene["objects"][0]
    runs = []
    frame_paths = []
    room_geometry_digest = None
    static_scene_digest = None
    emitter_hashes = []

    for tick in TICKS:
        command = [str(args.emitter), str(packet), str(room),
                   *(str(path) for path in mesh_paths), str(tick)]
        first = subprocess.run(command, check=True, capture_output=True,
                               text=True).stdout
        second = subprocess.run(command, check=True, capture_output=True,
                                text=True).stdout
        if first != second:
            raise RuntimeError(f"tick {tick}: H3 emitter was not deterministic")
        payload = json.loads(first)
        if (payload["schema"] != "ray_compound_scene_s9h3_visual_payload_v1" or
                payload["tick"] != tick or len(payload["planes"]) != 6 or
                sum(plane["visible"] for plane in payload["planes"]) != 5 or
                payload["planes"][5]["visible"]):
            raise RuntimeError(f"tick {tick}: H3 room payload mismatch")
        if room_geometry_digest is None:
            room_geometry_digest = payload["room_geometry_digest"]
        elif room_geometry_digest != payload["room_geometry_digest"]:
            raise RuntimeError("room geometry changed across exact ticks")
        emitter_hashes.append(hashlib.sha256(first.encode()).hexdigest())
        visible_planes = [plane for plane in payload["planes"]
                          if plane["visible"]]
        run_root = args.output_root / "runs" / f"tick_{tick:04d}"
        assets = []
        clearances = {}
        for source_asset, body in zip(source_assets, payload["bodies"]):
            asset = copy.deepcopy(source_asset)
            asset["asset_id"] += f"_h3_tick_{tick:04d}"
            asset["local_bounds"] = {
                "min": dict(zip(("x", "y", "z"), body["bounds_min"])),
                "max": dict(zip(("x", "y", "z"), body["bounds_max"])),
            }
            for vertex, position in zip(asset["mesh"]["vertices"],
                                        body["vertices"]):
                vertex.update(dict(zip(("x", "y", "z"), position)))
            rebuild_normals(asset)
            asset_path = run_root / "assets" / "mesh_assets" / \
                f"{asset['asset_id']}.runtime.json"
            write_json(asset_path, asset)
            assets.append((asset, asset_path))
            body_clearance = {
                plane["object_id"]: plane_clearance(body["vertices"], plane)
                for plane in payload["planes"]}
            if min(body_clearance.values()) < -1e-9:
                raise RuntimeError(
                    f"tick {tick} {body['object_id']}: source mesh crossed room plane")
            clearances[body["object_id"]] = body_clearance

        static_objects = [plane_object(plane) for plane in visible_planes]
        current_static_digest = hashlib.sha256(json.dumps(
            static_objects, sort_keys=True).encode()).hexdigest()
        if static_scene_digest is None:
            static_scene_digest = current_static_digest
        elif static_scene_digest != current_static_digest:
            raise RuntimeError("visible room planes changed across ticks")
        scene = {
            "schema_family": "codework_scene",
            "schema_variant": "scene_runtime_v1", "schema_version": 1,
            "scene_id": f"compound_scene_s9h3_tick_{tick:04d}",
            "source_scene_id": source_scene["scene_id"],
            "compile_meta": {
                "compiler_version": "ray_compound_scene_s9h3_visual_proof_v1",
                "compiled_at_ns": 0,
                "normalization": "H2 basis plus H3 exact room planes"},
            "space_mode_default": "3d", "unit_system": "meters",
            "world_scale": 1.0,
            "objects": static_objects + [
                mesh_object(template, "sim_body_c2", assets[0][0]["asset_id"],
                            "mat_c2_authored"),
                mesh_object(template, "sim_body_c1", assets[1][0]["asset_id"],
                            "mat_c1_authored")],
            "hierarchy": [],
            "materials": [
                {"material_id": "mat_sim_room_floor", "kind": "lambert",
                 "albedo": [0.30, 0.34, 0.40]},
                {"material_id": "mat_sim_room_ceiling", "kind": "lambert",
                 "albedo": [0.20, 0.23, 0.29]},
                {"material_id": "mat_sim_room_x_min", "kind": "lambert",
                 "albedo": [0.18, 0.25, 0.33]},
                {"material_id": "mat_sim_room_x_max", "kind": "lambert",
                 "albedo": [0.24, 0.20, 0.30]},
                {"material_id": "mat_sim_room_z_min", "kind": "lambert",
                 "albedo": [0.15, 0.19, 0.25]},
                {"material_id": "mat_c2_authored", "kind": "lambert",
                 "albedo": [0.90, 0.43, 0.16]},
                {"material_id": "mat_c1_authored", "kind": "lambert",
                 "albedo": [0.18, 0.72, 0.48]}],
            "lights": [{"light_id": "light_key", "kind": "point",
                        "position": {"x": -3.5, "y": -2.5, "z": 8.5},
                        "intensity": 5.0, "radius": 0.18}],
            "cameras": [{"camera_id": "cam_s9h3", "kind": "perspective",
                         "position": {"x": 0.0, "y": -15.0, "z": 6.2},
                         "target": {"x": 0.0, "y": 0.0, "z": 4.8},
                         "yaw": 0.0, "look_pitch": -0.08}],
            "constraints": [],
            "extensions": {"compound_scene_s9h3": {
                "source_tick": tick,
                "room_geometry_digest": payload["room_geometry_digest"],
                "mapped_room_digest": payload["mapped_room_digest"],
                "all_surface_ids": [plane["object_id"]
                                    for plane in payload["planes"]],
                "visible_surface_ids": [plane["object_id"]
                                        for plane in visible_planes],
                "camera_opening_surface_id": "sim_room_z_max",
                "collision_geometry_included": False}},
        }
        scene_path = run_root / "scene_runtime.json"
        write_json(scene_path, scene)
        request = s9e.build_request(scene_path, run_root / "render", tick)
        request["run_id"] = f"compound_scene_s9h3_tick_{tick:04d}"
        request["render"].update({"width": 400, "height": 300})
        request["inspection"].update({
            "camera_position": {"x": 0.0, "y": -15.0, "z": 6.2},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 4.8},
            "ambient_strength": 0.34, "top_fill_strength": 1.35,
            "light_intensity": 5.0, "light_radius": 0.18})
        request_path = run_root / "render_request.json"
        write_json(request_path, request)
        summary_path = Path(request["progress"]["summary_path"])
        subprocess.run([str(args.renderer), "--request", str(request_path),
                        "--render", "--summary", str(summary_path)], check=True,
                       capture_output=True, text=True)
        summary = json.loads(summary_path.read_text())
        audit = {item["object_id"]: item for item in summary["object_audit"]}
        for object_id, triangles in (("sim_body_c2", 28),
                                     ("sim_body_c1", 20)):
            if (audit[object_id]["triangle_count"] != triangles or
                    audit[object_id]["primary_hit_pixels"] <= 0):
                raise RuntimeError(
                    f"tick {tick} {object_id}: body render acceptance failed")
        for plane in visible_planes:
            if audit[plane["object_id"]]["triangle_count"] != 2:
                raise RuntimeError(
                    f"tick {tick} {plane['object_id']}: plane mismatch")
        frame_path = Path(summary["outputs"]["first_frame_path"])
        repeat_request = copy.deepcopy(request)
        repeat_root = run_root / "render_repeat"
        repeat_request["output"]["root"] = str(repeat_root)
        repeat_request["progress"]["summary_path"] = str(
            repeat_root / "render_summary.json")
        repeat_request["progress"]["progress_path"] = str(
            repeat_root / "render_progress.json")
        repeat_request_path = run_root / "render_repeat_request.json"
        write_json(repeat_request_path, repeat_request)
        subprocess.run([str(args.renderer), "--request",
                        str(repeat_request_path), "--render", "--summary",
                        repeat_request["progress"]["summary_path"]], check=True,
                       capture_output=True, text=True)
        repeat_summary = json.loads(Path(
            repeat_request["progress"]["summary_path"]).read_text())
        repeat_frame = Path(repeat_summary["outputs"]["first_frame_path"])
        if digest(frame_path) != digest(repeat_frame):
            raise RuntimeError(f"tick {tick}: render repeat mismatch")
        frame_paths.append(frame_path)
        runs.append({
            "tick": tick, "emitter_sha256": emitter_hashes[-1],
            "scene_path": str(scene_path), "scene_sha256": digest(scene_path),
            "request_path": str(request_path),
            "frame_path": str(frame_path), "frame_sha256": digest(frame_path),
            "render_repeat_equal": True,
            "body_primary_hit_pixels": {
                key: audit[key]["primary_hit_pixels"]
                for key in ("sim_body_c2", "sim_body_c1")},
            "visible_plane_primary_hit_pixels": {
                plane["object_id"]: audit[plane["object_id"]]
                ["primary_hit_pixels"] for plane in visible_planes},
            "source_mesh_plane_clearance_m": clearances,
        })

    if len({run["frame_sha256"] for run in runs}) != len(TICKS):
        raise RuntimeError("four exact H3 render frames are not visually distinct")
    contact_sheet = args.output_root / "review" / \
        "compound_scene_s9h3_room_contact_sheet.png"
    make_contact_sheet(frame_paths, contact_sheet)
    report = {
        "schema": "ray_compound_scene_s9h3_visual_proof_report_v1",
        "status": "local_visual_proof_ready",
        "visual_intent": "show both asymmetric source meshes moving and rotating inside the exact producer-owned collision room",
        "expected_signal": "four distinct views retain the same floor, ceiling, side walls, and back wall while both bodies remain inside all six mapped planes",
        "rejection_condition": "missing body, crossed wall, nonmatching plane, visible camera-facing wall, collision hull art, static-room drift, or nondeterministic repeat",
        "room_geometry_digest": room_geometry_digest,
        "mapped_room_digest": "a37ee71b3810d6ee",
        "static_visible_room_digest": static_scene_digest,
        "surface_count": 6, "visible_surface_count": 5,
        "camera_opening_surface": "sim_room_z_max",
        "exact_ticks": list(TICKS),
        "acceptance": {
            "six_producer_surfaces_matched": True,
            "five_renderer_planes_visible": True,
            "camera_opening_explicit": True,
            "both_source_meshes_visible_each_tick": True,
            "all_source_mesh_vertices_inside_all_six_planes": True,
            "room_static_across_ticks": True,
            "four_frames_distinct": True,
            "emitter_repeat_equal": True,
            "render_repeat_equal": True,
            "collision_proxy_rendered": False,
            "default_request_or_worker_integration": False},
        "ownership": {
            "simulation_transforms_and_collision_surfaces": "Ball Bounce",
            "coordinate_compatibility_and_plane_match": "RayTracing S9-H2/H3",
            "source_mesh_material_camera_light_sampling_final_image": "RayTracing"},
        "runs": runs,
        "contact_sheet_path": str(contact_sheet),
        "contact_sheet_sha256": digest(contact_sheet),
        "promotion_eligible": False, "remote_submit": False,
        "saved_scene_mutated": False, "latest_good_changed": False,
    }
    report_path = args.output_root / "visual_proof_report.json"
    write_json(report_path, report)
    print(report_path)
    print(contact_sheet)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
