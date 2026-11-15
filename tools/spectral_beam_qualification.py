"""Fixed-map independent seed and equal-CPU qualification of bounded glass/fog allocation.

Sequential alternating pairs; 180 regional launch ceiling, 300 s per job and
1800 s per phase. Discovery samples calibrate cost before held-out image review.
The 112-spp first cost match ran 15% high; held-out counts freeze at 96/128 for
bounded/camera-exit against 256 baseline. Results report actual cost ratios.
"""
from pathlib import Path
import sys,json,gzip,time,argparse,hashlib
import numpy as np
W=Path(__file__).resolve().parents[1];sys.path.insert(0,str(W/'tools'))
from spectral_camera_qualification import run
from spectral_camera_analysis import compare
from spectral_error_budget import ROIS
R=None;INPUTS=None  # Assigned by explicit CLI paths; no implicit workspace output.
DISC=(101,509,2027,8191,32749,131071,524287,2097143)
HELD=(211,1021,4093,16381,65521,262139,1048573,4194301)
MODES={'exit':{'medium_sampling':'split_first','camera_fresnel':'split_camera_exit'},'baseline':{'medium_sampling':'analog','camera_fresnel':'split_first'},'combined':{'medium_sampling':'split_first','camera_fresnel':'split_bounded'}}
def verify_fixed(root):
 """Verify the permitted camera/estimator axes; keep map and scene state exact."""
 first={};count=0
 for path in root.glob('*/result.json'):
  d=json.loads(path.read_text());case=path.parent
  assert hashlib.sha256((case/'scene.json').read_bytes()).hexdigest()==d['scene_sha256']
  assert hashlib.sha256((case/'roi.f64.gz').read_bytes()).hexdigest()==d['roi_sha256']
  scene=json.loads((case/'scene.json').read_text())
  p=scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
  for key in ('samples','seed','camera_sampler','medium_sampling'):p.pop(key,None)
  p['caustics'].pop('camera_fresnel',None)
  ref,report=first.setdefault(d['source'],(scene,d))
  assert scene==ref and d['film']==report['film']
  for key in ('map_key','ballistic_launch_key','bands','deposits','launched','radius_m','volume_cell_m'):
   assert d['map'][key]==report['map'][key],(case,key)
  count+=1
 (root/'invariants.json').write_text(json.dumps({'cases':count,'fixed_scene_map_film':True},indent=2))

def phase(cli,name,seeds,arms):
 assert len(list(R.glob('*/*/process.json')))<180, 'Regional launch bound exhausted'
 root=R/name;root.mkdir(exist_ok=True);start=time.monotonic();records=[]
 for i,seed in enumerate(seeds):
  for source in ('tungsten','led'):
   for mode,spp in (arms if i%2==0 else arms[::-1]):
    if time.monotonic()-start>1800:raise RuntimeError('phase launch time bound')
    assert len(list(R.glob('*/*/process.json')))<180, 'Regional launch bound exhausted'
    d=run(cli,INPUTS,root,source,spp,seed,'hashed_v1',transport=MODES[mode]);d['arm']=mode;records.append(d)
 verify_fixed(root)
 (root/'complete.json').write_text(json.dumps({'cases':len(records),'seconds':time.monotonic()-start,'arms':arms,'seeds':seeds},indent=2));return records

def summarize(root,reference=None):
 ds=[json.loads(p.read_text()) for p in root.glob('*/result.json')]
 if reference:ds += [json.loads(p.read_text())|{'external_root':str(reference)} for p in reference.glob('*/result.json') if '-glass_split_first' in str(p)]
 out={}
 for source in ('tungsten','led'):
  groups={}
  for d in ds:
   if d['source']!=source:continue
   typ=d['map']['camera_fresnel'];mode='baseline' if d['quality']['medium_sampling']=='analog' else typ
   groups.setdefault(mode,[]).append(d)
  for group in groups.values():group.sort(key=lambda d:d['camera_seed'])
  if 'baseline' not in groups:continue
  def data(rows):
   a=[]
   for d in rows:
    p=Path(d.get('external_root',root))/d['name'];a.append(np.frombuffer(gzip.decompress((p/'roi.f64.gz').read_bytes()),dtype='<f8').reshape(6,64,11,3))
   return np.array(a)
  base=groups.pop('baseline');a=data(base);out[source]={}
  for mode,rows in groups.items():
   assert [d['camera_seed'] for d in rows]==[d['camera_seed'] for d in base]
   b=data(rows);rr={r:compare(a[:,i],b[:,i],r) for i,r in enumerate(ROIS)}
   cpu=np.array([d['cpu_seconds'] for d in rows])/np.array([d['cpu_seconds'] for d in base])
   out[source][mode]={'regions':rr,'mean_cpu_ratio':float(sum(d['cpu_seconds'] for d in rows)/sum(d['cpu_seconds'] for d in base)),'paired_cpu_ratios':cpu.tolist(),'spp':rows[0]['spp'],'baseline_spp':base[0]['spp'],'median_variance':float(np.median([r['Y_variance_ratio'] for r in rr.values()])), 'all_region_gates':all(r['mean_pass'] and r['variance_pass'] for r in rr.values())}
 (root/'analysis.json').write_text(json.dumps(out,indent=2))
 for s,ms in out.items():
  for m,b in ms.items():print(s,m,'CPU',round(b['mean_cpu_ratio'],3),'gates',b['all_region_gates'],[(r,round(x['Y_variance_ratio'],3),x['mean_pass']) for r,x in b['regions'].items()],flush=True)
 return out
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--phase',choices=('discovery','equal-combined','candidate','heldout','original'),required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--inputs',type=Path,required=True);args=ap.parse_args();R=args.output.resolve();INPUTS=args.inputs.resolve()
 if args.phase=='discovery':phase(args.cli,'discovery',DISC,[('baseline',256),('combined',256)]);summarize(R/'discovery')
 if args.phase=='equal-combined':phase(args.cli,'equal-combined',DISC,[('baseline',256),('combined',112)]);summarize(R/'equal-combined')

 if args.phase=='candidate':phase(args.cli,'candidate',DISC,[('baseline',256),('exit',256)]);summarize(R/'candidate')

 if args.phase=='heldout':phase(args.cli,'heldout',HELD,[('baseline',256),('combined',96),('exit',128)]);summarize(R/'heldout')
 if args.phase=='original':phase(args.cli,'original',(19,104729,1237,65537,7919,16807),[('exit',256)])
