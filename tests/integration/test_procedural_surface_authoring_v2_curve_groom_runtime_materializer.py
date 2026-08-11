#!/usr/bin/env python3
"""No-render contract for V2 curve-groom runtime materialization."""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/procedural_surface_authoring_v2_curve_groom_runtime_materializer.py"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def entry(path: Path) -> dict[str, str]:
    return {"path": str(path), "digest_sha256": digest(path)}


def material(identifier: str, **identity: str) -> dict:
    return {"id": identifier, "base_color": {"r": 0.17, "g": 0.08, "b": 0.03},
            "roughness": 0.41, "metallic": 0.0, **identity}


def run(execution: Path, catalog: Path, output: Path, ok: bool = True) -> dict:
    result = subprocess.run([sys.executable, str(TOOL), "--execution-receipt", str(execution),
                             "--catalog", str(catalog), "--output-root", str(output)],
                            text=True, capture_output=True)
    if (result.returncode == 0) != ok:
        raise AssertionError(result.stdout + result.stderr)
    return json.loads(result.stdout)


with tempfile.TemporaryDirectory(prefix="surface_authoring_v2_curve_groom_runtime_") as temporary:
    root = Path(temporary)
    source, curve = root / "source.runtime.json", root / "hair.curve.runtime.json"
    source.write_text(json.dumps({"schema_variant": "mesh_asset_runtime_v1", "asset_id": "host"}))
    curve.write_text(json.dumps({"schema_variant": "curve_asset_runtime_v1", "asset_id": "top_hair"}))
    target = {"resource_id": "hair_material", "resource_digest_sha256": "a" * 64,
              "receipt_digest_sha256": "b" * 64}
    execution = {"schema": "ray_tracing.surface_authoring_curve_groom_execution_receipt",
                 "schema_version": 1, "source": {"object_id": "host", "mesh_digest_sha256": "c" * 64},
                 "requests": [{"consumer_id": "top_hair", "source_mesh_digest_sha256": "c" * 64,
                               "curve_asset_path": str(curve.resolve()), "curve_asset_digest_sha256": digest(curve),
                               "material_target": target}]}
    catalog = {"schema": "ray_tracing.surface_authoring_curve_groom_runtime_catalog", "schema_version": 1,
               "source": execution["source"], "requests": [{"consumer_id": "top_hair",
               "source_mesh": entry(source), "curve_asset": entry(curve), "runtime": {
                   "source_object_id": "host_surface", "curve_object_id": "host_top_hair",
                   "source_material": material("stone"),
                   "curve_material": material("hair_material", **target)}}]}
    execution_path, catalog_path = root / "execution.json", root / "catalog.json"
    execution_path.write_text(json.dumps(execution)); catalog_path.write_text(json.dumps(catalog))
    first = run(execution_path, catalog_path, root / "out")["receipt"]
    second = run(execution_path, catalog_path, root / "repeat")["receipt"]
    binding = first["bindings"][0]["binding"]
    assert binding == second["bindings"][0]["binding"]
    assert binding["source"]["object_id"] == "host_surface"
    assert binding["curve"]["object_id"] == "host_top_hair"
    assert binding["curve"]["object_type"] == "curve_asset_instance"
    assert binding["curve"]["material"]["roughness"] == 0.41
    assert binding["curve"]["material"]["metallic"] == 0.0
    catalog["requests"][0]["runtime"]["curve_material"]["resource_id"] = "wrong"
    catalog_path.write_text(json.dumps(catalog))
    assert "does not match planned material target" in run(
        execution_path, catalog_path, root / "bad", ok=False)["message"]

print("surface_authoring_v2_curve_groom_runtime_materializer curve=separate material=guarded scalars=preserved")
