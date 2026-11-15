"""Fixed-map camera sampler comparison. Independent seeds, no denoise or fit."""
import argparse,gzip,hashlib,json,math
from pathlib import Path
import numpy as np
from spectral_error_budget import SEEDS,SOURCES,ROIS
from spectral_camera_qualification import HELDOUT
from spectral_error_analysis import load,covariance_summary,xyz_ratio,variance_ratio_sensitivity

def candidate(root,source,spp,seed,mode):
 p=root/f'{source}-{mode}-s{spp}-c{seed}';d=json.loads((p/'result.json').read_text());raw=(p/'roi.f64.gz').read_bytes()
 assert hashlib.sha256(raw).hexdigest()==d['roi_sha256']
 return np.frombuffer(gzip.decompress(raw),dtype='<f8').reshape(len(ROIS),64,11,3),d

def compare(a,b,region):
 va=covariance_summary(a);vb=covariance_summary(b)
 delta=(b[:,:,0,1]-a[:,:,0,1]).mean(axis=1)
 se=float(delta.std(ddof=1)/math.sqrt(len(delta)));mean=float(delta.mean())
 allowance=max(.05*float(a[:,:,0,1].mean()),3*se)
 ratio=vb['total_Y_variance']/va['total_Y_variance']
 return dict(baseline=va,candidate=vb,Y_variance_ratio=ratio,RMS_ratio=math.sqrt(ratio),
  paired_mean_difference=mean,paired_mean_standard_error=se,mean_allowance=allowance,mean_pass=abs(mean)<=allowance,
  variance_gate=1.0 if region=='beam' else 1.1,variance_pass=ratio<=(1.0 if region=='beam' else 1.1),
  xyz=xyz_ratio(a,b),leave_one_out=variance_ratio_sensitivity(a,b) if len(a)>2 else None)

def analyze(old,new,physical):
 out=dict(schema='optic_v3_camera_sampling_v1',default='hashed_v1',candidate='scrambled_path_v1',
  limits='Small exact-camera patches; fixed photon maps. Four original plus two held-out seeds; no whole-film, temporal or asymptotic convergence claim.',sources={})
 ratios=[];passes=[]
 for source in SOURCES:
  base=[load(old,source,256,4,c,19) for c in SEEDS]
  trial=[candidate(new,source,256,c,'scrambled_path_v1') for c in SEEDS]
  hb=[candidate(new,source,256,c,'hashed_v1') for c in HELDOUT]
  ht=[candidate(new,source,256,c,'scrambled_path_v1') for c in HELDOUT]
  records=base+trial+hb+ht
  # Scene controls must match byte-for-value after removing only authorized
  # camera sample/audit fields. In particular, fog and reconstruction stay fixed.
  scenes=[]
  for _,d in records:
   case=(old if 'mode' not in d else new)/d['name'];raw=(case/'scene.json').read_bytes()
   assert hashlib.sha256(raw).hexdigest()==d['scene_sha256']
   scene=json.loads(raw);profile=scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
   for field in ('samples','seed','camera_sampler','sample_audit','path_trace'):profile.pop(field,None)
   scenes.append(scene)
  assert all(scene==scenes[0] for scene in scenes)
  first=records[0][1]
  for _,d in records:
   assert d['film']==first['film']
   for key in ('map_key','ballistic_launch_key','bands','deposits','launched','radius_m','volume_cell_m'):
    assert d['map'][key]==first['map'][key],(source,key)
  a,b,ha,hbarr=[np.array([v[0] for v in rows]) for rows in (base,trial,hb,ht)]
  cpu=float(np.mean([d['cpu_seconds'] for _,d in ht])/np.mean([d['cpu_seconds'] for _,d in hb]))
  region_rows={}
  for i,region in enumerate(ROIS):
   original=compare(a[:,i],b[:,i],region);heldout=compare(ha[:,i],hbarr[:,i],region)
   combined=compare(np.concatenate((a[:,i],ha[:,i])),np.concatenate((b[:,i],hbarr[:,i])),region)
   heldout['cpu_time_ratio']=cpu;heldout['CPU_variance_product_ratio']=cpu*heldout['Y_variance_ratio']
   region_rows[region]=dict(original=original,heldout=heldout,combined=combined)
   ratios.append(original['Y_variance_ratio']);passes.extend([original['mean_pass'],original['variance_pass']])
  work={}
  for key in ('camera_primary_paths','camera_reflections','camera_transmissions','camera_total_internal_reflections'):
   work[key]={mode:[d['map'][key] for _,d in rows] for mode,rows in [('baseline',hb),('candidate',ht)]}
  out['sources'][source]=dict(regions=region_rows,heldout_cpu_ratio=cpu,path_work=work)
 rows=json.loads((physical/'assessment.json').read_text())
 # Baseline nine groups and the previously failed 1cm volume cell are mandatory.
 baseline=[r for r in rows if not ('reconstruction_level' in r or 'spectral_bins' in r)]
 fine=[r for r in rows if r.get('kind')=='glass_fog' and r.get('reconstruction_level')==.01]
 out['physical']=dict(baseline_groups=baseline,fine_volume=fine,complete=(physical/'qualification-complete.json').is_file())
 physics= len(baseline)==9 and len(fine)==1 and all(r['mean_gate_pass'] and r['precision_gate_pass'] for r in baseline+fine) and out['physical']['complete']
 median=float(np.median(ratios));out['acceptance']=dict(median_Y_variance_ratio=median,median_gate=.85,original_region_gates=all(passes),physical_pass=physics,
  heldout_pass=all(d[k] for s in out['sources'].values() for r in s['regions'].values() for d in [r['heldout']] for k in ['mean_pass','variance_pass']))
 out['acceptance']['promote']=median<=.85 and all(passes) and physics and out['acceptance']['heldout_pass']
 return out
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('baseline','matrix','physical','output'):p.add_argument('--'+key,type=Path,required=True)
 a=p.parse_args();d=analyze(a.baseline,a.matrix,a.physical);a.output.write_text(json.dumps(d,indent=2));print(json.dumps(d['acceptance'],indent=2))
