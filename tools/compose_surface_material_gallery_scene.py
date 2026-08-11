#!/usr/bin/env python3
"""Compose an editable LineDrawing runtime scene with explicit surface bindings.

The LineDrawing request remains the layout source. This adapter only creates a
disposable RayTracing scene runtime payload, adding digest-bound procedural
material references and the inspection lights/camera for a local proof render.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def digest_json(value: dict) -> str:
    payload = json.dumps(value, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(payload).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--line-runtime", type=Path, required=True)
    parser.add_argument("--bindings", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    scene = read_json(args.line_runtime)
    bindings_doc = read_json(args.bindings)
    bindings = {entry["object_id"]: entry for entry in bindings_doc["bindings"]}

    for obj in scene.get("objects", []):
        object_id = obj.get("object_id")
        binding = bindings.get(object_id)
        if not binding:
            continue
        if isinstance(obj.get("geometry_ref"), dict):
            obj["geometry_ref"]["id"] = binding.get(
                "mesh_asset_id", obj["geometry_ref"].get("id"))
        if object_id == "wood_feature_wall":
            ref = dict(binding["procedural_solid_material_ref"])
            obj["procedural_solid_material_ref"] = ref
            obj.setdefault("extensions", {})["line_drawing"] = {
                "geometry_source": "mesh_asset_instance",
                "runtime_mesh_path": str(
                    Path("/Users/calebsv/Desktop/CodeWork/_worktrees/ray_tracing_procedural_surface_geometry_main_integration/build/agent_runs/ray_tracing/wood_natural_runtime_v2/control/runtime_mesh.json")
                ),
                "mesh_asset_id": binding["mesh_asset_id"],
                "source_lane": "surface_material_gallery_v1",
            }
        elif object_id == "concrete_feature_wall":
            obj["procedural_surface_ref"] = {
                "source_authority": "formed_concrete_preset_v1",
                "derived_asset_policy": "replaceable_cache",
                "manifest_path": binding["derived_asset_path"],
                "field_asset_path": binding["surface_feature_field_path"],
                "material_artifact_path": binding["material_artifact_path"],
                "mesh_digest_sha256": binding["mesh_digest_sha256"],
            }
            obj.setdefault("extensions", {})["line_drawing"] = {
                "geometry_source": "mesh_asset_instance",
                "runtime_mesh_path": binding.get(
                    "runtime_mesh_path",
                    str(Path(binding["derived_asset_path"]).with_name("runtime_mesh.json")),
                ),
                "mesh_asset_id": binding["mesh_asset_id"],
                "source_lane": "surface_material_gallery_v1",
            }

    scene["scene_id"] = "surface_material_gallery_v1_ray_tracing"
    scene["source_scene_id"] = "surface_material_gallery_v1"
    scene["materials"] = [
        {"material_id": "mat_gallery_neutral", "kind": "lambert", "albedo": [0.26, 0.27, 0.29]},
        {"material_id": "mat_wood_feature", "kind": "lambert", "albedo": [0.40, 0.22, 0.10]},
        {"material_id": "mat_concrete_feature", "kind": "lambert", "albedo": [0.72, 0.65, 0.52]},
    ]
    scene["lights"] = [
        {"light_id": "gallery_key", "kind": "point", "position": {"x": -4.5, "y": -2.0, "z": 6.5}, "intensity": 10.0, "radius": 0.0},
        {"light_id": "gallery_fill", "kind": "point", "position": {"x": 5.0, "y": 1.0, "z": 3.5}, "intensity": 4.0, "radius": 0.0},
        {"light_id": "gallery_rim", "kind": "point", "position": {"x": 0.0, "y": 5.0, "z": 4.5}, "intensity": 3.0, "radius": 0.0},
    ]
    scene["cameras"] = [
        {"camera_id": "gallery_entry", "kind": "perspective", "position": {"x": 0.0, "y": -10.5, "z": 3.0}, "target": {"x": 0.0, "y": 3.65, "z": 2.0}}
    ]
    scene.setdefault("extensions", {})["ray_tracing"] = {
        "authoring": {
            "surface_material_bindings": str(args.bindings),
            "surface_material_bindings_digest_sha256": digest_json(bindings_doc),
            "camera_focus_target": {"x": 0.0, "y": 3.65, "z": 2.0},
            "environment": {"ambient_strength": 0.22, "light_mode": 2, "top_fill_strength": 0.55},
            "object_materials": [
                {"object_id": "wood_feature_wall", "material_id": 1, "object_color": 7029810, "reflectivity": 0.04, "roughness": 0.70},
                {"object_id": "concrete_feature_wall", "material_id": 2, "object_color": 12102284, "reflectivity": 0.03, "roughness": 0.80},
            ],
        }
    }
    write_json(args.out, scene)
    print(json.dumps({"scene": str(args.out), "bindings_digest_sha256": digest_json(bindings_doc), "objects": len(scene.get("objects", []))}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
