"""Contained native complete-volume sequence and synchronized-delivery qualification."""
from pathlib import Path
import os,json,subprocess,shutil,sys,copy,struct,math,random
import argparse
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',type=Path,required=True);p.add_argument('--cli',type=Path,required=True);p.add_argument('--probe',type=Path,required=True);p.add_argument('--skip-delivery',action='store_true');args=p.parse_args()
P=Path(__file__).resolve().parents[2];R=args.out.resolve();R.mkdir(parents=True,exist_ok=False);S=R/'runtime';S.mkdir();binary=None
sys.path.insert(0,str(P/'tools'));from sequence_volume import bind
from sequence_multiview import deliver
from vf3d_initial_state_preset_tool import HEADER_STRUCT,VF3D_MAGIC,VF3D_VERSION
root=R/'proof';root.mkdir();vroot=root/'volume';vroot.mkdir();assets=vroot/'cache';assets.mkdir()
scene=json.loads((P/'tests/fixtures/emissive_light_preview_matrix/scene_complex_prism_runtime.json').read_text())
# Static primitive room, recorded complete VF3D state and a typed camera move.
a=scene.setdefault('extensions',{}).setdefault('ray_tracing',{}).setdefault('authoring',{})
scene['scene_id']='volume_sequence_complete_state'
rate=dict(numerator=6,denominator=1);ran=dict(start_frame=0,frame_count=18)
a.clear()
a['scene_timeline']=dict(version=1,rate=rate,range=ran,tracks=[])
a['sequence']=dict(schema='ray_sequence_clock_v1',version=1,rate=rate,range=ran,source={},segments=[dict(id='play',kind='play',start_frame=0,frame_count=6,source_start=dict(numerator=0,denominator=1),speed=dict(numerator=1,denominator=1)),dict(id='inspect',kind='hold',start_frame=6,frame_count=6,source_start=dict(numerator=1,denominator=1),speed=dict(numerator=0,denominator=1)),dict(id='resume',kind='play',start_frame=12,frame_count=6,source_start=dict(numerator=1,denominator=1),speed=dict(numerator=1,denominator=1))])
rig=dict(schema='ray_camera_rig_v1',version=1,rate=rate,range=ran)
cam=dict(id='camera/A',position=[2,-5,3],yaw=-2.76,pitch=-.35,fov_y=55,zoom=1,clock=dict(mode='continuous',offset=dict(numerator=0,denominator=1),speed=dict(numerator=1,denominator=1),duration_frames=18,end_behavior='hold'))
cam2=copy.deepcopy(cam);cam2.update(id='camera/B',position=[-4,-2,3],yaw=2.03)
rig['cameras']=[cam,cam2];rig['shots']=[dict(id='first',camera_id='camera/A',start_frame=0,frame_count=9,clip_start=dict(numerator=0,denominator=1)),dict(id='second',camera_id='camera/B',start_frame=9,frame_count=9,clip_start=dict(numerator=0,denominator=1))];a['camera_rig']=rig
track=dict(id='camera_move',target_id='camera/A',property_id='camera/position',value_type='vec3',unit='world_distance',source='authored',enabled=True,keys=[dict(frame=i,value=pos,interpolation='linear',incoming_handle=dict(frame_offset=0,value_offset=0),outgoing_handle=dict(frame_offset=0,value_offset=0)) for i,pos in [(0,[2,-5,3]),(17,[1,-5,3])]])
a['scene_timeline']['tracks']=[track]
a['atmosphere_attachment']=dict(schema_version='ray_atmosphere_attachment_v1',id='atmosphere/main',enabled=True,visible=True,affects_lighting=True,source_kind='manifest',source_path='cache/manifest.json',playback=dict(animated=True,rate_numerator=6,rate_denominator=1,timeline_start=0,source_start=0,missing='error'),appearance=dict(density_scale=1,absorption_gain=.6,scatter_gain=2),placement=dict(translation=[0,0,0],rotation_degrees=[0,0,0],scale=[1,1,1]))
frames=[];N=8;cells=N**3
for tick in range(13):
 path=assets/f'frame_{tick:06d}.vf3d';header=HEADER_STRUCT.pack(VF3D_MAGIC,VF3D_VERSION,N,N,N,tick/6,tick,1/6,-2,-2,.1,.5,0,0,1,0,0,0,0)
 density=[.9*math.exp(-((x-2-tick/6)**2+(y-3)**2+(z-2)**2)/3) for z in range(N) for y in range(N) for x in range(N)]
 data=header+b''.join(struct.pack(f'<{cells}f',*vals) for vals in [density,[tick*.01]*cells,[tick*.02]*cells,[tick*.03]*cells,[tick*.1]*cells])+bytes(cells)
 path.write_bytes(data);frames.append(dict(frame_index=tick,path=path.name,frame_contract='vf3d'))
