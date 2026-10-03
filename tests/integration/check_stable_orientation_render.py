#!/usr/bin/env python3
"""Bounded combined-follower render acceptance; input is the native stress fixture."""
import argparse,copy,json,math,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--cli',type=Path,required=True);a=p.parse_args()
root,cli=a.root.resolve(),a.cli.resolve();scene=json.loads((root/'orientation_stress.scene.json').read_text())
def render(s,frame,name):
    source=root/(name+'.scene.json');source.write_text(json.dumps(s))
    req={'schema_version':'ray_tracing_agent_render_request_v1','run_id':name,'scene':{'runtime_scene_path':str(source)},'volume':{'enabled':False},'render':{'start_frame':frame,'frame_count':1,'width':128,'height':128,'temporal_frames':1,'integrator_3d':'direct_light'},'output':{'root':str(root/name),'overwrite':True}}
    request=root/(name+'.request.json');request.write_text(json.dumps(req));summary=root/(name+'.summary.json')
    with (root/(name+'.log')).open('w') as log:
        subprocess.run([str(cli),'--request',str(request),'--render','--summary',str(summary),'--summary-file-only'],cwd=root,env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(root)),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
    return json.loads(summary.read_text()),(root/name/'frames'/f'frame_{frame:04d}.bmp').read_bytes()
def dot(a,b):return sum(x*y for x,y in zip(a,b))
for seed_up in [[0,0,0],[0,0,1],[1,0,0]]:
    s=copy.deepcopy(scene);binding=next(b for b in s['extensions']['ray_tracing']['authoring']['motion_paths']['bindings'] if b.get('target_id')=='camera/main');binding['orientation_frame'].update(start_up=seed_up,start_roll=0,end_enabled=False)
    report,_=render(s,0,'stable_focus_seed_'+str(seed_up.index(1) if 1 in seed_up else 'default'))
    orientation=report['evaluated_camera_orientation'];forward=orientation['forward'];up=seed_up if any(seed_up) else [0,0,1]
    expected=[u-dot(up,forward)*f for u,f in zip(up,forward)];length=math.sqrt(dot(expected,expected));expected=[v/length for v in expected]
    assert dot(expected,orientation['up'])>1-1e-10,'focus seed acquired dolly-induced roll'
for frame in [10,40,119]:
    report,pixels=render(scene,frame,f'stable_combined_{frame}')
    assert len(report['evaluated_objects'])==3 and all(o['has_rotation'] for o in report['evaluated_objects'])
    orientation=report['evaluated_camera_orientation'];assert orientation['enabled'] and not orientation['fallback']
    forward,up=orientation['forward'],orientation['up'];assert abs(dot(forward,up))<1e-10 and abs(dot(up,up)-1)<1e-10
    pos=report['evaluated_camera']['position'];target=[scene['extensions']['ray_tracing']['authoring']['camera_focus_target'][axis]*scene['world_scale'] for axis in 'xyz'];direction=[t-x for t,x in zip(target,pos)];length=math.sqrt(dot(direction,direction));direction=[v/length for v in direction];assert dot(direction,forward)>1-1e-10
    assert len(set(pixels[128:]))>8,'empty render'
images=[];ups=[]
for roll in [0,90,360]:
    s=copy.deepcopy(scene);binding=next(b for b in s['extensions']['ray_tracing']['authoring']['motion_paths']['bindings'] if b.get('target_id')=='camera/main');binding['orientation_frame'].update(start_roll=roll,end_enabled=False)
    report,pixels=render(s,40,f'stable_camera_roll_{roll}');images.append(pixels);ups.append(report['evaluated_camera_orientation']['up'])
assert images[0]!=images[1],'camera roll did not reach rendered rays'
assert images[0]==images[2],'full camera turn changed final pose pixels'
assert abs(dot(ups[0],ups[1]))<1e-10 and dot(ups[0],ups[2])>1-1e-10
print('Stable orientation render PASS: three objects, camera/light, exact target aim, nonempty frames, visible 90-degree roll and exact 360-degree image return')
