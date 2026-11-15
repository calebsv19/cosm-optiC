"""Independent conservative-fog furnace: uniform incident radiance stays uniform.
No renderer reference image or photon reconstruction supplies the expected value.
"""
from pathlib import Path
import argparse,json,sys,time
import numpy as np
W=Path(__file__).resolve().parents[2];sys.path.insert(0,str(W/'tools'))
import spectral_mixed_reference as f
ap=argparse.ArgumentParser();ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();root=a.output.resolve();root.mkdir(parents=True,exist_ok=False);rows=[];start=time.monotonic()
for policy in ('throughput','fog_survival'):
 values=[];orders=[]
 for seed in f.SEEDS[:8]:
  assert time.monotonic()-start<600
  scene=f.scene_root([f.plane('white',(3,0,0),(0,1,0),(0,0,1),(1,0,0),.03,.03)])
  mat=f.material('white',1);mat['two_sided']=True
  medium=f.medium([-1,-1,-1],[1,1,1],0,.8);medium.pop('caustic_cell_m');medium['g']=.2
  p=dict(samples=8192,max_depth=12,seed=seed,materials=[mat],medium=medium,medium_sampling='split_first',continuation_roulette=policy,
         light={'type':'sky_cap','axis':[0,0,1],'half_angle_deg':180,'spectrum':{'lines':[[550,1]]}},probes=[{'name':'furnace','x':0,'y':0}])
  f.attach(scene,p);d=f.run(a.cli.resolve(),root,f'{policy}-s{seed}',f.request((0,0,0),(0,0,1)),scene,None)
  total=f.projected(d['components'][0],550);values.append(total);orders.append([f.projected(d['components'][i],550) for i in (7,8,9,10)])
  assert d['report']['termination']['all_started_paths_classified']
  if policy=='fog_survival':assert d['report']['termination']['fog_roulette_events']>0
 v=np.array(values);row=dict(policy=policy,values=values,reference=1.0,mean=float(v.mean()),standard_error=float(v.std(ddof=1)/np.sqrt(len(v))),scattering_orders=np.mean(orders,axis=0).tolist())
 row['mean_gate']=abs(row['mean']-1)<.02;row['precision_gate']=row['standard_error']<.01
 assert row['mean_gate'] and row['precision_gate'],row
 assert row['scattering_orders'][3]>.001,'Fixture must exercise five-or-more scattering'
 rows.append(row);(root/'assessment.json').write_text(json.dumps(rows,indent=2));print(row,flush=True)
print('PASS conservative uniform-radiance fog furnace and repeated-scatter energy')