(assets/'manifest.json').write_text(json.dumps(dict(manifest_version=2,frame_contract='vf3d',space_mode='3d',frames=frames)))
seed=vroot/'seed.json';seed.write_text(json.dumps(scene,indent=2));scene_path=bind(seed,vroot/'scene_runtime.json');scene=json.loads(scene_path.read_text())
program=S/'program';program.mkdir();env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(program),XDG_CONFIG_HOME=str(S/'config'))
def run(label,args,success=True):
 with (R/(label+'.log')).open('w') as f:r=subprocess.run([str(x) for x in args],cwd=program,env=env,stdout=f,stderr=subprocess.STDOUT)
 assert (r.returncode==0)==success,(label,r.returncode,(R/(label+'.log')).read_text()[-2500:])
 return (R/(label+'.log')).read_text()
order=list(range(18));random.Random(37).shuffle(order);order += [6,11,12,0]
text=run('volume-native',[args.probe.resolve(),scene_path,*order]);rows=[json.loads(l[7:]) for l in text.splitlines() if l.startswith('VOLUME ')]
by={r['frame']:r for r in rows};assert all(r==by[r['frame']] for r in rows)
assert len({by[i]['channels'] for i in range(6,13)})==1 and len({by[i]['content'] for i in range(6,13)})==1
assert len({tuple(by[i]['camera']) for i in range(6,9)})==3 and len({tuple(by[i]['camera']) for i in range(6,12)})==4
assert by[13]['tick']==7 and by[13]['channels']!=by[12]['channels']
# Fail admission atomically for stale bytes, identities, coverage and malformed payloads.
mutations={}
def mutate(name,fn):
 c=copy.deepcopy(scene);fn(c['extensions']['ray_tracing']['authoring']);mutations[name]=c
mutate('bad-manifest-pin',lambda a:a['sequence']['source'].update(revision='0'*64))
mutate('bad-frame-pin',lambda a:a['sequence_volume']['frames'][6].update(sha256='0'*64))
mutate('missing-pin',lambda a:a['sequence_volume']['frames'].pop())
mutate('bad-source-id',lambda a:a['sequence']['source'].update(id='wrong'))
mutate('bad-source-rate',lambda a:a['sequence']['source']['rate'].update(numerator=12))
mutate('mixed-inspection',lambda a:a.update(inspection_moves=[]))
for name,c in mutations.items():
 path=vroot/(name+'.json');path.write_text(json.dumps(c));run(name,[args.probe.resolve(),scene_path,'--replacement',path])
# Same-size recorded-byte edit must refuse against retained pins, then restore.
path=assets/'frame_000006.vf3d';original=path.read_bytes();changed=bytearray(original);changed[HEADER_STRUCT.size]^=1;path.write_bytes(changed)
run('changed-payload',[args.probe.resolve(),scene_path,6],False);path.write_bytes(original)
request=dict(schema_version='ray_tracing_agent_render_request_v1',run_id='volume_sequence_test',render=dict(start_frame=0,frame_count=18,width=160,height=120,temporal_frames=1,integrator_3d='direct_light'),output=dict(video=dict(enabled=False)),volume=dict(enabled=False));request['scene']=dict(runtime_scene_path=str(scene_path));request['render'].update(width=160,height=120);request.pop('progress',None)
request_path=root/'volume-request.json';request_path.write_text(json.dumps(request,indent=2))
r={'ok':False}
if not args.skip_delivery:r=deliver(request_path,['camera/A','camera/B'],root/'volume-delivery',args.cli.resolve())
(R/'adapter-result.json').write_text(json.dumps(dict(ok=True,native_samples=len(rows),complete_channels_held=True,camera_moves_while_held=True,resume_changes_state=True,atomic_refusals=len(mutations),changed_bytes_refused=True,multiview=r['ok']),indent=2)+'\n')
print('Volume adapter PASS; delivery=' + str(r['ok']))
