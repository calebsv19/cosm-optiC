"""Independent equal-time repeated-volume sampling qualification."""
from pathlib import Path
import sys,json,time,hashlib,argparse
import numpy as np
import spectral_fog_roulette_qualification as f
from spectral_camera_qualification import run
from spectral_camera_analysis import compare
ap=argparse.ArgumentParser(description='Fixed-scene repeated-scatter MIS qualification; timing-only sample budgets.')
ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--inputs',type=Path,required=True)
ap.add_argument('--output',type=Path,required=True);ap.add_argument('--phase',choices=('calibrate','heldout','previous'),required=True)
args=ap.parse_args();cli=args.cli.resolve();INPUTS=args.inputs.resolve();R=args.output.resolve();R.mkdir(parents=True,exist_ok=True)
MODES={**f.MODES,'candidate':{**f.MODES['survival'],'volume_direction_sampling':'light_mis'}}
CAL=(401,1607,6421,25717);HELD=(809,3229,12911,51647,206603,826433,3305717,13222861)
def mode(d):return 'candidate' if d['quality'].get('volume_direction_sampling')=='light_mis' else f.mode(d)
def verify(root):
 first={}
 for p,d in f.records(root):
  assert hashlib.sha256((p/'scene.json').read_bytes()).hexdigest()==d['scene_sha256']
  assert hashlib.sha256((p/'roi.f64.gz').read_bytes()).hexdigest()==d['roi_sha256']
  s=json.loads((p/'scene.json').read_text());q=s['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
  for k in ('samples','seed','camera_sampler','medium_sampling','continuation_roulette','volume_direction_sampling'):q.pop(k,None)
  q['caustics'].pop('camera_fresnel',None)
  old,report=first.setdefault(d['source'],(s,d));assert s==old and d['film']==report['film']
  for k in ('map_key','ballistic_launch_key','bands','deposits','launched','radius_m','volume_cell_m'):assert d['map'][k]==report['map'][k]
  assert d['termination']['all_started_paths_classified']
 (root/'invariants.json').write_text(json.dumps({'cases':len(f.records(root)),'fixed_scene_map_film':True},indent=2))
def phase(name,seeds,budgets):
 start=time.monotonic()
 for i,seed in enumerate(seeds):
  for source in ('tungsten','led'):
   arms=list(budgets[source].items())
   for m,n in (arms if i%2==0 else arms[::-1]):
    assert time.monotonic()-start<1200
    assert len(list(R.glob('*/*/process.json')))<150
    run(cli,INPUTS,R/name,source,n,seed,'hashed_v1',transport=MODES[m])
 verify(R/name)
def analysis(root):
 out={}
 for s in ('tungsten','led'):
  groups={}
  for p,d in f.records(root):
   if d['source']==s:groups.setdefault(mode(d),[]).append((p,d))
  for a in groups.values():
   a.sort(key=lambda x:x[1]['camera_seed'])
   assert len({d['spp'] for p,d in a})==1, 'Mixed sample budgets invalidate an arm'
  out[s]={};bb=groups['candidate'];b=f.data(bb)
  for ref in ('baseline','survival'):
   if ref not in groups:continue
   aa=groups[ref];a=f.data(aa);assert [d['camera_seed'] for p,d in aa]==[d['camera_seed'] for p,d in bb]
   rr={r:compare(a[:,i],b[:,i],r) for i,r in enumerate(f.ROIS)}
   ratio=sum(d['cpu_seconds'] for p,d in bb)/sum(d['cpu_seconds'] for p,d in aa)
   out[s][ref]={'regions':rr,'cpu_ratio':ratio,'equal_time_within_10_percent':.9<=ratio<=1.1,'region_gates':all(v['mean_pass'] and v['variance_pass'] for v in rr.values()),'median_variance':float(np.median([v['Y_variance_ratio'] for v in rr.values()])),'median_gate':.85}
   print(s,ref,'cpu',round(ratio,3),'regions',[(k,round(v['Y_variance_ratio'],3),v['mean_pass']) for k,v in rr.items()],flush=True)
 (root/'analysis.json').write_text(json.dumps(out,indent=2));return out
if args.phase=='calibrate':
 phase('calibration',CAL,{s:dict(baseline=256,survival=112,candidate=112) for s in ('tungsten','led')})
 phase('cost-probes',CAL[:2],{s:dict(candidate=56) for s in ('tungsten','led')})
 budgets={};cost={}
 for s in ('tungsten','led'):
  rows=[d for p,d in f.records(R/'calibration') if d['source']==s]
  target=np.mean([d['cpu_seconds'] for d in rows if mode(d)=='baseline']);high=np.mean([d['cpu_seconds'] for d in rows if mode(d)=='candidate']);low=np.mean([d['cpu_seconds'] for p,d in f.records(R/'cost-probes') if d['source']==s]);slope=(high-low)/56;assert slope>0;intercept=low-slope*56
  n=max(32,min(256,8*round((target-intercept)/slope/8)))
  budgets[s]=dict(baseline=256,survival=112,candidate=n);cost[s]=dict(target=target,high=high,low=low,slope=slope,intercept=intercept)
 (R/'frozen-budgets.json').write_text(json.dumps({'budgets':budgets,'timing_fit':cost,'heldout_seeds':HELD,'timing_only':True},indent=2));analysis(R/'calibration')
elif args.phase=='heldout':
 budgets=json.loads((R/'frozen-budgets.json').read_text())['budgets'];phase('heldout',HELD,budgets);analysis(R/'heldout')
elif args.phase=='previous':
 budgets=json.loads((R/'frozen-budgets.json').read_text())['budgets'];phase('previous',f.HELDOUT+f.PREVIOUS,{s:dict(candidate=b['candidate']) for s,b in budgets.items()})
