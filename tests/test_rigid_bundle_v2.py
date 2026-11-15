#!/usr/bin/env python3
"""Data-only v1/v2 compatibility and attestation controls; never runs physics.
The supplied v1 producer bundle is the qualified fixture. Test-created v2 revisions
exercise admission only; they are not new numerical qualification.
"""
import argparse,copy,hashlib,json,os,shutil,subprocess
from pathlib import Path

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def canonical(v):return (json.dumps(v,sort_keys=True,separators=(',',':'))+'\n').encode()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['consumer','bundle','out']:p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();a.out.mkdir(exist_ok=False,parents=True);root=a.out.resolve();source=a.bundle.resolve();manifest=json.loads((source/'manifest.json').read_text());base=json.loads((source/'input/scene.json').read_text());aset=json.loads((source/'input/assets/asset_set.json').read_text());checks=[]
 env=dict(os.environ,XDG_CONFIG_HOME=str(root/'config'),RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled',UBSAN_OPTIONS='halt_on_error=1')
 def scene(bundle,revision,name):
  s=copy.deepcopy(base);s['schema_variant']='scene_runtime_v1';r=s.setdefault('extensions',{}).setdefault('ray_tracing',{});r['rigid_result']={'schema':'ray_rigid_result_binding_v1','bundle_path':str(bundle),'result_revision':revision}
  for o in s['objects']:
   b=next(b for b in s['extensions']['rigid_scene']['bindings'] if b['object_id']==o['object_id']);asset=next(x for x in aset['assets'] if x['asset_id']==b['collision_asset_id']);o.setdefault('extensions',{}).setdefault('line_drawing',{})['runtime_mesh_path']=str(bundle/'input/assets'/asset['root']/'visible.json')
  f=root/(name+'.json');f.write_text(json.dumps(s));return f
 def call(f,*ticks,ok=True):
  r=subprocess.run([str(a.consumer.resolve()),str(f),*map(str,ticks)],capture_output=True,text=True,env=env,timeout=180);assert (r.returncode==0)==ok,(r.returncode,r.stderr[-1500:]);return [json.loads(l[4:]) for l in r.stdout.splitlines() if l.startswith('BR4 ')]
 def seal(bundle,schema='ball_rigid_result_bundle_v2'):
  m={k:v for k,v in manifest.items() if k not in ['result_revision','artifacts']};m['schema']=schema;m['artifacts']=[dict(path=str(f.relative_to(bundle)),size_bytes=f.stat().st_size,sha256=digest(f)) for f in sorted(bundle.rglob('*')) if f.is_file() and f.name!='manifest.json'];m['result_revision']=hashlib.sha256(canonical(m)).hexdigest();(bundle/'manifest.json').write_bytes(canonical(m));return m['result_revision']
 initial=scene(source,manifest['result_revision'],'legacy');old=call(initial,0,24,1,0);checks.append('unmodified v1 bundle admitted and repeated seeking exact')
 target=root/'v2';shutil.copytree(source,target)
 for row in aset['assets']:
  folder=target/'input/assets'/row['root'];b=next(b for b in base['extensions']['rigid_scene']['bindings'] if b['collision_asset_id']==row['asset_id']);lines=(folder/'collision.bcr3d').read_text().splitlines();value=lambda key:next(l.split()[1:] for l in lines if l.startswith(key+' '));metadata={k:b[k] for k in ['source_asset_id','mesh_asset_id','source_sha256','mesh_sha256','collision_sha256']};metadata.update(asset_id=row['asset_id'],shape='convex_compound',child_count=int(value('compound')[-1]),source_center_m=list(map(float.fromhex,value('frame_center'))),principal_to_source=list(map(float.fromhex,value('frame_matrix'))),inertia_per_kg=list(map(float.fromhex,value('frame_moments'))));cert=dict(schema='ball_rigid_asset_certificate_v1',engine_sha256=manifest['engine_sha256'],kind='bcr3d',metadata=metadata,collision_model=None,files=[dict(path=f.name,size_bytes=f.stat().st_size,sha256=digest(f)) for f in sorted(folder.iterdir()) if f.is_file()]);(folder/'asset-certificate.json').write_text(json.dumps(cert))
 revision=seal(target);good=scene(target,revision,'v2');new=call(good,0,24,1,0)
 for x,y in zip(old,new):
  for p,q in zip(x['poses'],y['poses']):assert {k:v for k,v in p.items() if k!='revision'}=={k:v for k,v in q.items() if k!='revision'}
 checks.append('v2 certificate-backed BCR poses equal immutable v1 poses')
 for name,edit in [('engine',lambda c:c.update(engine_sha256='0'*64)),('source_frame',lambda c:c['metadata']['source_center_m'].__setitem__(0,2)),('false_approximate',lambda c:c.update(collision_model={})),('inventory_size',lambda c:c['files'][0].update(size_bytes=1)),('unknown_kind',lambda c:c.update(kind='future'))]:
  bad=root/name;shutil.copytree(target,bad);f=bad/'input/assets'/aset['assets'][0]['root']/'asset-certificate.json';c=json.loads(f.read_text());edit(c);f.write_text(json.dumps(c));badscene=scene(bad,seal(bad),name+'_scene');assert call(good,'--replacement',badscene)==[{'rollback':True}];checks.append(name+' refuses after manifest reseal and preserves prior playback')
 raw=target/'input/assets'/aset['assets'][0]['root']/'asset-certificate.json';saved=raw.read_bytes();raw.write_bytes(saved+b' ');call(good,0,ok=False);raw.write_bytes(saved);checks.append('changed bytes refuse under original accepted revision')
 (root/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,fixture=str(source),numerical_qualification=False),indent=2)+'\n');print(json.dumps(dict(passed=True,checks=len(checks))))
if __name__=='__main__':main()
