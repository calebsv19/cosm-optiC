#!/usr/bin/env python3
"""Run local authored-scene validation for the spatial caustic volume proof."""

from __future__ import annotations

import argparse
import json
import platform
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import run_ray_tracing_spatial_caustic_visual_sphere_mist_matrix as sphere_mist  # noqa: E402


THRESHOLDS = {
    "positive_pixel_count": 64.0,
    "max_luma_delta": 4.0,
    "volume_scatter_radiance_sum": 0.05,
    "volume_cache_sample_hit_ratio": 0.001,
    "volume_cache_nonzero_cell_ratio": 0.005,
    "volume_scatter_to_cache_radiance_ratio": 0.0001,
    "volume_lateral_to_vertical_ratio": 0.20,
}


VARIANTS = [
    {
        "variant_id": "mesh_sphere_256_center",
        "mesh_asset": "asset_sphere_256x128.runtime.json",
        "mesh_id": "asset_sphere_256x128",
        "position": {"x": 0.0, "y": 0.0, "z": 1.32},
        "scale": {"x": 0.56, "y": 0.56, "z": 0.56},
        "light_position": {"x": -0.10, "y": -0.10, "z": 3.55},
        "camera_position": {"x": 3.15, "y": -4.75, "z": 1.95},
        "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.08},
        "camera_zoom": 0.82,
    },
    {
        "variant_id": "mesh_sphere_128_center",
        "mesh_asset": "asset_sphere_128x64.runtime.json",
        "mesh_id": "asset_sphere_128x64",
        "position": {"x": 0.0, "y": 0.0, "z": 1.32},
        "scale": {"x": 0.56, "y": 0.56, "z": 0.56},
        "light_position": {"x": -0.10, "y": -0.10, "z": 3.55},
        "camera_position": {"x": 3.15, "y": -4.75, "z": 1.95},
        "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.08},
        "camera_zoom": 0.82,
    },
    {
        "variant_id": "mesh_sphere_64_raised",
        "mesh_asset": "asset_sphere_64x32.runtime.json",
        "mesh_id": "asset_sphere_64x32",
        "position": {"x": 0.0, "y": 0.0, "z": 1.40},
        "scale": {"x": 0.50, "y": 0.50, "z": 0.50},
        "light_position": {"x": -0.08, "y": -0.08, "z": 3.55},
        "camera_position": {"x": 3.15, "y": -4.75, "z": 2.05},
        "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.14},
        "camera_zoom": 0.84,
    },
]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def workspace_root() -> Path:
    return repo_root().parent


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = root / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless"
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def default_review_root() -> Path:
    return workspace_root() / "_private_workspace_artifacts" / "agent_runs" / "ray_tracing" / "caustic_authored_validation_matrix"


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--review-root", type=Path, default=default_review_root())
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--keep-going", action="store_true")
    return parser.parse_args()


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_variant_scene(variant_root: Path, variant: dict) -> Path:
    scene_dir = variant_root / "generated_scene"
    mesh_dir = scene_dir / "assets" / "mesh_assets"
    mesh_dir.mkdir(parents=True, exist_ok=True)
    source_mesh = (
        repo_root() /
        "tests" /
        "fixtures" /
        "mesh_asset_runtime_spheres" /
        "assets" /
        "mesh_assets" /
        variant["mesh_asset"]
    )
    shutil.copy2(source_mesh, mesh_dir / variant["mesh_asset"])

    floor = sphere_mist.plane_object(
        "matte_receiver_floor",
        {
            "origin": {"x": 0.0, "y": 0.0, "z": 0.0},
            "axis_u": {"x": 1.0, "y": 0.0, "z": 0.0},
            "axis_v": {"x": 0.0, "y": 1.0, "z": 0.0},
            "normal": {"x": 0.0, "y": 0.0, "z": 1.0},
        },
        6.0,
        5.2,
        "mat_warm_floor",
    )
    sphere = {
        "object_id": "authored_glass_sphere",
        "object_type": "mesh_asset_instance",
        "space_mode_intent": "3d",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": variant["position"],
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": variant["scale"],
        },
        "geometry_ref": {
            "kind": "mesh_asset",
            "id": variant["mesh_id"],
            "variant": "runtime_default",
        },
        "extensions": {
            "line_drawing": {
                "runtime_mesh_path": f"assets/mesh_assets/{variant['mesh_asset']}",
            }
        },
        "material_id": "mat_glass_sphere",
        "flags": {"visible": True, "locked": False, "selectable": True},
    }
    scene = {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"caustic_authored_validation_{variant['variant_id']}",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [floor, sphere],
        "materials": [
            {"material_id": "mat_warm_floor", "kind": "lambert", "albedo": [0.62, 0.54, 0.45]},
            {"material_id": "mat_glass_sphere", "kind": "dielectric", "albedo": [0.92, 0.97, 1.0]},
        ],
        "lights": [
            {
                "light_id": "overhead_focus_light",
                "kind": "sphere",
                "position": variant["light_position"],
                "radius": 0.08,
                "intensity": 8.8,
                "falloff_distance": 8.0,
                "color": {"r": 1.0, "g": 0.96, "b": 0.88},
                "enabled": True,
                "moving": False,
            }
        ],
        "cameras": [
            {
                "camera_id": "caustic_authored_validation_camera",
                "kind": "perspective",
                "position": variant["camera_position"],
                "target": variant["camera_look_at"],
                "yaw": 0.0,
                "look_pitch": -0.18,
            }
        ],
        "constraints": [],
        "hierarchy": [],
        "extensions": {
            "ray_tracing": {
                "authoring": {
                    "camera_focus_target": variant["camera_look_at"],
                    "environment": {
                        "light_mode": 1,
                        "ambient_strength": 0.08,
                        "top_fill_strength": 0.16,
                    },
                    "object_materials": [
                        {
                            "object_id": "matte_receiver_floor",
                            "material_id": 0,
                            "object_color": sphere_mist.rgb_u24(158, 138, 115),
                            "roughness": 0.86,
                            "reflectivity": 0.02,
                            "alpha": 1.0,
                        },
                        {
                            "object_id": "authored_glass_sphere",
                            "material_id": 5,
                            "object_color": sphere_mist.rgb_u24(231, 247, 255),
                            "alpha": 0.42,
                            "roughness": 0.015,
                            "reflectivity": 0.04,
                        },
                    ],
                }
            }
        },
    }
    scene_path = scene_dir / "scene_runtime.json"
    write_json(scene_path, scene)
    return scene_path


