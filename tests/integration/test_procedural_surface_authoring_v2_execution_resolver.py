#!/usr/bin/env python3
"""Receipt-bound no-render bridge test for executable v2 lanes."""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/procedural_surface_authoring_v2_execution_resolver.py"
AUTHORING_TOOL = ROOT / "tools/procedural_surface_authoring_document_v2.py"
FIXTURE = ROOT / "tests/fixtures/procedural_surface_authoring_document_v2/five_lane_layered_object.json"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(document: Path, catalog: Path, ok: bool = True) -> dict:
    result = subprocess.run([sys.executable, str(TOOL), "--document", str(document),
                             "--catalog", str(catalog)], text=True, capture_output=True)
    if (result.returncode == 0) != ok:
        raise AssertionError(result.stderr + result.stdout)
    return json.loads(result.stdout)


def edit_node_resource(document: Path, output: Path, node_id: str,
                       resource_id: str, expected_digest: str) -> dict:
    result = subprocess.run(
        [sys.executable, str(AUTHORING_TOOL), "edit-node-resource", "--input",
         str(document), "--output", str(output), "--node-id", node_id,
         "--resource-id", resource_id, "--expected-document-digest", expected_digest],
        text=True, capture_output=True)
    if result.returncode != 0:
        raise AssertionError(result.stderr + result.stdout)
    return json.loads(result.stdout)


def inspected_document_digest(path: Path) -> str:
    result = subprocess.run([sys.executable, str(AUTHORING_TOOL), "inspect",
                             "--input", str(path)], text=True, capture_output=True)
    if result.returncode != 0:
        raise AssertionError(result.stderr + result.stdout)
    return json.loads(result.stdout)["readback"]["document_digest_sha256"]


