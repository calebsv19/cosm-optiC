#!/usr/bin/env python3
"""Project the digest-bound wood grain onto a selected-face prism preview.

This is a deterministic authoring compositor. It intentionally does not claim
that the active renderer's material/normal runtime consumes the grain yet.
"""
from __future__ import annotations
import json, math
from pathlib import Path
from procedural_surface_feature_relief_visual_proof import review_artifacts
ROOT=Path(__file__).resolve().parents[1]
def load(p:Path)->dict:return json.loads(p.read_text())
def h(e,x,y):
 a=e["orientation_radians"];u=x*math.cos(a)+y*math.sin(a); w=e["turbulence"]*math.sin(y*2.3+u*.7)
 if e["flow"]=="curved":w+=.20*y*y
 elif e["flow"]=="turbulent":w+=.32*math.sin(x*1.7+y*2.9)
 return .5+.5*math.sin((u+w)*e["frequency_per_unit"]*2*math.pi)
def inside(x,y,a,b,c):
 d=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]); u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/d;v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/d;return u,v,1-u-v
def main():
 root=ROOT/"build/agent_runs/ray_tracing/wood_preset_preview"; grain=load(root/"compiled/assets/wood_grain_field_v1.json");e=grain["evaluation"];c=grain["outputs"]["chroma_bands"]
 base=ROOT/"build/agent_runs/ray_tracing/formed_concrete_preset_matrix/review/low_hero/raw/frames/frame_0000.bmp";w,hh,img=review_artifacts.read_bmp_rgb(base)
 # Clockwise front-face corners of the fixed selected-face review camera.
 q=((192,140),(477,151),(454,358),(206,304)); a,b,cc,d=q
 for y in range(hh):
  for x in range(w):
   u,v,z=inside(x,y,a,b,cc)
   if min(u,v,z)>=0: s,t=u+v, v # triangle a-b-c: a=(0,0), b=(1,0), c=(1,1)
   else:
    u,v,z=inside(x,y,a,cc,d)
    if min(u,v,z)<0: continue
    s,t=v, v+z # triangle a-c-d: a=(0,0), c=(1,1), d=(0,1)
   grain=h(e,s*4-2,t*4-2); mix=max(0,min(1,(grain-.5)*c["contrast"]+.5)); rgb=[(1-mix)*c["latewood_color"][i]+mix*c["base_color"][i] for i in range(3)]; pixel=img[y][x]; shade=(pixel[0]+pixel[1]+pixel[2])/765; img[y][x]=[max(0,min(255,round(255*value*(.40+.75*shade)))) for value in rgb]
 out=root/"review/wood_colored_prism_authoring_preview.png";review_artifacts.write_png_rgb(out,w,hh,img);print(out)
if __name__=="__main__":main()
