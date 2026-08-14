#!/usr/bin/env python3
"""Render fresh Ball Bounce body/body and body/wall collision event proofs."""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import subprocess
from pathlib import Path

import run_compound_scene_s9i_runtime_acceptance_proof as s9i


SCENARIOS = ("body_to_body", "body_to_wall")
EVENT_CONTEXT_TICKS = 96


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def make_event_sheet(frames: list[Path], output: Path) -> None:
    if len(frames) != 6:
        raise RuntimeError("collision event sheet requires exactly six frames")
    colors = ((220, 105, 45), (220, 155, 45), (190, 190, 65),
              (65, 145, 215), (120, 85, 205), (185, 80, 155))
    cells = []
    for path, color in zip(frames, colors):
        width, _, pixels = s9i.s9e.review_artifacts.read_bmp_rgb(path)
        cells.append([[color] * width for _ in range(8)] + pixels)
    separator = 8
    width = len(cells[0][0])
    height = len(cells[0])
    rows = []
    for row_index in range(2):
        row_cells = cells[row_index * 3:row_index * 3 + 3]
        for y in range(height):
            row = []
            for cell_index, cell in enumerate(row_cells):
                if cell_index:
                    row.extend([(25, 28, 34)] * separator)
                row.extend(cell[y])
            rows.append(row)
        if row_index == 0:
            rows.extend([[(25, 28, 34)] * len(rows[0])] * separator)
    output.parent.mkdir(parents=True, exist_ok=True)
    s9i.s9e.review_artifacts.write_png_rgb(
        output, len(rows[0]), len(rows), rows)


def validate_evidence(path: Path, scenario: str) -> dict:
    evidence = json.loads(path.read_text(encoding="utf-8"))
    if (evidence.get("schema") !=
            "ball_compound_scene_contact_event_evidence_z_up_v2" or
            evidence.get("schema_version") != 2 or
            evidence.get("coordinate_system") != "right_handed_z_up_meters" or
            evidence.get("scenario_id") != scenario or
            evidence.get("event", {}).get("kind") != scenario):
        raise RuntimeError(f"{scenario}: invalid producer event evidence")
    event = evidence["event"]
    ticks = [event[name] for name in
             ("before_tick", "contact_tick", "last_contact_tick", "after_tick")]
    if ticks != sorted(ticks) or len(set(ticks)) != 4:
        raise RuntimeError(f"{scenario}: event ticks are not strictly ordered")
    if scenario == "body_to_wall" and event["wall_mask"] == 0:
        raise RuntimeError("body_to_wall: producer wall mask is empty")
    if scenario == "body_to_body" and event["wall_mask"] != 0:
        raise RuntimeError("body_to_body: unexpected wall mask")
    penetration = evidence.get("maximum_solver_penetration_m")
    if not isinstance(penetration, (int, float)) or not 0 <= penetration < 0.01:
        raise RuntimeError(f"{scenario}: invalid solver penetration bound")
    return evidence


def render_scenario(args: argparse.Namespace, fixture: Path,
                    producer_root: Path, scenario: str) -> dict:
    source_root = producer_root / scenario
    packet = source_root / "compound_scene_handoff_z_up_v2.txt"
    room = source_root / "compound_scene_static_room_z_up_v2.txt"
    evidence_path = source_root / "compound_scene_contact_event_evidence_z_up_v2.json"
    for path in (packet, room, evidence_path):
        if not path.is_file():
            raise RuntimeError(f"{scenario}: missing fresh producer artifact {path}")
    evidence = validate_evidence(evidence_path, scenario)
    event = evidence["event"]
    event_ticks = [event[name] for name in
                   ("before_tick", "contact_tick", "last_contact_tick", "after_tick")]
    ticks = [max(0, event["contact_tick"] - EVENT_CONTEXT_TICKS),
             event["before_tick"], event["contact_tick"],
             event["last_contact_tick"], event["after_tick"],
             min(evidence["frame_count"] - 1,
                 event["after_tick"] + EVENT_CONTEXT_TICKS)]
    scenario_args = argparse.Namespace(
        emitter=args.emitter, renderer=args.renderer,
        output_root=args.output_root / scenario)
    runs, room_digests, visible_room_digests = [], set(), set()
    for tick in ticks:
        run, room_digest, visible_digest = s9i.render_tick(
            scenario_args, fixture, packet, room, tick,
            evidence["maximum_solver_penetration_m"])
        runs.append(run)
        room_digests.add(room_digest)
        visible_room_digests.add(visible_digest)
    if len(room_digests) != 1 or len(visible_room_digests) != 1:
        raise RuntimeError(f"{scenario}: room geometry drifted across event ticks")
    if len({run["frame_sha256"] for run in runs}) != len(runs):
        raise RuntimeError(f"{scenario}: event frames are not visually distinct")
    contact_sheet = args.output_root / "review" / f"{scenario}_event_sheet.png"
    make_event_sheet([Path(run["frame_path"]) for run in runs], contact_sheet)
    return {
        "scenario_id": scenario,
        "producer_evidence": evidence,
        "producer_artifacts": {
            "handoff_path": str(packet), "handoff_sha256": sha256(packet),
            "room_path": str(room), "room_sha256": sha256(room),
            "event_evidence_path": str(evidence_path),
            "event_evidence_sha256": sha256(evidence_path),
        },
        "producer_event_ticks": event_ticks,
        "render_ticks": ticks,
        "event_context_ticks": EVENT_CONTEXT_TICKS,
        "room_geometry_digest": room_digests.pop(),
        "static_visible_room_digest": visible_room_digests.pop(),
        "runs": runs,
        "contact_sheet_path": str(contact_sheet),
        "contact_sheet_sha256": sha256(contact_sheet),
    }


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
                        "ray_tracing" / "compound_scene_fresh_collision_events")
    args = parser.parse_args()
    args.ball_bounce_root = args.ball_bounce_root.resolve()
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
    scenarios = [render_scenario(args, fixture, producer_root, scenario)
                 for scenario in SCENARIOS]
    report = {
        "schema": "ray_tracing_fresh_compound_collision_event_proof_z_up_v2",
        "coordinate_system": "right_handed_z_up_meters",
        "basis_mapping": "native_z_up_identity_v2",
        "status": "local_visual_proof_ready",
        "visual_intent": "show source-mesh motion immediately around producer-reported body/body and body/wall collision episodes",
        "expected_signal": "both source meshes remain visible and inside the exact producer room while their trajectory changes across before/contact/last-contact/after ticks",
        "rejection_condition": "producer provenance failure, missing body or plane, source geometry outside a physical plane, nondeterministic repeat, or indistinguishable event frames",
        "runtime_path": "normal_local_request.scene.compound_scene_ingestion_path",
        "producer_generated_fresh": not args.skip_producer,
        "base_scene_mutated": False,
        "saved_scene_mutated": False,
        "promotion_eligible": False,
        "remote_submit": False,
        "scenarios": scenarios,
    }
    report_path = args.output_root / "visual_proof_report.json"
    write_json(report_path, report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
