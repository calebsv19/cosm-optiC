"""Bounded crossed camera/photon audit of saved ordinary V3 component films.
No sampler or scene optics changes. The input directory contains the preceding
matched-light qualification; each source must supply <source>-64-s19/{scene,request}.json.
"""
import argparse,copy,gzip,hashlib,json,shutil,subprocess,time,sys
from array import array
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from spectral_components import verify,read,LABELS

SEEDS=(19,104729,1237,65537)
ROIS={'floor_focus':(118,138,16,16),'glint':(152,32,16,16),'beam':(118,116,16,16),
      'haze':(210,60,16,16),'floor_edge':(97,138,16,16),'colored_surface':(84,84,16,16)}
ROIS={name:(x+4,y+4,8,8) for name,(x,y,w,h) in ROIS.items()}
SOURCES=('tungsten','led')
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def write(path,obj):Path(path).write_text(json.dumps(obj,indent=2))
def specifications(pilot=False):
 cells=[]
 for m in SEEDS:
  for c in SEEDS:
   for source in SOURCES:cells.append((source,64,4,c,m))
 if pilot:return cells[:2]
 for c in SEEDS:
  for source in SOURCES:cells.append((source,256,4,c,19))
 for m in SEEDS:
  for source in SOURCES:cells.append((source,64,16,19,m))
 return cells

def run_cell(cli,inputs,root,spec,roi_audit=False):
 source,spp,passes,camera_seed,map_seed=spec
 name=f'{source}-c{camera_seed}-m{map_seed}-s{spp}-p{passes}'
 case=root/name;result=case/'result.json';base=inputs/f'{source}-64-s19'
 scene=json.loads((base/'scene.json').read_text());q=json.loads((base/'request.json').read_text())
 p=scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
 p.update(samples=spp,seed=camera_seed);p['caustics'].update(passes=passes,seed=map_seed)
 q['run_id']=name;q['scene']['runtime_scene_path']='scene.json';q['output']={'root':'render','overwrite':False}
 if result.exists():
  existing=json.loads(result.read_text());assert existing['binary_sha256']==digest(cli)
  assert json.loads((case/'scene.json').read_text())==scene and json.loads((case/'request.json').read_text())==q
  assert existing['scene_sha256']==digest(case/'scene.json') and existing['roi_sha256']==digest(case/'roi.f64.gz')
  return existing
 if case.exists():raise RuntimeError(f'Unfinished identity requires inspection: {case}')
 case.mkdir()
 # Same byte-identical retained mesh/config; no reauthoring or scene resizing.
 (case/'assets').symlink_to(base/'assets',target_is_directory=True)
 (case/'config').symlink_to(base/'config',target_is_directory=True)
 q['run_id']=name;q['scene']['runtime_scene_path']='scene.json';q['output']={'root':'render','overwrite':False}
 write(case/'scene.json',scene);write(case/'request.json',q)
 start=time.monotonic()
 with (case/'render.log').open('w') as log:
  subprocess.run([str(cli),'--request','request.json','--render','--summary','summary.json','--summary-file-only'],cwd=case,stdout=log,stderr=subprocess.STDOUT,timeout=300,check=True)
 elapsed=time.monotonic()-start
 report=json.loads((case/'summary.json').read_text())['spectral_transport']
 image=case/'render/frames/frame_0000.bmp';linear=Path(str(image)+'.components.npy')
 if roi_audit:
  from spectral_region_validation import verify_regions
  verified=verify_regions(report,image,ROIS)
 else:verified=verify(report,image)
 width,height,values=read(linear)
 selected=array('d')
 if roi_audit:
  assert (width,height)==(8,48);selected.extend(values)
 else:
  assert (width,height)==(256,192)
  for x,y,w,h in ROIS.values():
   for row in range(y,y+h):selected.extend(values[(row*width+x)*33:(row*width+x+w)*33])
 if sys.byteorder!='little':selected.byteswap()
 with gzip.open(case/'roi.f64.gz','wb',compresslevel=6) as stream:stream.write(selected.tobytes())
 out=dict(name=name,source=source,spp=spp,passes=passes,camera_seed=camera_seed,map_seed=map_seed,
          seconds=elapsed,binary_sha256=digest(cli),roi_sha256=digest(case/'roi.f64.gz'),rois=ROIS,
          components=list(LABELS),film=report['film'],map=report['smooth_glass'],verification=verified,
          scene_sha256=digest(case/'scene.json'),raw_retained=camera_seed==19 and map_seed==19,
          native_image_sha256=digest(image))
 write(result,out)
 if not out['raw_retained']:linear.unlink()
 print(name,'verified',round(elapsed,2),'s',flush=True)
 return out

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--inputs',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--pilot',action='store_true');ap.add_argument('--roi-audit',action='store_true');args=ap.parse_args()
 if not args.cli.is_file():ap.error("Build the CLI before starting an audit")
 args.output.mkdir(parents=True,exist_ok=True)
 specs=specifications(args.pilot);started=time.monotonic()
 # Pairs are independent and bounded. Never resubmit uncertain cells.
 with ThreadPoolExecutor(max_workers=2) as pool:
  if args.roi_audit:
   from spectral_region_validation import compare_full
   rows=list(pool.map(lambda source:run_cell(args.cli.resolve(),args.inputs.resolve(),args.output.resolve(),(source,64,16,19,19),True),SOURCES))
   checks=[]
   for row in rows:
    relative='render/frames/frame_0000.bmp.components.npy'
    check=compare_full(args.output/row['name']/relative,args.inputs/f"{row['source']}-64-s19"/relative,ROIS)
    check['source']=row['source'];checks.append(check)
   write(args.output/'parity.json',checks)
  for i in range(0,len(specs),2):
   if time.monotonic()-started>3300:raise RuntimeError('Matrix launch budget exhausted before next pair')
   rows=list(pool.map(lambda s:run_cell(args.cli.resolve(),args.inputs.resolve(),args.output.resolve(),s,args.roi_audit),specs[i:i+2]))
   write(args.output/'status.json',dict(last_pair=[r['name'] for r in rows],elapsed_seconds=time.monotonic()-started,completed=len(list(args.output.glob('*/result.json')))))
 print('COMPLETE',len(specs),'scheduled cells',flush=True)
if __name__=='__main__':main()
