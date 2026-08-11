#!/usr/bin/env python3
"""No-render contract test for receipt-bound v2 attachment materialization."""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(ROOT := Path(__file__).resolve().parents[2] / "tools"))
from procedural_imported_surface_growth_visual_proof import load_runtime_binding, make_scene
from procedural_surface_visual_proof import render_request

ROOT = ROOT.parent
TOOL = ROOT / "tools/procedural_surface_authoring_v2_attachment_runtime_materializer.py"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def entry(path: Path) -> dict[str, str]:
    return {"path": str(path), "digest_sha256": digest(path)}


def run(receipt: Path, catalog: Path, output: Path, ok: bool = True) -> dict:
    result = subprocess.run([sys.executable, str(TOOL), "--execution-receipt", str(receipt),
                             "--catalog", str(catalog), "--output-root", str(output)],
                            text=True, capture_output=True)
    if (result.returncode == 0) != ok:
        raise AssertionError(result.stdout + result.stderr)
    return json.loads(result.stdout)


def material(identifier: str, **identity: str) -> dict:
    return {"id": identifier, "base_color": {"r": 0.42, "g": 0.31, "b": 0.16},
            "roughness": 0.71, "metallic": 0.05, **identity}


with tempfile.TemporaryDirectory(prefix="surface_authoring_v2_attachment_runtime_") as temporary:
    root = Path(temporary)
    source = root / "source.runtime.json"
    growth = root / "growth.runtime.json"
    source.write_text(json.dumps({"schema_variant": "mesh_asset_runtime_v1", "asset_id": "source_finial"}))
    growth.write_text(json.dumps({"schema_variant": "mesh_asset_runtime_v1", "asset_id": "growth_finial"}))
    target = {"resource_id": "brass_material", "resource_digest_sha256": "a" * 64,
              "receipt_digest_sha256": "b" * 64}
    execution = {"schema": "ray_tracing.surface_authoring_attachment_execution_receipt", "schema_version": 1,
                 "source": {"object_id": "finial", "mesh_digest_sha256": "c" * 64},
                 "requests": [{"consumer_id": "moss", "asset_path": str(growth.resolve()),
                               "asset_digest_sha256": digest(growth), "source_mesh_digest_sha256": "c" * 64,
                               "growth_mesh_digest_sha256": "d" * 64, "material_target": target}]}
    lighting = {"ambient_strength": 0.42, "environment_brightness": 0.42,
                "background_brightness": 0.08, "top_fill_strength": 0.5,
                "light_intensity": 1.36, "environment_light_mode": "ambient",
                "background_color": {"r": 0.075, "g": 0.085, "b": 0.095}}
    catalog = {"schema": "ray_tracing.surface_authoring_attachment_runtime_catalog", "schema_version": 1,
               "source": execution["source"], "requests": [{"consumer_id": "moss",
               "source_mesh": entry(source), "growth_mesh": entry(growth), "runtime": {
                   "source_object_id": "finial_source", "attachment_object_id": "finial_moss",
                   "source_material": material("weathered_stone"),
                   "attachment_material": material("brass_material", **target), "lighting": lighting}}]}
    receipt_path, catalog_path = root / "execution.json", root / "catalog.json"
    receipt_path.write_text(json.dumps(execution)); catalog_path.write_text(json.dumps(catalog))
    first = run(receipt_path, catalog_path, root / "out")["receipt"]
    second = run(receipt_path, catalog_path, root / "repeat")["receipt"]
    assert first["bindings"][0]["binding"] == second["bindings"][0]["binding"]
    binding = first["bindings"][0]["binding"]
    binding_path = Path(first["bindings"][0]["binding_path"])
    assert load_runtime_binding(
        binding_path, first["bindings"][0]["binding_digest_sha256"]) == binding
    assert binding["source"]["asset_id"] == "source_finial"
    assert binding["attachment"]["asset_id"] == "growth_finial"
    assert binding["attachment"]["material"]["resource_id"] == "brass_material"
    assert binding["lighting"]["environment_light_mode"] == "ambient"
    scene = make_scene("bound", True, binding)
    assert [item["object_id"] for item in scene["objects"]] == ["finial_source", "finial_moss"]
    assert scene["objects"][1]["material_ref"]["id"] == "brass_material"
    request = render_request("bound", {"id": "bound", "camera_position": {"x": 1, "y": 1, "z": 1},
                                        "camera_look_at": {"x": 0, "y": 0, "z": 0}},
                             root / "scene.json", root / "request.json", root / "raw",
                             {"render": {"width": 1, "height": 1, "temporal_frames": 1, "integrator_3d": "disney_v2", "camera_zoom": 1},
                              "lighting": binding["lighting"]})
    assert request["inspection"]["ambient_strength"] == 0.42
    assert request["inspection"]["light_intensity"] == 1.36
    catalog["requests"][0]["runtime"]["attachment_material"]["resource_id"] = "wrong"
    catalog_path.write_text(json.dumps(catalog))
    rejected = run(receipt_path, catalog_path, root / "bad", ok=False)
    assert "does not match planned material target" in rejected["message"]

print("surface_authoring_v2_attachment_runtime_materializer binding=ok material=guarded lighting=normalized")
