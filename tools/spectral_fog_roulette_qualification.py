"""Independent fixed-map roulette qualification with frozen CPU-calibrated budgets."""
from pathlib import Path
import argparse,gzip,json,hashlib,time
import numpy as np
from spectral_camera_qualification import run
from spectral_camera_analysis import compare
from spectral_error_budget import ROIS
CALIBRATION=(307,1201,4001,16001)
HELDOUT=(601,2401,9601,38401,153601,614401,2457601,9830401)
PREVIOUS=(211,1021,4093,16381,65521,262139,1048573,4194301)
MODES={'baseline':{'medium_sampling':'analog','camera_fresnel':'split_first'},
       'exit':{'medium_sampling':'split_first','camera_fresnel':'split_camera_exit'},
       'survival':{'medium_sampling':'split_first','camera_fresnel':'split_camera_exit','continuation_roulette':'fog_survival'}}
def phase(cli,inputs,out,name,seeds,budgets):
 root=out/name;root.mkdir(parents=True,exist_ok=True);start=time.monotonic()
 for i,seed in enumerate(seeds):
  for source in ('tungsten','led'):
   arms=list(budgets[source].items())
   for mode,spp in (arms if i%2==0 else arms[::-1]):
    if time.monotonic()-start>1200:raise RuntimeError('phase launch bound exhausted')
    if len(list(out.glob('*/*/process.json')))>=130:raise RuntimeError('130 regional launch bound exhausted')
    run(cli,inputs,root,source,spp,seed,'hashed_v1',transport=MODES[mode])
 verify(root)
 (root/'complete.json').write_text(json.dumps({'seeds':seeds,'budgets':budgets,'seconds':time.monotonic()-start},indent=2))
def records(root):return [(p.parent,json.loads(p.read_text())) for p in root.glob('*/result.json')]
def mode(d):return 'survival' if d['quality'].get('continuation_roulette')=='fog_survival' else 'baseline' if d['quality']['medium_sampling']=='analog' else 'exit'
def verify(root):
 first={};count=0
 for p,d in records(root):
  assert hashlib.sha256((p/'scene.json').read_bytes()).hexdigest()==d['scene_sha256']
  assert hashlib.sha256((p/'roi.f64.gz').read_bytes()).hexdigest()==d['roi_sha256']
  scene=json.loads((p/'scene.json').read_text());q=scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
  for key in ('samples','seed','camera_sampler','medium_sampling','continuation_roulette'):q.pop(key,None)
  q['caustics'].pop('camera_fresnel',None)
  ref,report=first.setdefault(d['source'],(scene,d));assert scene==ref and d['film']==report['film']
  for key in ('map_key','ballistic_launch_key','bands','deposits','launched','radius_m','volume_cell_m'):assert d['map'][key]==report['map'][key]
  assert d['termination']['all_started_paths_classified'];count+=1
 (root/'invariants.json').write_text(json.dumps({'cases':count,'fixed_scene_map_film':True},indent=2))
def data(rows):return np.array([np.frombuffer(gzip.decompress((p/'roi.f64.gz').read_bytes()),dtype='<f8').reshape(6,64,11,3) for p,d in rows])
def analyze(root):
 result={}
 for source in ('tungsten','led'):
  groups={}
  for p,d in records(root):
   if d['source']==source:groups.setdefault(mode(d),[]).append((p,d))
  for rows in groups.values():rows.sort(key=lambda v:v[1]['camera_seed'])
  result[source]={}
  for reference in ('baseline','exit'):
   if reference not in groups:continue
   aa=groups[reference];a=data(aa)
   for name,bb in groups.items():
    if name==reference or name=='baseline':continue
    assert [d['camera_seed'] for p,d in aa]==[d['camera_seed'] for p,d in bb]
    b=data(bb);rr={r:compare(a[:,i],b[:,i],r) for i,r in enumerate(ROIS)}
    ratios=[d['cpu_seconds']/base['cpu_seconds'] for (_,d),(_,base) in zip(bb,aa)]
    ratio=sum(d['cpu_seconds'] for p,d in bb)/sum(d['cpu_seconds'] for p,d in aa)
    result[source][name+'_vs_'+reference]={'regions':rr,'cpu_ratio':ratio,'paired_cpu_ratios':ratios,'equal_time_within_10_percent':.9<=ratio<=1.1,'reference_samples':aa[0][1]['spp'],'candidate_samples':bb[0][1]['spp'],'median_variance':float(np.median([v['Y_variance_ratio'] for v in rr.values()])),'region_gates':all(v['mean_pass'] and v['variance_pass'] for v in rr.values())}
 (root/'analysis.json').write_text(json.dumps(result,indent=2))
 for s,rows in result.items():
  for name,b in rows.items():print(s,name,'CPU',round(b['cpu_ratio'],3),'gates',b['region_gates'],[(r,round(v['Y_variance_ratio'],3),v['mean_pass']) for r,v in b['regions'].items()],flush=True)
 return result
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--inputs',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--phase',choices=('calibrate','heldout','previous'),required=True);a=ap.parse_args();out=a.output.resolve();cli=a.cli.resolve();inputs=a.inputs.resolve()
 if a.phase=='calibrate':
  phase(cli,inputs,out,'calibration',CALIBRATION,{s:dict(baseline=256,exit=128,survival=128) for s in ('tungsten','led')})
  phase(cli,inputs,out,'cost-probes',CALIBRATION[:2],{s:dict(survival=64) for s in ('tungsten','led')})
  budgets={};cost={}
  for s in ('tungsten','led'):
   rows=[d for p,d in records(out/'calibration') if d['source']==s];target=float(np.mean([d['cpu_seconds'] for d in rows if mode(d)=='baseline']));high=float(np.mean([d['cpu_seconds'] for d in rows if mode(d)=='survival']));low=float(np.mean([d['cpu_seconds'] for p,d in records(out/'cost-probes') if d['source']==s]));slope=(high-low)/64;assert slope>0
   intercept=low-slope*64;n=max(32,min(256,8*round((target-intercept)/slope/8)))
   budgets[s]=dict(baseline=256,exit=128,survival=n);cost[s]=dict(target_cpu=target,low_cpu=low,high_cpu=high,fitted_slope=slope,fitted_intercept=intercept,survival_samples=n)
  (out/'frozen-budgets.json').write_text(json.dumps({'budgets':budgets,'timing_fit':cost,'heldout_seeds':HELDOUT,'selection_uses_timing_only':True},indent=2));analyze(out/'calibration')
 else:
  budgets=json.loads((out/'frozen-budgets.json').read_text())['budgets'];phase(cli,inputs,out,a.phase,HELDOUT if a.phase=='heldout' else PREVIOUS,budgets if a.phase=='heldout' else {s:{'survival':b['survival']} for s,b in budgets.items()})
  if a.phase=='heldout':analyze(out/'heldout')