with tempfile.TemporaryDirectory(prefix="surface_authoring_v2_resolver_") as temporary:
    root = Path(temporary)
    document = json.loads(FIXTURE.read_text())
    source_digest = document["source"]["mesh_digest_sha256"]
    document["resources"].extend([
        {"id": "top_hair_carrier", "kind": "selector_carrier", "digest_sha256": "a" * 64,
         "source_mesh_digest_sha256": source_digest, "output_domains": ["selector_mask"],
         "receipt": {"id": "top_hair_carrier_receipt", "digest_sha256": "b" * 64}},
        {"id": "top_hair_recipe", "kind": "curve_groom_recipe", "digest_sha256": "c" * 64,
         "source_mesh_digest_sha256": source_digest, "output_domains": ["curve_groom"],
         "receipt": {"id": "top_hair_recipe_receipt", "digest_sha256": "d" * 64}},
    ])
    groom = {"selection_threshold": .56, "strand_count": 50, "guide_count": 8, "points_per_strand": 9,
             "length": .54, "length_variation": .18, "root_radius": .01, "tip_radius": .0025,
             "root_penetration": .008, "lift": 1., "comb_direction": [0., -1., 0.], "comb_strength": .35,
             "part_axis": [1., 0., 0.], "part_strength": .35, "bend": .16, "curl": .035,
             "clump_strength": .55, "clump_tip_spread": .035, "seed": 23005}
    document["nodes"].extend([
        {"id": "top_hair_selector", "kind": "selector", "selector_name": "top", "resource": "top_hair_carrier",
         "inputs": [], "outputs": [{"name": "mask", "domains": ["selector_mask"]}]},
        {"id": "top_hair_weight", "kind": "scalar_field", "resource": "weather_scalar",
         "inputs": [{"name": "selector", "domains": ["selector_mask"]}],
         "outputs": [{"name": "value", "domains": ["scalar_field"]}]},
        {"id": "top_hair_output", "kind": "domain_output", "domain": "curve_groom",
         "inputs": [{"name": "value", "domains": ["scalar_field"]}],
         "outputs": [{"name": "output", "domains": ["curve_groom"]}]},
        {"id": "top_hair_request", "kind": "consumer", "adapter": "curve_groom_compile_request",
         "resource": "top_hair_recipe", "root_policy": "carrier_weighted_surface",
         "material_resource": "brass_material", "groom": groom,
         "inputs": [{"name": "input", "domains": ["curve_groom"]}],
         "outputs": [{"name": "request", "domains": ["curve_groom"]}]},
    ])
    document["connections"].extend([
        {"from": {"node": "top_hair_selector", "port": "mask"}, "to": {"node": "top_hair_weight", "port": "selector"}},
        {"from": {"node": "top_hair_weight", "port": "value"}, "to": {"node": "top_hair_output", "port": "value"}},
        {"from": {"node": "top_hair_output", "port": "output"}, "to": {"node": "top_hair_request", "port": "input"}},
    ])
    catalog_resources = []
    inset_feature_field = None
    for resource in document["resources"]:
        artifact = root / f"{resource['id']}.artifact"
        receipt = root / f"{resource['id']}.receipt"
        artifact.write_text(f"artifact:{resource['id']}\n", encoding="utf-8")
        receipt.write_text(f"receipt:{resource['id']}\n", encoding="utf-8")
        resource["digest_sha256"] = digest(artifact)
        resource["receipt"]["digest_sha256"] = digest(receipt)
        item = {"id": resource["id"],
                "artifact": {"path": str(artifact), "digest_sha256": digest(artifact)},
                "receipt": {"path": str(receipt), "digest_sha256": digest(receipt)},
                "runtime": {}}
        if resource["kind"] == "material_graph":
            binding = root / "binding.json"; binding.write_text("binding\n")
            authored = root / "authored.json"; authored.write_text("authored\n")
            item["runtime"] = {
                "binding": {"path": str(binding), "digest_sha256": digest(binding)},
                "authored_binding": {"path": str(authored), "digest_sha256": digest(authored)},
                "graph": {"path": str(artifact), "digest_sha256": digest(artifact)},
            }
        elif resource["kind"] == "selector_carrier":
            item["runtime"] = {"surface_region": {"path": str(artifact), "digest_sha256": digest(artifact)}}
        elif resource["kind"] == "microdetail_field":
            item["runtime"] = {"surface_feature_field": {"path": str(artifact), "digest_sha256": digest(artifact)}}
        elif resource["kind"] == "signed_relief_recipe":
            relief_field = root / "pore_relief.features.json"
            relief_field.write_text(json.dumps({"features": [
                {"feature_id": 1, "height_or_depth": -0.02},
                {"feature_id": 2, "height_or_depth": 0.02}]}), encoding="utf-8")
            item["runtime"] = {
                "relief_recipe": {"path": str(artifact), "digest_sha256": digest(artifact)},
                "surface_feature_field": {"path": str(relief_field), "digest_sha256": digest(relief_field)}}
        elif resource["kind"] == "deep_inset_recipe":
            inset_feature_field = root / "chip_inset.features.json"
            inset_feature_field.write_text(json.dumps({"features": [
                {"feature_id": 17, "height_or_depth": -0.02},
                {"feature_id": 23, "height_or_depth": -0.04},
                {"feature_id": 31, "height_or_depth": 0.01}]}), encoding="utf-8")
            item["runtime"] = {
                "inset_recipe": {"path": str(artifact), "digest_sha256": digest(artifact)},
                "surface_feature_field": {"path": str(inset_feature_field), "digest_sha256": digest(inset_feature_field)}}
        elif resource["kind"] == "attachment_recipe":
            item["runtime"] = {"attachment_recipe": {"path": str(artifact), "digest_sha256": digest(artifact)}}
        elif resource["kind"] == "curve_groom_recipe":
            groom_tool = root / "groom_tool.py"
            groom_tool.write_text("# receipt-bound PSG-23E tool\n", encoding="utf-8")
            item["runtime"] = {
                "groom_authoring": {"path": str(artifact), "digest_sha256": digest(artifact)},
                "groom_tool": {"path": str(groom_tool), "digest_sha256": digest(groom_tool)},
            }
        catalog_resources.append(item)
    document_path = root / "document.json"
    catalog_path = root / "catalog.json"
    document_path.write_text(json.dumps(document), encoding="utf-8")
    catalog = {"schema": "ray_tracing.surface_authoring_execution_catalog", "schema_version": 1,
               "source": document["source"], "resources": catalog_resources}
    catalog_path.write_text(json.dumps(catalog), encoding="utf-8")

    first = run(document_path, catalog_path)["execution_plan"]
    second = run(document_path, catalog_path)["execution_plan"]
    assert first == second
    runtime = first["runtime_material_ref"]
    assert runtime["named_surface_selectors"] == [{
        "name": "upper", "surface_region_path": str((root / "upper_carrier.artifact").resolve()),
        "resource_id": "upper_carrier", "resource_digest_sha256": document["resources"][0]["digest_sha256"],
        "receipt_digest_sha256": document["resources"][0]["receipt"]["digest_sha256"]}]
    assert set(first["verified_resources"]) == {item["id"] for item in document["resources"]}
    assert first["signed_relief_requests"] == [{
        "consumer_id": "relief_request",
        "execution": "request_only_no_geometry_mutation",
        "executor": "procedural_surface_feature_relief_shell",
        "source": document["source"],
        "scalar_field": {
            "node_id": "weather_weight", "resource_id": "weather_scalar",
            "resource_digest_sha256": document["resources"][2]["digest_sha256"],
            "receipt_digest_sha256": document["resources"][2]["receipt"]["digest_sha256"]},
        "signed_relief_recipe": {
            "resource_id": "pore_relief", "path": str((root / "pore_relief.artifact").resolve()),
            "surface_feature_field_path": str((root / "pore_relief.features.json").resolve()),
            "resource_digest_sha256": document["resources"][6]["digest_sha256"],
            "receipt_digest_sha256": document["resources"][6]["receipt"]["digest_sha256"]}}]
    assert first["deep_inset_requests"][0]["consumer_id"] == "inset_request"
    assert first["deep_inset_requests"][0]["execution"] == "request_only_no_geometry_mutation"
    assert first["deep_inset_requests"][0]["executor"] == "procedural_surface_feature_inset_compiler"
    assert first["deep_inset_requests"][0]["feature_ids"] == [17, 23]
    assert first["deep_inset_requests"][0]["derived_asset_requirement"] == "distinct_closed_shell_required"
    assert first["deep_inset_requests"][0]["feature_field_path"] == str(inset_feature_field.resolve())
    attachment = first["attachment_requests"]
    assert attachment[0]["consumer_id"] == "attachment_request"
    assert attachment[0]["execution"] == "request_only_no_geometry_mutation"
    assert attachment[0]["executor"] == "procedural_imported_surface_growth"
    assert attachment[0]["root"]["policy"] == "carrier_weighted_surface"
    assert attachment[0]["root"]["selector_name"] == "upper"
    assert attachment[0]["clearance_factor"] == 1.25
    assert attachment[0]["material_target"]["resource_id"] == "brass_material"
    assert attachment[0]["derived_asset_requirement"] == "separate_closed_attached_asset_required"
    curve = first["curve_groom_requests"]
    assert curve[0]["consumer_id"] == "top_hair_request"
    assert curve[0]["executor"] == "procedural_carrier_curve_groom_authoring"
    assert curve[0]["root"]["selector_name"] == "top"
    assert curve[0]["groom"]["strand_count"] == 50
    assert curve[0]["groom"]["root_penetration"] > 0.0
    assert curve[0]["material_target"]["resource_id"] == "brass_material"
    assert curve[0]["derived_asset_requirement"] == "separate_serialized_curve_asset_required"

    rewired_path = root / "rewired.json"
    original_digest = inspected_document_digest(document_path)
    rewire = edit_node_resource(document_path, rewired_path, "upper_selector",
                                "middle_carrier", original_digest)
    assert rewire["readback"]["document_digest_sha256"] != original_digest
    rewired = run(rewired_path, catalog_path)["execution_plan"]
    rewired_selector = rewired["runtime_material_ref"]["named_surface_selectors"]
    assert rewired_selector == [{
        "name": "upper", "surface_region_path": str((root / "middle_carrier.artifact").resolve()),
        "resource_id": "middle_carrier", "resource_digest_sha256": document["resources"][1]["digest_sha256"],
        "receipt_digest_sha256": document["resources"][1]["receipt"]["digest_sha256"]}]

    duplicate_owner = json.loads(document_path.read_text())
    next(item for item in duplicate_owner["nodes"]
         if item["id"] == "carrier_binding")["resource"] = "upper_carrier"
    duplicate_owner_path = root / "duplicate_owner.json"
    duplicate_owner_path.write_text(json.dumps(duplicate_owner), encoding="utf-8")
    rejected = run(duplicate_owner_path, catalog_path, ok=False)
    assert "consumer ports/resource" in rejected["message"]

    (root / "pore_relief.artifact").write_text("tampered relief recipe\n", encoding="utf-8")
    rejected = run(document_path, catalog_path, ok=False)
    assert "artifact.digest_sha256 is stale" in rejected["message"]
    (root / "pore_relief.artifact").write_text("artifact:pore_relief\n", encoding="utf-8")

    inset_feature_field.write_text(json.dumps({"features": [
        {"feature_id": 17, "height_or_depth": 0.01},
        {"feature_id": 23, "height_or_depth": -0.04}]}), encoding="utf-8")
    next(item for item in catalog_resources if item["id"] == "chip_inset")["runtime"][
        "surface_feature_field"]["digest_sha256"] = digest(inset_feature_field)
    catalog_path.write_text(json.dumps(catalog), encoding="utf-8")
    rejected = run(document_path, catalog_path, ok=False)
    assert "deep inset requires explicitly negative feature values" in rejected["message"]

    (root / "weather_scalar.artifact").write_text("tampered\n", encoding="utf-8")
    rejected = run(document_path, catalog_path, ok=False)
    assert "artifact.digest_sha256 is stale" in rejected["message"]

print("surface_authoring_v2_execution_resolver paths=ok receipts=ok repeat=ok stale=ok")
