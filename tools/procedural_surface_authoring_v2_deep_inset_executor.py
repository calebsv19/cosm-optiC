#!/usr/bin/env python3
"""Execute receipt-bound v2 deep-inset requests through the PSG-24C compiler."""
from __future__ import annotations
import argparse, hashlib, json, subprocess
from pathlib import Path
from typing import Any

SCHEMA = "ray_tracing.surface_authoring_deep_inset_execution_catalog"

class ExecutionError(ValueError): pass

def digest(path: Path) -> str: return hashlib.sha256(path.read_bytes()).hexdigest()
def entry(value: object, field: str) -> Path:
    if not isinstance(value, dict) or not isinstance(value.get("path"), str) or not isinstance(value.get("digest_sha256"), str):
        raise ExecutionError(f"{field} needs path and digest_sha256")
    path = Path(value["path"]).resolve()
    if not path.is_file() or digest(path) != value["digest_sha256"]: raise ExecutionError(f"{field} is stale or missing")
    return path
def positive(value: object, field: str, zero: bool = False) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or (float(value) < 0 if zero else float(value) <= 0):
        raise ExecutionError(f"{field} is out of range")
    return float(value)

def execute(plan: dict[str, Any], catalog: dict[str, Any], output_root: Path) -> dict[str, Any]:
    if catalog.get("schema") != SCHEMA or catalog.get("schema_version") != 1 or catalog.get("source") != plan.get("source"):
        raise ExecutionError("unsupported or source-mismatched execution catalog")
    planned = {item["consumer_id"]: item for item in plan.get("deep_inset_requests", [])}
    configured = {item.get("consumer_id"): item for item in catalog.get("requests", []) if isinstance(item, dict)}
    if not planned or set(planned) != set(configured): raise ExecutionError("catalog requests do not exactly match deep inset plan")
    output_root = output_root.resolve(); output_root.mkdir(parents=True, exist_ok=True); results = []
    for consumer_id in sorted(planned):
        request, config = planned[consumer_id], configured[consumer_id]
        compiler = entry(config.get("compiler"), f"{consumer_id}.compiler")
        selection_tool = entry(config.get("selection_tool"), f"{consumer_id}.selection_tool")
        inset_tool = entry(config.get("inset_tool"), f"{consumer_id}.inset_tool")
        mesh = entry(config.get("source_mesh"), f"{consumer_id}.source_mesh")
        base_region = entry(config.get("base_region"), f"{consumer_id}.base_region")
        field = Path(request["feature_field_path"]).resolve()
        if not field.is_file() or digest(field) != config.get("feature_field_digest_sha256"):
            raise ExecutionError("resolved deep-inset feature field is stale")
        mesh_json = json.loads(mesh.read_text())
        if mesh_json.get("mesh_digest_sha256") != request["source"]["mesh_digest_sha256"]:
            raise ExecutionError("source mesh does not match planned mesh identity")
        options = config.get("options", {})
        required = ("derived_asset_id", "region_id")
        if any(not isinstance(options.get(key), str) or not options[key] for key in required): raise ExecutionError("deep-inset identity options are required")
        root = output_root / consumer_id
        command = [str(compiler), "--selection-tool", str(selection_tool), "--inset-tool", str(inset_tool),
                   "--mesh", str(mesh), "--field", str(field), "--base-region", str(base_region),
                   "--feature-ids", ",".join(map(str, request["feature_ids"])), "--out-root", str(root),
                   "--derived-asset-id", options["derived_asset_id"], "--region-id", options["region_id"],
                   "--threshold", str(positive(options.get("threshold"), "threshold")),
                   "--depth", str(positive(options.get("depth"), "depth")),
                   "--depth-variation", str(positive(options.get("depth_variation"), "depth_variation", True)),
                   "--minimum-component-triangles", str(int(positive(options.get("minimum_component_triangles"), "minimum_component_triangles")))]
        result = subprocess.run(command, text=True, capture_output=True)
        if result.returncode: raise ExecutionError(f"deep inset compiler failed: {result.stderr.strip()}")
        receipt_path = root / "receipts/surface_feature_inset.receipt.json"; receipt = json.loads(receipt_path.read_text())
        provenance_path = root / "provenance/surface_feature_inset.provenance.json"; provenance = json.loads(provenance_path.read_text())
        roles = {item["role"] for item in provenance.get("triangles", [])}
        if (receipt.get("source_mesh_digest_sha256") != request["source"]["mesh_digest_sha256"] or
            receipt.get("derived_mesh_digest_sha256") == request["source"]["mesh_digest_sha256"] or
            receipt.get("unselected_moved_vertex_count") != 0 or not receipt.get("closed_manifold_positive_volume") or
            roles != {"retained_surface", "transition_wall", "inset_floor"}):
            raise ExecutionError("deep inset output failed source/topology/provenance gates")
        results.append({"consumer_id": consumer_id, "execution": "executed_distinct_retained_wall_floor_shell",
                        "source_mesh_digest_sha256": receipt["source_mesh_digest_sha256"], "derived_mesh_digest_sha256": receipt["derived_mesh_digest_sha256"],
                        "source_mesh_immutable": True, "topology_roles": sorted(roles), "receipt_path": str(receipt_path),
                        "provenance_path": str(provenance_path), "receipt": receipt})
    return {"schema": "ray_tracing.surface_authoring_deep_inset_execution_receipt", "schema_version": 1,
            "source": plan["source"], "requests": results, "geometry_mutation": "derived_shell_only_source_immutable", "scene_promotion": "forbidden"}
def main() -> int:
    parser=argparse.ArgumentParser(); parser.add_argument("--plan",type=Path,required=True); parser.add_argument("--catalog",type=Path,required=True); parser.add_argument("--output-root",type=Path,required=True); args=parser.parse_args()
    try: print(json.dumps({"status":"ok","receipt":execute(json.loads(args.plan.read_text()),json.loads(args.catalog.read_text()),args.output_root)},sort_keys=True)); return 0
    except (ExecutionError,json.JSONDecodeError,OSError) as error: print(json.dumps({"status":"error","message":str(error)},sort_keys=True)); return 1
if __name__ == "__main__": raise SystemExit(main())
