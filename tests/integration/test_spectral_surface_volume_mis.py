"""Surface vertices after repeated fog must use the surface light proposal.
Also checks that non-scattered path classes recombine through the ordinary route.
"""
from pathlib import Path
import argparse,sys,json
import numpy as np
W=Path(__file__).resolve().parents[2];sys.path.insert(0,str(W/'tools'));import spectral_mixed_reference as f
ap=argparse.ArgumentParser();ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();R=a.output;R.mkdir(parents=True,exist_ok=False)
groups={};rows=[]
for mode in ('phase','light_mis','light_only','rectangle_mis'):
 values=[];events=0
 for seed in f.SEEDS[:8]:
  scene=f.scene_root([f.plane('floor',(0,0,0),(1,0,0),(0,1,0),(0,0,1),4,4)])
  medium=f.medium([-2,-2,.001],[2,2,2],.03,1.2);medium.pop('caustic_cell_m')
  p=dict(samples=8192,max_depth=6,seed=seed,materials=[f.material('floor',.6)],medium=medium,medium_sampling='split_first',continuation_roulette='fog_survival',volume_direction_sampling=mode,
   light=dict(type='area',origin_m=[.3,0,1.5],axis_u=[1,0,0],axis_v=[0,-1,0],width_m=1.2,height_m=1,spectrum={'lines':[[550,1]]}),probes=[{'name':'target','x':0,'y':0}])
  f.attach(scene,p);d=f.run(a.cli.resolve(),R,mode+'-'+str(seed),f.request((0,0,.4),(.2,0,0)),scene,None)
  ledger=d['report']['nonscattered_indirect'];assert ledger['surface_nee_pdf_mismatches']==0,ledger
  events+=ledger['surface_nee_after_multiple_scattering']
  assert np.max(np.abs(ledger['film_xyz']['recombination_residual_xyz']))<1e-10
  assert all(np.max(np.abs(v['recombination_residual_xyz']))<1e-10 for v in ledger['probes'])
  values.append(np.array(d['components'])[[0,2,3,8,9,10],1])
 assert events>100,'Must exercise diffuse surfaces after multiple collisions'
 groups[mode]=np.array(values);print(mode,'surface events after repeated fog',events,flush=True)
for mode in ('light_mis','light_only','rectangle_mis'):
 delta=groups[mode]-groups['phase'];mean=groups['phase'].mean(0);se=delta.std(0,ddof=1)/np.sqrt(8)
 passed=np.abs(delta.mean(0))<=np.maximum(.05*np.abs(mean),3*se)
 row=dict(mode=mode,paired_mean_difference=delta.mean(0).tolist(),reference=mean.tolist(),standard_error=se.tolist(),pass_mean=passed.tolist());rows.append(row);assert all(passed),row
(R/'assessment.json').write_text(json.dumps(rows,indent=2));print('PASS mixed surface/repeated-volume MIS PDFs, class recombination and paired means')
