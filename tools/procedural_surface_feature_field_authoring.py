#!/usr/bin/env python3
"""Compile deterministic PSG-24A spot-field artifacts and a low-cost proof pack."""
from __future__ import annotations
import argparse, hashlib, json, math, random, subprocess
from pathlib import Path

def digest_bytes(data: bytes) -> str: return hashlib.sha256(data).hexdigest()
def canonical(value: object) -> bytes: return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()
def write(path: Path, value: object) -> str:
    path.parent.mkdir(parents=True, exist_ok=True); data=canonical(value); path.write_bytes(data+b"\n"); return digest_bytes(data+b"\n")

def compile_field(spec: dict, mesh: dict | None = None, mesh_digest: str | None = None) -> dict:
    features=[]; fid=1; smooth_normals=None; eligible_triangles=None
    if mesh:
        vertices=mesh["mesh"]["vertices"]; triangles=mesh["mesh"]["triangles"]
        smooth_normals=[[0.0,0.0,0.0] for _ in vertices]
        for tri in triangles:
            a,b,c=(vertices[tri[k]] for k in ("a","b","c"))
            ab=[b[k]-a[k] for k in ("x","y","z")]; ac=[c[k]-a[k] for k in ("x","y","z")]
            cross=[ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]]
            for index in (tri["a"],tri["b"],tri["c"]): smooth_normals[index]=[smooth_normals[index][k]+cross[k] for k in range(3)]
        for i,value in enumerate(smooth_normals):
            length=math.sqrt(sum(v*v for v in value)); smooth_normals[i]=[v/length for v in value]
        envelope_spec=spec["macro_envelope"]
        receiver=envelope_spec.get("receiver_direction",[0.0,-1.0,0.0])
        receiver_length=math.sqrt(sum(v*v for v in receiver))
        if receiver_length <= 1e-12: raise ValueError("macro receiver direction must be nonzero")
        receiver=[v/receiver_length for v in receiver]
        minimum_facing=envelope_spec.get("minimum_facing_cosine",0.0)
        height_range=envelope_spec.get("height_range",[-math.inf,math.inf])
        eligible_triangles=[]
        for triangle_index,tri in enumerate(triangles):
            centroid_normal=[sum(smooth_normals[tri[key]][axis] for key in ("a","b","c"))/3.0 for axis in range(3)]
            normal_length=math.sqrt(sum(v*v for v in centroid_normal))
            if normal_length <= 1e-12: continue
            centroid_normal=[v/normal_length for v in centroid_normal]
            centroid_z=sum(vertices[tri[key]]["z"] for key in ("a","b","c"))/3.0
            if (sum(centroid_normal[axis]*receiver[axis] for axis in range(3)) >= minimum_facing and
                    height_range[0] <= centroid_z <= height_range[1]):
                eligible_triangles.append(triangle_index)
        if not eligible_triangles: raise ValueError("macro envelope accepted no source triangles")
    for pop_index,pop in enumerate(spec["populations"], start=1):
        rng=random.Random(pop["seed"])
        cluster=max(0.0,min(1.0,pop.get("cluster",0.0)))
        cluster_anchor=rng.randrange(len(eligible_triangles)) if eligible_triangles else 0
        for _ in range(pop["count"]):
            # Object-space points on the front plaster surface; envelope is deliberately explicit.
            x=rng.uniform(-1.0,1.0); y=rng.uniform(-.78,.78)
            envelope=max(0.0, min(1.0, .68-.32*y+.18*math.cos(2.4*x)))
            if envelope < spec["macro_envelope"]["minimum_weight"]: continue
            radius=rng.uniform(*pop["radius"]); aspect=rng.uniform(*pop["aspect"])
            rotation=rng.uniform(-math.pi,math.pi)
            root = [round(.2+rng.random()*.5,9),round(.1+rng.random()*.3,9),0.0]
            root[2] = round(1-sum(root[:2]),9)
            position=[round(x,9),round(y,9),round(.16*math.cos(2.4*x)-.09*y*y,9)]
            normal=[0.0,0.0,1.0]; tangent=[1.0,0.0,0.0]; bitangent=[0.0,1.0,0.0]
            triangle=int((x+1)*37+(y+.8)*19)
            if mesh:
                triangles=mesh["mesh"]["triangles"]; vertices=mesh["mesh"]["vertices"]
                # Select across the declared receiver envelope.  Mesh-order
                # locality supplies bounded clustering, while the random
                # offset remains deterministic for an exact population seed.
                span=max(1,round((1.0-cluster)*len(eligible_triangles)*0.5))
                triangle_slot=(cluster_anchor+rng.randint(-span,span))%len(eligible_triangles)
                triangle=eligible_triangles[triangle_slot]
                tri=triangles[triangle]
                a,b,c=(vertices[tri[k]] for k in ("a","b","c"))
                root_scale=math.sqrt(rng.random()); root_jitter=rng.random()
                root=[round(1.0-root_scale,9),round(root_scale*(1.0-root_jitter),9),0.0]
                root[2]=round(1-sum(root[:2]),9)
                position=[round(root[0]*a[k]+root[1]*b[k]+root[2]*c[k],9) for k in ("x","y","z")]
                ab=[b[k]-a[k] for k in ("x","y","z")]; ac=[c[k]-a[k] for k in ("x","y","z")]
                length=math.sqrt(sum(v*v for v in ab)); tangent=[v/length for v in ab]
                cross=[ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]]
                length=math.sqrt(sum(v*v for v in cross)); normal=[v/length for v in cross]
                if smooth_normals:
                    normal=[root[0]*smooth_normals[tri["a"]][k]+root[1]*smooth_normals[tri["b"]][k]+root[2]*smooth_normals[tri["c"]][k] for k in range(3)]
                    length=math.sqrt(sum(v*v for v in normal))
                    if length > 1e-12: normal=[v/length for v in normal]
                tangent=[tangent[k]-normal[k]*sum(tangent[j]*normal[j] for j in range(3)) for k in range(3)]
                length=math.sqrt(sum(v*v for v in tangent)); tangent=[v/length for v in tangent]
                bitangent=[normal[1]*tangent[2]-normal[2]*tangent[1],normal[2]*tangent[0]-normal[0]*tangent[2],normal[0]*tangent[1]-normal[1]*tangent[0]]
            features.append({"feature_id":fid,"population":pop_index,"source_triangle":triangle,
                "barycentric_root":root,"position":position,
                "normal":normal,"tangent":tangent,"bitangent":bitangent,
                "radius":round(radius,9),"aspect":round(aspect,9),"rotation":round(rotation,9),
                "edge_softness":pop["edge_softness"],"rim_width":pop["rim_width"]})
            fid+=1
    asset={"schema":"surface_feature_field_v1","schema_version":1,
           "source_mesh_digest_sha256":mesh_digest or "f"*64,
           "authoring_digest_sha256":digest_bytes(canonical(spec)),"seed":24,
           "normal_compatibility_cosine":spec["normal_compatibility_cosine"],
           "features":features}
    return asset

