#!/usr/bin/env python3
"""Accepted compound metadata, presentation reuse and failed attachment rollback."""
import argparse,copy,json,os,shutil,subprocess
from pathlib import Path

def main():
 p=argparse.ArgumentParser()
 for n in ['scene','consumer','out']:p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);source=json.loads(a.scene.read_text())
 env=dict(os.environ,XDG_CONFIG_HOME=str(a.out/'config'),RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled',ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
 def call(*args,ok=True):
  r=subprocess.run([str(a.consumer),*map(str,args)],capture_output=True,text=True,env=env,timeout=180)
  assert (r.returncode==0)==ok,(args,r.returncode,r.stderr[-1000:]);return [json.loads(l[4:]) for l in r.stdout.splitlines() if l.startswith('BR4 ')]
 def standalone(s):
  s['extensions']['ray_tracing']['rigid_result']['bundle_path']=str((a.scene.parent/source['extensions']['ray_tracing']['rigid_result']['bundle_path']).resolve())
  for o in s['objects']:
   path=o['extensions']['line_drawing']['runtime_mesh_path'];o['extensions']['line_drawing']['runtime_mesh_path']=str((a.scene.parent/path).resolve())
  return s
 inspection=call(a.scene,'--inspection')[0]
 counts={b['object_id']:b['collision_child_count'] for b in inspection['bodies']}
 assert sorted(counts.values())==[2,3],counts
 baseline=call(a.scene,0,120,480,1,0)
 checks=['authenticated two/three child counts','random and reverse exact seeks']
 for name,change in [('source_frame',lambda s:s['extensions']['rigid_scene']['bindings'][0]['source_center_m'].__setitem__(0,2)),('collision_identity',lambda s:s['extensions']['rigid_scene']['bindings'][0].__setitem__('collision_sha256','0'*64)),('physical_mass',lambda s:s['extensions']['rigid_scene']['bindings'][0].__setitem__('mass_kg',8))]:
  s=standalone(copy.deepcopy(source));change(s)
  path=a.out/(name+'.json');path.write_text(json.dumps(s));assert call(a.scene,'--replacement',path)==[{'rollback':True}];checks.append(name+' refuses and preserves accepted poses')
 s=standalone(copy.deepcopy(source));s['cameras'][0]['position']['x']+=1
 for o in s['objects']:
  path=o['extensions']['line_drawing']['runtime_mesh_path'];o['extensions']['line_drawing']['runtime_mesh_path']=str((a.scene.parent/path).resolve())
 path=a.out/'camera.json';path.write_text(json.dumps(s));changed=call(path,0,120,480,1,0)
 assert [r['poses'] for r in changed]==[r['poses'] for r in baseline];checks.append('camera reuses sealed poses')
 bundle=(a.scene.parent/source['extensions']['ray_tracing']['rigid_result']['bundle_path']).resolve();target=a.out/'tampered';shutil.copytree(bundle,target)
 asset=json.loads((target/'input/assets/asset_set.json').read_text())['assets'][0]
 f=target/'input/assets'/asset['root']/'collision.bcr3d';f.write_bytes(f.read_bytes()+b'\n')
 s=standalone(copy.deepcopy(source));s['extensions']['ray_tracing']['rigid_result']['bundle_path']=str(target);path=a.out/'tampered.json';path.write_text(json.dumps(s));assert call(a.scene,'--replacement',path)==[{'rollback':True}];checks.append('tampered artifact refuses and preserves accepted poses')
 (a.out/'verification.json').write_text(json.dumps(dict(ok=True,checks=checks,counts=counts),indent=2)+'\n');print(json.dumps(dict(ok=True,checks=len(checks))))
if __name__=='__main__':main()
