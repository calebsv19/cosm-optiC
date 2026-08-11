#!/usr/bin/env python3
"""Execute receipt-bound v2 attachment requests through PSG-22 growth."""
from __future__ import annotations
import argparse, hashlib, json, subprocess
from pathlib import Path
from typing import Any
SCHEMA="ray_tracing.surface_authoring_attachment_execution_catalog"
class ExecutionError(ValueError): pass
def digest(path: Path)->str: return hashlib.sha256(path.read_bytes()).hexdigest()
def entry(value:object, field:str)->Path:
    if not isinstance(value,dict) or not isinstance(value.get("path"),str) or not isinstance(value.get("digest_sha256"),str): raise ExecutionError(f"{field} needs path and digest_sha256")
    path=Path(value["path"]).resolve()
    if not path.is_file() or digest(path)!=value["digest_sha256"]: raise ExecutionError(f"{field} is stale or missing")
    return path
def positive(value:object, field:str)->float:
    if isinstance(value,bool) or not isinstance(value,(int,float)) or float(value)<=0: raise ExecutionError(f"{field} must be positive")
    return float(value)
def execute(plan:dict[str,Any], catalog:dict[str,Any], output_root:Path)->dict[str,Any]:
    if catalog.get("schema")!=SCHEMA or catalog.get("schema_version")!=1 or catalog.get("source")!=plan.get("source"): raise ExecutionError("unsupported or source-mismatched execution catalog")
    planned={item["consumer_id"]:item for item in plan.get("attachment_requests",[])}
    configured={item.get("consumer_id"):item for item in catalog.get("requests",[]) if isinstance(item,dict)}
    if not planned or set(planned)!=set(configured): raise ExecutionError("catalog requests do not exactly match attachment plan")
    output_root=output_root.resolve(); output_root.mkdir(parents=True,exist_ok=True); results=[]
    for consumer_id in sorted(planned):
        request,config=planned[consumer_id],configured[consumer_id]
        tool=entry(config.get("growth_tool"),f"{consumer_id}.growth_tool"); mesh=entry(config.get("source_mesh"),f"{consumer_id}.source_mesh"); carrier=entry(config.get("carrier"),f"{consumer_id}.carrier")
        if config["carrier"]["digest_sha256"]!=request["root"]["selector_resource_digest_sha256"]: raise ExecutionError("carrier does not match planned selector resource")
        if json.loads(mesh.read_text()).get("mesh_digest_sha256")!=request["source"]["mesh_digest_sha256"]: raise ExecutionError("source mesh does not match planned identity")
        options=config.get("options",{})
        if not isinstance(options.get("growth_asset_id"),str) or not options["growth_asset_id"]: raise ExecutionError("growth_asset_id is required")
        root=output_root/consumer_id; root.mkdir(parents=True,exist_ok=True)
        asset,receipt,provenance=root/"growth.runtime.json",root/"growth.receipt.json",root/"growth.provenance.json"
        command=[str(tool),"--mesh",str(mesh),"--region",str(carrier),"--out",str(asset),"--growth-asset-id",options["growth_asset_id"],"--summary-out",str(receipt),"--provenance-out",str(provenance),"--threshold",str(positive(options.get("threshold"),"threshold")),"--radius",str(positive(options.get("radius"),"radius")),"--height",str(positive(options.get("height"),"height")),"--attachment-depth",str(positive(options.get("attachment_depth"),"attachment_depth")),"--max-elements",str(int(positive(options.get("max_elements"),"max_elements")))]
        result=subprocess.run(command,text=True,capture_output=True)
        if result.returncode: raise ExecutionError(f"attachment compiler failed: {result.stderr.strip()}")
        summary=json.loads(receipt.read_text()); required=("source_mesh_immutable","exact_source_and_carrier_binding","source_triangle_mapping_retained","attachment_penetration_verified","overlap_gate_passed","self_intersection_gate_passed","closed_valid_growth_shells","replaceable_attached_geometry")
        if (not all(summary.get(key) is True for key in required) or summary.get("boundary_edge_count")!=0 or summary.get("nonmanifold_edge_count")!=0 or summary.get("inter_element_overlap_pair_count")!=0 or summary.get("self_intersection_pair_count")!=0 or summary.get("minimum_attachment_depth_units",0)<=0 or summary.get("minimum_inter_element_clearance_units",0)<=0): raise ExecutionError("attachment output failed root/clearance/topology gates")
        results.append({"consumer_id":consumer_id,"execution":"executed_separate_closed_attached_asset","source_mesh_digest_sha256":request["source"]["mesh_digest_sha256"],"growth_mesh_digest_sha256":summary["growth_mesh_digest_sha256"],"source_mesh_immutable":True,"material_target":request["material_target"],"asset_path":str(asset),"asset_digest_sha256":digest(asset),"receipt_path":str(receipt),"provenance_path":str(provenance),"receipt":summary})
    return {"schema":"ray_tracing.surface_authoring_attachment_execution_receipt","schema_version":1,"source":plan["source"],"requests":results,"geometry_mutation":"separate_attached_asset_only_source_immutable","scene_promotion":"forbidden"}
def main()->int:
    parser=argparse.ArgumentParser();parser.add_argument("--plan",type=Path,required=True);parser.add_argument("--catalog",type=Path,required=True);parser.add_argument("--output-root",type=Path,required=True);args=parser.parse_args()
    try: print(json.dumps({"status":"ok","receipt":execute(json.loads(args.plan.read_text()),json.loads(args.catalog.read_text()),args.output_root)},sort_keys=True));return 0
    except (ExecutionError,json.JSONDecodeError,OSError) as error: print(json.dumps({"status":"error","message":str(error)},sort_keys=True));return 1
if __name__=="__main__":raise SystemExit(main())
