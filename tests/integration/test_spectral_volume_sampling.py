"""Ordinary V3 repeated-scatter proposal equivalence and source scaling.
Analytic PDF/MIS integrals live in spectral_volume_sampling_test.c. Here the
unchanged phase walk is the independent sampling technique, not ground truth.
"""
from pathlib import Path
import argparse,json,sys,time
import numpy as np
W=Path(__file__).resolve().parents[2];sys.path.insert(0,str(W/'tools'))
import spectral_mixed_reference as f
ap=argparse.ArgumentParser();ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
root=a.output.resolve();root.mkdir(parents=True,exist_ok=False);start=time.monotonic();results=[]
for g in (0,.6):
 groups={}
 for policy in ('phase','light_mis','light_only','rectangle_mis'):
  values=[]
  for seed in f.SEEDS[:8]:
   assert time.monotonic()-start<600
   scene=f.scene_root([f.plane('black',(3,0,0),(0,1,0),(0,0,1),(1,0,0),.03,.03)])
   medium=f.medium([-1,-1,-1],[1,1,1],.05,1.2);medium.pop('caustic_cell_m');medium['g']=g
   p=dict(samples=8192,max_depth=12,seed=seed,materials=[f.material('black',0)],medium=medium,
          medium_sampling='split_first',continuation_roulette='fog_survival',volume_direction_sampling=policy,
          light=dict(type='area',origin_m=[0,0,.5],axis_u=[1,0,0],axis_v=[0,-1,0],width_m=2,height_m=1.8,spectrum={'lines':[[550,1]]}),
          probes=[{'name':'target','x':0,'y':0}])
   f.attach(scene,p);q=f.request((0,0,-.5),(0,0,.5));d=f.run(a.cli.resolve(),root,f'g{g}-{policy}-s{seed}',q,scene,None)
   assert d['report']['termination']['all_started_paths_classified']
   assert d['report']['quality']['volume_direction_sampling']==policy
   for b in d['report']['volume_light_diagnostics']['buckets']:
    assert b['attempts']==b['missed']+b['visible']+b['occluded']
    assert 0<=b['rectangle']<=b['angular']<=b['attempts'] and b['angular_missed']<=b['missed']
    assert all(np.isfinite(b[k]) and b[k]>=0 for k in ('nee_sum','nee_sum2','nee_max','hit_sum','hit_sum2','hit_max'))
   row=[f.projected(d['components'][i],550) for i in (0,1,7,8,9,10)];values.append(row)
   if seed==f.SEEDS[0] and policy!='phase':
    p['light']['spectrum']['lines'][0][1]=2
    doubled=f.run(a.cli.resolve(),root,f'g{g}-{policy}-double',q,scene,None)
    assert np.allclose(doubled['components'],np.array(d['components'])*2,rtol=1e-10,atol=1e-12)
  groups[policy]=np.array(values)
 for candidate in ('light_mis','light_only','rectangle_mis'):
  delta=groups[candidate]-groups['phase'];mean=groups['phase'].mean(axis=0);se=delta.std(axis=0,ddof=1)/np.sqrt(len(delta))
  passed=np.abs(delta.mean(axis=0))<=np.maximum(.05*np.abs(mean),3*se)
  assert mean[3:5].sum()>.02,'Must exercise repeated scattering'
  row=dict(g=g,policy=candidate,components=['total','direct','scatter1','scatter2','scatter3_4','scatter5plus'],reference_mean=mean.tolist(),candidate_mean=groups[candidate].mean(axis=0).tolist(),paired_standard_error=se.tolist(),mean_gates=passed.tolist(),source_scaling=True)
  results.append(row);(root/'assessment.json').write_text(json.dumps(results,indent=2));print(row,flush=True)
  assert all(passed),row
print('PASS ordinary area/fog repeated-scatter means and exact source scaling')