def request_for_cell(variant_root: Path, variant: dict, scene_path: Path, volume_path: Path, cell_id: str) -> dict:
    run_id = f"caustic_authored_{variant['variant_id']}_{cell_id}"
    output_root = variant_root / "runs" / cell_id
    summary_path = output_root / "render_summary.json"
    request = sphere_mist.base_request(run_id, scene_path, output_root, summary_path, volume_path)
    request["inspection"].update({
        "camera_position": variant["camera_position"],
        "camera_look_at": variant["camera_look_at"],
        "camera_zoom": variant["camera_zoom"],
    })
    if cell_id == "mist_no_caustic":
        request["inspection"].update({
            "caustic_mode": "off",
            "caustic_volume_enabled": False,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 0,
        })
    elif cell_id == "volume_caustic_only":
        request["inspection"].update({
            "caustic_mode": "transport",
            "caustic_volume_enabled": True,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 3072,
            "caustic_max_path_depth": 2,
            "caustic_surface_receiver_fallback_enabled": False,
        })
    else:
        raise ValueError(f"unknown authored validation cell: {cell_id}")
    request_path = variant_root / "generated_requests" / f"request_{cell_id}.json"
    write_json(request_path, request)
    return {"cell_id": cell_id, "request_path": request_path, "summary_path": summary_path}


def threshold_result(name: str, observed: float, minimum: float) -> dict:
    return {
        "observed": observed,
        "minimum": minimum,
        "margin": observed - minimum,
        "passed": observed >= minimum,
    }


