#!/usr/bin/env python3
"""Render one bounded cell of the formed-concrete preset matrix."""
from __future__ import annotations
import argparse
import platform
from pathlib import Path
from procedural_surface_feature_relief_visual_proof import (
    load, render_request, review_artifacts, run_render_cli, runtime_scene, write_json,
)
ROOT = Path(__file__).resolve().parents[1]
def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variant", choices=("low", "medium", "high"), required=True)
    parser.add_argument("--view", choices=("hero", "grazing"), default="hero")
    parser.add_argument("--output-root", type=Path, default=ROOT / "build/agent_runs/ray_tracing/formed_concrete_preset_matrix")
    parser.add_argument("--render-cli", type=Path, default=ROOT / "build/toolchains/clang" / platform.machine() / "tools/cli/ray_tracing_render_headless")
    args = parser.parse_args(); root = args.output_root.resolve(); variant = args.variant
    relief = root / f"generated/{variant}/relief"
    summary = load(relief / "asset_summary.json")
    output = root / "review" / f"{variant}_{args.view}"; output.mkdir(parents=True, exist_ok=True)
    scene, request, raw = output / "scene.json", output / "request.json", output / "raw"
    view = ({"id":"hero", "camera_position":{"x":3.8,"y":8.5,"z":3.1}, "camera_look_at":{"x":0,"y":0,"z":0}}
            if args.view == "hero" else {"id":"grazing", "camera_position":{"x":8.8,"y":4.0,"z":.8}, "camera_look_at":{"x":0,"y":0,"z":0}})
    contract = {"render":{"width":640,"height":480,"integrator_3d":"direct_light","temporal_frames":1,"camera_zoom":1.0},
                "lighting":{"environment_light_mode":"ambient","ambient_strength":.30,"top_fill_strength":.8,"light_intensity":3.6,"light_radius":0.0},
                "light_rig":[{"id":"raking_key","position":{"x":-4,"y":5,"z":5.5},"intensity":3.8},{"id":"soft_fill","position":{"x":4.5,"y":2.5,"z":-2},"intensity":1.6}]}
    object_id = f"formed_concrete_{variant}"
    write_json(scene, runtime_scene(object_id, object_id, summary["selected_face_shell"]["derived_asset_id"], relief / "runtime_mesh.json", relief / "derived_asset.json", scene, summary, contract["light_rig"]))
    write_json(request, render_request(object_id, view, scene, request, raw, contract))
    run_render_cli(args.render_cli, request, raw / "render_summary.json")
    width, height, pixels = review_artifacts.read_bmp_rgb(raw / "frames/frame_0000.bmp")
    png = output / "render.png"; review_artifacts.write_png_rgb(png, width, height, pixels)
    print(png)
    return 0
if __name__ == "__main__": raise SystemExit(main())
