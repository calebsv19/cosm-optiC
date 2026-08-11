#!/usr/bin/env python3
"""Render one local-only S9-F two-body typed assembly acceptance frame."""

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


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


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
        if length == 0.0:
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


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    fixture = root / "tests" / "fixtures" / "compound_scene_handoff"
    machine = platform.machine()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emitter", type=Path, default=root / "build" /
                        "toolchains" / "clang" / machine / "tools" /
                        "compound_scene_assembly_visual_proof_emit")
    parser.add_argument("--renderer", type=Path, default=root / "build" /
                        "toolchains" / "clang" / machine / "tools" / "cli" /
                        "ray_tracing_render_headless")
    parser.add_argument("--output-root", type=Path,
                        default=s9e.codework_root(root) /
                        "_private_workspace_artifacts" / "agent_runs" /
                        "ray_tracing" /
                        "compound_scene_s9f_two_body_assembly")
    parser.add_argument("--assembly-archive", type=Path,
                        help="optional S9-G archive whose canonical floor plane is replayed")
    args = parser.parse_args()
    packet = fixture / "compound_scene_renderer_handoff_v1.txt"
    mesh_paths = [fixture / "assets" / "mesh_assets" /
                  "mesh_c2_u_channel.runtime.json",
                  fixture / "assets" / "mesh_assets" /
                  "mesh_c1_l_bracket.runtime.json"]
    command = [str(args.emitter), str(packet), *(str(path) for path in mesh_paths)]
    first = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    second = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    if first != second:
        raise RuntimeError("S9-F assembly emitter was not byte deterministic")
    payload = json.loads(first)
    if (payload["assembly_schema"] != "ray_tracing_compound_scene_assembly_v1" or
            [item["membership"] for item in payload["objects"]] !=
            ["simulated", "simulated", "static", "static", "static"]):
        raise RuntimeError("typed membership payload mismatch")

    run_root = args.output_root / "run_tick_0480"
    source_scene = json.loads((fixture / "source_scene_runtime.json").read_text())
    assets = []
    for path, body in zip(mesh_paths, payload["bodies"]):
        asset = json.loads(path.read_text())
        asset["asset_id"] += "_tick_0480"
        asset["local_bounds"] = {
            "min": dict(zip(("x", "y", "z"), body["bounds_min"])),
            "max": dict(zip(("x", "y", "z"), body["bounds_max"])),
        }
        for vertex, position in zip(asset["mesh"]["vertices"], body["vertices"]):
            vertex.update(dict(zip(("x", "y", "z"), position)))
        rebuild_normals(asset)
        asset_path = run_root / "assets" / "mesh_assets" / f"{asset['asset_id']}.runtime.json"
        write_json(asset_path, asset)
        assets.append((asset, asset_path))

    template = source_scene["objects"][0]
    requested_floor_clearance_m = 0.25
    floor_z = (min(body["bounds_min"][2] for body in payload["bodies"]) -
               requested_floor_clearance_m)
    floor_authority = "renderer_set_dressing"
    if args.assembly_archive:
        floor_line = next((line for line in args.assembly_archive.read_text().splitlines()
                           if line.startswith("static=set_dressing_floor|")), None)
        if floor_line is None:
            raise RuntimeError("S9-G archive has no canonical renderer floor")
        fields = floor_line.removeprefix("static=").split("|")
        if len(fields) != 14 or fields[3] != floor_authority:
            raise RuntimeError("S9-G floor authority/shape mismatch")
        floor_z = float.fromhex(fields[6])
    measured_floor_clearance_m = min(
        body["bounds_min"][2] - floor_z for body in payload["bodies"])
    if measured_floor_clearance_m < requested_floor_clearance_m:
        raise RuntimeError("renderer floor violates source-mesh clearance")
    floor = s9e.primitive("set_dressing_floor", "mat_floor", 10.0, 16.0,
                         [0.0, 4.0, floor_z], [1.0, 0.0, 0.0],
                         [0.0, 1.0, 0.0], [0.0, 0.0, 1.0])
    back = s9e.primitive("set_dressing_backdrop", "mat_backdrop", 10.0, 7.0,
                        [0.0, 7.8, 2.35], [1.0, 0.0, 0.0],
                        [0.0, 0.0, 1.0], [0.0, -1.0, 0.0])
    marker = s9e.primitive("set_dressing_reference_marker", "mat_marker", 1.0, 2.2,
                          [-2.2, 5.55, 0.0], [1.0, 0.0, 0.0],
                          [0.0, 0.0, 1.0], [0.0, -1.0, 0.0])
    scene = {
        "schema_family": "codework_scene", "schema_variant": "scene_runtime_v1",
        "schema_version": 1, "scene_id": "compound_scene_s9f_tick_0480",
        "source_scene_id": source_scene["scene_id"],
        "compile_meta": {"compiler_version": "ray_compound_scene_s9f_visual_proof_v1",
                         "compiled_at_ns": 0,
                         "normalization": "S9-F typed exact assembly"},
        "space_mode_default": "3d", "unit_system": "meters", "world_scale": 1.0,
        "objects": [floor, back, marker,
                    mesh_object(template, "sim_body_c2", assets[0][0]["asset_id"],
                                "mat_c2_authored"),
                    mesh_object(template, "sim_body_c1", assets[1][0]["asset_id"],
                                "mat_c1_authored")],
        "hierarchy": [],
        "materials": [
            {"material_id": "mat_floor", "kind": "lambert", "albedo": [0.22, 0.25, 0.30]},
            {"material_id": "mat_backdrop", "kind": "lambert", "albedo": [0.12, 0.15, 0.20]},
            {"material_id": "mat_marker", "kind": "lambert", "albedo": [0.22, 0.52, 0.82]},
            {"material_id": "mat_c2_authored", "kind": "lambert", "albedo": [0.88, 0.42, 0.16]},
            {"material_id": "mat_c1_authored", "kind": "lambert", "albedo": [0.20, 0.72, 0.48]}],
        "lights": [], "cameras": [], "constraints": [],
        "extensions": {"compound_scene_s9f": {
            "assembly_digest": payload["assembly_digest"], "source_tick": 480,
            "simulated_object_ids": ["sim_body_c2", "sim_body_c1"],
            "static_object_ids": ["set_dressing_floor", "set_dressing_backdrop",
                                  "set_dressing_reference_marker"],
            "collision_geometry_included": False}},
    }
    scene_path = run_root / "scene_runtime.json"
    write_json(scene_path, scene)
    request = s9e.build_request(scene_path, run_root / "render", 480)
    request["run_id"] = "compound_scene_s9f_tick_0480"
    request_path = run_root / "render_request.json"
    write_json(request_path, request)
    summary_path = Path(request["progress"]["summary_path"])
    subprocess.run([str(args.renderer), "--request", str(request_path), "--render",
                    "--summary", str(summary_path)], check=True,
                   capture_output=True, text=True)
    summary = json.loads(summary_path.read_text())
    audit = {item["object_id"]: item for item in summary["object_audit"]}
    for object_id, triangles in (("sim_body_c2", 28), ("sim_body_c1", 20)):
        if (audit[object_id]["triangle_count"] != triangles or
                audit[object_id]["primary_hit_pixels"] <= 0):
            raise RuntimeError(f"{object_id}: two-body render acceptance failed")
    frame_path = Path(summary["outputs"]["first_frame_path"])
    review_path = args.output_root / "review" / "s9f_two_body_tick_0480.png"
    width, height, pixels = s9e.review_artifacts.read_bmp_rgb(frame_path)
    review_path.parent.mkdir(parents=True, exist_ok=True)
    s9e.review_artifacts.write_png_rgb(review_path, width, height, pixels)
    report = {
        "schema": "ray_compound_scene_s9f_visual_proof_report_v1",
        "status": "local_visual_proof_ready", "tick": 480,
        "assembly_digest": payload["assembly_digest"],
        "assembly_emitter_repeat_equal": True,
        "membership": {item["object_id"]: item["membership"]
                       for item in payload["objects"]},
        "source_meshes": [{"object_id": body["object_id"],
                           "source_asset_id": body["source_asset_id"],
                           "triangle_count": body["triangle_count"]}
                          for body in payload["bodies"]],
        "acceptance": {"both_packet_bodies_visible": True,
                       "static_membership_explicit": True,
                       "renderer_floor_authority": floor_authority,
                       "renderer_floor_requested_clearance_m": requested_floor_clearance_m,
                       "renderer_floor_measured_tick_clearance_m": measured_floor_clearance_m,
                       "renderer_floor_z": floor_z,
                       "simulation_collision_surface_count": 0,
                       "collision_proxy_rendered": False,
                       "default_request_or_worker_integration": False},
        "object_audit": {key: {"triangle_count": audit[key]["triangle_count"],
                               "primary_hit_pixels": audit[key]["primary_hit_pixels"]}
                         for key in ("sim_body_c2", "sim_body_c1")},
        "frame_path": str(frame_path), "frame_sha256": digest(frame_path),
        "review_path": str(review_path), "review_sha256": digest(review_path),
        "scene_path": str(scene_path), "request_path": str(request_path),
        "promotion_eligible": False, "remote_submit": False,
        "saved_scene_mutated": False, "latest_good_changed": False,
    }
    report_path = args.output_root / "visual_proof_report.json"
    write_json(report_path, report)
    print(report_path)
    print(review_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