def classify_thresholds(delta: dict, digest: dict, readback: dict) -> dict:
    span = readback.get("volume_world_span", {})
    results = {
        "positive_pixel_count": threshold_result(
            "positive_pixel_count",
            float(delta.get("positive_pixel_count", 0)),
            THRESHOLDS["positive_pixel_count"],
        ),
        "max_luma_delta": threshold_result(
            "max_luma_delta",
            float(delta.get("max_luma_delta", 0.0)),
            THRESHOLDS["max_luma_delta"],
        ),
        "volume_scatter_radiance_sum": threshold_result(
            "volume_scatter_radiance_sum",
            float(digest.get("volume_scatter_radiance_sum", 0.0)),
            THRESHOLDS["volume_scatter_radiance_sum"],
        ),
        "volume_cache_sample_hit_ratio": threshold_result(
            "volume_cache_sample_hit_ratio",
            float(digest.get("volume_cache_sample_hit_ratio", 0.0)),
            THRESHOLDS["volume_cache_sample_hit_ratio"],
        ),
        "volume_cache_nonzero_cell_ratio": threshold_result(
            "volume_cache_nonzero_cell_ratio",
            float(digest.get("volume_cache_nonzero_cell_ratio", 0.0)),
            THRESHOLDS["volume_cache_nonzero_cell_ratio"],
        ),
        "volume_scatter_to_cache_radiance_ratio": threshold_result(
            "volume_scatter_to_cache_radiance_ratio",
            float(digest.get("volume_scatter_to_cache_radiance_ratio", 0.0)),
            THRESHOLDS["volume_scatter_to_cache_radiance_ratio"],
        ),
        "volume_lateral_to_vertical_ratio": threshold_result(
            "volume_lateral_to_vertical_ratio",
            float(span.get("lateral_to_vertical_ratio", 0.0)),
            THRESHOLDS["volume_lateral_to_vertical_ratio"],
        ),
    }
    results["not_narrow_column"] = {
        "observed": not bool(readback.get("volume_narrow_column", True)),
        "expected": True,
        "passed": not bool(readback.get("volume_narrow_column", True)),
    }
    return results


