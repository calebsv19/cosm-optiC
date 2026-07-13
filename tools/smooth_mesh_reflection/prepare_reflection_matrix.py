#!/usr/bin/env python3
"""Prepare generated assets, scene, and requests for smooth/flat reflection QA."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def mesh_object(object_id: str, asset_id: str, x: float, z: float, material: str,
                y: float = 0.4,
                scale: tuple[float, float, float] = (0.72, 0.72, 0.72)) -> dict:
    return {
        "object_id": object_id, "object_type": "mesh_asset_instance",
        "space_mode_intent": "3d", "dimensional_mode": "full_3d",
        "transform": {"position": {"x": x, "y": y, "z": z},
                      "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                      "scale": dict(zip(("x", "y", "z"), scale))},
        "geometry_ref": {"kind": "mesh_asset", "id": asset_id},
        "material_ref": {"id": material},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def prism_object(object_id: str, position: tuple[float, float, float],
                 size: tuple[float, float, float]) -> dict:
    return {
        "object_id": object_id, "object_type": "rect_prism_primitive",
        "space_mode_intent": "3d", "dimensional_mode": "full_3d",
        "transform": {"position": dict(zip(("x", "y", "z"), position)),
                      "scale": {"x": 1.0, "y": 1.0, "z": 1.0}},
        "primitive": {"kind": "rect_prism_primitive", "width": size[0],
                      "height": size[1], "depth": size[2]},
    }


def build_scene(asset_ids: dict[str, str]) -> dict:
    crease_asset = asset_ids["crease"]
    objects = [
        mesh_object("matte_floor", crease_asset, 0.0, -0.28, "mat_floor",
                    y=0.5, scale=(4.75, 3.0, 0.06)),
        mesh_object("red_panel", crease_asset, -3.7, 1.5, "mat_red",
                    y=2.6, scale=(0.22, 0.16, 2.0)),
        mesh_object("green_panel", crease_asset, 0.0, 3.2, "mat_green",
                    y=2.8, scale=(1.0, 0.14, 0.18)),
        mesh_object("blue_panel", crease_asset, 3.7, 1.5, "mat_blue",
                    y=2.6, scale=(0.22, 0.16, 2.0)),
        mesh_object("white_stripe_left", crease_asset, -2.0, 1.5, "mat_white",
                    y=2.9, scale=(0.09, 0.12, 2.0)),
        mesh_object("white_stripe_right", crease_asset, 2.0, 1.5, "mat_white",
                    y=2.9, scale=(0.09, 0.12, 2.0)),
    ]
    columns = (("analytic_sphere", -3.0), ("icosphere", -1.0),
               ("organic_blob", 1.0), ("crease", 3.0))
    object_materials = [
        {"object_id": "matte_floor", "material_id": 0, "object_color": 5263440,
         "roughness": 0.82, "reflectivity": 0.02},
        {"object_id": "red_panel", "material_id": 0, "object_color": 15145517,
         "roughness": 0.38, "reflectivity": 0.04},
        {"object_id": "green_panel", "material_id": 0, "object_color": 3985470,
         "roughness": 0.38, "reflectivity": 0.04},
        {"object_id": "blue_panel", "material_id": 0, "object_color": 3107327,
         "roughness": 0.38, "reflectivity": 0.04},
        {"object_id": "white_stripe_left", "material_id": 4, "object_color": 16777215,
         "roughness": 1.0, "reflectivity": 0.0, "emissive_strength": 2.4},
        {"object_id": "white_stripe_right", "material_id": 4, "object_color": 16777215,
         "roughness": 1.0, "reflectivity": 0.0, "emissive_strength": 2.4},
    ]
    for family, x in columns:
        mirror_id = f"{family}_mirror"
        metal_id = f"{family}_metal"
        objects.append(mesh_object(mirror_id, asset_ids[family], x, 2.1, "mat_mirror"))
        objects.append(mesh_object(metal_id, asset_ids[family], x, 0.65, "mat_metal"))
        object_materials.extend((
            {"object_id": mirror_id, "material_id": 1, "object_color": 15132390,
             "roughness": 0.015, "reflectivity": 0.98},
            {"object_id": metal_id, "material_id": 2, "object_color": 12619069,
             "roughness": 0.16, "reflectivity": 0.72},
        ))

    return {
        "schema_family": "codework_scene", "schema_variant": "scene_runtime_v1",
        "schema_version": 1, "scene_id": "smooth_mesh_reflection_matrix_v1",
        "space_mode_default": "3d", "unit_system": "meters", "world_scale": 1.0,
        "objects": objects, "hierarchy": [],
        "materials": [
            {"id": "mat_floor", "name": "Floor", "base_color": {"r": 0.31, "g": 0.31, "b": 0.31},
             "roughness": 0.82, "metallic": 0.0},
            {"id": "mat_red", "name": "Red panel", "base_color": {"r": 0.9, "g": 0.12, "b": 0.12},
             "roughness": 0.38, "metallic": 0.0},
            {"id": "mat_green", "name": "Green panel", "base_color": {"r": 0.12, "g": 0.82, "b": 0.24},
             "roughness": 0.38, "metallic": 0.0},
            {"id": "mat_blue", "name": "Blue panel", "base_color": {"r": 0.12, "g": 0.22, "b": 0.92},
             "roughness": 0.38, "metallic": 0.0},
            {"id": "mat_white", "name": "White emitter", "base_color": {"r": 1.0, "g": 1.0, "b": 1.0},
             "roughness": 1.0, "metallic": 0.0},
            {"id": "mat_mirror", "name": "Mirror", "base_color": {"r": 0.9, "g": 0.9, "b": 0.92},
             "roughness": 0.015, "metallic": 0.0},
            {"id": "mat_metal", "name": "Rough metal", "base_color": {"r": 0.78, "g": 0.55, "b": 0.32},
             "roughness": 0.16, "metallic": 1.0},
        ],
        "lights": [{"light_id": "matrix_key", "kind": "point",
                    "position": {"x": -2.8, "y": -4.0, "z": 5.5},
                    "intensity": 11.0, "radius": 0.22}],
        "cameras": [{"camera_id": "cam_main", "kind": "perspective",
                     "position": {"x": 0.0, "y": -11.0, "z": 2.3},
                     "target": {"x": 0.0, "y": 0.5, "z": 1.35},
                     "yaw": 0.0, "look_pitch": -0.06}],
        "constraints": [],
        "extensions": {"ray_tracing": {"authoring": {
            "environment": {"light_mode": 2, "ambient_strength": 0.12, "top_fill_strength": 0.55},
            "object_materials": object_materials,
        }}},
    }


def build_request(out_root: Path, mode: str, route: str) -> dict:
    render_root = (out_root / "renders" / f"{mode}_{route}").resolve()
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": f"smooth_mesh_reflection_{mode}_{route}",
        "scene": {"runtime_scene_path": "scene_runtime.json"}, "volume": {"enabled": False},
        "render": {"start_frame": 0, "frame_count": 1, "width": 480, "height": 270,
                   "normalized_t": 0.0, "temporal_frames": 1,
                   "integrator_3d": "disney_v2", "denoise_enabled": False},
        "inspection": {"trace_route": route, "camera_position": {"x": 0.0, "y": -11.0, "z": 2.3},
                       "camera_look_at": {"x": 0.0, "y": 0.5, "z": 1.35},
                       "camera_zoom": 1.0, "environment_light_mode": "ambient",
                       "ambient_strength": 0.12, "top_fill_strength": 0.55,
                       "light_intensity": 11.0, "light_radius": 0.22,
                       "secondary_diffuse_samples_3d": 2, "transmission_samples_3d": 1,
                       "object_audit_enabled": True, "object_audit_max_dimension": 128},
        "output": {"root": str(render_root), "overwrite": True},
        "progress": {"summary_path": str(render_root / "render_summary.json"),
                     "progress_path": str(render_root / "render_progress.json")},
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-root", type=Path, required=True)
    parser.add_argument("--generator", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--tier", default="high")
    args = parser.parse_args()
    out_root = args.out_root.resolve()
    asset_root = out_root / "assets" / "mesh_assets"
    asset_root.mkdir(parents=True, exist_ok=True)
    asset_ids: dict[str, str] = {}
    for family in ("analytic_sphere", "icosphere", "organic_blob", "crease"):
        asset_id = f"smooth_reflection_{family}_{args.tier}"
        asset_ids[family] = asset_id
        stl = out_root / "source" / f"{asset_id}.stl"
        authoring = out_root / "source" / f"{asset_id}.authoring.json"
        runtime = asset_root / f"{asset_id}.runtime.json"
        subprocess.run([str(args.generator), "--family", family, "--tier", args.tier,
                        "--output", str(stl), "--authoring-output", str(authoring)], check=True)
        subprocess.run([str(args.compiler), str(authoring), "/", asset_id, str(runtime)],
                       check=True)
    (out_root / "scene_runtime.json").write_text(
        json.dumps(build_scene(asset_ids), indent=2, sort_keys=True) + "\n", encoding="utf-8")
    for mode in ("smooth", "flat"):
        for route in ("tlas_blas_parity", "flattened_bvh"):
            request = build_request(out_root, mode, route)
            (out_root / f"request_{mode}_{route}.json").write_text(
                json.dumps(request, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(out_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
