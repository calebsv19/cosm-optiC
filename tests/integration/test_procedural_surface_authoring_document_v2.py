#!/usr/bin/env python3
"""No-render contract test for typed-port surface authoring v2."""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/procedural_surface_authoring_document_v2.py"
MATRIX_TOOL = ROOT / "tools/procedural_surface_authoring_v2_contract_matrix.py"
FIXTURE = ROOT / "tests/fixtures/procedural_surface_authoring_document_v2/five_lane_layered_object.json"
MATRIX = ROOT / "tests/fixtures/procedural_surface_authoring_document_v2/five_lane_contract_matrix.json"


def run(tool: Path, *arguments: object, ok: bool = True) -> dict:
    result = subprocess.run([sys.executable, str(tool), *map(str, arguments)],
                            text=True, capture_output=True)
    if (result.returncode == 0) != ok:
        raise AssertionError(result.stderr + result.stdout)
    return json.loads(result.stdout)


with tempfile.TemporaryDirectory(prefix="surface_authoring_v2_") as temporary:
    root = Path(temporary)
    created = root / "created.json"
    repeated = root / "repeated.json"
    created_result = run(TOOL, "create", "--input", FIXTURE, "--output", created)
    run(TOOL, "create", "--input", FIXTURE, "--output", repeated)
    readback = created_result["readback"]
    assert len(readback["resources"]) == 8
    assert len(readback["connections"]) == 12
    adapters = {item["adapter"] for item in readback["compile_plan"]["adapters"]}
    assert adapters == {"material_runtime_binding", "microdetail_field_binding",
                        "selector_carrier_binding", "attachment_compile_request",
                        "geometry_derived_asset_request"}
    order = readback["compile_plan"]["topological_node_ids"]
    assert order.index("upper_selector") < order.index("weather_weight") < order.index("material_output") < order.index("material_binding")
    assert created.read_text() == repeated.read_text()

    edited = root / "edited.json"
    old_digest = readback["document_digest_sha256"]
    changed = run(TOOL, "edit-resource", "--input", created, "--output", edited,
                  "--resource-id", "weather_scalar", "--resource-digest", "a" * 64,
                  "--expected-document-digest", old_digest)
    assert changed["undo_document_digest_sha256"] == old_digest
    assert changed["readback"]["document_digest_sha256"] != old_digest
    run(TOOL, "edit-resource", "--input", created, "--output", root / "bad.json",
        "--resource-id", "weather_scalar", "--resource-digest", "b" * 64,
        "--expected-document-digest", "0" * 64, ok=False)

    rewired = root / "rewired.json"
    rewire = run(TOOL, "edit-node-resource", "--input", created,
                  "--output", rewired, "--node-id", "upper_selector",
                  "--resource-id", "middle_carrier",
                  "--expected-document-digest", old_digest)
    selector = next(item for item in rewire["readback"]["nodes"]
                    if item["id"] == "upper_selector")
    assert selector["resource"] == "middle_carrier"
    carrier_adapter = next(item for item in rewire["readback"]["compile_plan"]["adapters"]
                           if item["adapter"] == "selector_carrier_binding")
    assert carrier_adapter["resource"]["id"] == "middle_carrier"
    run(TOOL, "edit-node-resource", "--input", created,
        "--output", root / "bad_rewire.json", "--node-id", "upper_selector",
        "--resource-id", "brass_material",
        "--expected-document-digest", old_digest, ok=False)

    duplicate_owner = json.loads(FIXTURE.read_text())
    next(item for item in duplicate_owner["nodes"]
         if item["id"] == "carrier_binding")["resource"] = "upper_carrier"
    duplicate_owner_path = root / "duplicate_owner.json"
    duplicate_owner_path.write_text(json.dumps(duplicate_owner), encoding="utf-8")
    rejected = run(TOOL, "inspect", "--input", duplicate_owner_path, ok=False)
    assert "consumer ports/resource" in rejected["message"]

    stale = run(TOOL, "check-staleness", "--input", created,
                "--observed-resources", json.dumps({"weather_scalar": "0" * 64}))
    assert not stale["staleness"]["valid"]
    assert set(stale["staleness"]["invalidated_consumer_ids"]) == {
        "attachment_request", "inset_request", "material_binding",
        "microdetail_binding", "relief_request"}

    first = run(MATRIX_TOOL, "--matrix", MATRIX)["receipt"]
    second = run(MATRIX_TOOL, "--matrix", MATRIX)["receipt"]
    assert first == second
    cells = {item["id"]: item for item in first["cells"]}
    assert set(cells) == {"selector_only", "material_only", "microdetail_only",
                          "attachment_only", "signed_relief_only", "deep_inset_only",
                          "material_microdetail", "selector_material",
                          "selector_attachment", "full_lanes"}
    assert cells["material_microdetail"]["adapter_ids"] == ["material_binding", "microdetail_binding"]
    assert cells["selector_attachment"]["adapter_ids"] == ["attachment_request", "carrier_binding"]
    assert len(cells["full_lanes"]["adapter_ids"]) == 6
    assert {item["domain"] for item in cells["full_lanes"]["adapters"]} == {
        "selector_mask", "material", "microdetail_normal", "attached_asset",
        "signed_relief", "deep_inset"}

    invalid = json.loads(FIXTURE.read_text())
    invalid["nodes"][2]["outputs"][0]["domains"] = ["microdetail_normal"]
    invalid_path = root / "invalid_domain.json"
    invalid_path.write_text(json.dumps(invalid), encoding="utf-8")
    rejected = run(TOOL, "inspect", "--input", invalid_path, ok=False)
    assert "domain_output ports" in rejected["message"]

    invalid_attachment = json.loads(FIXTURE.read_text())
    next(item for item in invalid_attachment["nodes"]
         if item["id"] == "attachment_request")["clearance_factor"] = 0.0
    invalid_attachment_path = root / "invalid_attachment.json"
    invalid_attachment_path.write_text(json.dumps(invalid_attachment), encoding="utf-8")
    rejected = run(TOOL, "inspect", "--input", invalid_attachment_path, ok=False)
    assert "clearance_factor" in rejected["message"]

    # Curve grooming is data-authored: top, ring, and side are distinct
    # selector carriers, not placement-mode branches in the executor.
    hair = json.loads(FIXTURE.read_text())
    groom = {
        "selection_threshold": 0.56, "strand_count": 50, "guide_count": 8,
        "points_per_strand": 9, "length": 0.54, "length_variation": 0.18,
        "root_radius": 0.01, "tip_radius": 0.0025, "root_penetration": 0.008,
        "lift": 1.0, "comb_direction": [0.0, -1.0, 0.0], "comb_strength": 0.35,
        "part_axis": [1.0, 0.0, 0.0], "part_strength": 0.35, "bend": 0.16,
        "curl": 0.035, "clump_strength": 0.55, "clump_tip_spread": 0.035,
        "seed": 23005,
    }
    source_digest = hair["source"]["mesh_digest_sha256"]
    for index, (placement, count) in enumerate((("top", 50), ("ring", 10),
                                                  ("side_clump", 50)), start=1):
        carrier_id, recipe_id = f"hair_{placement}_carrier", f"hair_{placement}_recipe"
        hair["resources"].extend([
            {"id": carrier_id, "kind": "selector_carrier", "digest_sha256": f"{index:02x}" * 32,
             "source_mesh_digest_sha256": source_digest, "output_domains": ["selector_mask"],
             "receipt": {"id": f"{carrier_id}_receipt", "digest_sha256": f"{index + 10:02x}" * 32}},
            {"id": recipe_id, "kind": "curve_groom_recipe", "digest_sha256": f"{index + 20:02x}" * 32,
             "source_mesh_digest_sha256": source_digest, "output_domains": ["curve_groom"],
             "receipt": {"id": f"{recipe_id}_receipt", "digest_sha256": f"{index + 30:02x}" * 32}},
        ])
        groom_value = {**groom, "strand_count": count, "guide_count": min(8, count),
                       "seed": groom["seed"] + index}
        hair["nodes"].extend([
            {"id": f"hair_{placement}_selector", "kind": "selector", "selector_name": placement,
             "resource": carrier_id, "inputs": [], "outputs": [{"name": "mask", "domains": ["selector_mask"]}]},
            {"id": f"hair_{placement}_weight", "kind": "scalar_field", "resource": "weather_scalar",
             "inputs": [{"name": "selector", "domains": ["selector_mask"]}],
             "outputs": [{"name": "value", "domains": ["scalar_field"]}]},
            {"id": f"hair_{placement}_output", "kind": "domain_output", "domain": "curve_groom",
             "inputs": [{"name": "value", "domains": ["scalar_field"]}],
             "outputs": [{"name": "output", "domains": ["curve_groom"]}]},
            {"id": f"hair_{placement}_request", "kind": "consumer",
             "adapter": "curve_groom_compile_request", "resource": recipe_id,
             "root_policy": "carrier_weighted_surface", "material_resource": "brass_material",
             "groom": groom_value,
             "inputs": [{"name": "input", "domains": ["curve_groom"]}],
             "outputs": [{"name": "request", "domains": ["curve_groom"]}]},
        ])
        hair["connections"].extend([
            {"from": {"node": f"hair_{placement}_selector", "port": "mask"},
             "to": {"node": f"hair_{placement}_weight", "port": "selector"}},
            {"from": {"node": f"hair_{placement}_weight", "port": "value"},
             "to": {"node": f"hair_{placement}_output", "port": "value"}},
            {"from": {"node": f"hair_{placement}_output", "port": "output"},
             "to": {"node": f"hair_{placement}_request", "port": "input"}},
        ])
    hair_path = root / "three_curve_grooms.json"
    hair_path.write_text(json.dumps(hair), encoding="utf-8")
    hair_readback = run(TOOL, "inspect", "--input", hair_path)["readback"]
    curve_requests = [item for item in hair_readback["compile_plan"]["adapters"]
                      if item["adapter"] == "curve_groom_compile_request"]
    assert [(item["consumer_id"], item["groom"]["strand_count"]) for item in curve_requests] == [
        ("hair_ring_request", 10), ("hair_side_clump_request", 50), ("hair_top_request", 50)]
    changed_hair = root / "hair_100.json"
    updated_groom = next(item for item in curve_requests if item["consumer_id"] == "hair_top_request")["groom"]
    updated_groom["strand_count"] = 100
    update = run(TOOL, "edit-curve-groom", "--input", hair_path, "--output", changed_hair,
                 "--node-id", "hair_top_request", "--groom-json", json.dumps(updated_groom),
                 "--expected-document-digest", hair_readback["document_digest_sha256"])
    assert update["undo_document_digest_sha256"] == hair_readback["document_digest_sha256"]
    top = next(item for item in update["readback"]["nodes"] if item["id"] == "hair_top_request")
    assert top["groom"]["strand_count"] == 100
    run(TOOL, "edit-curve-groom", "--input", hair_path, "--output", root / "stale_hair.json",
        "--node-id", "hair_top_request", "--groom-json", json.dumps(updated_groom),
        "--expected-document-digest", "0" * 64, ok=False)

print("surface_authoring_document_v2 ports=ok lanes=ok adapters=ok stale=ok matrix=ok curve_groom=top_ring_side")
