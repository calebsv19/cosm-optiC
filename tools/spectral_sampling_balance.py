"""Bounded ordinary V3 balance study: camera, photon realization, budget and sampler.
Geometry, fog, spectral responses and reconstruction are immutable across cases.
"""
from pathlib import Path
import argparse,json,time,hashlib,gzip
import numpy as np
from spectral_camera_qualification import run
from spectral_camera_analysis import compare
from spectral_error_budget import ROIS
CAMERA=(30011,30013,30029,30047,30059,30071,30089,30091)
MAPS=(19,30103,30109,30113)
TRANSPORT=dict(medium_sampling='split_first',camera_fresnel='split_camera_exit',continuation_roulette='fog_survival')
CLASSES=('opaque_no_glass','glass_reflection_only','glass_transmission_only','glass_reflection_and_transmission','opaque_and_glass')
def records(root):return [(f.parent,json.loads(f.read_text())) for f in root.glob('*/result.json')]
def arrays(p):
 a=np.frombuffer(gzip.decompress((p/'roi.f64.gz').read_bytes()),dtype='<f8').reshape(6,64,11,3)
 c=json.loads((p/'render/frames/frame_0000.bmp.components.npy.classes.json').read_text());assert c['shape']==[6,64,5,3]
 c=np.array(c['values']).reshape(6,64,5,3);assert np.allclose(c.sum(2),a[:,:,2],atol=1e-10,rtol=1e-9)
 assert c.min()>-1e-10*max(1,float(c.max()));return a,c

def verify(root):
 ref={};out=[]
 for p,d in records(root):
  a,c=arrays(p);q=json.loads((p/'scene.json').read_text());v=q['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1'];m=v['caustics']
  for k in ('samples','seed','camera_sampler','volume_direction_sampling'):v.pop(k,None)
  for k in ('passes','seed','camera_fresnel'):m.pop(k,None)
  old=ref.setdefault(d['source'],q);assert q==old,'Scene/reconstruction drift'
  diag=d['nonscattered_indirect'];assert diag['surface_nee_pdf_mismatches']==0
  assert d['termination']['all_started_paths_classified'];out.append(d['name'])
 (root.parent/'matrix-invariants.json').write_text(json.dumps(dict(cases=len(out),class_recombination=True,zero_surface_pdf_mismatches=True,fixed_camera_fog_glass_spectra_reconstruction=True,allowed_variations=['camera samples/seed/sampler','photon count/seed','phase vs rectangle light proposal','camera Fresnel branch policy']),indent=2))

def matrix(cli,inputs,out):
 root=out/'matrix';root.mkdir(parents=True,exist_ok=True)
 phases=[]
 # First group: independent camera replicas and corrected light proposals.
 phases.append([(s,128,c,4,19,m,'hashed_v1') for c in CAMERA for s in ('tungsten','led') for m in ('phase','rectangle_mis')])
 # Crossed camera/map seeds: baseline row already exists; new seeds isolate map noise.
 phases.append([(s,128,c,4,m,'rectangle_mis','hashed_v1') for c in CAMERA[:4] for m in MAPS[1:] for s in ('tungsten','led')])
 phases.append([(s,512,c,4,19,'rectangle_mis','hashed_v1') for c in CAMERA[:4] for s in ('tungsten','led')]+[(s,128,CAMERA[0],16,m,'rectangle_mis','hashed_v1') for m in MAPS for s in ('tungsten','led')])
 phases.append([(s,128,c,4,19,'rectangle_mis','scrambled_path_v1') for c in CAMERA for s in ('tungsten','led')])
 phases.append([(s,128,c,4,19,'rectangle_mis','hashed_v1') for c in CAMERA[:6] for s in ('tungsten','led')])
 for i,specs in enumerate(phases):
  start=time.monotonic()
  for s,n,c,p,m,direction,sampler in specs:
   assert time.monotonic()-start<1200,'Bounded phase exhausted'
   assert len(list(out.glob('*/*/process.json')))<110,'Launch bound exhausted'
   run(cli,inputs,root,s,n,c,sampler,transport={**TRANSPORT,'camera_fresnel':'split_bounded' if i==4 else 'split_camera_exit','volume_direction_sampling':direction},maps=dict(passes=p,seed=m))
  verify(root);(out/f'phase-{i}-complete.json').write_text(json.dumps(dict(cases=len(specs),seconds=time.monotonic()-start)))
  print('Completed balance phase',i,flush=True)

