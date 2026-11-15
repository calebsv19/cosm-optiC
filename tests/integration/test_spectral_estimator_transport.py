from pathlib import Path
import sys,json,math,copy
import numpy as np
W=Path(__file__).resolve().parents[2];sys.path.insert(0,str(W/'tools'));import spectral_mixed_reference as f
import argparse
ap=argparse.ArgumentParser(description='Independent ballistic visibility and strong-glass slab controls')
ap.add_argument('--roulette',choices=('throughput','fog_survival'),default='throughput');ap.add_argument('--glass-mode',choices=('split_bounded','split_camera_exit'),default='split_bounded');ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
cli=args.cli.resolve();root=args.output.resolve();rows=[]
for seed in (19,1237,65537,104729):
 q,s,m=f.fixture('surface_fog',550,seed,black=True);p=f.profile(s);p['continuation_roulette']=args.roulette;p.update(samples=64,medium_sampling='split_first');p['light'].update(origin_m=[.8,0,.4],axis_u=[0,1,0],axis_v=[0,0,-1],width_m=2,height_m=2)
 for density in ('scattering','absorbing','zero'):
  ss=.4 if density=='scattering' else 0;sa=.15 if density!='zero' else 0
  p['medium']['sigma_s_per_m']=f.curve(ss);p['medium']['sigma_a_per_m']=f.curve(sa)
  d=f.run(cli,root,f'fog-{density}-{seed}',q,s,m);got=f.projected(d['components'][1],550);ref=math.exp(-(sa+ss)*.4)
  assert abs(got/ref-1)<1e-6,(density,got,ref)
  assert d['report']['termination']['all_started_paths_classified']
  rows.append({'case':f'fog-{density}-{seed}','value':got,'reference':ref,'relative_error':got/ref-1})
print('PASS analytic ballistic visibility across scattering/absorbing/zero fog and four seeds',flush=True)
for seed in (19,1237,65537,104729):
 q,s,m=f.fixture('glass_fog',550,seed);s['objects']=[f.plane('black',(3,0,0),(1,0,0),(0,1,0),(0,0,1),.03,.03),f.glass_object()];p=f.profile(s);p['continuation_roulette']=args.roulette;p.pop('medium');p['materials']=[f.material('black',0),dict(object_id='glass',model='smooth_glass',glass='constant',ior=1.5)]
 p.update(samples=8192,max_depth=12);p['caustics'].update(passes=0,camera_fresnel=args.glass_mode);p['light'].update(width_m=2,height_m=2)
 q=f.request((0,0,-.4),(0,0,1));d=f.run(cli,root,f'glass-slab-{seed}',q,s,m);got=f.projected(d['components'][0],550)
 F=((1.5-1)/(1.5+1))**2;ref=(1-F)**2*sum(F**(2*k) for k in range(5))
 assert abs(got/ref-1)<.003,(got,ref)
 assert d['report']['termination']['all_started_paths_classified']
 assert d['report']['termination']['camera_paths']<=8192*(3 if args.glass_mode=='split_camera_exit' else 5)
 rows.append({'case':f'glass-slab-{seed}','value':got,'reference':ref,'relative_error':got/ref-1})
print('PASS IOR1.5 normal-incidence finite-depth internal-reflection series, branch bounds and terminal ledger',flush=True)
(root/'assessment.json').write_text(json.dumps(rows,indent=2))
