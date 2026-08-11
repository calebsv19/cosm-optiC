#!/usr/bin/env python3
"""Contract test for the V2-to-PSG-23E curve-groom executor adapter."""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/procedural_surface_authoring_v2_curve_groom_executor.py"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def entry(path: Path) -> dict[str, str]:
    return {"path": str(path), "digest_sha256": digest(path)}


def run(plan: Path, catalog: Path, output: Path, ok: bool = True) -> dict:
    result = subprocess.run([sys.executable, str(TOOL), "--plan", str(plan), "--catalog", str(catalog),
                             "--output-root", str(output)], text=True, capture_output=True)
    if (result.returncode == 0) != ok:
        raise AssertionError(result.stdout + result.stderr)
    return json.loads(result.stdout)


GROOM = {"selection_threshold": .56, "strand_count": 50, "guide_count": 8, "points_per_strand": 9,
         "length": .54, "length_variation": .18, "root_radius": .01, "tip_radius": .0025,
         "root_penetration": .008, "lift": 1., "comb_direction": [0., -1., 0.], "comb_strength": .35,
         "part_axis": [1., 0., 0.], "part_strength": .35, "bend": .16, "curl": .035,
         "clump_strength": .55, "clump_tip_spread": .035, "seed": 23005}


with tempfile.TemporaryDirectory(prefix="surface_authoring_v2_curve_groom_executor_") as temporary:
    root = Path(temporary)
    mesh, carrier, authoring, compiler = (root / "host.runtime.json", root / "top.region.json",
                                          root / "groom.authoring.json", root / "groom_tool.py")
    mesh.write_text(json.dumps({"mesh_digest_sha256": "a" * 64}))
    carrier.write_text("carrier")
    authoring.write_text(json.dumps({"asset_id": "top_hair", "groom": GROOM}))
    compiler.write_text('''import json, pathlib, sys\nargs=sys.argv[1:]\ndef val(flag): return pathlib.Path(args[args.index(flag)+1])\nif args[0] == "edit":\n d=json.loads(val("--input").read_text()); sets=args[args.index("--set"):]\n for i in range(0,len(sets),2):\n  if sets[i] == "--set":\n   k,v=sets[i+1].split("=",1); d["groom"][k.split(".",1)[1]]=json.loads(v)\n val("--output").write_text(json.dumps(d,sort_keys=True))\nelse:\n d=json.loads(val("--authoring").read_text()); val("--output").write_text(json.dumps({"schema_variant":"curve_asset_runtime_v1","asset_id":d["asset_id"]}))\n r={k:True for k in ["exact_source_and_carrier_binding","root_triangle_mapping_retained","root_barycentrics_valid","finite_positive_curve_asset","guide_assignment_complete","replaceable_serialized_curve_asset"]}; r["strand_count"]=d["groom"]["strand_count"]; val("--receipt").write_text(json.dumps(r,sort_keys=True))\n''')
    recipe = {"authoring_path": str(authoring.resolve()), "authoring_digest_sha256": digest(authoring),
              "groom_tool_path": str(compiler.resolve()), "groom_tool_digest_sha256": digest(compiler)}
    root_info = {"policy": "carrier_weighted_surface", "selector_resource_digest_sha256": digest(carrier)}
    request = {"consumer_id": "top_hair", "source": {"mesh_digest_sha256": "a" * 64},
               "curve_groom_recipe": recipe, "root": root_info, "groom": GROOM,
               "material_target": {"resource_id": "hair", "resource_digest_sha256": "b" * 64,
                                   "receipt_digest_sha256": "c" * 64}}
    plan = {"source": {"object_id": "host", "mesh_digest_sha256": "a" * 64},
            "curve_groom_requests": [request]}
    catalog = {"schema": "ray_tracing.surface_authoring_curve_groom_execution_catalog", "schema_version": 1,
               "source": plan["source"], "requests": [{"consumer_id": "top_hair",
                                                         "source_mesh": entry(mesh), "carrier": entry(carrier)}]}
    plan_path, catalog_path = root / "plan.json", root / "catalog.json"
    plan_path.write_text(json.dumps(plan)); catalog_path.write_text(json.dumps(catalog))
    first = run(plan_path, catalog_path, root / "out")["receipt"]
    second = run(plan_path, catalog_path, root / "repeat")["receipt"]
    item = first["requests"][0]
    assert item["source_mesh_immutable"] is True and item["receipt"]["strand_count"] == 50
    assert json.loads(Path(item["effective_authoring_path"]).read_text())["groom"]["strand_count"] == 50
    assert item["curve_asset_digest_sha256"] == second["requests"][0]["curve_asset_digest_sha256"]
    plan["curve_groom_requests"][0]["groom"]["strand_count"] = 100
    plan_path.write_text(json.dumps(plan))
    updated = run(plan_path, catalog_path, root / "hundred")["receipt"]["requests"][0]
    assert updated["receipt"]["strand_count"] == 100
    catalog["requests"][0]["carrier"]["digest_sha256"] = "0" * 64
    catalog_path.write_text(json.dumps(catalog))
    assert "stale or missing" in run(plan_path, catalog_path, root / "stale", ok=False)["message"]

print("surface_authoring_v2_curve_groom_executor count=50_to_100 repeat=ok source=immutable carrier=guarded")
