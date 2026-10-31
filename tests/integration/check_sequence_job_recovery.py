"""Native queued cuts/independent views, interrupted resume and completion proofs."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from sequence_video import assemble
from sequence_temporal_review import verify_video
from fractions import Fraction

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--fixture',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--runner',type=Path,required=True);a=p.parse_args()
 r=a.out.resolve();r.mkdir();fixture=r/'input';shutil.copytree(a.fixture,fixture);jobs=r/'jobs';jobs.mkdir()
 env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(r/'program'),XDG_CONFIG_HOME=str(r/'config'));(r/'program').mkdir();(r/'config').mkdir()
 runner=a.runner.resolve()
 def command(*args,ok=True):
  x=subprocess.run([str(runner),*map(str,args),'--jobs-root',str(jobs)],env=env,capture_output=True,text=True,timeout=135)
  if ok:assert x.returncode==0,(args,x.returncode,x.stderr,x.stdout[-1000:])
  return x
 def request(name,root,camera=None):
  q=read(fixture/'render_request.json');q['scene']=dict(runtime_scene_path=str(fixture/'scene_runtime.json'))
  if camera:q['scene']['camera_id']=camera
  q['render'].update(width=80,height=60,temporal_frames=1);q['output']=dict(root=str(root),overwrite=False);q.pop('progress',None)
  q['resources']=dict(cpu_percent=25,max_workers=1,reserve_cpu_count=1)
  path=r/(name+'.json');path.write_text(json.dumps(q,indent=2)+'\n');return path
 def submit(req,resume=False):
  return json.loads(command('submit','--request',req,*(['--resume'] if resume else [])).stdout)['job_id']
 def wait(job,ok=True):
  x=command('wait','--job-id',job,'--timeout-seconds','120',ok=ok)
  (r/(job+'-wait.json')).write_text(x.stdout);return x
 scene=fixture/'scene_runtime.json';scene_hash=sha(scene)
 edit=r/'edit';eid=submit(request('edit-request',edit));read_status=json.loads(wait(eid).stdout)
 assert read_status['completion_verified'] is True and read_status['execution_mode']=='worker_protocol'
 independent=r/'independent';iid=submit(request('independent-request',independent,'camera/event_moving_event'));assert json.loads(wait(iid).stdout)['completion_verified'] is True
 partial=r/'resumed';req=request('interrupted-request',partial);jid=submit(req)
 deadline=time.monotonic()+120
 while len(list((partial/'frames').glob('*.sequence.json')))<45:
  assert time.monotonic()<deadline,'no interruption checkpoint';time.sleep(.01)
 command('cancel','--job-id',jid)
 cancelled=wait(jid,False);assert cancelled.returncode==1
 events=[read(p) for p in (jobs/jid/'worker_events').glob('*.json')]
 assert any(e['message_type']=='interruption' for e in events),'native stop not witnessed'
 images=sorted((partial/'frames').glob('*.bmp'));prefix=len(images);assert 0<prefix<102,prefix
 retained={p.name:sha(p) for p in (partial/'frames').iterdir() if p.is_file()}
 for label,change in [('wrong-source-tick',lambda row:row.update(source_tick=row['source_tick']+1)),('wrong-camera',lambda row:row.update(camera_id='camera/A'))]:
  bad=r/label;shutil.copytree(partial,bad);target=bad/'frames/frame_0012.bmp.sequence.json';row=read(target);change(row);target.write_text(json.dumps(row)+'\n')
  before={str(p.relative_to(bad)):sha(p) for p in bad.rglob('*') if p.is_file()}
  failure=command('submit','--request',request(label+'-request',bad),'--resume',ok=False)
  assert failure.returncode!=0 and 'sequence receipt/hash/source/view mismatch' in failure.stderr,(label,failure.stderr)
  assert before=={str(p.relative_to(bad)):sha(p) for p in bad.rglob('*') if p.is_file()}
 rid=submit(req,True);resumed=json.loads(wait(rid).stdout);assert resumed['completion_verified'] is True
 assert all(sha(partial/'frames'/name)==digest for name,digest in retained.items()),'retained prefix overwritten'
 rows=[]
 for root in (edit,independent,partial):rows.append([read(root/'frames'/f'frame_{i:04d}.bmp.sequence.json') for i in range(12,114)])
 physical=('frame','rate','presentation_time','simulation_time','source_tick','source_revision','bodies')
 camera=('camera_id','shot_id','camera','camera_yaw','camera_pitch','camera_fov_y','camera_frame','camera_motion_time')
 for i,(x,y,z) in enumerate(zip(*rows)):
  assert all(x[k]==y[k]==z[k] for k in physical),(i,'physical')
  assert all(x[k]==z[k] for k in camera),(i,'resumed camera')
  assert x['image_sha256']==z['image_sha256'],(i,'resumed pixels')
  if not 45<=i<51:assert all(x[k]==y[k] for k in camera if k not in ('camera_selected',)),(i,'independent')
 assert rows[1][0]['camera']!=rows[1][14]['camera']
 assert all(x['bodies']==rows[1][15]['bodies'] for x in rows[1][15:87])
 assert rows[1][15]['camera']!=rows[1][86]['camera']
 assert rows[1][88]['source_tick']>rows[1][87]['source_tick']
 videos=[]
 for label,root in [('edit',edit),('independent',independent),('resumed',partial)]:
  video=r/(label+'.mp4');proof=assemble(scene,root/'frames',video,camera_id='camera/event_moving_event' if label=='independent' else None,encoder='mpeg4');verify_video(video,Fraction(24),102);videos.append(dict(path=str(video),sha256=sha(video),frame_count=proof['frame_count'],all_pts_exact=proof['all_pts_exact'],decoded=proof['decoded']))
 # Malformed completion and timeout must never become success or a new submit.
 fake=jobs/'forged_completion';fake.mkdir();(fake/'job_status.json').write_text(json.dumps(dict(schema_version='ray_tracing_detached_job_status_v1',job_id=fake.name,state='completed',stage='completed',pid=0,exit_code=0)))
 (fake/'result_summary.json').write_text('{}\n');x=wait(fake.name,False);assert x.returncode==1 and not json.loads(x.stdout)['completion_verified']
 pending=jobs/'timeout_observation';pending.mkdir();(pending/'job_status.json').write_text(json.dumps(dict(schema_version='ray_tracing_detached_job_status_v1',job_id=pending.name,state='running',stage='rendering',pid=os.getpid(),updated_at_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()))))
 x=command('wait','--job-id',pending.name,'--timeout-seconds','1',ok=False);assert x.returncode==124 and not json.loads(x.stdout)['completion_verified']
 broken=jobs/'unrelated_broken_history';broken.mkdir()
 (broken/'job_status.json').write_text(json.dumps(dict(schema_version='ray_tracing_detached_job_status_v1',job_id=broken.name,state='starting',stage='starting',pid=0)))
 assert json.loads(wait(eid).stdout)['completion_verified'] is True,'unrelated history blocked selected wait'
 assert sha(scene)==scene_hash
 result=dict(ok=True,native_queue=True,worker_protocol=True,frames_per_view=102,fps=24,duration='17/4',prefix_preserved=prefix,resume_remaining=102-prefix,physical_comparisons=102,camera_resume_comparisons=102,independent_noncut_comparisons=96,pixel_resume_comparisons=102,source_and_view_refusals=2,malformed_completion_refused=True,wait_timeout_same_identity=True,unrelated_history_did_not_block=True,videos=videos,scene_unchanged=True,production_fleet_dispatch=False)
 (r/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('OPTIC_QUEUED_RECOVERY_RESULT '+json.dumps(result),flush=True)
if __name__=='__main__':main()
