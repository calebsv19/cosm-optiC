"""Strict estimator admission and worst-case branch-work refusal."""
from pathlib import Path
import argparse,json,copy,subprocess
ap=argparse.ArgumentParser();ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--fixture',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
root=a.output.resolve();root.mkdir(parents=True,exist_ok=False);base=a.fixture.resolve()
(root/'config').symlink_to(base/'config',target_is_directory=True)
(root/'slab.runtime.json').write_bytes((base/'slab.runtime.json').read_bytes())
scene=json.loads((base/'scene.json').read_text());request=json.loads((base/'request.json').read_text())
for i,(kind,value) in enumerate([('direction',True),('direction',17),('direction','light_mis\0hidden'),('direction','light_only\0hidden'),('direction','rectangle_mis\0hidden'),('direction','unknown'),('roulette',True),('roulette',17),('roulette','fog_survival\0hidden'),('roulette','unknown'),('medium',True),('medium',17),('medium','split_first\0hidden'),('medium','unknown'),('glass',True),('glass','split_bounded\0hidden'),('glass','split_camera_exit\0hidden'),('glass','unknown'),('work',None)]):
 s=copy.deepcopy(scene);q=copy.deepcopy(request);p=s['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
 if kind=='direction':p['volume_direction_sampling']=value
 elif kind=='roulette':p['continuation_roulette']=value
 elif kind=='medium':p['medium_sampling']=value
 elif kind=='glass':p['caustics']['camera_fresnel']=value
 else:q['render'].update(width=64,height=64);p.update(samples=8192,max_depth=12)
 q.update(run_id=f'bad-{i}',scene={'runtime_scene_path':f'{i}.scene.json'},output={'root':f'out-{i}','overwrite':False})
 (root/f'{i}.scene.json').write_text(json.dumps(s));(root/f'{i}.request.json').write_text(json.dumps(q))
 process=subprocess.run([str(a.cli.resolve()),'--request',f'{i}.request.json','--preflight','--summary',f'{i}.summary.json','--summary-file-only'],cwd=root,capture_output=True,timeout=30)
 assert process.returncode!=0,(kind,value)
 if kind=='work':assert 'branch budget' in (root/f'{i}.summary.json').read_text()
print('PASS strict medium/glass estimator admission and branch-aware work guard')

# Identical large request: the camera-exit three-branch policy fits while the
# older five-branch policy must still refuse. No render or guard relaxation.
for mode,accepted in [('split_camera_exit',True),('split_bounded',False)]:
 s=copy.deepcopy(scene);q=copy.deepcopy(request);p=s['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
 p.update(samples=6400,max_depth=12,medium_sampling='analog');p['caustics']['camera_fresnel']=mode
 q['render'].update(width=64,height=64)
 q.update(run_id=mode,scene={'runtime_scene_path':mode+'.scene.json'},output={'root':mode+'-output','overwrite':False})
 (root/(mode+'.scene.json')).write_text(json.dumps(s));(root/(mode+'.request.json')).write_text(json.dumps(q))
 child=subprocess.run([str(a.cli.resolve()),'--request',mode+'.request.json','--preflight','--summary',mode+'.summary.json','--summary-file-only'],cwd=root,capture_output=True,timeout=30)
 assert (child.returncode==0)==accepted,(mode,child.stderr)
 if not accepted:assert 'branch budget' in (root/(mode+'.summary.json')).read_text()
print('PASS camera-exit admission and bounded-mode refusal at unchanged work guard')
