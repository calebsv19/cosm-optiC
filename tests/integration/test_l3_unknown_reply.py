#!/usr/bin/env python3
"""Local durable job survives a lost submit reply; recover the same explicit identity."""
import argparse,hashlib,json,os,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runner',type=Path,required=True);p.add_argument('--output-root',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[2];out=a.output_root.resolve();out.mkdir(parents=True,exist_ok=False);jobs=out/'jobs';jobs.mkdir();bundle=out/'bundle';(bundle/'input').mkdir(parents=True)
request=json.loads((r/'tests/fixtures/agent_render_job_runner_bundle_request.json').read_text());request['render'].update(width=32,height=24,temporal_frames=1);(bundle/'input/request.json').write_text(json.dumps(request))
job_id='l3-unknown-reply';envelope={'schema_family':'codework_job','schema_variant':'headless_bundle_v1','job_id':job_id,'program':'ray_tracing','tool':{'name':'ray_tracing','version':'0.1.0','target_os':'macos','target_arch':'arm64'},'scene_payload':{'schema_family':'codework_scene','schema_variant':'scene_runtime_v1','path':str(r/'config/samples/ps4d_runtime_scene_visual_test.json')},'run_config':{'schema_family':'ray_tracing_request','schema_variant':'ray_tracing_agent_render_request_v1','path':'input/request.json'},'outputs':{'root':'.','report_path':'output/report.json','logs_dir':'.','artifacts_dir':'output/artifacts'},'metadata':{'title':'L3 unknown reply','description':'Same identity recovery','created_by':'test','created_at':'2026-11-09T00:00:00Z'}}
job=bundle/'job.json';job.write_text(json.dumps(envelope));runner=str(a.runner.resolve());submit=[runner,'submit','--request',str(job),'--jobs-root',str(jobs)]
# Deliberately close the reply channel. The caller cannot infer admission from an acknowledgement.
with (out/'submit.stderr').open('w') as errors:
 child=subprocess.Popen(submit,stdout=subprocess.PIPE,stderr=errors);child.stdout.close();submit_code=child.wait(timeout=15)
root=jobs/job_id;status=None
for _ in range(200):
 result=subprocess.run([runner,'status','--job-id',job_id,'--jobs-root',str(jobs)],capture_output=True,text=True,timeout=10)
 if result.returncode==0:
  status=json.loads(result.stdout)
  if status['state'] in ('completed','failed','cancelled','interrupted'):break
 time.sleep(.1)
assert status and status['state']=='completed',status
# Renderer completion precedes the worker's final event publication.
for _ in range(200):
 if any(json.loads(p.read_text()).get('message_type')=='completion' for p in (root/'worker_events').glob('*.json')):break
 time.sleep(.01)
else:raise AssertionError('worker terminal event missing')
frame=root/'output/artifacts/frames/frame_0000.bmp';assert frame.is_file();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={str(p.relative_to(root)):sha(p) for p in root.rglob('*') if p.is_file()};pid=(root/'pid.txt').read_text()
# Event files are observations, not a second command/commit channel. Replayed,
# out-of-order and foreign event observations must not reapply output or status.
events=root/'worker_events';existing=list(events.glob('*.json'));assert existing
sample=json.loads(existing[-1].read_text());injected=[]
for index,payload in enumerate([sample,dict(sample,sequence=0),dict(sample,job_id='foreign-job',sequence=999999)]):
 path=events/f'l3-observation-{index}.json';assert not path.exists();path.write_text(json.dumps(payload));injected.append(path)
readback=subprocess.run([runner,'status','--job-id',job_id,'--jobs-root',str(jobs)],check=True,capture_output=True,text=True,timeout=10)
assert json.loads(readback.stdout)['state']=='completed'
for path in injected:path.unlink()
# A duplicate known ID is refused; successful inspection never creates another attempt.
duplicate=subprocess.run(submit,capture_output=True,text=True,timeout=15);assert duplicate.returncode!=0
subprocess.run([runner,'reconcile','--jobs-root',str(jobs)],check=True,capture_output=True,text=True,timeout=10)
after={str(p.relative_to(root)):sha(p) for p in root.rglob('*') if p.is_file()};assert before==after,(before.keys(),after.keys());assert (root/'pid.txt').read_text()==pid;assert len(list(jobs.iterdir()))==1
(out/'result.json').write_text(json.dumps({'submit_reply_returncode':submit_code,'state':status['state'],'job_id':job_id,'frame_sha256':sha(frame),'duplicate_refused':True,'event_replay_no_reapply':True,'same_job_files_unchanged':True},indent=2));print('PASS lost submit reply, same-ID inspection, duplicate refusal and terminal reconciliation')
