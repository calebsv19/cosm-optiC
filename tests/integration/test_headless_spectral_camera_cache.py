"""Native camera-cache parity and live convergence publication, including write failure."""
import argparse,copy,hashlib,json,subprocess,time
from pathlib import Path
REPO=Path(__file__).resolve().parents[2]
def qualify(cli,root,repo=REPO):
 root.mkdir(parents=True,exist_ok=False);(root/'config').symlink_to(repo/'config');(root/'data/runtime/videos').mkdir(parents=True)
 q=json.loads((repo/'tests/fixtures/spectral_volume/request.json').read_text());q['scene']['runtime_scene_path']=str(repo/'tests/fixtures/spectral_prism/scene_runtime.json');q['spectral'].update(glass_id='prism',glass='schott_n_bk7');q['render'].update(width=48,height=32);q['spectral']['surface_map']['photons_per_source']=4096
 def run(name,request,valid=True,live=False):
  request=copy.deepcopy(request);d=root/name;d.mkdir();request['output']=dict(root=str(d/'output'),overwrite=False);request['progress']=dict(progress_path=str(d/'progress.json'))
  path=d/'request.json';path.write_text(json.dumps(request));events=[];observed=False
  if name=='publication_failure':(d/'progress.json.spectral.json').mkdir()
  with (d/'stdout.log').open('w') as out,(d/'stderr.log').open('w') as log:
   p=subprocess.Popen([str(cli),'--request',str(path),'--render','--summary',str(d/'summary.json'),'--summary-file-only'],cwd=root,stdout=out,stderr=subprocess.PIPE,text=True)
   for line in p.stderr:
    log.write(line)
    if line.startswith('SPECTRAL_PROGRESS '):
     event=json.loads(line[len('SPECTRAL_PROGRESS '):]);events.append(event)
     if p.poll() is None and (d/'progress.json.spectral.json').is_file():
      snap=json.loads((d/'progress.json.spectral.json').read_text());assert snap['run_id']==request['run_id'];observed=True
   code=p.wait()
  assert (code==0)==valid,(name,code,(d/'stderr.log').read_text()[-500:])
  if not valid:
   assert not (d/'output/frames/frame_0000.bmp').exists();return
  summary=json.loads((d/'summary.json').read_text());image=(d/'output/frames/frame_0000.bmp').read_bytes()
  if live:
   assert observed and any(e['stage']=='preparing_map' for e in events)
   checkpoints=[e for e in events if e['stage']=='checkpoint'];assert len(checkpoints)==2
   assert [e['sampling']['passes'] for e in checkpoints]==[8,16]
   assert checkpoints[-1]['sampling']==summary['spectral_sampling']
   assert json.loads((d/'progress.json.spectral.json').read_text())['sampling']==summary['spectral_sampling']
   assert all(b['elapsed_seconds']>=a['elapsed_seconds'] for a,b in zip(events,events[1:]))
   assert summary['spectral_sampling']['status']=='converged'
  return summary,hashlib.sha256(image).hexdigest()
 for mode in ('box','continuous','adaptive'):
  volume=q['spectral']['participating_volume_v1'];volume['reconstruction']=mode
  volume['camera_spatial_cache']=False;ref,h=run(mode+'-reference',q)
  volume['camera_spatial_cache']=True;candidate,c=run(mode+'-cached',q)
  assert h==c and candidate['spectral_volume']['bands']==ref['spectral_volume']['bands']
  assert candidate['spectral_center_pixel_xyz']==ref['spectral_center_pixel_xyz']
  assert candidate['spectral_camera_cache']['hits']>0 and ref['spectral_camera_cache']['bytes']==0
 # Closed convex prism geometry is now admitted by continuous reconstruction.
 bounds=q['spectral']['participating_volume_v1']['bounds_min_m'][:]
 q['spectral']['participating_volume_v1']['bounds_min_m'][0]=.1
 q['spectral']['participating_volume_v1']['camera_spatial_cache']=False
 ref,h=run('intersecting-prism-reference',q)
 q['spectral']['participating_volume_v1']['camera_spatial_cache']=True
 candidate,c=run('intersecting-prism-admitted',q)
 assert h==c and candidate['spectral_center_pixel_xyz']==ref['spectral_center_pixel_xyz']
 assert candidate['spectral_volume']['bands']==ref['spectral_volume']['bands']
 q['spectral']['participating_volume_v1']['bounds_min_m']=bounds
 q['spectral']['sampling']=dict(mode='converge',max_passes=16,visible_error=255,edge_error=255,energy_error=1)
 q['render'].update(width=24,height=16);q['spectral']['surface_map']['photons_per_source']=1024
 a,h=run('live',q,live=True);b,j=run('replay',q,live=True);assert h==j and a['spectral_sampling']==b['spectral_sampling']
 run('publication_failure',q,valid=False)
 q['spectral']['participating_volume_v1']['camera_spatial_cache']=1;run('invalid-cache-type',q,valid=False)
 (root/'result.json').write_text(json.dumps(dict(passed=True,live_before_completion=True,exact_cache_image_and_band_parity=True,deterministic_replay=True,publication_failure_refused=True),indent=2))
 print('PASS native box/continuous/adaptive cache parity, live checkpoint readback, replay and publication failure')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--cli',type=Path,required=True);p.add_argument('--output-root',type=Path,required=True);p.add_argument('--repo',type=Path,default=REPO);a=p.parse_args();qualify(a.cli.resolve(),a.output_root.resolve(),a.repo.resolve())
