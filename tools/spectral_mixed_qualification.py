from pathlib import Path
import sys,json,math,time,copy
import numpy as np
import argparse
W=Path(__file__).resolve().parents[1];sys.path.insert(0,str(W/'tools'))
import spectral_mixed_reference as f
ap=argparse.ArgumentParser(description='Bounded independent mixed-path qualification; no production transport changes.')
ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
ap.add_argument('--sources',type=Path,required=True);ap.add_argument('--pilot',action='store_true')
ap.add_argument('--wall-budget',type=int,default=1800)
ap.add_argument('--continuation-roulette',choices=('throughput','fog_survival'),default='throughput')
ap.add_argument('--camera-sampler',choices=('hashed_v1','scrambled_path_v1'),default='hashed_v1')
ap.add_argument('--medium-sampling',choices=('analog','split_first'),default='analog')
ap.add_argument('--camera-fresnel',choices=('roulette','split_first','split_bounded','split_camera_exit'),default='split_first')
args=ap.parse_args();CLI=args.cli.resolve();R=args.output.resolve();R.mkdir(parents=True,exist_ok=True)
assert CLI.is_file()
started=time.monotonic();rows=[]
def controls(s):
 p=f.profile(s);p['medium_sampling']=args.medium_sampling;p['continuation_roulette']=args.continuation_roulette
 if 'caustics' in p:p['caustics']['camera_fresnel']=args.camera_fresnel
def run(kind,nm,seed,black=False,scale=1,cell=.04,radius=.03):
 if time.monotonic()-started>args.wall_budget:raise RuntimeError('physical wall budget exhausted')
 q,s,m=f.fixture(kind,nm,seed,black,scale,cell,radius);controls(s);f.profile(s)['camera_sampler']=args.camera_sampler;f.profile(s)['path_trace']={'x':0,'y':0,'sample':0}
 name=f'{kind}-w{nm}-s{seed}-b{int(black)}-l{scale}-c{cell}-r{radius}'
 d=f.run(CLI,R,name,q,s,m)
 assert all(e['vacuum_nm']==nm for e in d['report']['path_trace']['events'])
 return d

def assess(kind,nm,values,ref,**kw):
 values=np.asarray(values);mean=float(values.mean());se=float(values.std(ddof=1)/math.sqrt(len(values))) if len(values)>1 else None
 row=dict(kind=kind,wavelength_nm=nm,values=values.tolist(),mean=mean,reference=ref,relative_error=mean/ref-1,
          standard_error=se,three_standard_error_interval=[mean-3*se,mean+3*se] if se is not None else None,relative_standard_error=se/ref if se is not None else None,
          mean_gate_pass=abs(mean/ref-1)<.10,precision_gate_pass=se is not None and se/ref<.03,**kw)
 rows.append(row);(R/'assessment.json').write_text(json.dumps(rows,indent=2));print(kind,nm,kw,'ratio',round(mean/ref,5),'SE/ref',round(se/ref,5) if se else se,flush=True)

if args.pilot:
 for kind in f.REFERENCES:
  d=run(kind,550,19);index={'surface_fog':7,'glass_surface':5,'glass_fog':6}[kind]
  value=f.projected(d['components'][index],550)
  if kind=='surface_fog':value-=f.projected(run(kind,550,19,True)['components'][7],550)
  ref=f.REFERENCES[kind](550);print(kind,'pilot value/reference',value/ref,'seconds',d['seconds'],flush=True)
else:
 refs={}
 for kind,ref_fn in f.REFERENCES.items():
  for nm in f.WAVES:
   a=ref_fn(nm);b=ref_fn(nm,**({'n':80,'k':24,'steps':160} if kind=='surface_fog' else {'n':120,'k':24} if kind=='glass_surface' else {'n':240,'k':32}))
   assert abs(a/b-1)<.01,(kind,nm,a,b);refs[kind,nm]=b
   (R/'reference-refinement.json').write_text(json.dumps({f'{k[0]}-{k[1]}':v for k,v in refs.items()},indent=2))
 for kind in f.REFERENCES:
  for nm in f.WAVES:
   values=[];seeds=f.SEEDS if kind=='surface_fog' else f.SEEDS[:4]
   for seed in seeds:
    d=run(kind,nm,seed);index={'surface_fog':7,'glass_surface':5,'glass_fog':6}[kind]
    value=f.projected(d['components'][index],nm)
    if kind=='surface_fog':value-=f.projected(run(kind,nm,seed,True)['components'][7],nm)
    values.append(value)
   assess(kind,nm,values,refs[kind,nm])
   doubled=run(kind,nm,19,scale=2)
   baseline=run(kind,nm,19)
   assert np.allclose(doubled['components'],np.array(baseline['components'])*2,rtol=1e-9,atol=1e-12),(kind,nm,'source scaling')
 # Separate reconstruction sensitivity, never blended into the fixed-reconstruction noise matrix.
 for kind,levels in [('glass_surface',(.06,.015)),('glass_fog',(.02,.01))]:
  for level in levels:
   vals=[]
   for seed in f.SEEDS[:4]:
    d=run(kind,550,seed,**({'radius':level} if kind=='glass_surface' else {'cell':level}))
    vals.append(f.projected(d['components'][5 if kind=='glass_surface' else 6],550))
   assess(kind,550,vals,refs[kind,550],reconstruction_level=level)
 print('PHYSICAL MATRIX COMPLETE',len(rows),'reference groups',flush=True)
if not args.pilot:
 from spectral_response_fixture import xyz
 previous=args.sources
 continuum=json.loads(previous.read_text())['sources']['led']['continuum']
 expected=np.array(xyz(continuum))*refs['glass_fog',550]
 for bins in (8,32,128):
  vals=[];triples=[]
  for seed in f.SEEDS[:4]:
   if time.monotonic()-started>args.wall_budget:raise RuntimeError('physical wall budget exhausted')
   q,s,m=f.fixture('glass_fog',550,seed);controls(s);p=f.profile(s);p['camera_sampler']=args.camera_sampler;p['light']['spectrum']={'continuum':continuum};p['caustics']['bins']=bins
   d=f.run(CLI,R,f'glass_fog-measured-led-bins{bins}-s{seed}',q,s,m)
   triples.append(d['components'][6]);vals.append(d['components'][6][1])
  assess('glass_fog_continuum',550,vals,float(expected[1]),spectral_bins=bins,
          mean_xyz=np.mean(triples,axis=0).tolist(),reference_xyz=expected.tolist(),
          relative_xyz_error=float(np.linalg.norm(np.mean(triples,axis=0)-expected)/np.linalg.norm(expected)))
 print('SPECTRAL REFINEMENT COMPLETE',flush=True)

# Completion means all scheduled cases finished; failed acceptance gates remain
# explicit in assessment.json and must not be inferred from process exit alone.
if not args.pilot:
 import hashlib
 completed=[json.loads(p.read_text()) for p in R.glob('*/result.json')]
 assert len(completed)==157
 (R/'qualification-complete.json').write_text(json.dumps(dict(cases=len(completed),
  binary_sha256=hashlib.sha256(CLI.read_bytes()).hexdigest(),camera_sampler=args.camera_sampler,medium_sampling=args.medium_sampling,camera_fresnel=args.camera_fresnel,continuation_roulette=args.continuation_roulette,
  wall_seconds=time.monotonic()-started,render_seconds_sum=sum(r['seconds'] for r in completed),
  gates_are_in_assessment=True),indent=2))
