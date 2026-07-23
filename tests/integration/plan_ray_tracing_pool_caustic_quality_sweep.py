#!/usr/bin/env python3
"""Generate and preflight a static pool-caustic water-resolution sweep."""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

import run_ray_tracing_animated_water_photon_caustics as lane


DEFAULT_GRIDS = (72, 96, 144, 192, 256)


def preflight(cli: Path, request: Path, output: Path) -> dict:
    completed = subprocess.run(
        [str(cli), "--request", str(request), "--preflight"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    output.mkdir(parents=True, exist_ok=True)
    (output / "stdout.json").write_text(completed.stdout)
    (output / "stderr.txt").write_text(completed.stderr)
    if completed.returncode:
        raise RuntimeError(
            f"{request.name}: preflight failed {completed.returncode}: "
            f"{completed.stderr}"
        )
    json_start = completed.stdout.find("{")
    if json_start < 0:
        raise RuntimeError(f"{request.name}: preflight emitted no JSON summary")
    return json.JSONDecoder().raw_decode(completed.stdout[json_start:])[0]


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--water-manifest", type=Path, required=True)
    parser.add_argument(
        "--cli",
        type=Path,
        default=root / "build/toolchains/clang" / platform.machine()
        / "tools/cli/ray_tracing_render_headless",
    )
    parser.add_argument(
        "--review-root",
        type=Path,
        default=root / "build/agent_runs/ray_tracing/pool_caustic_quality_sweep_preflight",
    )
    parser.add_argument("--grids", default=",".join(str(value) for value in DEFAULT_GRIDS))
    parser.add_argument("--photon-budget", type=int, default=65536)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=768)
    args = parser.parse_args()
    grids = tuple(int(value) for value in args.grids.split(","))
    if any(value < 32 or value > 512 for value in grids):
        raise SystemExit("grid values must be between 32 and 512")
    if len(set(grids)) != len(grids):
        raise SystemExit("grid values must be unique")

    args.cli = args.cli.resolve()
    args.review_root = args.review_root.resolve()
    source_manifest = args.water_manifest.resolve()
    if args.review_root.exists():
        shutil.rmtree(args.review_root)
    generated = args.review_root / "generated"
    scene_path = generated / "scene_runtime.json"
    lane.scene(scene_path, pool_review=True)

    cells = []
    for grid in grids:
        cell_id = f"water_{grid:04d}"
        bundle, source = lane.water_bundle(
            generated / cell_id / "water",
            source_manifest,
            capillary_review=True,
            capillary_grid=grid,
        )
        output = args.review_root / "planned_outputs" / cell_id
        request_path = args.review_root / "requests" / f"{cell_id}.json"
        request = lane.render_request(
            scene_path,
            bundle,
            output,
            1,
            True,
            args.photon_budget,
            pool_review=True,
        )
        request["run_id"] = f"pool_caustic_quality_{cell_id}"
        request["render"]["width"] = args.width
        request["render"]["height"] = args.height
        request["scene"]["runtime_scene_path"] = os.path.relpath(
            scene_path, request_path.parent
        )
        request["volume"]["source_path"] = os.path.relpath(
            bundle, request_path.parent
        )
        request["output"]["root"] = os.path.relpath(output, request_path.parent)
        request["progress"]["summary_path"] = os.path.relpath(
            output / "render_summary.json", request_path.parent
        )
        request["progress"]["progress_path"] = os.path.relpath(
            output / "render_progress.json", request_path.parent
        )
        lane.dump(request_path, request)
        summary = preflight(
            args.cli,
            request_path,
            args.review_root / "preflight" / cell_id,
        )
        spacing_x = lane.EXTENT_X / (grid - 1)
        shortest_wavelength = min(mode[0] for mode in lane.CAPILLARY_MODES)
        cells.append(
            {
                "cell_id": cell_id,
                "grid": [grid, grid],
                "nominal_triangle_count": 2 * (grid - 1) * (grid - 1),
                "sample_spacing_x_m": spacing_x,
                "samples_per_shortest_capillary_wavelength":
                    shortest_wavelength / spacing_x,
                "photon_budget": args.photon_budget,
                "request_path": str(request_path),
                "water_bundle_path": str(bundle),
                "planned_output_root": str(output),
                "source": source,
                "preflight": {
                    "request_loaded": summary.get("request_loaded"),
                    "scene_applied": summary.get("scene_applied"),
                    "water_surface_source_found":
                        summary.get("water_surface_source_found"),
                    "water_surface_loaded": summary.get("water_surface_loaded"),
                    "water_surface_mesh_attached":
                        summary.get("water_surface_mesh_attached"),
                    "prepared_frame": summary.get("prepared_frame"),
                    "rendered_frames": summary.get("rendered_frames"),
                    "diagnostics": summary.get("diagnostics"),
                },
            }
        )

    report = {
        "schema": "ray_tracing_pool_caustic_quality_sweep_plan_v1",
        "status": "preflight_pass",
        "promotion_eligible": False,
        "render_started": False,
        "request_paths_payload_relative": True,
        "visual_intent": (
            "Find the smallest water grid that preserves the accepted connected "
            "pool-caustic ridge network."
        ),
        "expected_signal": (
            "Increasing grid density should reduce aliased/broken ridges and "
            "converge toward the 256-square reference."
        ),
        "rejection_conditions": [
            "any cell changes light, camera, photon seed, gather, material, or water phase",
            "preflight renders frames or mutates a scene pointer",
            "a high-density cell collapses into bands or broad pockets",
            "comparison proceeds without beauty, raw-landing, and floor-light AOV outputs",
        ],
        "invariants": {
            "resolution": [args.width, args.height],
            "photon_budget": args.photon_budget,
            "photon_seed": 1592594996,
            "water_extent_m": [lane.EXTENT_X, lane.EXTENT_Y],
            "water_base_height_m": lane.BASE_HEIGHT,
            "light": "distant_point_sun_proxy",
            "surface_query_radius_m": 0.045,
            "surface_gather_max_radius_m": 0.075,
            "surface_gather_neighbors": 8,
            "source_manifest": str(source_manifest),
            "source_frame_index": 80,
        },
        "cells": cells,
        "next_gate": "render the five static cells only after source/package selection",
    }
    lane.dump(args.review_root / "quality_sweep_plan.json", report)
    print(args.review_root / "quality_sweep_plan.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
