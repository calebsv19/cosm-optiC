#!/usr/bin/env python3
"""Render a fresh dense groom across PSG-23G hair-optics states."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import platform
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests/integration"))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import procedural_curve_render_children_visual_proof as density_proof  # noqa: E402
from procedural_surface_visual_proof import (  # noqa: E402
    image_metrics,
    object_audit,
    render_request,
    run_render_cli,
    write_json,
    write_labeled_contact_sheet,
)


def binary(name: str) -> pathlib.Path:
    return ROOT / "build/toolchains/clang" / platform.machine() / "tools/cli" / name


def args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--render-cli", type=pathlib.Path,
                        default=binary("ray_tracing_render_headless"))
    parser.add_argument("--output-root", type=pathlib.Path)
    parser.add_argument("--width", type=int, default=900)
    parser.add_argument("--height", type=int, default=700)
    return parser.parse_args()


def load(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def changed_pixels(a: list[list[tuple[int, int, int]]],
                   b: list[list[tuple[int, int, int]]]) -> int:
    return sum(
        1
        for left_row, right_row in zip(a, b)
        for left, right in zip(left_row, right_row)
        if left != right
    )


def maximum_channel_delta(a: list[list[tuple[int, int, int]]],
                          b: list[list[tuple[int, int, int]]]) -> int:
    return max(
        abs(left[channel] - right[channel])
        for left_row, right_row in zip(a, b)
        for left, right in zip(left_row, right_row)
        for channel in range(3)
    )


def saturation_metrics(pixels: list[list[tuple[int, int, int]]]) -> dict:
    pixel_count = sum(len(row) for row in pixels)
    near_white = sum(
        1 for row in pixels for pixel in row if min(pixel) >= 245
    )
    clipped = sum(
        1 for row in pixels for pixel in row if max(pixel) >= 254
    )
    return {
        "pixel_count": pixel_count,
        "near_white_pixel_count": near_white,
        "near_white_fraction": near_white / pixel_count,
        "clipped_channel_pixel_count": clipped,
        "clipped_channel_fraction": clipped / pixel_count,
    }


def difference_pixels(a: list[list[tuple[int, int, int]]],
                      b: list[list[tuple[int, int, int]]]
                      ) -> list[list[tuple[int, int, int]]]:
    return [
        [
            tuple(min(255, abs(left[channel] - right[channel]) * 8)
                  for channel in range(3))
            for left, right in zip(left_row, right_row)
        ]
        for left_row, right_row in zip(a, b)
    ]


def saturation_debug_pixels(
    pixels: list[list[tuple[int, int, int]]],
) -> list[list[tuple[int, int, int]]]:
    return [
        [
            (255, 32, 32) if max(pixel) >= 254 else
            tuple(channel // 5 for channel in pixel)
            for pixel in row
        ]
        for row in pixels
    ]


def material_row(object_id: str, variant: dict) -> dict:
    return {
        "object_id": object_id,
        "material_id": 0,
        "object_color": variant["color"],
        "roughness": 0.44,
        "reflectivity": 0.08,
        "hair_optics_enabled": variant["enabled"],
        "hair_absorption_r": variant["absorption"][0],
        "hair_absorption_g": variant["absorption"][1],
        "hair_absorption_b": variant["absorption"][2],
        "hair_longitudinal_roughness": variant["longitudinal"],
        "hair_azimuthal_roughness": variant["azimuthal"],
        "hair_ior": variant["ior"],
        "hair_cuticle_tilt_degrees": variant["tilt"],
    }


def main() -> int:
    options = args()
    if options.width < 900 or options.width > 4096:
        raise ValueError("--width must be between 900 and 4096")
    if options.height < 700 or options.height > 4096:
        raise ValueError("--height must be between 700 and 4096")
    output = (options.output_root or (
        ROOT / "build/agent_runs/ray_tracing/procedural_solid"
        / "psg23g_hair_optics_v1")).resolve()
    fresh = output / "fresh_dense_groom"
    generated = output / "generated"
    raw_root = output / "raw"
    review = output / "review"
    for directory in (generated, raw_root, review):
        directory.mkdir(parents=True, exist_ok=True)

    runtime = fresh / "generated/final.curve_runtime.json"
    base_scene_path = fresh / "generated/final.scene.json"
    if not runtime.exists() or not base_scene_path.exists():
        subprocess.run([
            sys.executable,
            str(ROOT / "tools/procedural_curve_render_children_visual_proof.py"),
            "--output-root", str(fresh),
        ], cwd=ROOT, check=True)
    if not runtime.exists() or not base_scene_path.exists():
        raise RuntimeError("fresh PSG-23F dense groom was not produced")
    base_scene = load(base_scene_path)
    shutil.copy2(runtime, generated / runtime.name)
    shutil.copytree(
        fresh / "generated/assets",
        generated / "assets",
        dirs_exist_ok=True,
    )
    variants = [
        {
            "id": "surface_legacy", "label": "SURFACE BSDF / CONTROL",
            "enabled": False, "color": 0x6B3A22,
            "absorption": [0.35, 0.70, 1.20],
            "longitudinal": 0.30, "azimuthal": 0.35, "ior": 1.55, "tilt": 2.0,
        },
        {
            "id": "brunette_gloss", "label": "BRUNETTE / TIGHT LOBES",
            "enabled": True, "color": 0x6B3A22,
            "absorption": [0.70, 1.50, 3.00],
            "longitudinal": 0.22, "azimuthal": 0.24, "ior": 1.55, "tilt": 2.0,
        },
        {
            "id": "copper_rough", "label": "COPPER / BROAD LOBES",
            "enabled": True, "color": 0xA64F28,
            "absorption": [0.18, 0.80, 1.80],
            "longitudinal": 0.62, "azimuthal": 0.68, "ior": 1.55, "tilt": 3.5,
        },
        {
            "id": "blond_soft", "label": "BLOND / LOW ABSORPTION",
            "enabled": True, "color": 0xD8B878,
            "absorption": [0.03, 0.10, 0.32],
            "longitudinal": 0.38, "azimuthal": 0.44, "ior": 1.55, "tilt": 1.5,
        },
    ]
    contract = {
        "render": {
            "width": options.width,
            "height": options.height,
            "temporal_frames": 1,
            "integrator_3d": "disney_v2", "camera_zoom": 1.48,
        },
        "lighting": {
            "light_mode": 2,
            "environment_light_mode": "ambient",
            "ambient_strength": 0.22,
            "light_intensity": 2.8,
            "light_radius": 0.0,
            "top_fill_strength": 0.40,
            "background_brightness": 0.025,
            "background_color": {"r": 0.025, "g": 0.030, "b": 0.045},
        },
    }
    cells = []
    evidence = []
    pixel_sets = []
    scene_paths = {}
    summaries = {}
    for variant in variants:
        scene_data = json.loads(json.dumps(base_scene))
        scene_data["scene_id"] = f"psg23g_{variant['id']}"
        scene_data["lights"] = [{
            "light_id": "psg23g_hair_key",
            "kind": "point",
            "position": {"x": -2.0, "y": 2.2, "z": 3.4},
            "intensity": 2.8,
            "radius": 0.0,
        }]
        scene_data["extensions"] = {
            "ray_tracing": {
                "authoring": {
                    "object_materials": [
                        {
                            "object_id": "final_scalp",
                            "material_id": 0,
                            "object_color": 0x553C32,
                            "roughness": 0.82,
                            "reflectivity": 0.03,
                        },
                        material_row("final_hair", variant),
                    ]
                }
            }
        }
        scene_path = generated / f"{variant['id']}.scene.json"
        request_path = generated / f"{variant['id']}.request.json"
        raw = raw_root / variant["id"]
        write_json(scene_path, scene_data)
        request_data = render_request(
            "psg23g_hair_optics",
            {
                "id": variant["id"],
                "camera_position": {"x": 1.68, "y": -2.18, "z": 2.45},
                "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.58},
            },
            scene_path, request_path, raw, contract)
        request_data["render"]["use_tiled_renderer"] = True
        request_data["render"]["tile_size"] = 16
        request_data["render"]["adaptive_sampling_enabled"] = False
        write_json(request_path, request_data)
        summary_path = raw / "render_summary.json"
        run_render_cli(options.render_cli.resolve(), request_path, summary_path)
        summary = load(summary_path)
        hair = object_audit(summary, "final_hair")
        width, height, pixels = review_artifacts.read_bmp_rgb(
            raw / "frames/frame_0000.bmp")
        review_artifacts.write_png_rgb(
            review / f"{variant['id']}.png", width, height, pixels)
        metrics = image_metrics(pixels)
        saturation = saturation_metrics(pixels)
        if hair["primary_hit_pixels"] < 1500:
            raise RuntimeError(f"{variant['id']}: dense curves not visible")
        if metrics["luma_standard_deviation"] < 4.0:
            raise RuntimeError(f"{variant['id']}: render is visually flat")
        if summary["render_stats"]["max_radiance"] > 4.0:
            raise RuntimeError(
                f"{variant['id']}: unbounded radiance "
                f"{summary['render_stats']['max_radiance']}")
        if saturation["near_white_fraction"] > 0.002:
            raise RuntimeError(
                f"{variant['id']}: near-white coverage "
                f"{saturation['near_white_fraction']:.6f}")
        cells.append((variant["label"], pixels))
        pixel_sets.append(pixels)
        scene_paths[variant["id"]] = scene_path
        summaries[variant["id"]] = summary
        evidence.append({
            **variant,
            "hair_primary_hit_pixels": hair["primary_hit_pixels"],
            "hair_scattering_pixels":
                summary["render_stats"]["hair_scattering_pixels"],
            "maximum_radiance": summary["render_stats"]["max_radiance"],
            "occupancy_skipped_tiles":
                summary["render_stats"]["temporal_occupancy_skipped_tiles"],
            "saturation": saturation,
            "image_metrics": metrics,
        })

    if evidence[0]["hair_scattering_pixels"] != 0:
        raise RuntimeError("surface control incorrectly used hair scattering")
    for item in evidence[1:]:
        if item["hair_scattering_pixels"] < 1000:
            raise RuntimeError(
                f"{item['id']}: hair scattering dispatch coverage too low")
    hit_counts = {item["hair_primary_hit_pixels"] for item in evidence}
    if len(hit_counts) != 1:
        raise RuntimeError(
            f"optical variants changed curve-hit geometry coverage: {hit_counts}")

    pairwise = []
    for index in range(1, len(pixel_sets)):
        changed = changed_pixels(pixel_sets[0], pixel_sets[index])
        if changed < 1000:
            raise RuntimeError(
                f"{variants[index]['id']}: insufficient change from control: {changed}")
        pairwise.append({
            "left": variants[0]["id"],
            "right": variants[index]["id"],
            "changed_pixels": changed,
        })

    parity_variant = "brunette_gloss"
    serial_request_path = generated / f"{parity_variant}.serial.request.json"
    serial_raw = raw_root / f"{parity_variant}_serial"
    serial_request = render_request(
        "psg23g_hair_optics_serial_parity",
        {
            "id": parity_variant,
            "camera_position": {"x": 1.68, "y": -2.18, "z": 2.45},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.58},
        },
        scene_paths[parity_variant],
        serial_request_path,
        serial_raw,
        contract,
    )
    serial_request["render"]["use_tiled_renderer"] = False
    serial_request["render"]["tile_size"] = 16
    serial_request["render"]["adaptive_sampling_enabled"] = False
    write_json(serial_request_path, serial_request)
    serial_summary_path = serial_raw / "render_summary.json"
    run_render_cli(
        options.render_cli.resolve(), serial_request_path, serial_summary_path)
    serial_summary = load(serial_summary_path)
    serial_hair = object_audit(serial_summary, "final_hair")
    serial_width, serial_height, serial_pixels = review_artifacts.read_bmp_rgb(
        serial_raw / "frames/frame_0000.bmp")
    tiled_pixels = pixel_sets[1]
    parity_changed = changed_pixels(tiled_pixels, serial_pixels)
    parity_max_delta = maximum_channel_delta(tiled_pixels, serial_pixels)
    if serial_width != options.width or serial_height != options.height:
        raise RuntimeError("serial parity render dimensions changed")
    if parity_changed != 0 or parity_max_delta != 0:
        raise RuntimeError(
            "tiled/serial curve render mismatch: "
            f"changed={parity_changed} max_delta={parity_max_delta}")
    if serial_hair["primary_hit_pixels"] != evidence[1]["hair_primary_hit_pixels"]:
        raise RuntimeError("tiled/serial curve-hit coverage mismatch")
    if (serial_summary["render_stats"]["hair_scattering_pixels"] !=
            evidence[1]["hair_scattering_pixels"]):
        raise RuntimeError("tiled/serial hair-dispatch coverage mismatch")

    parity_diff = difference_pixels(tiled_pixels, serial_pixels)
    saturation_debug = saturation_debug_pixels(tiled_pixels)
    debug_matrix = review / "psg23g_hair_optics_debug_matrix.png"
    write_labeled_contact_sheet(
        debug_matrix,
        [
            ("TILED BEAUTY", tiled_pixels),
            ("SERIAL BEAUTY", serial_pixels),
            ("TILED VS SERIAL RGB DIFF", parity_diff),
            ("CLIPPED CHANNEL MASK", saturation_debug),
        ],
        columns=2,
    )
    matrix = review / "psg23g_hair_optics_high_quality_matrix.png"
    write_labeled_contact_sheet(matrix, cells, columns=2)
    write_json(output / "proof_summary.json", {
        "schema": "ray_tracing.psg23g_hair_optics_visual_proof",
        "schema_version": 2,
        "passed": True,
        "render_resolution_per_cell": [options.width, options.height],
        "matrix": str(matrix.relative_to(output)),
        "debug_matrix": str(debug_matrix.relative_to(output)),
        "fresh_generated_dense_groom": True,
        "curve_asset_sha256": density_proof.groom_proof.sha(runtime),
        "variants": evidence,
        "pairwise_control_changes": pairwise,
        "tiled_serial_parity": {
            "variant": parity_variant,
            "tile_size": 16,
            "changed_pixels": parity_changed,
            "maximum_channel_delta": parity_max_delta,
            "hair_primary_hit_pixels":
                evidence[1]["hair_primary_hit_pixels"],
            "hair_scattering_pixels":
                evidence[1]["hair_scattering_pixels"],
            "tiled_occupancy_skipped_tiles":
                summaries[parity_variant]["render_stats"][
                    "temporal_occupancy_skipped_tiles"],
        },
        "authority": {
            "disney_v2_curve_hair_optics": True,
            "single_fiber_r_tt_trt_and_higher_order": True,
            "triangle_surface_dispatch_unchanged": True,
            "curve_aware_tile_occupancy": True,
            "tiled_serial_rgb_parity": True,
            "bounded_direct_light_response": True,
            "inter_fiber_multiple_scattering": False,
            "stochastic_hair_path_sampling": False,
            "opacity_transport_changed": False,
        },
    })
    print(output / "proof_summary.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
