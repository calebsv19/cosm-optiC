#!/usr/bin/env python3
"""Render local-only S9-I runtime-ingestion acceptance images.

This deliberately leaves source mesh assets untouched.  The ordinary local
render request reads a typed ingestion descriptor and installs only its
digest-valid derived runtime result for the render.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import platform
import subprocess
from pathlib import Path

import run_compound_scene_s9e_visual_proof as s9e


HERO_TICK = 480
MATRIX_TICKS = (0, 240, 480, 720)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def plane_object(plane: dict) -> dict:
    return s9e.primitive(plane["object_id"], plane["material_id"],
                         plane["width"], plane["height"], plane["origin"],
                         plane["axis_u"], plane["axis_v"], plane["normal"])


def plane_clearance(vertices: list[list[float]], plane: dict) -> float:
    return min(sum((vertex[index] - plane["origin"][index]) *
                   plane["normal"][index] for index in range(3))
               for vertex in vertices)


def make_contact_sheet(frames: list[Path], output: Path) -> None:
    cells = []
    colors = ((220, 105, 45), (220, 155, 45),
              (65, 145, 215), (120, 85, 205))
    for path, color in zip(frames, colors):
        width, _, pixels = s9e.review_artifacts.read_bmp_rgb(path)
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


def source_scene_for(payload: dict, fixture: Path, tick: int) -> dict:
    visible_planes = [plane for plane in payload["planes"] if plane["visible"]]
    return {
        "schema_family": "codework_scene", "schema_variant": "scene_runtime_v1",
        "schema_version": 1, "scene_id": f"compound_scene_s9i_tick_{tick:04d}",
        "unit_system": "meters", "world_scale": 1.0, "space_mode_default": "3d",
        "objects": visible_planes and [plane_object(plane) for plane in visible_planes] + [
            {"object_id": "sim_body_c2", "object_type": "mesh_asset_instance",
             "transform": {"position": {"x": 0, "y": 0, "z": 0},
                           "rotation": {"x": 0, "y": 0, "z": 0},
                           "scale": {"x": 1, "y": 1, "z": 1}},
             "geometry_ref": {"kind": "mesh_asset", "id": "mesh_c2_u_channel"},
             "material_ref": {"id": "mat_c2_authored"},
             "extensions": {"line_drawing": {"runtime_mesh_path": str(
                 fixture / "assets" / "mesh_assets" / "mesh_c2_u_channel.runtime.json")}}},
            {"object_id": "sim_body_c1", "object_type": "mesh_asset_instance",
             "transform": {"position": {"x": 0, "y": 0, "z": 0},
                           "rotation": {"x": 0, "y": 0, "z": 0},
                           "scale": {"x": 1, "y": 1, "z": 1}},
             "geometry_ref": {"kind": "mesh_asset", "id": "mesh_c1_l_bracket"},
             "material_ref": {"id": "mat_c1_authored"},
             "extensions": {"line_drawing": {"runtime_mesh_path": str(
                 fixture / "assets" / "mesh_assets" / "mesh_c1_l_bracket.runtime.json")}}}],
        "assets": {"mesh_assets": [
            {"asset_id": "mesh_c2_u_channel", "path": str(fixture / "assets" / "mesh_assets" / "mesh_c2_u_channel.runtime.json")},
            {"asset_id": "mesh_c1_l_bracket", "path": str(fixture / "assets" / "mesh_assets" / "mesh_c1_l_bracket.runtime.json")}]},
        "hierarchy": [],
        "materials": [
            {"material_id": "mat_sim_room_floor", "kind": "lambert", "albedo": [0.30, 0.34, 0.40]},
            {"material_id": "mat_sim_room_ceiling", "kind": "lambert", "albedo": [0.20, 0.23, 0.29]},
            {"material_id": "mat_sim_room_x_min", "kind": "lambert", "albedo": [0.18, 0.25, 0.33]},
            {"material_id": "mat_sim_room_x_max", "kind": "lambert", "albedo": [0.24, 0.20, 0.30]},
            {"material_id": "mat_sim_room_z_min", "kind": "lambert", "albedo": [0.15, 0.19, 0.25]},
            {"material_id": "mat_c2_authored", "kind": "lambert", "albedo": [0.90, 0.43, 0.16]},
            {"material_id": "mat_c1_authored", "kind": "lambert", "albedo": [0.18, 0.72, 0.48]}],
        "lights": [{"light_id": "light_key", "kind": "point",
                    "position": {"x": -3.5, "y": -2.5, "z": 8.5},
                    "intensity": 5.0, "radius": 0.18}],
        "cameras": [{"camera_id": "cam_s9i", "kind": "perspective",
                     "position": {"x": 0.0, "y": -15.0, "z": 6.2},
                     "target": {"x": 0.0, "y": 0.0, "z": 4.8},
                     "yaw": 0.0, "look_pitch": -0.08}],
        "constraints": [],
        "extensions": {"compound_scene_s9i": {
            "source_tick": tick, "room_geometry_digest": payload["room_geometry_digest"],
            "camera_opening_surface_id": "sim_room_z_max",
            "collision_geometry_included": False}},
    }


def descriptor_for(packet: Path, room: Path, payload: dict, tick: int) -> dict:
    return {
        "schema": "ray_tracing_compound_scene_ingestion_v1",
        "handoff_path": str(packet), "room_path": str(room), "tick": tick,
        "bodies": [
            {"body_id": 4101, "object_id": "sim_body_c2", "mesh_asset_id": "mesh_c2_u_channel"},
            {"body_id": 4102, "object_id": "sim_body_c1", "mesh_asset_id": "mesh_c1_l_bracket"}],
        "room": [{"object_id": plane["object_id"], "material_id": plane["material_id"]}
                 if plane["visible"] else {"material_id": plane["material_id"]}
                 for plane in payload["planes"]],
    }


def render_tick(args: argparse.Namespace, fixture: Path, packet: Path, room: Path,
                tick: int) -> tuple[dict, str, str]:
    command = [str(args.emitter), str(packet), str(room),
               str(fixture / "assets" / "mesh_assets" / "mesh_c2_u_channel.runtime.json"),
               str(fixture / "assets" / "mesh_assets" / "mesh_c1_l_bracket.runtime.json"), str(tick)]
    first = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    second = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    if first != second:
        raise RuntimeError(f"tick {tick}: exact producer emitter is nondeterministic")
    payload = json.loads(first)
    if (payload["schema"] != "ray_compound_scene_s9h3_visual_payload_v1" or
            len(payload["planes"]) != 6 or sum(p["visible"] for p in payload["planes"]) != 5 or
            payload["planes"][5]["visible"]):
        raise RuntimeError(f"tick {tick}: six-plane room presentation mismatch")
    clearances = {body["object_id"]: min(plane_clearance(body["vertices"], plane)
                                          for plane in payload["planes"])
                  for body in payload["bodies"]}
    if min(clearances.values()) < -1e-9:
        raise RuntimeError(f"tick {tick}: producer body crossed a collision plane")
    run_root = args.output_root / "runs" / f"tick_{tick:04d}"
    scene_path = run_root / "scene_runtime.json"
    descriptor_path = run_root / "compound_scene_ingestion.json"
    write_json(scene_path, source_scene_for(payload, fixture, tick))
    write_json(descriptor_path, descriptor_for(packet, room, payload, tick))
    request = s9e.build_request(scene_path, run_root / "render", tick)
    request["run_id"] = f"compound_scene_s9i_tick_{tick:04d}"
    request["scene"]["compound_scene_ingestion_path"] = str(descriptor_path)
    request["render"].update({"width": 400, "height": 300})
    request["inspection"].update({"camera_position": {"x": 0.0, "y": -15.0, "z": 6.2},
                                  "camera_look_at": {"x": 0.0, "y": 0.0, "z": 4.8},
                                  "ambient_strength": 0.34, "top_fill_strength": 1.35,
                                  "light_intensity": 5.0, "light_radius": 0.18})
    request_path = run_root / "render_request.json"
    write_json(request_path, request)
    summary_path = Path(request["progress"]["summary_path"])
    subprocess.run([str(args.renderer), "--request", str(request_path), "--render",
                    "--summary", str(summary_path)], check=True, capture_output=True, text=True)
    summary = json.loads(summary_path.read_text())
    audit = {item["object_id"]: item for item in summary["object_audit"]}
    for object_id, triangles in (("sim_body_c2", 28), ("sim_body_c1", 20)):
        if audit[object_id]["triangle_count"] != triangles or audit[object_id]["primary_hit_pixels"] <= 0:
            raise RuntimeError(f"tick {tick}: {object_id} failed rendered-body acceptance")
    for plane in (plane for plane in payload["planes"] if plane["visible"]):
        if audit[plane["object_id"]]["triangle_count"] != 2:
            raise RuntimeError(f"tick {tick}: {plane['object_id']} plane mismatch")
    frame_path = Path(summary["outputs"]["first_frame_path"])
    repeat = copy.deepcopy(request)
    repeat_root = run_root / "render_repeat"
    repeat["output"]["root"] = str(repeat_root)
    repeat["progress"] = {"summary_path": str(repeat_root / "render_summary.json"),
                          "progress_path": str(repeat_root / "render_progress.json")}
    repeat_path = run_root / "render_repeat_request.json"
    write_json(repeat_path, repeat)
    subprocess.run([str(args.renderer), "--request", str(repeat_path), "--render",
                    "--summary", repeat["progress"]["summary_path"]], check=True,
                   capture_output=True, text=True)
    repeat_summary = json.loads(Path(repeat["progress"]["summary_path"]).read_text())
    if digest(frame_path) != digest(Path(repeat_summary["outputs"]["first_frame_path"])):
        raise RuntimeError(f"tick {tick}: render repeat mismatch")
    return {"tick": tick, "frame_path": str(frame_path), "frame_sha256": digest(frame_path),
            "descriptor_path": str(descriptor_path), "scene_path": str(scene_path),
            "emitter_sha256": hashlib.sha256(first.encode()).hexdigest(),
            "minimum_clearance_m": clearances, "render_repeat_equal": True,
            "body_primary_hit_pixels": {key: audit[key]["primary_hit_pixels"]
                                        for key in ("sim_body_c2", "sim_body_c1")}}, payload["room_geometry_digest"], hashlib.sha256(json.dumps(
                [plane_object(p) for p in payload["planes"] if p["visible"]], sort_keys=True).encode()).hexdigest()


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    fixture = root / "tests" / "fixtures" / "compound_scene_handoff"
    machine = platform.machine()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", action="store_true", help="render ticks 0/240/480/720")
    parser.add_argument("--emitter", type=Path, default=root / "build" / "toolchains" / "clang" / machine / "tools" / "compound_scene_room_visual_proof_emit")
    parser.add_argument("--renderer", type=Path, default=root / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless")
    parser.add_argument("--output-root", type=Path, default=s9e.codework_root(root) / "_private_workspace_artifacts" / "agent_runs" / "ray_tracing" / "compound_scene_s9i_runtime_acceptance")
    args = parser.parse_args()
    ticks = MATRIX_TICKS if args.matrix else (HERO_TICK,)
    packet = fixture / "compound_scene_renderer_handoff_v1.txt"
    room = fixture / "compound_scene_static_room_v1.txt"
    runs, room_digests, static_digests = [], set(), set()
    for tick in ticks:
        run, room_digest, static_digest = render_tick(args, fixture, packet, room, tick)
        runs.append(run); room_digests.add(room_digest); static_digests.add(static_digest)
    if len(room_digests) != 1 or len(static_digests) != 1:
        raise RuntimeError("room geometry drifted across exact ticks")
    if args.matrix and len({run["frame_sha256"] for run in runs}) != len(runs):
        raise RuntimeError("four I2 runtime frames are not distinct")
    report = {"schema": "ray_tracing_compound_scene_s9i_runtime_acceptance_v1",
              "status": "local_visual_proof_ready", "visual_intent": "show two asymmetric source meshes resolved through the normal local ingestion request inside the producer-owned room",
              "expected_signal": "both colored bodies render inside a static five-plane room with the sixth plane held as the explicit camera opening",
              "rejection_condition": "missing body, room-plane mismatch, collision proxy art, static-room drift, nondeterministic render, or indistinct exact ticks",
              "runtime_mode": "request_local_derived_scene_only", "base_scene_mutated": False,
              "saved_scene_mutated": False, "promotion_eligible": False, "remote_submit": False,
              "exact_ticks": list(ticks), "room_geometry_digest": room_digests.pop(),
              "static_visible_room_digest": static_digests.pop(), "runs": runs}
    if args.matrix:
        contact = args.output_root / "review" / "compound_scene_s9i_runtime_contact_sheet.png"
        make_contact_sheet([Path(run["frame_path"]) for run in runs], contact)
        report["contact_sheet_path"] = str(contact); report["contact_sheet_sha256"] = digest(contact)
    write_json(args.output_root / "visual_proof_report.json", report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
