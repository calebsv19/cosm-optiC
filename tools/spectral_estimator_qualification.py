"""Separate fixed-map estimator and fixed-camera photon precision controls."""
from pathlib import Path
import sys,json,gzip,time,argparse
from concurrent.futures import ThreadPoolExecutor
import numpy as np
import spectral_mixed_reference as f
from spectral_camera_qualification import run
from spectral_error_budget import SEEDS,SOURCES
MODES={'medium':{'medium_sampling':'split_first','camera_fresnel':'split_first'},
 'glass':{'medium_sampling':'analog','camera_fresnel':'split_bounded'},
 'combined':{'medium_sampling':'split_first','camera_fresnel':'split_bounded'}}

def regions(cli,inputs,out,baseline):
 out.mkdir(exist_ok=False,parents=True);start=time.monotonic()
 for source in SOURCES:
  d=run(cli,inputs,out,source,256,19,'hashed_v1')
  old=baseline/f'{source}-c19-m19-s256-p4/roi.f64.gz'
  assert gzip.decompress((out/d['name']/'roi.f64.gz').read_bytes())==gzip.decompress(old.read_bytes())
 print('PASS exact legacy replay both sources after traversal extraction',flush=True)
 spec=[(s,c,m) for m in MODES for c in (*SEEDS,7919,16807) for s in SOURCES]
 with ThreadPoolExecutor(max_workers=2) as pool:
  for i in range(0,len(spec),2):
   if time.monotonic()-start>1800:raise RuntimeError('Regional launch bound exhausted')
   list(pool.map(lambda x:run(cli,inputs,out,x[0],256,x[1],'hashed_v1',transport=MODES[x[2]]),spec[i:i+2]))
 (out/'complete.json').write_text(json.dumps({'cases':38,'seconds':time.monotonic()-start},indent=2))

def photons(cli,out):
 out.mkdir(exist_ok=False,parents=True);start=time.monotonic();rows=[]
 ref=f.glass_fog_reference(550,n=240,k=32)
 for passes,seeds in ((4,f.SEEDS[:8]),(16,f.SEEDS)):
  values=[]
  for seed in seeds:
   if time.monotonic()-start>1200:raise RuntimeError('Photon precision launch bound exhausted')
   q,s,m=f.fixture('glass_fog',550,19,cell=.01);p=f.profile(s)
   p['samples']=8192;p['caustics'].update(passes=passes,seed=seed)
   d=f.run(cli,out,f'photon-p{passes}-m{seed}-camera19-s8192',q,s,m)
   values.append(f.projected(d['components'][6],550))
  arr=np.array(values);row={'passes':passes,'photons':passes*16384,'map_seeds':list(seeds),'camera_seed':19,'camera_samples':8192,'cell_m':.01,'values':values,'reference':ref,'mean':float(arr.mean()),'relative_error':float(arr.mean()/ref-1),'map_relative_SD':float(arr.std(ddof=1)/ref),'mean_relative_SE':float(arr.std(ddof=1)/np.sqrt(len(arr))/ref)}
  row['mean_gate']=abs(row['relative_error'])<.1;row['precision_gate']=row['mean_relative_SE']<.03
  rows.append(row);(out/'assessment.json').write_text(json.dumps(rows,indent=2));print('PHOTON',passes,row,flush=True)
 (out/'complete.json').write_text(json.dumps({'cases':24,'seconds':time.monotonic()-start,'scope':'camera/reconstruction held fixed; precision of ensemble mean is not one-map noise'},indent=2))

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--inputs',type=Path);ap.add_argument('--baseline',type=Path);ap.add_argument('--phase',choices=('regions','photons'),required=True);a=ap.parse_args()
 if a.phase=='regions':regions(a.cli.resolve(),a.inputs.resolve(),a.output.resolve(),a.baseline.resolve())
 else:photons(a.cli.resolve(),a.output.resolve())
