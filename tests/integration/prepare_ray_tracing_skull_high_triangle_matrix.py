#!/usr/bin/env python3
"""Prepare a private skull/high-triangle Disney v2 visual-matrix package."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path


def codework_root() -> Path:
    return Path(__file__).resolve().parents[3]


def ray_tracing_root() -> Path:
    return Path(__file__).resolve().parents[2]


def default_output_root() -> Path:
    return (
        codework_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "disney_v2_visual_matrix"
        / "skull_high_triangle_local"
        / "source_scene"
    )


def first_existing(candidates: list[Path], label: str) -> Path:
    for candidate in candidates:
        if candidate.exists():
            return candidate
    joined = "\n  ".join(str(path) for path in candidates)
    raise FileNotFoundError(f"missing {label}; checked:\n  {joined}")


def parse_args() -> argparse.Namespace:
    root = codework_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-scene",
        type=Path,
        default=root.parent / "Simulations" / "scenes" / "skull_plane_with_platform" / "scene_runtime.json",
    )
    parser.add_argument(
        "--skull-sidecar",
        type=Path,
        default=None,
        help="Override skull runtime sidecar path.",
    )
    parser.add_argument(
        "--column-sidecar",
        type=Path,
        default=None,
        help="Override stepped-column runtime sidecar path.",
    )
    parser.add_argument("--output-root", type=Path, default=default_output_root())
    parser.add_argument("--width", type=int, default=96)
    parser.add_argument("--height", type=int, default=72)
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def copy_sidecar(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() and dst.stat().st_size == src.stat().st_size:
        return
    shutil.copy2(src, dst)


def line_drawing_extension(obj: dict) -> dict | None:
    extensions = obj.get("extensions")
    if not isinstance(extensions, dict):
        return None
    line_drawing = extensions.get("line_drawing")
    return line_drawing if isinstance(line_drawing, dict) else None


def rewrite_scene(scene: dict, sidecar_names: dict[str, str]) -> dict:
    for obj in scene.get("objects", []):
        if not isinstance(obj, dict):
            continue
        line_drawing = line_drawing_extension(obj)
        if not line_drawing:
            continue
        mesh_asset_id = line_drawing.get("mesh_asset_id")
        if mesh_asset_id in sidecar_names:
            line_drawing["runtime_mesh_path"] = f"assets/mesh_assets/{sidecar_names[mesh_asset_id]}"
    return scene


def common_request(run_id: str,
                   integrator: str,
                   width: int,
                   height: int,
                   output_root: str) -> dict:
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": run_id,
        "scene": {
            "runtime_scene_path": "scene_runtime.json",
        },
        "volume": {
            "enabled": False,
        },
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": width,
            "height": height,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "integrator_3d": integrator,
        },
        "inspection": {
            "camera_position": {"x": 0.35, "y": -95.0, "z": 350.0},
            "camera_look_at": {"x": 0.0, "y": -20.0, "z": 340.0},
            "camera_zoom": 0.95,
            "environment_light_mode": "ambient",
            "ambient_strength": 0.36,
            "top_fill_strength": 1.6,
            "light_intensity": 3.8,
            "light_radius": 0.16,
            "secondary_diffuse_samples_3d": 2,
            "transmission_samples_3d": 1,
            "object_audit_enabled": True,
            "object_audit_max_dimension": 64,
        },
        "output": {
            "root": output_root,
            "overwrite": True,
        },
        "progress": {
            "summary_path": f"{output_root}/render_summary.json",
            "progress_path": f"{output_root}/render_progress.json",
        },
    }


def write_requests_and_manifest(output_root: Path, width: int, height: int) -> None:
    requests = {
        "request_direct_light.json": common_request(
            "skull_high_triangle_local_direct_light",
            "direct_light",
            width,
            height,
            "renders/direct_light",
        ),
        "request_disney_v2.json": common_request(
            "skull_high_triangle_local_disney_v2",
            "disney_v2",
            width,
            height,
            "renders/disney_v2",
        ),
    }
    for name, request in requests.items():
        write_json(output_root / name, request)
    manifest = {
        "schema_version": "ray_tracing_visual_matrix_v1",
        "matrix_id": "skull_high_triangle_local",
        "title": "Skull High-Triangle Local Disney V2 Matrix",
        "scene_path": "scene_runtime.json",
        "default_output_root": "renders",
        "private_workspace_output_root": "..",
        "default_review_root": "matrix_review",
        "groups": [
            {
                "id": "skull_high_triangle_smoke",
                "purpose": "Portable local high-triangle proof for direct light and Disney v2.",
                "requests": [
                    "request_direct_light.json",
                    "request_disney_v2.json",
                ],
            }
        ],
        "comparisons": [
            {
                "id": "skull_direct_light_vs_disney_v2",
                "group": "skull_high_triangle_smoke",
                "before": "request_direct_light.json",
                "after": "request_disney_v2.json",
                "purpose": "Check the same high-triangle scene renders through Disney v2 after portable sidecar staging.",
            }
        ],
    }
    write_json(output_root / "matrix_manifest.json", manifest)


def main() -> int:
    args = parse_args()
    source_scene = args.source_scene.resolve()
    skull_sidecar = (
        args.skull_sidecar.resolve()
        if args.skull_sidecar
        else first_existing(
            [
                Path("/Users/calebsv/Desktop/stls/sCulpt_STL_Test_Library/large_proofs/imported_bodyparts3d_skull.runtime.json"),
                Path("/Users/calebsv/Desktop/sCulpt_STL_Test_Library/large_proofs/imported_bodyparts3d_skull.runtime.json"),
            ],
            "skull runtime sidecar",
        )
    )
    column_sidecar = (
        args.column_sidecar.resolve()
        if args.column_sidecar
        else first_existing(
            [
                Path("/Users/calebsv/Desktop/stls/sCulpt_STL_Test_Library/imported_stepped_column_ascii.runtime.json"),
                Path("/Users/calebsv/Desktop/sCulpt_STL_Test_Library/imported_stepped_column_ascii.runtime.json"),
            ],
            "stepped-column runtime sidecar",
        )
    )
    output_root = args.output_root.resolve()
    assets_root = output_root / "assets" / "mesh_assets"
    skull_name = "imported_bodyparts3d_skull.runtime.json"
    column_name = "imported_stepped_column_ascii.runtime.json"

    if not source_scene.exists():
        raise FileNotFoundError(f"missing source scene: {source_scene}")

    output_root.mkdir(parents=True, exist_ok=True)
    copy_sidecar(skull_sidecar, assets_root / skull_name)
    copy_sidecar(column_sidecar, assets_root / column_name)
    scene = rewrite_scene(
        load_json(source_scene),
        {
            "imported_bodyparts3d_skull": skull_name,
            "imported_stepped_column_ascii": column_name,
        },
    )
    write_json(output_root / "scene_runtime.json", scene)
    write_requests_and_manifest(output_root, args.width, args.height)
    print(output_root / "matrix_manifest.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
