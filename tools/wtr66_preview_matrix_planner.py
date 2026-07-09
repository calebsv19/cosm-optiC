#!/usr/bin/env python3
"""Generate a WTR-6.6 cache-first preview matrix without running renders."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def repo_paths() -> tuple[Path, Path, Path]:
    ray_dir = Path(__file__).resolve().parents[1]
    codework_root = ray_dir.parent
    physics_dir = codework_root / "physics_sim"
    return codework_root, ray_dir, physics_dir


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def write_lines(path: Path, values: list[int]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(f"{value}\n" for value in values), encoding="utf-8")


def runtime_scene() -> dict[str, Any]:
    def point(x: float, y: float, z: float | None = None) -> dict[str, float]:
        value = {"x": x, "y": y}
        if z is not None:
            value["z"] = z
        return value

    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "scene_wtr66_preview_matrix_water_object",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            {
                "object_id": "basin_floor",
                "object_type": "plane_primitive",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": point(2.0, 2.0, 0.02),
                    "rotation": point(0.0, 0.0, 0.0),
                    "scale": point(1.0, 1.0, 1.0),
                },
                "primitive": {
                    "kind": "plane_primitive",
                    "width": 4.35,
                    "height": 4.35,
                    "lock_to_construction_plane": False,
                    "lock_to_bounds": False,
                    "frame": {
                        "origin": point(2.0, 2.0, 0.02),
                        "axis_u": point(1.0, 0.0, 0.0),
                        "axis_v": point(0.0, 1.0, 0.0),
                        "normal": point(0.0, 0.0, 1.0),
                    },
                },
            },
            {
                "object_id": "basin_back_wall",
                "object_type": "rect_prism_primitive",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": point(2.0, 4.08, 0.42),
                    "rotation": point(0.0, 0.0, 0.0),
                    "scale": point(1.0, 1.0, 1.0),
                },
                "primitive": {
                    "kind": "rect_prism_primitive",
                    "width": 4.35,
                    "height": 0.14,
                    "depth": 0.82,
                },
            },
            {
                "object_id": "water_coupled_block",
                "object_type": "rect_prism_primitive",
                "space_mode_intent": "3d",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": point(2.0, 2.0, 0.47),
                    "rotation": point(0.0, 0.0, 0.0),
                    "scale": point(1.0, 1.0, 1.0),
                },
                "primitive": {
                    "kind": "rect_prism_primitive",
                    "width": 0.64,
                    "height": 0.64,
                    "depth": 0.66,
                },
            },
        ],
        "materials": [],
        "lights": [],
        "cameras": [],
        "constraints": [],
        "hierarchy": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "light_settings": {"intensity": 11.5, "radius": 0.14},
                    "environment": {
                        "light_mode": 2,
                        "ambient_strength": 0.12,
                        "top_fill_strength": 0.70,
                    },
                    "object_materials": [
                        {
                            "object_id": "basin_floor",
                            "material_id": 0,
                            "object_color": 3365436,
                            "roughness": 0.62,
                            "reflectivity": 0.07,
                        },
                        {
                            "object_id": "basin_back_wall",
                            "material_id": 0,
                            "object_color": 6381921,
                            "roughness": 0.66,
                            "reflectivity": 0.06,
                        },
                        {
                            "object_id": "water_coupled_block",
                            "material_id": 0,
                            "object_color": 15115520,
                            "roughness": 0.38,
                            "reflectivity": 0.16,
                        },
                    ],
                }
            }
        },
    }


def default_matrix_request(matrix_slug: str) -> dict[str, Any]:
    return {
        "schema": "wtr66_preview_matrix_request_v1",
        "matrix_slug": matrix_slug,
        "mode": "dry_run",
        "physics_caches": [
            {
                "slug": "wtr65_reference_cache_120f",
                "grid": "36x18x36",
                "frames": 120,
                "sim_steps_per_frame": 3,
                "water_level": 0.58,
                "water_object_fixture": True,
                "review_ripples": True,
                "review_ripple_amplitude_m": 0.035,
                "selected_frame_sets": {
                    "contact_short": [12, 18, 24],
                    "early_mid_late": [20, 60, 110],
                    "every_10": list(range(10, 120, 10)),
                },
            }
        ],
        "render_variants": [
            {
                "slug": "direct_light_t1_contact_short",
                "cache_slug": "wtr65_reference_cache_120f",
                "frame_set": "contact_short",
                "integrator_3d": "emission_transparency",
                "temporal_frames": 1,
                "width": 240,
                "height": 180,
                "fps": 6,
                "camera": "close",
            },
            {
                "slug": "disney_v2_t2_contact_short",
                "cache_slug": "wtr65_reference_cache_120f",
                "frame_set": "contact_short",
                "integrator_3d": "disney_v2",
                "temporal_frames": 2,
                "width": 240,
                "height": 180,
                "fps": 6,
                "camera": "close",
            },
        ],
    }


def physics_command(physics_dir: Path, cache: dict[str, Any], cache_root: Path) -> list[str]:
    return [
        str(physics_dir / "physics_sim_headless"),
        "--water-mode",
        "--frames",
        str(cache["frames"]),
        "--sim-steps-per-frame",
        str(cache["sim_steps_per_frame"]),
        "--grid",
        str(cache["grid"]),
        "--water-level",
        str(cache["water_level"]),
        "--water-object-fixture",
        "--water-review-ripples",
        "--water-review-ripple-amplitude",
        str(cache["review_ripple_amplitude_m"]),
        "--output-root",
        str(cache_root / "physics_sim"),
        "--summary",
        str(cache_root / "physics_sim" / "run_summary.json"),
        "--progress",
        str(cache_root / "physics_sim" / "run_progress.json"),
        "--overwrite",
        "--save-volume-frames",
    ]


def render_request(
    runtime_scene_path: Path,
    scene_bundle_path: Path,
    variant_root: Path,
    variant: dict[str, Any],
    frame: int,
    normalized_t: float,
) -> dict[str, Any]:
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": f"wtr66_{variant['slug']}_{frame:04d}",
        "scene": {"runtime_scene_path": str(runtime_scene_path)},
        "volume": {
            "enabled": True,
            "source_kind": "scene_bundle",
            "source_path": str(scene_bundle_path),
            "affects_lighting": True,
            "debug_overlay": False,
        },
        "render": {
            "start_frame": frame,
            "frame_count": 1,
            "width": int(variant["width"]),
            "height": int(variant["height"]),
            "normalized_t": normalized_t,
            "temporal_frames": int(variant["temporal_frames"]),
            "integrator_3d": str(variant["integrator_3d"]),
        },
        "inspection": {
            "camera_zoom": 0.75 if variant.get("camera") == "close" else 0.62,
            "camera_position": {"x": 2.0, "y": -4.75, "z": 2.42},
            "camera_look_at": {"x": 2.0, "y": 1.95, "z": 0.50},
            "environment_brightness": 0.016,
            "ambient_strength": 0.12,
            "environment_light_mode": "ambient",
            "top_fill_strength": 0.70,
            "background_brightness": 0.010,
            "background_color": {"r": 0.004, "g": 0.007, "b": 0.010},
            "light_intensity": 11.5,
            "light_radius": 0.14,
            "forward_decay": 260.0,
            "secondary_diffuse_samples_3d": 3,
            "transmission_samples_3d": 3,
            "object_audit_enabled": True,
            "object_audit_max_dimension": 180,
            "volume_scatter_gain": 1.35,
            "volume_step_scale": 0.85,
            "volume_tint": {"r": 0.28, "g": 0.72, "b": 1.45},
        },
        "output": {"root": str(variant_root), "overwrite": True},
        "progress": {
            "summary_path": str(variant_root / "summaries" / f"render_summary_{frame:04d}.json"),
            "progress_path": str(variant_root / "summaries" / f"render_progress_{frame:04d}.json"),
        },
    }


def validate_request(request: dict[str, Any]) -> None:
    cache_slugs = {cache["slug"] for cache in request["physics_caches"]}
    if len(cache_slugs) != len(request["physics_caches"]):
        raise SystemExit("duplicate physics cache slug in matrix request")
    for cache in request["physics_caches"]:
        for name, frames in cache["selected_frame_sets"].items():
            if not frames:
                raise SystemExit(f"selected frame set {name} is empty")
            if min(frames) < 0 or max(frames) >= int(cache["frames"]):
                raise SystemExit(f"selected frame set {name} exceeds cache frame count")
    for variant in request["render_variants"]:
        if variant["cache_slug"] not in cache_slugs:
            raise SystemExit(f"variant {variant['slug']} references unknown cache")
        if int(variant["temporal_frames"]) < 1:
            raise SystemExit(f"variant {variant['slug']} has invalid temporal_frames")


def generate_matrix(matrix_root: Path, request: dict[str, Any], overwrite: bool) -> dict[str, Any]:
    codework_root, _ray_dir, physics_dir = repo_paths()
    if matrix_root.exists() and not overwrite:
        raise SystemExit(f"{matrix_root} exists; pass --overwrite to update it")
    matrix_root.mkdir(parents=True, exist_ok=True)
    runtime_scene_path = matrix_root / "runtime_scene.json"
    write_json(runtime_scene_path, runtime_scene())
    write_json(matrix_root / "matrix_request.json", request)

    cache_records: dict[str, dict[str, Any]] = {}
    for cache in request["physics_caches"]:
        cache_root = matrix_root / "physics_caches" / cache["slug"]
        water_basin_dir = cache_root / "physics_sim" / "volume_frames" / "Water Basin"
        scene_bundle_path = water_basin_dir / "scene_bundle.json"
        selected_root = cache_root / "selected_frame_sets"
        for name, frames in cache["selected_frame_sets"].items():
            write_lines(selected_root / f"{name}.txt", [int(frame) for frame in frames])
        cache_manifest = {
            "schema": "wtr66_physics_cache_manifest_v1",
            "execution_mode": "dry_run",
            "status": "planned_not_run",
            "cache_slug": cache["slug"],
            "cache_root": str(cache_root),
            "physics_sim_output_root": str(cache_root / "physics_sim"),
            "scene_bundle_path": str(scene_bundle_path),
            "water_manifest_path": str(water_basin_dir / "water_manifest_v1.json"),
            "grid": cache["grid"],
            "frames_requested": cache["frames"],
            "sim_steps_per_frame": cache["sim_steps_per_frame"],
            "water_level": cache["water_level"],
            "water_object_fixture": cache["water_object_fixture"],
            "review_ripples": cache["review_ripples"],
            "review_ripple_amplitude_m": cache["review_ripple_amplitude_m"],
            "selected_frame_sets": {
                name: {
                    "path": str(selected_root / f"{name}.txt"),
                    "frames": frames,
                    "count": len(frames),
                }
                for name, frames in cache["selected_frame_sets"].items()
            },
            "planned_command": physics_command(physics_dir, cache, cache_root),
        }
        write_json(cache_root / "cache_manifest.json", cache_manifest)
        cache_records[cache["slug"]] = cache_manifest

    variant_records = []
    for variant in request["render_variants"]:
        cache = cache_records[variant["cache_slug"]]
        frame_set = cache["selected_frame_sets"][variant["frame_set"]]
        frames = [int(frame) for frame in frame_set["frames"]]
        variant_root = matrix_root / "render_variants" / variant["slug"]
        request_root = variant_root / "requests"
        for index, frame in enumerate(frames):
            normalized_t = 0.0 if len(frames) <= 1 else index / float(len(frames) - 1)
            request_path = request_root / f"ray_tracing_request_{index:04d}_frame_{frame:04d}.json"
            write_json(
                request_path,
                render_request(
                    runtime_scene_path,
                    Path(cache["scene_bundle_path"]),
                    variant_root,
                    variant,
                    frame,
                    normalized_t,
                ),
            )
        variant_summary = {
            "schema": "wtr66_render_variant_summary_v1",
            "execution_mode": "dry_run",
            "status": "planned_not_run",
            "variant_slug": variant["slug"],
            "cache_slug": variant["cache_slug"],
            "frame_set": variant["frame_set"],
            "selected_frames": frames,
            "integrator_3d": variant["integrator_3d"],
            "temporal_frames": variant["temporal_frames"],
            "width": variant["width"],
            "height": variant["height"],
            "fps": variant["fps"],
            "request_dir": str(request_root),
            "summary_dir": str(variant_root / "summaries"),
            "frame_dir": str(variant_root / "frames"),
            "route_validation_contract": {
                "summary_integrator_must_equal": variant["integrator_3d"],
                "summary_temporal_frames_must_equal": variant["temporal_frames"],
                "loaded_water_frame_must_equal_requested_frame": True,
                "water_mesh_attached_required": True,
                "block_primary_hits_required": True,
                "image_deltas_required": True,
            },
        }
        write_json(variant_root / "variant_summary.json", variant_summary)
        variant_records.append(variant_summary)

    cache_usage: dict[str, list[str]] = {}
    for variant in variant_records:
        cache_usage.setdefault(variant["cache_slug"], []).append(variant["variant_slug"])

    matrix_summary = {
        "schema": "wtr66_preview_matrix_summary_v1",
        "execution_mode": "dry_run",
        "status": "planned_not_run",
        "matrix_slug": request["matrix_slug"],
        "matrix_root": str(matrix_root),
        "codework_root": str(codework_root),
        "runtime_scene_path": str(runtime_scene_path),
        "physics_cache_count": len(cache_records),
        "render_variant_count": len(variant_records),
        "cache_reuse": {
            slug: {"variant_slugs": variants, "variant_count": len(variants)}
            for slug, variants in sorted(cache_usage.items())
        },
        "physics_caches": list(cache_records.values()),
        "render_variants": variant_records,
        "next_execution_boundary": (
            "Run one PhysicsSim cache, then submit/render variants over the same "
            "scene_bundle without regenerating cache output."
        ),
    }
    write_json(matrix_root / "matrix_summary.json", matrix_summary)
    return matrix_summary


def main() -> int:
    _codework_root, ray_dir, _physics_dir = repo_paths()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--matrix-root",
        type=Path,
        default=ray_dir / "build" / "agent_runs" / "physics_trio" / "wtr66_preview_matrix_dry_run",
    )
    parser.add_argument("--matrix-slug", default="wtr66_preview_matrix_dry_run")
    parser.add_argument("--request", type=Path, help="Optional matrix request JSON")
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args()

    if args.request:
        request = json.loads(args.request.read_text(encoding="utf-8"))
    else:
        request = default_matrix_request(args.matrix_slug)
    validate_request(request)
    summary = generate_matrix(args.matrix_root.resolve(), request, args.overwrite)
    print(json.dumps({
        "status": "planned",
        "matrix_root": summary["matrix_root"],
        "physics_cache_count": summary["physics_cache_count"],
        "render_variant_count": summary["render_variant_count"],
        "matrix_summary": str(Path(summary["matrix_root"]) / "matrix_summary.json"),
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
