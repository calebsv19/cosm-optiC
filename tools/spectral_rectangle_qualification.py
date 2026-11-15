"""Qualify rectangle solid-angle sampling against cap and phase at fixed scene cost."""
from pathlib import Path
import argparse,json,time,hashlib
import numpy as np
from spectral_camera_qualification import run
from spectral_fog_roulette_qualification import records,data
from spectral_camera_analysis import compare
from spectral_error_budget import ROIS
CAL=(29009,29017,29021,29023)
HELD=(29101,29123,29129,29131,29137,29147,29153,29167)
CONFIRM=(29201,29207,29209,29221,29231,29243,29251,29269)
MODES=('phase','light_mis','rectangle_mis')
TRANSPORT=dict(medium_sampling='split_first',camera_fresnel='split_camera_exit',continuation_roulette='fog_survival')
def policy(d):return d['quality']['volume_direction_sampling']
def verify(root):
 first={};rows=records(root)
 for p,d in rows:
  assert hashlib.sha256((p/'scene.json').read_bytes()).hexdigest()==d['scene_sha256']
  assert hashlib.sha256((p/'roi.f64.gz').read_bytes()).hexdigest()==d['roi_sha256']
  s=json.loads((p/'scene.json').read_text());q=s['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
  for k in ('samples','seed','volume_direction_sampling'):q.pop(k,None)
  old,report=first.setdefault(d['source'],(s,d));assert s==old and d['film']==report['film']
  for k in ('map_key','ballistic_launch_key','bands','deposits','launched','radius_m','volume_cell_m'):assert d['map'][k]==report['map'][k]
  assert d['termination']['all_started_paths_classified']
 (root/'invariants.json').write_text(json.dumps(dict(cases=len(rows),fixed_scene_map_film=True),indent=2))
def phase(cli,inputs,out,name,seeds,budgets):
 start=time.monotonic()
 for i,seed in enumerate(seeds):
  for si,source in enumerate(('tungsten','led')):
   arms=list(budgets[source].items());offset=(i+si)%len(arms);arms=arms[offset:]+arms[:offset]
   for mode,spp in arms:
    assert time.monotonic()-start<1200,'Phase launch bound exhausted'
    target=out/name;target.mkdir(parents=True,exist_ok=True)
    if not any(d['source']==source and policy(d)==mode and d['camera_seed']==seed for p,d in records(target)):
     assert len(list(out.glob('*/*/process.json')))<150,'Regional launch bound exhausted'
    d=run(cli,inputs,target,source,spp,seed,'hashed_v1',transport={**TRANSPORT,'volume_direction_sampling':mode})
    assert d['spp']==budgets[source][mode] and policy(d)==mode
 verify(out/name)
 (out/name/'complete.json').write_text(json.dumps(dict(seeds=seeds,budgets=budgets,seconds=time.monotonic()-start),indent=2))
def analyze(root):
 result={}
 for source in ('tungsten','led'):
  groups={m:sorted([(p,d) for p,d in records(root) if d['source']==source and policy(d)==m],key=lambda r:r[1]['camera_seed']) for m in MODES}
  for rows in groups.values():assert len({d['spp'] for p,d in rows})==1,'Mixed sample budgets invalidate arm'
  result[source]={}
  for candidate,reference in [('light_mis','phase'),('rectangle_mis','phase'),('rectangle_mis','light_mis')]:
   aa,bb=groups[reference],groups[candidate]
   assert [d['camera_seed'] for p,d in aa]==[d['camera_seed'] for p,d in bb]
   a,b=data(aa),data(bb);rr={r:compare(a[:,i],b[:,i],r) for i,r in enumerate(ROIS)}
   cpu=sum(d['cpu_seconds'] for p,d in bb)/sum(d['cpu_seconds'] for p,d in aa)
   median=float(np.median([v['Y_variance_ratio'] for v in rr.values()]));regional=all(v['variance_pass'] and v['mean_pass'] for v in rr.values())
   row=dict(regions=rr,cpu_ratio=cpu,wall_ratio=sum(d['seconds'] for p,d in bb)/sum(d['seconds'] for p,d in aa),equal_time_within_10_percent=.9<=cpu<=1.1,median_variance=median,median_gate=.85,region_gates=regional,all_gates=regional and median<=.85 and .9<=cpu<=1.1)
   result[source][candidate+'_vs_'+reference]=row
   print(source,candidate,reference,'CPU',round(cpu,3),'all',row['all_gates'],[(r,round(v['Y_variance_ratio'],3),v['mean_pass']) for r,v in rr.items()],flush=True)
 (root/'analysis.json').write_text(json.dumps(result,indent=2));return result

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for k in ('cli','inputs','output'):ap.add_argument('--'+k,type=Path,required=True)
 ap.add_argument('--phase',choices=('calibrate','heldout','confirmation'),required=True);args=ap.parse_args()
 cli,inputs,out=args.cli.resolve(),args.inputs.resolve(),args.output.resolve();out.mkdir(parents=True,exist_ok=True)
 if args.phase=='calibrate':
  phase(cli,inputs,out,'warmup',CAL[:1],{s:{'phase':112} for s in ('tungsten','led')})
  phase(cli,inputs,out,'calibration',CAL,{s:{m:112 for m in MODES} for s in ('tungsten','led')})
  phase(cli,inputs,out,'cost-probes',CAL[:2],{s:{m:56 for m in MODES if m!='phase'} for s in ('tungsten','led')})
  budgets={};fits={}
  for source in ('tungsten','led'):
   high=[d for p,d in records(out/'calibration') if d['source']==source];low=[d for p,d in records(out/'cost-probes') if d['source']==source]
   target=float(np.mean([d['cpu_seconds'] for d in high if policy(d)=='phase']));budgets[source]={'phase':112};fits[source]={}
   for m in ('light_mis','rectangle_mis'):
    hi=float(np.mean([d['cpu_seconds'] for d in high if policy(d)==m]));lo=float(np.mean([d['cpu_seconds'] for d in low if policy(d)==m]));slope=(hi-lo)/56;assert slope>0
    intercept=lo-slope*56;count=max(32,min(256,4*round((target-intercept)/slope/4)));budgets[source][m]=count;fits[source][m]=dict(target=target,high=hi,low=lo,slope=slope,intercept=intercept,samples=count)
  frozen=dict(budgets=budgets,timing_fit=fits,heldout_seeds=HELD,confirmation_seeds=CONFIRM,selection_uses_timing_only=True)
  p=out/'frozen-budgets.json'
  if p.exists():assert json.loads(p.read_text())==frozen,'Frozen budget changed'
  p.write_text(json.dumps(frozen,indent=2));analyze(out/'calibration')
 else:
  budgets=json.loads((out/'frozen-budgets.json').read_text())['budgets'];phase(cli,inputs,out,args.phase,HELD if args.phase=='heldout' else CONFIRM,budgets);analyze(out/args.phase)
if __name__=='__main__':main()
