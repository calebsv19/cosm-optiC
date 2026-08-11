#!/usr/bin/env python3
"""End-to-end v2 signed-relief request -> distinct-shell execution contract."""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from procedural_surface_feature_relief_visual_proof import compile_shell, create_feature_field

EXECUTOR = ROOT / "tools/procedural_surface_authoring_v2_signed_relief_executor.py"
ASSET_TOOL = ROOT / "build/toolchains/clang/arm64/tools/cli/procedural_surface_field_preset_asset_tool"
GRAPH = ROOT / "tests/fixtures/procedural_surface_field_presets/pitted_concrete.json"
BINDING = ROOT / "tests/fixtures/procedural_surface_feature_relief_psg24r/positive_y.binding.json"
BASE_RECIPE = ROOT / "tests/fixtures/procedural_surface_rock_prism_psg0/recipe.json"


def entry(path: Path) -> dict:
    return {"path": str(path.resolve()), "digest_sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


with tempfile.TemporaryDirectory(prefix="surface_authoring_v2_signed_relief_") as temporary:
    root = Path(temporary)
    control = compile_shell(ASSET_TOOL, GRAPH, BINDING, BASE_RECIPE,
                            root / "control", "v2_relief_source", 0.0)
    field = root / "signed.field.json"
    create_feature_field(control["paths"]["asset"], control["mesh_digest_sha256"], field)
    source = {"object_id": "v2_relief_source", "mesh_digest_sha256": control["mesh_digest_sha256"]}
    plan = {"source": source, "signed_relief_requests": [{
        "consumer_id": "relief_request", "source": source,
        "signed_relief_recipe": {"surface_feature_field_path": str(field.resolve())}}]}
    catalog = {"schema": "ray_tracing.surface_authoring_signed_relief_execution_catalog",
               "schema_version": 1, "source": source, "requests": [{
        "consumer_id": "relief_request", "asset_tool": entry(ASSET_TOOL),
        "graph": entry(GRAPH), "binding": entry(BINDING), "base_recipe": entry(BASE_RECIPE),
        "feature_field_digest_sha256": entry(field)["digest_sha256"],
        "options": {"derived_asset_id": "v2_relief_derived", "source_asset_id": "v2_relief_source",
                    "selected_face": "positive_y", "width": 6.0, "height": 0.5, "depth": 4.0,
                    "target_edge": 0.10, "amplitude": 0.10, "edge_lock": 0.28, "relief_scale": 1.0}}]}
    plan_path, catalog_path = root / "plan.json", root / "catalog.json"
    plan_path.write_text(json.dumps(plan), encoding="utf-8")
    catalog_path.write_text(json.dumps(catalog), encoding="utf-8")
    result = subprocess.run([sys.executable, str(EXECUTOR), "--plan", str(plan_path),
                             "--catalog", str(catalog_path), "--output-root", str(root / "derived")],
                            text=True, capture_output=True, check=True)
    receipt = json.loads(result.stdout)["receipt"]["requests"][0]
    assert receipt["execution"] == "executed_distinct_derived_shell"
    assert receipt["source_mesh_digest_sha256"] != receipt["derived_mesh_digest_sha256"]
    assert receipt["source_mesh_immutable"] is True
    assert receipt["topology"] == {"boundary_edge_count": 0, "connected_component_count": 1,
                                   "euler_characteristic": 2}
    assert receipt["signed_feature_relief"]["feature_source_identity_bound"] is True
    assert receipt["signed_feature_relief"]["one_coherent_derived_shell"] is True

print("surface_authoring_v2_signed_relief_execution derived=ok source=immutable topology=ok")
