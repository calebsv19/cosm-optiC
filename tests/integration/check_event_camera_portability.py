"""Unchanged saved-plan admission, exact integrity and bounded geometry refusal."""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--fixture',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--probe',type=Path,required=True);a=p.parse_args()
 out=a.out.resolve();out.mkdir();fixture=out/'input';shutil.copytree(a.fixture,fixture)
 scene=fixture/'scene_runtime.json';original=json.loads(scene.read_text());before=hashlib.sha256(scene.read_bytes()).hexdigest()
 env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(out/'program'),XDG_CONFIG_HOME=str(out/'config'))
 (out/'program').mkdir();(out/'config').mkdir();probe=a.probe.resolve()
 def run(label,args,ok=True):
  x=subprocess.run([str(probe),*map(str,args)],cwd=fixture,env=env,capture_output=True,text=True,timeout=60)
  (out/(label+'.log')).write_text(x.stdout+x.stderr)
  assert (x.returncode==0)==ok,(label,x.returncode,x.stdout[-1000:],x.stderr[-1000:])
 def write(label,change,rehash=False):
  s=copy.deepcopy(original);entry=s['extensions']['ray_tracing']['authoring']['event_cameras'][0];change(entry)
  path=fixture/(label+'.json');path.write_text(json.dumps(s,indent=2)+'\n')
  if rehash:
   hp=out/(label+'-hash.json');run(label+'-hash',[scene,'--hash-retained-test',path,hp])
   entry['resolved']['proposal_sha256']=json.loads(hp.read_text())['proposal_sha256'];path.write_text(json.dumps(s,indent=2)+'\n')
  return path
 run('unaltered-saved-plan',[scene,0,15,45,86,87,101])
 for label,change in [
  ('forged-tiny-bounds',lambda e:e['resolved']['bounds_low'].__setitem__(0,e['resolved']['bounds_low'][0]+1e-14)),
  ('forged-hash',lambda e:e['resolved'].update(proposal_sha256='0'*64)),
  ('changed-selection',lambda e:e.update(selection=1)),
  ('changed-intent',lambda e:e['intent']['framing'].update(fov_y=70))]:
  run(label,[write(label,change),0],False)
 run('one-ulp-with-own-integrity',[write('one-ulp',lambda e:e['resolved']['bounds_low'].__setitem__(0,math.nextafter(e['resolved']['bounds_low'][0],0)),True),0,15,86,101])
 run('meaningful-rehashed-geometry',[write('meaningful-geometry',lambda e:e['resolved']['bounds_low'].__setitem__(0,e['resolved']['bounds_low'][0]+1e-5),True),0],False)
 run('rehashed-exact-lens',[write('changed-lens',lambda e:e['resolved'].update(fov_y=e['resolved']['fov_y']+1e-10),True),0],False)
 assert hashlib.sha256(scene.read_bytes()).hexdigest()==before
 result=dict(ok=True,policy='ray_event_geometry_validation_v1',original_scene_sha256=before,original_unchanged=True,one_ulp_geometry_accepted=True,integrity_refusals=4,rehashed_geometry_and_lens_refusals=2)
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('OPTIC_EVENT_PORTABILITY_RESULT '+json.dumps(result),flush=True)
if __name__=='__main__':main()
