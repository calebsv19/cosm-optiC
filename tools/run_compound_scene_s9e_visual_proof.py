#!/usr/bin/env python3
"""Build the local-only S9-E exact-packet source-mesh visual proof."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import platform
import subprocess
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
INTEGRATION_DIR = SCRIPT_DIR.parent / "tests" / "integration"
if str(INTEGRATION_DIR) not in sys.path:
    sys.path.insert(0, str(INTEGRATION_DIR))
import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402


TICKS = (0, 240, 480, 720)
PACKET_SHA256 = "dc15b7376ab82fad5a45d33096e24c905c012a99e49b3fd8acc2fc3526b5ef6d"
SOURCE_RUNTIME_SHA256 = "1cfde852d4d594303aa692e35122bedb52ebccd5d1dc79f448e16fa0f287b0e8"


def repo_root() -> Path:
    return SCRIPT_DIR.parent


def codework_root(root: Path) -> Path:
    if root.parent.name == "_worktrees":
        return root.parent.parent
    return root.parent


def default_binary(root: Path, name: str) -> Path:
    return root / "build" / "toolchains" / "clang" / platform.machine() / name


def parse_args() -> argparse.Namespace:
    root = repo_root()
    fixture = root / "tests" / "fixtures" / "compound_scene_handoff"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packet", type=Path,
                        default=fixture / "compound_scene_renderer_handoff_v1.txt")
    parser.add_argument("--source-scene", type=Path,
                        default=fixture / "source_scene_runtime.json")
    parser.add_argument("--emitter", type=Path,
                        default=default_binary(root, "tools/compound_scene_visual_proof_emit"))
    parser.add_argument("--renderer", type=Path,
                        default=default_binary(root, "tools/cli/ray_tracing_render_headless"))
    parser.add_argument(
        "--output-root", type=Path,
        default=codework_root(root) / "_private_workspace_artifacts" /
        "agent_runs" / "ray_tracing" / "compound_scene_s9e_visual_proof")
    parser.add_argument("--prepare-only", action="store_true")
    return parser.parse_args()


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def emit_exact_frames(args: argparse.Namespace, source_mesh: Path) -> tuple[dict, str]:
    command = [str(args.emitter), str(args.packet), str(source_mesh)]
    first = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    second = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    if first != second:
        raise RuntimeError("exact transform emitter was not byte deterministic")
    payload = json.loads(first)
    if payload["schema"] != "ray_compound_scene_s9e_visual_proof_frames_v1":
        raise RuntimeError("unexpected exact transform payload schema")
    if tuple(frame["tick"] for frame in payload["frames"]) != TICKS:
        raise RuntimeError("exact transform payload tick set mismatch")
    if payload["source_asset_id"] != "c2_u_channel_v1":
        raise RuntimeError("exact transform source identity mismatch")
    if payload["source_sha256"] != (
        "0da0eacd10e7197c77923c29bcb6fcb0325616e9c21f7f18e2523a7440057aac"
    ):
        raise RuntimeError("exact transform source SHA mismatch")
    return payload, hashlib.sha256(first.encode("utf-8")).hexdigest()


def primitive(object_id: str, material_id: str, width: float, height: float,
              origin: list[float], axis_u: list[float], axis_v: list[float],
              normal: list[float]) -> dict:
    vec = lambda value: {"x": value[0], "y": value[1], "z": value[2]}
    return {
        "object_id": object_id,
        "object_type": "plane_primitive",
        "dimensional_mode": "plane_locked",
        "locked_plane": "xy",
        "transform": {
            "position": vec(origin),
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "geometry_ref": {"kind": "shape_asset", "id": f"shape_{object_id}"},
        "material_ref": {"id": material_id},
        "flags": {"visible": True, "locked": True, "selectable": False},
        "primitive": {
            "kind": "plane_primitive", "width": width, "height": height,
            "frame": {"origin": vec(origin), "axis_u": vec(axis_u),
                      "axis_v": vec(axis_v), "normal": vec(normal)},
            "lock_to_construction_plane": False, "lock_to_bounds": False,
        },
    }


def build_scene(source_scene: dict, frame: dict, asset_id: str) -> dict:
    mesh_object = copy.deepcopy(source_scene["objects"][0])
    mesh_object["geometry_ref"]["id"] = asset_id
    floor = primitive("set_dressing_floor", "mat_floor", 10.0, 16.0,
                      [0.0, 4.0, -1.15], [1.0, 0.0, 0.0],
                      [0.0, 1.0, 0.0], [0.0, 0.0, 1.0])
    back = primitive("set_dressing_backdrop", "mat_backdrop", 10.0, 7.0,
                     [0.0, 7.8, 2.35], [1.0, 0.0, 0.0],
                     [0.0, 0.0, 1.0], [0.0, -1.0, 0.0])
    marker = primitive("set_dressing_reference_marker", "mat_marker", 1.0, 2.2,
                       [-2.2, 5.55, 0.0], [1.0, 0.0, 0.0],
                       [0.0, 0.0, 1.0], [0.0, -1.0, 0.0])
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"compound_scene_s9e_tick_{frame['tick']:04d}",
        "source_scene_id": source_scene["scene_id"],
        "compile_meta": {
            "compiler_version": "ray_compound_scene_s9e_visual_proof_v1",
            "compiled_at_ns": 0,
            "normalization": "S9-D exact detached world vertices",
        },
        "space_mode_default": "3d", "unit_system": "meters", "world_scale": 1.0,
        "objects": [floor, back, marker, mesh_object],
        "hierarchy": [],
        "materials": [
            {"material_id": "mat_floor", "kind": "lambert",
             "albedo": [0.22, 0.25, 0.30]},
            {"material_id": "mat_backdrop", "kind": "lambert",
             "albedo": [0.12, 0.15, 0.20]},
            {"material_id": "mat_marker", "kind": "lambert",
             "albedo": [0.22, 0.52, 0.82]},
            {"material_id": "mat_c2_authored", "kind": "lambert",
             "albedo": [0.88, 0.42, 0.16]},
        ],
        "lights": [{
            "light_id": "light_key", "kind": "point",
            "position": {"x": -3.8, "y": 1.8, "z": 7.5},
            "intensity": 4.5, "radius": 0.15,
        }],
        "cameras": [{
            "camera_id": "cam_s9e", "kind": "perspective",
            "position": {"x": 0.0, "y": -9.0, "z": 5.5},
            "target": {"x": 0.4, "y": 5.45, "z": 0.65},
            "yaw": 0.0, "look_pitch": -0.1,
        }],
        "constraints": [],
        "extensions": {"compound_scene_s9e": {
            "source_tick": frame["tick"],
            "dynamic_object_ids": ["sim_body_c2"],
            "static_object_ids": ["set_dressing_floor", "set_dressing_backdrop",
                                  "set_dressing_reference_marker"],
            "collision_geometry_included": False,
        }},
    }


def build_request(scene_path: Path, run_root: Path, tick: int) -> dict:
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": f"compound_scene_s9e_tick_{tick:04d}",
        "scene": {"runtime_scene_path": str(scene_path)},
        "volume": {"enabled": False},
        "render": {"start_frame": 0, "frame_count": 1, "width": 320,
                   "height": 240, "normalized_t": 0.0, "temporal_frames": 1,
                   "integrator_3d": "direct_light"},
        "inspection": {
            "trace_route": "tlas_blas", "object_audit_enabled": True,
            "object_audit_max_dimension": 160,
            "camera_position": {"x": 0.0, "y": -9.0, "z": 5.5},
            "camera_look_at": {"x": 0.4, "y": 5.45, "z": 0.65},
            "camera_zoom": 1.0, "environment_light_mode": "ambient",
            "ambient_strength": 0.32, "top_fill_strength": 1.25,
            "light_intensity": 4.5, "light_radius": 0.15,
        },
        "output": {"root": str(run_root), "overwrite": True},
        "progress": {"summary_path": str(run_root / "render_summary.json"),
                     "progress_path": str(run_root / "render_progress.json")},
    }


def make_contact_sheet(frames: list[Path], output: Path) -> None:
    cells = []
    colors = ((222, 104, 46), (222, 154, 46), (75, 145, 225), (117, 90, 210))
    for path, color in zip(frames, colors):
        width, height, pixels = review_artifacts.read_bmp_rgb(path)
        bar = [[color] * width for _ in range(8)]
        cells.append(bar + pixels)
    separator = 8
    width = len(cells[0][0])
    height = len(cells[0])
    sep_row = [(25, 28, 34)] * (width * 2 + separator)
    rows = []
    for row_index in range(2):
        left, right = cells[row_index * 2:row_index * 2 + 2]
        for y in range(height):
            rows.append(left[y] + [(25, 28, 34)] * separator + right[y])
        if row_index == 0:
            rows.extend([sep_row] * separator)
    output.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(output, len(rows[0]), len(rows), rows)


def main() -> int:
    args = parse_args()
    root = repo_root()
    source_mesh = (args.source_scene.parent / "assets" / "mesh_assets" /
                   "mesh_c2_u_channel.runtime.json")
    if digest(args.packet) != PACKET_SHA256:
        raise RuntimeError("frozen handoff packet SHA mismatch")
    if digest(source_mesh) != SOURCE_RUNTIME_SHA256:
        raise RuntimeError("frozen source runtime mesh SHA mismatch")
    source_scene = json.loads(args.source_scene.read_text(encoding="utf-8"))
    source_asset = json.loads(source_mesh.read_text(encoding="utf-8"))
    exact, emitter_sha = emit_exact_frames(args, source_mesh)
    args.output_root.mkdir(parents=True, exist_ok=True)
    exact_path = args.output_root / "exact_frames.json"
    write_json(exact_path, exact)

    runs = []
    frame_paths = []
    static_digest = None
    for frame in exact["frames"]:
        tick = frame["tick"]
        frame_root = args.output_root / "runs" / f"tick_{tick:04d}"
        asset_id = f"mesh_c2_u_channel_tick_{tick:04d}"
        asset = copy.deepcopy(source_asset)
        asset["asset_id"] = asset_id
        asset["local_bounds"] = {
            "min": dict(zip(("x", "y", "z"), frame["bounds_min"])),
            "max": dict(zip(("x", "y", "z"), frame["bounds_max"])),
        }
        for vertex, position in zip(asset["mesh"]["vertices"],
                                    frame["vertices"]):
            vertex.update(dict(zip(("x", "y", "z"), position)))
        for target, normal in zip(asset["mesh"]["normals"], frame["normals"]):
            target.update(dict(zip(("x", "y", "z"), normal)))
        asset_path = frame_root / "assets" / "mesh_assets" / f"{asset_id}.runtime.json"
        write_json(asset_path, asset)
        scene = build_scene(source_scene, frame, asset_id)
        scene_path = frame_root / "scene_runtime.json"
        write_json(scene_path, scene)
        request = build_request(scene_path, frame_root / "render", tick)
        request_path = frame_root / "render_request.json"
        write_json(request_path, request)
        current_static = hashlib.sha256(json.dumps(
            scene["objects"][:3], sort_keys=True).encode("utf-8")).hexdigest()
        if static_digest is None:
            static_digest = current_static
        elif current_static != static_digest:
            raise RuntimeError("renderer-owned static scene objects changed across ticks")
        run = {"tick": tick, "center": frame["center"],
               "asset_path": str(asset_path), "asset_sha256": digest(asset_path),
               "scene_path": str(scene_path), "scene_sha256": digest(scene_path),
               "request_path": str(request_path), "request_sha256": digest(request_path)}
        if not args.prepare_only:
            summary_path = Path(request["progress"]["summary_path"])
            stdout_path = request_path.parent / "renderer_stdout.json"
            completed = subprocess.run(
                [str(args.renderer), "--request", str(request_path), "--render",
                 "--summary", str(summary_path)], check=True, capture_output=True,
                text=True)
            stdout_path.write_text(completed.stdout, encoding="utf-8")
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            frame_path = Path(summary["outputs"]["first_frame_path"])
            audit = {item["object_id"]: item for item in summary["object_audit"]}
            mesh_audit = audit["sim_body_c2"]
            if (summary["diagnostics"] != "ok" or summary["frames_rendered"] != 1 or
                    mesh_audit["triangle_count"] != 28 or
                    mesh_audit["primary_hit_pixels"] <= 0):
                raise RuntimeError(f"tick {tick}: renderer acceptance failed")
            run.update({"summary_path": str(summary_path),
                        "frame_path": str(frame_path),
                        "frame_sha256": digest(frame_path),
                        "primary_hit_pixels": mesh_audit["primary_hit_pixels"],
                        "trace_route": summary["prepared_acceleration"]["active_trace_route"]})
            repeat_request = copy.deepcopy(request)
            repeat_root = frame_root / "render_repeat"
            repeat_request["output"]["root"] = str(repeat_root)
            repeat_request["progress"]["summary_path"] = str(
                repeat_root / "render_summary.json")
            repeat_request["progress"]["progress_path"] = str(
                repeat_root / "render_progress.json")
            repeat_request_path = frame_root / "render_repeat_request.json"
            write_json(repeat_request_path, repeat_request)
            subprocess.run(
                [str(args.renderer), "--request", str(repeat_request_path),
                 "--render", "--summary",
                 repeat_request["progress"]["summary_path"]],
                check=True, capture_output=True, text=True)
            repeat_summary = json.loads(Path(
                repeat_request["progress"]["summary_path"]).read_text(
                    encoding="utf-8"))
            repeat_frame = Path(repeat_summary["outputs"]["first_frame_path"])
            repeat_sha = digest(repeat_frame)
            if repeat_sha != run["frame_sha256"]:
                raise RuntimeError(f"tick {tick}: rendered frame was not deterministic")
            run.update({"render_repeat_frame_sha256": repeat_sha,
                        "render_repeat_equal": True})
            frame_paths.append(frame_path)
        runs.append(run)

    unique_assets = len({run["asset_sha256"] for run in runs}) == len(TICKS)
    unique_frames = args.prepare_only or (
        len({run["frame_sha256"] for run in runs}) == len(TICKS))
    contact_sheet = args.output_root / "review" / "compound_scene_s9e_contact_sheet.png"
    if not args.prepare_only:
        make_contact_sheet(frame_paths, contact_sheet)
    report = {
        "schema": "ray_compound_scene_s9e_visual_proof_report_v1",
        "status": "prepared_only" if args.prepare_only else "local_visual_proof_ready",
        "visual_intent": "show the asymmetric authored C2 source mesh translating and rotating at four exact packet ticks",
        "expected_signal": "the orange U-channel changes orientation and position while blue and gray set dressing remains fixed",
        "rejection_condition": "blank mesh, symmetric glyph, collision hull art, unchanged four panels, changed static scene, or zero primary-hit pixels",
        "packet_sha256": PACKET_SHA256,
        "source_runtime_mesh_sha256": SOURCE_RUNTIME_SHA256,
        "source_asset_sha256": exact["source_sha256"],
        "exact_emitter_output_sha256": emitter_sha,
        "exact_emitter_repeat_equal": True,
        "render_repeat_equal": args.prepare_only or all(
            run.get("render_repeat_equal", False) for run in runs),
        "static_scene_digest": static_digest,
        "unique_transformed_asset_count": len({run["asset_sha256"] for run in runs}),
        "unique_rendered_frame_count": 0 if args.prepare_only else len({run["frame_sha256"] for run in runs}),
        "acceptance": {"four_exact_ticks": len(runs) == 4,
                       "transforms_visibly_distinct_candidate": unique_assets and unique_frames,
                       "mesh_topology_preserved": exact["triangle_count"] == 28,
                       "collision_proxy_rendered": False,
                       "static_scene_unchanged": True},
        "ownership": {
            "transforms": "frozen Ball Bounce packet via RayTracing S9-C and S9-D",
            "source_mesh_material_static_scene_camera_light_sampling_final_image": "RayTracing",
            "collision_hulls": "not loaded or rendered",
        },
        "promotion_eligible": False, "remote_submit": False,
        "saved_scene_mutated": False, "latest_good_changed": False,
        "contact_sheet_path": "" if args.prepare_only else str(contact_sheet),
        "panel_order": [{"panel": index, "tick": tick}
                        for index, tick in enumerate(TICKS)],
        "runs": runs,
    }
    report_path = args.output_root / "visual_proof_report.json"
    write_json(report_path, report)
    print(report_path)
    if not args.prepare_only:
        print(contact_sheet)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
