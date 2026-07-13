#!/usr/bin/env python3
"""Render a low-quality aquarium preview with Benchy lifted above the water."""

from __future__ import annotations

import argparse
import json
import os
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
import run_ray_tracing_aquarium_transparent_receiver_fixture as fixture  # noqa: E402


RUN_ID = "aquarium_benchy_above_water_low_preview"
BENCHY_OBJECT_ID = "benchy_floating_inside_aquarium"


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def workspace_root() -> Path:
    return repo_root().parent


def aquarium_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "physics_trio"
        / "aquarium_glass_room_v1"
    )


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = (
        root
        / "build"
        / "toolchains"
        / "clang"
        / machine
        / "tools"
        / "cli"
        / "ray_tracing_render_headless"
    )
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument(
        "--source-scene",
        type=Path,
        default=aquarium_root() / "line_drawing_quality_v6_object_fit" / "scene_runtime.json",
    )
    parser.add_argument(
        "--source-request",
        type=Path,
        default=(
            aquarium_root()
            / "ray_tracing_quality_v7_disney_water_1f"
            / "request_disney_v2_960x540_water_1f.json"
        ),
    )
    parser.add_argument(
        "--water-root",
        type=Path,
        default=(
            aquarium_root()
            / "ray_tracing_quality_v7_disney_water_1f"
            / "water_cache"
            / "Water Basin"
        ),
    )
    parser.add_argument(
        "--review-root",
        type=Path,
        default=(
            workspace_root()
            / "_private_workspace_artifacts"
            / "agent_runs"
            / "ray_tracing"
            / "aquarium_benchy_above_water_low_preview"
        ),
    )
    parser.add_argument("--benchy-z", type=float, default=1.34)
    parser.add_argument("--width", type=int, default=480)
    parser.add_argument("--height", type=int, default=270)
    parser.add_argument("--integrator", default="disney_v2", choices=("disney_v2", "direct_light"))
    parser.add_argument("--trace-route", default="tlas_blas", choices=("tlas_blas", "flattened_bvh"))
    parser.add_argument("--skip-render", action="store_true")
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def lift_benchy(scene: dict, benchy_z: float) -> dict:
    changed = False
    for obj in scene.get("objects", []):
        if obj.get("object_id") != BENCHY_OBJECT_ID:
            continue
        transform = obj.setdefault("transform", {})
        position = transform.setdefault("position", {})
        position["z"] = benchy_z
        changed = True
    if not changed:
        raise ValueError(f"missing object {BENCHY_OBJECT_ID}")

    authoring = scene.get("extensions", {}).get("ray_tracing", {}).get("authoring", {})
    if isinstance(authoring.get("camera_focus_target"), dict):
        authoring["camera_focus_target"]["z"] = max(1.1, benchy_z - 0.12)
    return scene


def prepare_payload(args: argparse.Namespace) -> tuple[Path, Path]:
    review_root = args.review_root.resolve()
    if review_root.exists():
        shutil.rmtree(review_root)
    review_root.mkdir(parents=True, exist_ok=True)

    scene = lift_benchy(load_json(args.source_scene), args.benchy_z)
    scene["scene_id"] = RUN_ID
    scene_path = args.source_scene.resolve().parent / "scene_runtime_benchy_above_water_preview.json"
    write_json(scene_path, scene)
    shutil.copy2(scene_path, review_root / scene_path.name)

    water_target = review_root / "water_cache" / "Water Basin"
    shutil.copytree(args.water_root, water_target)

    request = load_json(args.source_request)
    render_root = review_root / "render"
    request["run_id"] = RUN_ID
    request["scene"] = {"runtime_scene_path": str(scene_path)}
    request["volume"] = {
        "enabled": True,
        "source_kind": "scene_bundle",
        "source_path": str(water_target / "scene_bundle.json"),
        "visible": False,
        "affects_lighting": False,
        "debug_overlay": False,
    }
    request["render"] = dict(request.get("render", {}))
    request["render"].update(
        {
            "width": args.width,
            "height": args.height,
            "frame_count": 1,
            "temporal_frames": 1,
            "integrator_3d": args.integrator,
            "transmission_samples_3d": 2,
            "secondary_diffuse_samples_3d": 2,
            "denoise_enabled": False,
        }
    )
    request["inspection"] = dict(request.get("inspection", {}))
    request["inspection"].update(
        {
            "trace_route": args.trace_route,
            "camera_position": {"x": -2.35, "y": -5.35, "z": 2.18},
            "camera_look_at": {"x": -0.35, "y": -0.08, "z": 1.14},
            "camera_zoom": 0.92,
            "caustic_mode": "off",
            "caustic_sidecar_enabled": False,
            "caustic_sidecar_strength": 0.0,
            "object_audit_enabled": True,
            "object_audit_max_dimension": 96,
            "transmission_samples_3d": 2,
            "secondary_diffuse_samples_3d": 2,
        }
    )
    request["output"] = {"root": str(render_root), "overwrite": True}
    request["progress"] = {
        "summary_path": str(render_root / "render_summary.json"),
        "progress_path": str(render_root / "render_progress.json"),
    }
    request_path = review_root / "request.json"
    write_json(request_path, request)
    return request_path, render_root / "render_summary.json"


