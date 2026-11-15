"""Replicate variance/covariance analysis for spectral_error_budget.py outputs.
Estimates repeatability at fixed reconstruction, not error against ground truth.
"""
import argparse,gzip,json,math
from pathlib import Path
import numpy as np
from spectral_error_budget import SEEDS,SOURCES,ROIS
from spectral_components import LABELS
DISJOINT=(1,2,4,5,6,7,8,9,10)

def load(root,source,spp,passes,camera,mapseed):
 name=f'{source}-c{camera}-m{mapseed}-s{spp}-p{passes}';case=root/name
 d=json.loads((case/'result.json').read_text())
 import hashlib
 assert hashlib.sha256((case/'roi.f64.gz').read_bytes()).hexdigest()==d['roi_sha256']
 a=np.frombuffer(gzip.decompress((case/'roi.f64.gz').read_bytes()),dtype='<f8').reshape(len(ROIS),64,11,3)
 return a,d

def variance_budget(y):
 """Two-factor random-effects method-of-moments, balanced single observation/cell.
 y: map, camera, ... . Interaction includes map/camera coupling. Negative main
 effect estimates are retained, exposing finite-replica uncertainty.
 """
 m,c=y.shape[:2];grand=y.mean(axis=(0,1));mm=y.mean(axis=1);cm=y.mean(axis=0)
 residual=y-mm[:,None]-cm[None,:]+grand
 im=(residual**2).sum(axis=(0,1))/((m-1)*(c-1))
 ms_m=c*((mm-grand)**2).sum(axis=0)/(m-1)
 ms_c=m*((cm-grand)**2).sum(axis=0)/(c-1)
 return (ms_m-im)/c,(ms_c-im)/m,im

def covariance_summary(a):
 # Replica, pixel, component, XYZ. Observer Y is the scalar reported here.
 y=a[...,1];components=np.take(y,DISJOINT,axis=2)
 centered=components-components.mean(axis=0)
 cov=np.einsum('rpi,rpj->ij',centered,centered)/((len(a)-1)*a.shape[1])
 total_var=float(y[:,:,0].var(axis=0,ddof=1).mean())
 assert math.isclose(float(cov.sum()),total_var,rel_tol=1e-9,abs_tol=1e-15)
 total=y[:,:,0];error=total-total.mean(axis=0)
 squares=error**2
 top=max(1,math.ceil(squares.size*.01));tail=float(np.sort(squares.ravel())[-top:].sum()/squares.sum()) if squares.sum() else 0
 means=total.mean(axis=1)
 return dict(total_Y_variance=total_var,relative_pixel_RMS=math.sqrt(total_var)/max(1e-300,float(total.mean())),
  component_covariance=cov.tolist(),signed_variance_attribution=(cov.sum(axis=1)/total_var).tolist() if total_var else [0.]*len(DISJOINT),
  sum_individual_variances=float(np.trace(cov)),cross_covariance=float(cov.sum()-np.trace(cov)),
  region_mean_Y_by_seed=means.tolist(),region_mean_standard_error=float(means.std(ddof=1)/math.sqrt(len(a))),
  largest_one_percent_squared_deviation_share=tail,maximum_observed_Y=float(total.max()))

def variance_ratio_sensitivity(low,high):
 """Paired leave-one-seed-out range; descriptive sensitivity, not a CI."""
 def measure(a):return float(a[:,:,0,1].var(axis=0,ddof=1).mean())
 ratios=[]
 for omit in range(len(low)):
  keep=[i for i in range(len(low)) if i!=omit]
  denominator=measure(low[keep])
  ratios.append(measure(high[keep])/denominator if denominator else None)
 finite=[x for x in ratios if x is not None]
 return dict(by_omitted_seed=ratios,range=[min(finite),max(finite)] if finite else None)

def xyz_variance(a):
 """Raw XYZ channel variance avoids mistaking luminance stability for color stability."""
 return a[:,:,0,:].var(axis=0,ddof=1).mean(axis=0)

def xyz_ratio(low,high):
 a=xyz_variance(low);b=xyz_variance(high)
 return dict(low_variance=a.tolist(),high_variance=b.tolist(),
             ratio=[float(y/x) if x else None for x,y in zip(a,b)],
             sum_channel_variance_ratio=float(b.sum()/a.sum()) if a.sum() else None)

def analyze(root):
 out={'schema':'optic_v3_error_budget_v1','seeds':SEEDS,'disjoint_components':[LABELS[k] for k in DISJOINT],
      'limits':'Four seeds per axis; conditional repeatability and interaction estimates, not ground-truth bias or a convergence certificate. Negative method-of-moments estimates are retained.','sources':{}}
 for source in SOURCES:
  records=[];cross=[]
  for m in SEEDS:
   row=[]
   for c in SEEDS:
    a,d=load(root,source,64,4,c,m);row.append(a);records.append(d)
   cross.append(row)
  cross=np.array(cross)
  highcam=np.array([load(root,source,256,4,c,19)[0] for c in SEEDS])
  highmap=np.array([load(root,source,64,16,19,m)[0] for m in SEEDS])
  # Each fixed-map sweep is actually the same map, not just the same seed.
  for m in SEEDS:
   same=[d for d in records if d['map_seed']==m]
   assert len({d['map']['map_key'] for d in same})==1
   assert all(d['map']['bands']==same[0]['map']['bands'] for d in same)
  assert all(load(root,source,256,4,c,19)[1]['map']['map_key']==records[0]['map']['map_key'] for c in SEEDS)
  basefilm=records[0]['film'];assert all(d['film']==basefilm for d in records)
  summary={}
  for region_index,region in enumerate(ROIS):
   lowcam=covariance_summary(cross[0,:,region_index]);cam=covariance_summary(highcam[:,region_index])
   lowmap=covariance_summary(cross[:,0,region_index]);maps=covariance_summary(highmap[:,region_index])
   vm,vc,vi=variance_budget(cross[:,:,region_index,:,0,1])
   summary[region]=dict(low_camera=lowcam,high_camera=cam,low_photon=lowmap,high_photon=maps,
    camera_variance_ratio_256_to_64=cam['total_Y_variance']/lowcam['total_Y_variance'],
    photon_variance_ratio_262144_to_65536=maps['total_Y_variance']/lowmap['total_Y_variance'],
    crossed_variance_estimate=dict(photon=float(vm.mean()),camera=float(vc.mean()),interaction=float(vi.mean())),
    mean_XYZ=cross[:,:,region_index,:,0,:].mean(axis=(0,1,2)).tolist(),
    camera_ratio_seed_sensitivity=variance_ratio_sensitivity(cross[0,:,region_index],highcam[:,region_index]),
    photon_ratio_seed_sensitivity=variance_ratio_sensitivity(cross[:,0,region_index],highmap[:,region_index]),
    camera_xyz_variance=xyz_ratio(cross[0,:,region_index],highcam[:,region_index]),
    photon_xyz_variance=xyz_ratio(cross[:,0,region_index],highmap[:,region_index]))
  out['sources'][source]=summary
 return out
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--matrix',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 result=analyze(a.matrix);a.output.write_text(json.dumps(result,indent=2));print('PASS covariance closure, fixed map identities, final cameras and crossed analysis')