def run_variant(cli: Path, review_root: Path, variant: dict, skip_render: bool) -> dict:
    variant_root = review_root / variant["variant_id"]
    scene_path = write_variant_scene(variant_root, variant)
    volume_path = variant_root / "generated_volume" / "soft_mist.vf3d"
    sphere_mist.write_soft_mist_vf3d(volume_path)
    runs = []
    runs_by_cell: dict[str, dict] = {}
    failures: list[str] = []
    for cell_id in ("mist_no_caustic", "volume_caustic_only"):
        item = request_for_cell(variant_root, variant, scene_path, volume_path, cell_id)
        elapsed = sphere_mist.render_request(cli, item["request_path"], item["summary_path"], skip_render)
        summary = load_json(item["summary_path"])
        frame_path, png_path = sphere_mist.copy_frame_png(summary, variant_root, cell_id)
        summary_copy = variant_root / "summaries" / f"summary_{cell_id}.json"
        summary_copy.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(item["summary_path"], summary_copy)
        digest = sphere_mist.caustic_digest(summary)
        cell_failures = sphere_mist.validate_cell(cell_id, digest)
        failures.extend([f"{cell_id}: {failure}" for failure in cell_failures])
        run = {
            "cell_id": cell_id,
            "request_path": str(item["request_path"]),
            "summary_path": str(item["summary_path"]),
            "summary_copy": str(summary_copy),
            "frame_path": str(frame_path),
            "png_path": str(png_path),
            "elapsed_seconds": elapsed,
            "caustic": digest,
            "failures": cell_failures,
        }
        runs.append(run)
        runs_by_cell[cell_id] = run

    frame_deltas = sphere_mist.frame_delta_metrics(runs_by_cell)
    readback = sphere_mist.phase11k_readback(runs_by_cell, frame_deltas)
    volume_delta = frame_deltas.get("volume_caustic_only", {})
    volume_digest = runs_by_cell["volume_caustic_only"]["caustic"]
    thresholds = classify_thresholds(volume_delta, volume_digest, readback)
    for name, result in thresholds.items():
        if not result.get("passed", False):
            failures.append(f"volume_caustic_only: threshold `{name}` failed")
    contact_sheet_path = variant_root / "contact_sheet.png"
    sphere_mist.write_contact_sheet(contact_sheet_path, runs)
    delta_artifacts = sphere_mist.write_delta_artifacts(variant_root, runs_by_cell)
    return {
        "variant_id": variant["variant_id"],
        "mesh_asset": variant["mesh_asset"],
        "scene_path": str(scene_path),
        "volume_path": str(volume_path),
        "runs": runs,
        "frame_deltas_vs_off": frame_deltas,
        "phase11l_readback": readback,
        "threshold_results": thresholds,
        "thresholds_passed": all(result.get("passed", False) for result in thresholds.values()),
        "failures": failures,
        "passed": len(failures) == 0,
        "contact_sheet_path": str(contact_sheet_path),
        "delta_artifacts": delta_artifacts,
    }


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Spatial Caustic Authored Validation Matrix",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- review root: `{report['review_root']}`",
        "",
        "## Thresholds",
        "",
    ]
    for key, value in THRESHOLDS.items():
        lines.append(f"- `{key}` minimum `{value}`")
    lines.extend(["", "## Variants", ""])
    for variant in report["variants"]:
        rb = variant["phase11l_readback"]
        span = rb.get("volume_world_span", {})
        screen = rb.get("volume_screen_span", {})
        lines.append(f"### {variant['variant_id']}")
        lines.append("")
        lines.append(f"- passed: `{variant['passed']}`")
        lines.append(f"- mesh asset: `{variant['mesh_asset']}`")
        lines.append(
            f"- volume: positive `{rb.get('volume_positive_pixels', 0)}`, "
            f"cells `{rb.get('volume_cache_nonzero_cells', 0)}`, "
            f"occupancy `{rb.get('volume_cache_occupancy', 0.0):.9f}`, "
            f"hit ratio `{rb.get('volume_cache_hit_ratio', 0.0):.9f}`, "
            f"scatter pixels `{rb.get('volume_scatter_pixels', 0)}`, "
            f"footprint radius `{rb.get('volume_average_footprint_radius_voxels', 0.0):.6f}`"
        )
        lines.append(
            f"- span: world `{span.get('x', 0.0):.6f} x {span.get('y', 0.0):.6f} x "
            f"{span.get('z', 0.0):.6f}`, lateral/z `{span.get('lateral_to_vertical_ratio', 0.0):.6f}`, "
            f"screen `{screen.get('x', 0)} x {screen.get('y', 0)}`, aspect "
            f"`{screen.get('aspect_x_over_y', 0.0):.6f}`"
        )
        lines.append("- threshold margins:")
        for name, result in variant["threshold_results"].items():
            if "minimum" in result:
                lines.append(
                    f"  - `{name}`: observed `{result['observed']}`, minimum `{result['minimum']}`, "
                    f"margin `{result['margin']}`, passed `{result['passed']}`"
                )
            else:
                lines.append(
                    f"  - `{name}`: observed `{result['observed']}`, expected `{result['expected']}`, "
                    f"passed `{result['passed']}`"
                )
        if variant["failures"]:
            lines.append("- failures:")
            for failure in variant["failures"]:
                lines.append(f"  - {failure}")
        lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    cli = args.cli.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2
    review_root.mkdir(parents=True, exist_ok=True)

    variants = []
    failures = []
    for variant in VARIANTS:
        try:
            result = run_variant(cli, review_root, variant, args.skip_render)
            variants.append(result)
            failures.extend([f"{variant['variant_id']}: {failure}" for failure in result["failures"]])
        except Exception as exc:
            failures.append(f"{variant['variant_id']}: {exc}")
            if not args.keep_going:
                break

    report = {
        "schema_version": "ray_tracing_spatial_caustic_authored_validation_matrix_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "thresholds": THRESHOLDS,
        "variants": variants,
        "failures": failures,
        "passed": len(failures) == 0 and len(variants) == len(VARIANTS),
    }
    report_path = review_root / "authored_validation_matrix_report.json"
    write_json(report_path, report)
    write_index(review_root / "authored_validation_matrix_index.md", report)
    print(report_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