def render_request(cli: Path, request_path: Path, summary_path: Path, skip_render: bool) -> float | None:
    if skip_render:
        return None
    stdout_path = summary_path.parent / "stdout_summary.json"
    stderr_path = summary_path.parent / "stderr.txt"
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env["RAY_TRACING_RENDER_TRACE_COST_LEDGER"] = "1"
    start = time.perf_counter()
    with stdout_path.open("w", encoding="utf-8") as stdout:
        result = subprocess.run(
            [str(cli), "--request", str(request_path), "--render", "--summary", str(summary_path)],
            stdout=stdout,
            stderr=subprocess.PIPE,
            env=env,
            text=True,
        )
    elapsed = time.perf_counter() - start
    stderr_path.write_text(result.stderr or "", encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(
            f"render failed for {request_path} with exit {result.returncode}; stderr: {stderr_path}"
        )
    return elapsed


def first_frame_path(summary: dict) -> Path:
    frame_path = Path(summary.get("outputs", {}).get("first_frame_path", ""))
    if frame_path.exists():
        return frame_path
    return Path(summary.get("output_root", "")) / "frames" / "frame_0200.bmp"


def write_png(summary: dict, review_root: Path) -> Path:
    frame_path = first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "aquarium_benchy_above_water_low_preview.png"
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return png_path


def object_audit_digest(summary: dict) -> dict:
    result: dict[str, dict] = {}
    for entry in summary.get("object_audit", []):
        object_id = entry.get("object_id") or "<generated>"
        result[object_id] = {
            "object_type": entry.get("object_type", ""),
            "material_id": entry.get("material_id", -1),
            "alpha": entry.get("alpha", 1.0),
            "triangle_count": entry.get("triangle_count", 0),
            "primary_hit_pixels": entry.get("primary_hit_pixels", 0),
            "center_screen": entry.get("center_screen", {}),
        }
    return result


def main() -> int:
    args = parse_args()
    if not args.skip_render and not args.cli.exists():
        raise FileNotFoundError(f"missing ray_tracing_render_headless binary: {args.cli}")
    if not args.water_root.joinpath("scene_bundle.json").exists():
        raise FileNotFoundError(f"missing water scene bundle: {args.water_root / 'scene_bundle.json'}")

    request_path, summary_path = prepare_payload(args)
    elapsed = render_request(args.cli.resolve(), request_path, summary_path, args.skip_render)
    if args.skip_render:
        print(f"prepared preview payload: {request_path}")
        return 0

    summary = load_json(summary_path)
    png_path = write_png(summary, args.review_root.resolve())
    objects = object_audit_digest(summary)
    ledger = fixture.ledger_digest(summary)
    report_path = args.review_root.resolve() / "aquarium_benchy_above_water_low_preview_report.json"
    report = {
        "schema": "codework_aquarium_benchy_above_water_low_preview_report_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "run_id": RUN_ID,
        "benchy_z": args.benchy_z,
        "request_path": str(request_path),
        "summary_path": str(summary_path),
        "png_path": str(png_path),
        "render_seconds": elapsed,
        "render": {
            "width": args.width,
            "height": args.height,
            "integrator_3d": args.integrator,
            "trace_route": args.trace_route,
            "transmission_samples_3d": 2,
            "secondary_diffuse_samples_3d": 2,
        },
        "ledger": ledger,
        "object_audit": objects,
    }
    write_json(report_path, report)
    index_path = args.review_root.resolve() / "aquarium_benchy_above_water_low_preview_index.md"
    benchy = objects.get(BENCHY_OBJECT_ID, {})
    index_path.write_text(
        "\n".join(
            [
                "# Aquarium Benchy Above-Water Low Preview",
                "",
                f"- generated: `{report['generated_at_utc']}`",
                f"- png: `{png_path.name}`",
                f"- report: `{report_path.name}`",
                f"- render seconds: `{elapsed:.3f}`",
                f"- Benchy z: `{args.benchy_z:.3f}`",
                f"- Benchy primary pixels: `{benchy.get('primary_hit_pixels', 0)}`",
                f"- transmission rays: `{ledger.get('transmission_rays', 0)}`",
                f"- transparent hits: `{ledger.get('transparent_surface_hits', 0)}`",
                "",
            ]
        ),
        encoding="utf-8",
    )
    print(f"preview png: {png_path}")
    print(f"preview report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
