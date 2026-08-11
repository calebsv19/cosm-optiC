#!/usr/bin/env python3
"""Create a labeled authoring preview of one wood grain/knot preset."""
from __future__ import annotations
import argparse, json, math, subprocess
from pathlib import Path
from procedural_surface_feature_relief_visual_proof import review_artifacts, write_labeled_contact_sheet
ROOT=Path(__file__).resolve().parents[1]
def load(p:Path)->dict:return json.loads(p.read_text())
def clamp(x:float)->int:return max(0,min(255,round(x*255)))
def height(e:dict,x:float,y:float)->float:
    a=e["orientation_radians"]; u=x*math.cos(a)+y*math.sin(a); flow=e["flow"]; warp=e["turbulence"]*math.sin(y*2.3+u*.7)
    if flow=="curved": warp+=.20*y*y
    elif flow=="turbulent": warp+=.32*math.sin(x*1.7+y*2.9)
    return .5+.5*math.sin((u+warp)*e["frequency_per_unit"]*2*math.pi)
def main()->int:
 p=argparse.ArgumentParser(description=__doc__);p.add_argument("--preset",type=Path,default=ROOT/"tests/fixtures/procedural_surface_wood_presets/oak_knots.json");p.add_argument("--output-root",type=Path,default=ROOT/"build/agent_runs/ray_tracing/wood_preset_preview");a=p.parse_args();root=a.output_root.resolve(); root.mkdir(parents=True,exist_ok=True)
 mesh=root/"preview_plane.json"; mesh.write_text(json.dumps({"mesh":{"vertices":[{"x":-2,"y":0,"z":-2},{"x":2,"y":0,"z":-2},{"x":-2,"y":0,"z":2},{"x":2,"y":0,"z":2}],"triangles":[{"a":0,"b":2,"c":1},{"a":1,"b":2,"c":3}]}}))
 subprocess.run(["python3",str(ROOT/"tools/procedural_surface_wood_preset.py"),"--preset",str(a.preset),"--mesh",str(mesh),"--source-mesh-digest","a"*64,"--output-root",str(root/"compiled")],check=True)
 grain=load(root/"compiled/assets/wood_grain_field_v1.json"); knots=load(root/"compiled/assets/surface_feature_field_v1.json"); e=grain["evaluation"]; c=grain["outputs"]["chroma_bands"]; w=h=640; chroma=[]; normal=[]; knots_img=[]
 for py in range(h):
  cr=[];nr=[];kr=[]; y=(py/(h-1)*4)-2
  for px in range(w):
   x=(px/(w-1)*4)-2; v=height(e,x,y); t=max(0,min(1,(v-.5)*c["contrast"]+0.5)); rgb=[(1-t)*c["latewood_color"][i]+t*c["base_color"][i] for i in range(3)]; cr.append(tuple(clamp(q) for q in rgb)); eps=.012; dx=(height(e,x+eps,y)-height(e,x-eps,y))/(2*eps);dy=(height(e,x,y+eps)-height(e,x,y-eps))/(2*eps); n=(-dx*grain["outputs"]["microdetail_height"]["normal_strength"],1,-dy*grain["outputs"]["microdetail_height"]["normal_strength"]); mag=math.sqrt(sum(q*q for q in n));nr.append(tuple(clamp(q/mag*.5+.5) for q in n)); value=[.30,.20,.12]
   for f in knots["features"]:
    d=math.hypot(x-f["position"][0],y-f["position"][2]);
    if d<f["radius"]: value=[.14,.07,.025] if f["height_or_depth"]<0 else [.55,.33,.13]
   kr.append(tuple(clamp(q) for q in value))
  chroma.append(cr);normal.append(nr);knots_img.append(kr)
 review=root/"review";review.mkdir(exist_ok=True)
 for name,img in (("chroma",chroma),("microdetail_normal",normal),("knot_relief_routing",knots_img)) : review_artifacts.write_png_rgb(review/f"{name}.png",w,h,img)
 write_labeled_contact_sheet(review/"wood_authoring_contract_matrix.png",[("SHARED GRAIN CHROMA",chroma),("SAME GRAIN: MICRODETAIL NORMAL",normal),("SIGNED KNOT RELIEF ROUTING",knots_img)],columns=2)
 (root/"summary.json").write_text(json.dumps({"schema":"wood_authoring_preview_v1","grain_digest_sha256":grain.get("preset_digest_sha256"),"same_grain_used_for":["chroma","microdetail_normal"],"knot_geometry":"separate signed PSG-18 input","runtime_consumers_executed":False,"matrix":"review/wood_authoring_contract_matrix.png"},sort_keys=True,indent=2)+"\n");print(review/"wood_authoring_contract_matrix.png")
if __name__=="__main__":main()
