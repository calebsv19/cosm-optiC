"""Prove native detached execution preserves budget, denoise, and video intent."""
import argparse,json,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runner',type=Path,required=True);p.add_argument('--root',type=Path,required=True);p.add_argument('--disable-checkpoint',action='store_true');a=p.parse_args()
repo=Path(__file__).resolve().parents[2];root=a.root.resolve();root.mkdir(parents=True,exist_ok=False);jobs=root/'jobs';jobs.mkdir();output=root/'output';video=root/'render.mp4'
request={'schema_version':'ray_tracing_agent_render_request_v1','run_id':'detached_settings_regression','scene':{'runtime_scene_path':str(repo/'config/samples/ps4d_runtime_scene_visual_test.json')},'volume':{'enabled':False},'render':{'start_frame':0,'frame_count':1,'width':160,'height':96,'temporal_frames':8,'integrator_3d':'disney_v2','denoise_enabled':False},'resources':{'cpu_percent':50,'max_workers':2,'reserve_cpu_count':1},'output':{'root':str(output),'overwrite':False,'video':{'enabled':True,'path':str(video),'fps':24}}}
if a.disable_checkpoint:request['checkpoint']={'enabled':False,'root':str(output/'disabled_checkpoints')}
path=root/'request.json';path.write_text(json.dumps(request,indent=2)+'\n');runner=str(a.runner.resolve())
result=json.loads(subprocess.check_output([runner,'submit','--request',str(path),'--jobs-root',str(jobs)],text=True));job=result['job_id'];canonical=json.loads((jobs/job/'job_request.json').read_text());assert canonical['render']['denoise_enabled'] is False;assert canonical['resources']==request['resources'];assert canonical['output']['video']==request['output']['video']
deadline=time.monotonic()+90
while time.monotonic()<deadline:
 state=json.loads(subprocess.check_output([runner,'status','--job-id',job,'--jobs-root',str(jobs)],text=True))
 if state['state']=='completed':break
 assert state['state'] not in ('failed','cancelled'),state
 time.sleep(.5)
else:raise AssertionError('detached render did not finish within 90 seconds')
summary=json.loads((jobs/job/'result_summary.json').read_text());assert summary['resources']['max_workers']==2;assert summary['denoise']['enabled'] is False;assert summary['frames_rendered']==1;assert summary['checkpoint']['enabled'] is (not a.disable_checkpoint);assert (summary['checkpoint']['generations_written']>0) is (not a.disable_checkpoint);assert video.stat().st_size>0
print(json.dumps({'status':'pass','job_id':job,'checks':['budget preserved and applied','denoise override preserved and applied','video path/fps preserved and encoded','explicit checkpoint disable respected' if a.disable_checkpoint else 'Disney tile checkpoints committed']},indent=2))
