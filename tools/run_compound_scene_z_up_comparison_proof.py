#!/usr/bin/env python3
"""Compare explicit legacy Y-up mapping with authoritative native Z-up ingestion."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import platform
import subprocess
from pathlib import Path

import run_compound_scene_s9i_runtime_acceptance_proof as s9i


SCENARIOS = ("body_to_body", "body_to_wall")


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def payload_for(emitter: Path, fixture: Path, packet: Path, room: Path,
                tick: int) -> dict:
    command = [str(emitter), str(packet), str(room),
               str(fixture / "assets" / "mesh_assets" /
                   "mesh_c2_u_channel.runtime.json"),
               str(fixture / "assets" / "mesh_assets" /
                   "mesh_c1_l_bracket.runtime.json"), str(tick)]
    first = subprocess.run(command, check=True, capture_output=True,
                           text=True).stdout
    second = subprocess.run(command, check=True, capture_output=True,
                            text=True).stdout
    if first != second:
        raise RuntimeError(f"tick {tick}: emitter output is nondeterministic")
    return json.loads(first)


def distance(a: list[float], b: list[float]) -> float:
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def centroid(vertices: list[list[float]]) -> list[float]:
    return [sum(vertex[axis] for vertex in vertices) / len(vertices)
            for axis in range(3)]


def plane_signature(plane: dict) -> tuple:
    origin = plane["origin"]
    axis_u = plane["axis_u"]
    axis_v = plane["axis_v"]
    half_u = plane["width"] * 0.5
    half_v = plane["height"] * 0.5
    corners = []
    for sign_u, sign_v in ((-1, -1), (-1, 1), (1, -1), (1, 1)):
        corners.append(tuple(round(origin[axis] +
                                   sign_u * half_u * axis_u[axis] +
                                   sign_v * half_v * axis_v[axis], 12)
                             for axis in range(3)))
    return (tuple(round(float(value), 12) for value in origin),
            tuple(round(float(value), 12) for value in plane["normal"]),
            tuple(sorted(corners)))


def pixel_delta(first: Path, second: Path) -> dict:
    width_a, height_a, pixels_a = s9i.s9e.review_artifacts.read_bmp_rgb(first)
    width_b, height_b, pixels_b = s9i.s9e.review_artifacts.read_bmp_rgb(second)
    if (width_a, height_a) != (width_b, height_b):
        raise RuntimeError("legacy/native render dimensions differ")
    changed = 0
    channel_delta = 0
    channel_count = width_a * height_a * 3
    for row_a, row_b in zip(pixels_a, pixels_b):
        for rgb_a, rgb_b in zip(row_a, row_b):
            if rgb_a != rgb_b:
                changed += 1
            channel_delta += sum(abs(a - b) for a, b in zip(rgb_a, rgb_b))
    return {
        "changed_pixel_fraction": changed / (width_a * height_a),
        "mean_absolute_channel_delta_normalized":
            channel_delta / (channel_count * 255.0),
    }


def make_comparison_sheet(rows: list[tuple[Path, Path]], output: Path) -> None:
    separator = 8
    output_rows: list[list[tuple[int, int, int]]] = []
    expected_width = 0
    for row_index, (legacy_path, native_path) in enumerate(rows):
        width_l, height_l, legacy = s9i.s9e.review_artifacts.read_bmp_rgb(
            legacy_path)
        width_n, height_n, native = s9i.s9e.review_artifacts.read_bmp_rgb(
            native_path)
        if (width_l, height_l) != (width_n, height_n):
            raise RuntimeError("comparison-sheet dimensions differ")
        expected_width = width_l * 2 + separator
        for y in range(height_l):
            output_rows.append(legacy[y] + [(25, 28, 34)] * separator +
                               native[y])
        if row_index + 1 != len(rows):
            output_rows.extend([[(25, 28, 34)] * expected_width] * separator)
    output.parent.mkdir(parents=True, exist_ok=True)
    s9i.s9e.review_artifacts.write_png_rgb(
        output, expected_width, len(output_rows), output_rows)


def compare_scenario(args: argparse.Namespace, fixture: Path,
                     producer_root: Path, scenario: str) -> tuple[dict, list]:
    source = producer_root / scenario
    legacy_evidence_path = (
        source / "compound_scene_contact_event_evidence_legacy_y_up_v1.json")
    native_evidence_path = (
        source / "compound_scene_contact_event_evidence_z_up_v2.json")
    legacy_packet = source / "compound_scene_renderer_handoff_legacy_y_up_v1.txt"
    legacy_room = source / "compound_scene_static_room_legacy_y_up_v1.txt"
    native_packet = source / "compound_scene_handoff_z_up_v2.txt"
    native_room = source / "compound_scene_static_room_z_up_v2.txt"
    required = (legacy_evidence_path, native_evidence_path, legacy_packet,
                legacy_room, native_packet, native_room)
    if missing := [str(path) for path in required if not path.is_file()]:
        raise RuntimeError(f"{scenario}: missing comparison inputs {missing}")
    legacy_evidence = json.loads(legacy_evidence_path.read_text())
    native_evidence = json.loads(native_evidence_path.read_text())
    if (legacy_evidence.get("coordinate_system") !=
            "right_handed_y_up_meters" or
            native_evidence.get("coordinate_system") !=
            "right_handed_z_up_meters"):
        raise RuntimeError(f"{scenario}: comparison coordinate metadata mismatch")
    ticks = sorted({legacy_evidence["event"]["before_tick"],
                    legacy_evidence["event"]["contact_tick"],
                    native_evidence["event"]["before_tick"],
                    native_evidence["event"]["contact_tick"]})
    comparisons = []
    sheet_rows = []
    for tick in ticks:
        legacy_payload = payload_for(args.emitter, fixture, legacy_packet,
                                     legacy_room, tick)
        native_payload = payload_for(args.emitter, fixture, native_packet,
                                     native_room, tick)
        if (legacy_payload.get("basis_mapping") !=
                "ball_y_up_to_ray_z_up_v1" or
                native_payload.get("basis_mapping") !=
                "native_z_up_identity_v2"):
            raise RuntimeError(f"{scenario} tick {tick}: basis metadata mismatch")
        legacy_planes = sorted(plane_signature(plane)
                               for plane in legacy_payload["planes"])
        native_planes = sorted(plane_signature(plane)
                               for plane in native_payload["planes"])
        if legacy_planes != native_planes:
            raise RuntimeError(f"{scenario} tick {tick}: physical room mismatch")
        body_metrics = {}
        legacy_bodies = {body["object_id"]: body
                         for body in legacy_payload["bodies"]}
        native_bodies = {body["object_id"]: body
                         for body in native_payload["bodies"]}
        for object_id in sorted(native_bodies):
            legacy_vertices = legacy_bodies[object_id]["vertices"]
            native_vertices = native_bodies[object_id]["vertices"]
            if len(legacy_vertices) != len(native_vertices):
                raise RuntimeError(f"{scenario} tick {tick}: vertex count mismatch")
            body_metrics[object_id] = {
                "vertex_count": len(native_vertices),
                "maximum_corresponding_vertex_delta_m": max(
                    distance(a, b) for a, b in
                    zip(legacy_vertices, native_vertices)),
                "centroid_delta_m": distance(centroid(legacy_vertices),
                                             centroid(native_vertices)),
            }
        legacy_args = argparse.Namespace(
            emitter=args.emitter, renderer=args.renderer,
            output_root=args.output_root / scenario / "legacy_y_up_v1")
        native_args = argparse.Namespace(
            emitter=args.emitter, renderer=args.renderer,
            output_root=args.output_root / scenario / "native_z_up_v2")
        legacy_run, _, _ = s9i.render_tick(
            legacy_args, fixture, legacy_packet, legacy_room, tick,
            legacy_evidence["maximum_solver_penetration_m"])
        native_run, _, _ = s9i.render_tick(
            native_args, fixture, native_packet, native_room, tick,
            native_evidence["maximum_solver_penetration_m"])
        legacy_frame = Path(legacy_run["frame_path"])
        native_frame = Path(native_run["frame_path"])
        sheet_rows.append((legacy_frame, native_frame))
        comparisons.append({
            "tick": tick,
            "physical_room_equal": True,
            "body_geometry_delta": body_metrics,
            "legacy_frame_path": str(legacy_frame),
            "native_frame_path": str(native_frame),
            "legacy_frame_sha256": legacy_run["frame_sha256"],
            "native_frame_sha256": native_run["frame_sha256"],
            "pixel_delta": pixel_delta(legacy_frame, native_frame),
            "both_render_repeats_equal": True,
        })
    return ({
        "scenario_id": scenario,
        "comparison_ticks": ticks,
        "legacy_event": legacy_evidence["event"],
        "native_event": native_evidence["event"],
        "ticks": comparisons,
    }, sheet_rows)


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    codework = root.parent
    machine = platform.machine()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ball-bounce-root", type=Path,
                        default=codework / "ball_bounce_sim")
    parser.add_argument("--producer-build-dir",
                        default="build/contracts/ray_tracing_collision_event_export")
    parser.add_argument("--skip-producer", action="store_true")
    parser.add_argument("--emitter", type=Path, default=root / "build" /
                        "toolchains" / "clang" / machine / "tools" /
                        "compound_scene_room_visual_proof_emit")
    parser.add_argument("--renderer", type=Path, default=root / "build" /
                        "toolchains" / "clang" / machine / "tools" / "cli" /
                        "ray_tracing_render_headless")
    parser.add_argument("--output-root", type=Path, default=codework /
                        "_private_workspace_artifacts" / "agent_runs" /
                        "ray_tracing" / "compound_scene_z_up_comparison")
    args = parser.parse_args()
    if not args.skip_producer:
        subprocess.run([
            "make", "-C", str(args.ball_bounce_root),
            f"CONTRACT_BUILD_DIR={args.producer_build_dir}",
            "rigid3d-compound-scene-contact-event-evidence-contract",
        ], check=True)
    build_dir = Path(args.producer_build_dir)
    if not build_dir.is_absolute():
        build_dir = args.ball_bounce_root / build_dir
    producer_root = build_dir / "p43" / "contact_event_evidence"
    fixture = root / "tests" / "fixtures" / "compound_scene_handoff"
    scenarios = []
    sheet_rows = []
    for scenario in SCENARIOS:
        result, rows = compare_scenario(args, fixture, producer_root, scenario)
        scenarios.append(result)
        sheet_rows.extend(rows)
    sheet = args.output_root / "review" / "legacy_vs_native_collision_ticks.png"
    make_comparison_sheet(sheet_rows, sheet)
    report = {
        "schema": "ray_tracing_compound_scene_z_up_comparison_v1",
        "status": "local_comparison_proof_ready",
        "authoritative_path": "native_z_up_identity_v2",
        "compatibility_reference": "legacy_y_up_v1",
        "comparison_policy":
            "legacy is a compatibility reference, not the native acceptance oracle",
        "expected_room_relationship": "same physical six-plane room",
        "expected_geometry_relationship":
            "differences are recorded because native source-frame binding removes the legacy post-bake basis workaround",
        "base_scene_mutated": False,
        "saved_scene_mutated": False,
        "scenarios": scenarios,
        "comparison_sheet_path": str(sheet),
        "comparison_sheet_sha256": hashlib.sha256(sheet.read_bytes()).hexdigest(),
    }
    report_path = args.output_root / "comparison_report.json"
    write_json(report_path, report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