def analyze(out):
 rows=records(out/'matrix');result={}
 def get(s,n,c,p,m,dir='rectangle_mis',sampler='hashed_v1',glass='split_camera_exit'):
  hits=[(f,d) for f,d in rows if d['source']==s and d['spp']==n and d['camera_seed']==c and d['photon_budget']==dict(passes=p,seed=m) and d['quality']['volume_direction_sampling']==dir and d['mode']==sampler and d['map']['camera_fresnel']==glass];assert len(hits)==1,(s,n,c,p,m,dir,sampler)
  f,d=hits[0];a,k=arrays(f);return a,k,d
 def comparison(aa,bb):
  a=np.array([v[0] for v in aa]);b=np.array([v[0] for v in bb]);cpu=sum(v[2]['cpu_seconds'] for v in bb)/sum(v[2]['cpu_seconds'] for v in aa)
  r={name:compare(a[:,i],b[:,i],name) for i,name in enumerate(ROIS)}
  for v in r.values():v['CPU_variance_product_ratio']=cpu*v['Y_variance_ratio']
  return dict(cpu_ratio=cpu,equal_time_gate=.9<=cpu<=1.1,regions=r,median_variance=float(np.median([v['Y_variance_ratio'] for v in r.values()])))
 for source in ('tungsten','led'):
  base=[get(source,128,c,4,19) for c in CAMERA];phase=[get(source,128,c,4,19,'phase') for c in CAMERA]
  high=[get(source,512,c,4,19) for c in CAMERA[:4]];pbase=[get(source,128,CAMERA[0],4,m) for m in MAPS];phigh=[get(source,128,CAMERA[0],16,m) for m in MAPS]
  qmc=[get(source,128,c,4,19,sampler='scrambled_path_v1') for c in CAMERA]
  split=[get(source,128,c,4,19,glass='split_bounded') for c in CAMERA[:6]]
  result[source]=dict(glass_split_vs_exit=comparison(base[:6],split),rectangle_vs_phase=comparison(phase,base),camera_4x=comparison(base[:4],high),photons_4x=comparison(pbase,phigh),qmc_vs_hashed=comparison(base,qmc))
  # Two-factor random-effects ANOVA; report raw signed estimates, not clipped shares.
  x=np.array([[get(source,128,c,4,m)[0][:,:,0,1] for m in MAPS] for c in CAMERA[:4]])
  cm=x.mean(1);pm=x.mean(0);mean=x.mean((0,1));e=x-cm[:,None]-pm[None,:]+mean
  interaction=(e*e).sum((0,1))/9
  cv=cm.var(0,ddof=1)-interaction/4;pv=pm.var(0,ddof=1)-interaction/4
  result[source]['crossed_variance']={name:dict(camera=float(cv[i].mean()),photon_map=float(pv[i].mean()),interaction=float(interaction[i].mean()),total=float((cv[i]+pv[i]+interaction[i]).mean())) for i,name in enumerate(ROIS)}
  k=np.array([v[1] for v in base]);result[source]['glint_classes']={}
  y=k[:,1,:,:,1];z=y-y.mean(0);cov=np.einsum('rpi,rpj->ij',z,z)/((len(base)-1)*64);total=float(cov.sum())
  result[source]['glint_classes']=dict(class_order=CLASSES,mean_Y=y.mean((0,1)).tolist(),covariance=cov.tolist(),signed_variance_attribution=(cov.sum(1)/total).tolist(),scope='glint pixel variance within nonscattered indirect, eight independent camera seeds, fixed map')
 (out/'analysis.json').write_text(json.dumps(result,indent=2));print('Wrote balance analysis',flush=True)
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--cli',type=Path);a.add_argument('--inputs',type=Path);a.add_argument('--output',type=Path,required=True);a.add_argument('--analyze',action='store_true');q=a.parse_args()
 if not q.analyze:matrix(q.cli.resolve(),q.inputs.resolve(),q.output.resolve())
 analyze(q.output.resolve())
