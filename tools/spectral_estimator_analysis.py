"""Compare isolated estimators at fixed reconstruction, maps and camera."""
from pathlib import Path
import argparse,json,gzip,hashlib
import numpy as np
from spectral_error_budget import SOURCES,SEEDS,ROIS
from spectral_error_analysis import load
from spectral_camera_analysis import candidate,compare
from spectral_estimator_qualification import MODES
HELD=(7919,16807)

def read(root,source,seed,mode):
 controls=MODES[mode];name=f"{source}-hashed_v1-s256-c{seed}-fog_{controls['medium_sampling']}-glass_{controls['camera_fresnel']}"
 p=root/name;d=json.loads((p/'result.json').read_text());raw=(p/'roi.f64.gz').read_bytes();assert hashlib.sha256(raw).hexdigest()==d['roi_sha256']
 return np.frombuffer(gzip.decompress(raw),dtype='<f8').reshape(6,64,11,3),d,p

def analyze(old,prior,new):
 out={}
 for mode in MODES:
  out[mode]={}
  for source in SOURCES:
   baselines=[load(old,source,256,4,c,19) for c in SEEDS]+[candidate(prior,source,256,c,'hashed_v1') for c in HELD]
   trials=[read(new,source,c,mode) for c in (*SEEDS,*HELD)]
   a=np.array([d[0] for d in baselines]);b=np.array([d[0] for d in trials]);base=baselines[0][1]
   for _,d,p in trials:
    assert d['film']==base['film']
    for key in ('map_key','ballistic_launch_key','bands','radius_m','volume_cell_m','launched','deposits'):assert d['map'][key]==base['map'][key],(mode,source,key)
    original=json.loads((old/f'{source}-c19-m19-s256-p4/scene.json').read_text());sc=json.loads((p/'scene.json').read_text())
    for scene in (original,sc):
     profile=scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
     for k in ('seed','samples','medium_sampling','camera_sampler'):profile.pop(k,None)
     profile['caustics'].pop('camera_fresnel',None)
    assert sc==original
   rows={}
   for i,r in enumerate(ROIS):rows[r]={group:compare(a[index,i],b[index,i],r) for group,index in [('original',slice(0,4)),('heldout',slice(4,6)),('combined',slice(None))]}
   # Current binary's unchanged seed19 run supplies the equal-budget CPU anchor.
   anchor=json.loads((new/f'{source}-hashed_v1-s256-c19/result.json').read_text())
   cpu=float(np.mean([d[1]['cpu_seconds'] for d in trials])/anchor['cpu_seconds'])
   for row in rows.values():row['CPU_variance_product_ratio']=cpu*row['combined']['Y_variance_ratio']
   out[mode][source]={'regions':rows,'cpu_ratio_to_current_seed19_anchor':cpu,'mean_cpu_seconds':float(np.mean([d[1]['cpu_seconds'] for d in trials])),
    'maximum_branches':trials[0][1]['quality']['maximum_camera_branches'],
    'all_region_gates':all(row[group][key] for row in rows.values() for group in ('original','heldout') for key in ('mean_pass','variance_pass'))}
 return {'schema':'optic_estimator_comparison_v1','modes':out,'limits':'4 original +2 held-out seeds; tiny exact-sensor patches; CPU anchor is same-binary seed19, not a full-render benchmark. No denoise or reconstruction change.'}
if __name__=='__main__':
 ap=argparse.ArgumentParser()
 for name in ('baseline','prior','matrix','output'):ap.add_argument('--'+name,type=Path,required=True)
 a=ap.parse_args();d=analyze(a.baseline,a.prior,a.matrix);a.output.write_text(json.dumps(d,indent=2))
 for m,ss in d['modes'].items():
  for s,block in ss.items():print(m,s,'gates',block['all_region_gates'],'CPU',round(block['cpu_ratio_to_current_seed19_anchor'],2),[(r,round(v['combined']['Y_variance_ratio'],3)) for r,v in block['regions'].items()])