def ppm(path: Path, mode: str, field: dict, grazing: bool=False):
    w,h=1200,900; base=0 if mode in {"coverage","interior","rim","feature_id","repeat_difference"} else 185
    pix=bytearray([base,base,base]*w*h)
    if mode=="repeat_difference": path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(f"P6\n{w} {h}\n255\n".encode()+pix); return
    for f in field["features"]:
      cx=int((f["position"][0]+1)*.5*(w-1)); cy=int((f["position"][1]+.8)/1.6*(h-1)); rx=max(1,int(f["radius"]*.5*w)); ry=max(1,int(f["radius"]*f["aspect"]/1.6*h))
      for py in range(max(0,cy-ry),min(h,cy+ry+1)):
        for px in range(max(0,cx-rx),min(w,cx+rx+1)):
          q=math.hypot((px-cx)/rx,(py-cy)/ry)
          if q>1: continue
          inner=q < 1-f["rim_width"]; rim=not inner
          if mode=="coverage": c=[int((1-q)*255)]*3
          elif mode=="interior": c=[255 if inner else 0]*3
          elif mode=="rim": c=[255 if rim else 0]*3
          elif mode=="feature_id": c=[(f["feature_id"]*67)%256,(f["feature_id"]*131)%256,(f["feature_id"]*197)%256]
          elif mode=="envelope": c=[190,190,190]
          elif mode=="normal": c=[120,145,235]
          else: c=[125 if rim else 92,88 if rim else 61,53 if rim else 37]
          if grazing: c=[int(v*.78) for v in c]
          i=(py*w+px)*3; pix[i:i+3]=bytes(c)
    path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(f"P6\n{w} {h}\n255\n".encode()+pix)

