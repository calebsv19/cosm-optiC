"""Bounded fixed-map camera sampler comparison and sample-tail replay fixtures."""
import argparse,copy,gzip,hashlib,json,os,re,signal,subprocess,time,sys
from array import array
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from spectral_error_budget import ROIS,SEEDS,SOURCES
from spectral_region_validation import verify_regions
from spectral_components import read
HELDOUT=(7919,16807)
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2))
def run(cli,inputs,root,source,spp,seed,mode,audit=None,trace=None,transport=None,maps=None):
 suffix=('' if audit is None else '-audit'+','.join(map(str,audit)))+('' if trace is None else '-trace'+str(trace))
 if transport:
  assert {'medium_sampling','camera_fresnel'}<=set(transport)<= {'medium_sampling','camera_fresnel','continuation_roulette','volume_direction_sampling'}
  suffix+='-fog_'+transport['medium_sampling']+'-glass_'+transport['camera_fresnel']
  if 'volume_direction_sampling' in transport:suffix+='-dir_'+transport['volume_direction_sampling']
  if 'continuation_roulette' in transport:suffix+='-rr_'+transport['continuation_roulette']
 if maps:
  assert set(maps)=={'passes','seed'} and maps['passes']>0
  suffix+=f"-maps_p{maps['passes']}_m{maps['seed']}"
 name=f'{source}-{mode}-s{spp}-c{seed}'+suffix;case=root/name;base=inputs/f'{source}-64-s19'
 scene=json.loads((base/'scene.json').read_text());q=json.loads((base/'request.json').read_text())
 p=scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1'];p.update(samples=spp,seed=seed,camera_sampler=mode)
 p['caustics'].update(passes=4,seed=19)
 if maps:p['caustics'].update(maps)
 if transport:p['medium_sampling']=transport['medium_sampling'];p['caustics']['camera_fresnel']=transport['camera_fresnel']
 if transport and 'continuation_roulette' in transport:p['continuation_roulette']=transport['continuation_roulette']
 if transport and 'volume_direction_sampling' in transport:p['volume_direction_sampling']=transport['volume_direction_sampling']
 if audit:p['sample_audit']={'x':audit[0],'y':audit[1],'limit':8}
 if trace is not None:p['path_trace']={'x':audit[0],'y':audit[1],'sample':trace}
 q.update(run_id=name,output={'root':'render','overwrite':False});q['scene']['runtime_scene_path']='scene.json'
 if (case/'result.json').exists():
  result=json.loads((case/'result.json').read_text());assert result['binary_sha256']==digest(cli)
  assert json.loads((case/'scene.json').read_text())==scene and json.loads((case/'request.json').read_text())==q
  return result
 if case.exists():raise RuntimeError('Inspect unfinished identity '+str(case))
 case.mkdir(parents=True);(case/'assets').symlink_to(base/'assets',target_is_directory=True);(case/'config').symlink_to(base/'config',target_is_directory=True)
 write(case/'scene.json',scene);write(case/'request.json',q);start=time.monotonic()
 with (case/'render.log').open('w') as log,(case/'cpu.log').open('w') as cpu:
  child=subprocess.Popen(['/usr/bin/time','-p',str(cli),'--request','request.json','--render','--summary','summary.json','--summary-file-only'],cwd=case,stdout=log,stderr=cpu,start_new_session=True)
  write(case/'process.json',{'pid':child.pid,'process_group':child.pid,'started_unix':time.time()})
  try:code=child.wait(timeout=300)
  except subprocess.TimeoutExpired:
   os.killpg(child.pid,signal.SIGTERM)
   try:child.wait(timeout=5)
   except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait()
   write(case/'failure.json',{'reason':'300 second bound','terminated_group':child.pid});raise
  if code:raise RuntimeError(f'{name} returned {code}; inspect retained summary and logs')
 report=json.loads((case/'summary.json').read_text())['spectral_transport'];image=case/'render/frames/frame_0000.bmp'
 verified=verify_regions(report,image,ROIS);_,_,values=read(str(image)+'.components.npy')
 data=array('d',values)
 if sys.byteorder!='little':data.byteswap()
 with gzip.open(case/'roi.f64.gz','wb') as f:f.write(data.tobytes())
 timing={k:float(v) for k,v in re.findall(r'^(real|user|sys)\s+([0-9.]+)',(case/'cpu.log').read_text(),re.M)}
 assert 'user' in timing and 'sys' in timing
 out=dict(name=name,source=source,spp=spp,camera_seed=seed,mode=mode,seconds=time.monotonic()-start,cpu_seconds=timing['user']+timing['sys'],binary_sha256=digest(cli),scene_sha256=digest(case/'scene.json'),roi_sha256=digest(case/'roi.f64.gz'),verification=verified,map=report['smooth_glass'],film=report['film'],audit=report['sample_audit'],path_trace=report['path_trace'],quality=report['quality'])
 if 'nonscattered_indirect' in report:out['nonscattered_indirect']=report['nonscattered_indirect']
 if maps:out['photon_budget']=dict(maps)
 if 'volume_light_diagnostics' in report:out['volume_light_diagnostics']=report['volume_light_diagnostics']
 assert out['quality']['camera_sampler']==mode
 if transport:
  assert report['termination']['all_started_paths_classified']
  if 'volume_direction_sampling' in transport:assert out['quality']['volume_direction_sampling']==transport['volume_direction_sampling']
  if 'continuation_roulette' in transport:assert out['quality']['continuation_roulette']==transport['continuation_roulette']
  assert out['quality']['medium_sampling']==transport['medium_sampling']
  assert out['map']['camera_fresnel']==transport['camera_fresnel']
  assert report['termination']['camera_paths']<=384*spp*out['quality']['maximum_camera_branches']
  out['termination']=report['termination']
 write(case/'result.json',out);print(name,'verified',round(out['seconds'],2),'s',flush=True);return out

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--cli',type=Path,required=True);a.add_argument('--inputs',type=Path,required=True);a.add_argument('--output',type=Path,required=True);a.add_argument('--phase',choices=('candidate','heldout','audit'),required=True);args=a.parse_args();assert args.cli.is_file()
 root=args.output.resolve();root.mkdir(parents=True,exist_ok=True)
 if (root/'STOP_BEFORE_NEXT_PHASE').exists():raise RuntimeError('Qualification phase held by its owner')
 start=time.monotonic();spec=[]
 if args.phase=='candidate':spec=[(s,n,c,'scrambled_path_v1') for n in (64,256) for c in (SEEDS if n==256 else SEEDS[:1]) for s in SOURCES]
 elif args.phase=='heldout':spec=[(s,256,c,mode) for c in HELDOUT for s in SOURCES for mode in ('hashed_v1','scrambled_path_v1')]
 else:spec=[('tungsten',256,65537,'hashed_v1',(129,125)),('tungsten',256,1237,'hashed_v1',(127,125)),('led',256,19,'hashed_v1',(160,40))]
 with ThreadPoolExecutor(max_workers=2) as pool:
  for i in range(0,len(spec),2):
   if time.monotonic()-start>2100:raise RuntimeError('Bounded phase launch budget exhausted')
   list(pool.map(lambda x:run(args.cli.resolve(),args.inputs.resolve(),root,*x),spec[i:i+2]))
 write(root/(args.phase+'-complete.json'),dict(cases=len(spec),wall_seconds=time.monotonic()-start))
if __name__=='__main__':main()