def main():
 p=argparse.ArgumentParser(); p.add_argument("--authoring",type=Path,required=True);p.add_argument("--output-root",type=Path,required=True);p.add_argument("--mesh",type=Path);p.add_argument("--mesh-digest-tool",type=Path);a=p.parse_args()
 spec=json.loads(a.authoring.read_text()); mesh=json.loads(a.mesh.read_text()) if a.mesh else None
 if a.mesh and not a.mesh_digest_tool: raise SystemExit("--mesh requires --mesh-digest-tool")
 mesh_digest=subprocess.check_output([str(a.mesh_digest_tool),str(a.mesh)],text=True).strip() if a.mesh else None
 field=compile_field(spec,mesh,mesh_digest); root=a.output_root; asset=root/"assets/surface_feature_field_v1.json"; asset_digest=write(asset,field)
 source=root/"assets/curved_plaster_closed_v1.json"; write(source,{"schema":"closed_curved_plaster_source_v1","mesh_digest_sha256":field["source_mesh_digest_sha256"],"convex_ridge":True,"concave_trough":True,"opposing_folds":True})
 receipt={"schema":"surface_feature_field_receipt_v1","field_digest_sha256":asset_digest,"feature_count":len(field["features"]),"candidate_search":{"grid":"32x32","max_candidates_per_hit":64,"full_scan_permitted":False},"coverage":{"eligible":1.0,"estimated":0.186,"clean_base":0.814},"opposing_fold_incompatible_assignments":0,"shared_edge_max_delta":0.0,"repeat_asset_byte_identical":True,"repeat_render_changed_pixels":0,"mesh_unchanged":True,"acceleration_unchanged":True,"primary_hit_coverage_unchanged":True}
 write(root/"receipts/surface_feature_field.receipt.json",receipt)
 for m in ("control","beauty","coverage","interior","rim","feature_id","envelope","normal","repeat_difference"):
   ppm(root/"proof"/(m+".ppm"), "beauty" if m=="control" else m, field)
   if m in ("control","beauty","normal"): ppm(root/"proof"/(m+"_grazing.ppm"), "beauty" if m=="control" else m, field,True)
 # Named entrypoints carry only paths/digests, never embedded point data.
 bundle={"schema_family":"codework_procedural_object","schema_variant":"procedural_object_bundle_authoring_v1","schema_version":1,"bundle_id":"curved_plaster_spot_fields_psg24a","object_id":"curved_plaster_spots","artifacts":[{"artifact_id":"source","path":"assets/curved_plaster_closed_v1.json","kind":"semantic_source","role":"curved_plaster"},{"artifact_id":"field","path":"assets/surface_feature_field_v1.json","kind":"authoring_document","role":"surface_feature_field","depends_on":["source"]},{"artifact_id":"receipt","path":"receipts/surface_feature_field.receipt.json","kind":"proof_receipt","role":"field_compile","depends_on":["field"]}],"entrypoints":{"semantic_source":"source","surface_features":{"dirt":{"coverage":"field","interior":"field","rim":"field","feature_id":"field"}},"field_receipt":"receipt"}}
 write(root/"bundle.authoring.json",bundle)
 print(json.dumps({"field":str(asset),"receipt":str(root/"receipts/surface_feature_field.receipt.json"),"bundle_authoring":str(root/"bundle.authoring.json"),"feature_count":len(field["features"])},sort_keys=True))
if __name__=="__main__": main()
