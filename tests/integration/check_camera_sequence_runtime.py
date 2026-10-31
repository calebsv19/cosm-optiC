"""Contained native named-camera/shot qualification; no physics solver or install."""
import argparse
import copy
from fractions import Fraction
import hashlib
import json
import math
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys


def main():
    p = argparse.ArgumentParser()
    for key in ['base', 'out', 'renderer', 'probe', 'camera-probe']:
        p.add_argument('--'+key, type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(exist_ok=False)
    repo = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(repo/'tools'))
    from camera_sequence_schedule import sample as expected_sample
    from sequence_video import assemble
    root = a.out/'fixture'
    shutil.copytree(a.base, root)
    scene_path = root/'scene_runtime.json'
    scene = json.loads(scene_path.read_text())
    author = scene['extensions']['ray_tracing']['authoring']
    ratio = lambda n, d=1: dict(numerator=n, denominator=d)
    rig = dict(schema='ray_camera_rig_v1', version=1, rate=ratio(24),
        range=dict(start_frame=12, frame_count=48), cameras=[], shots=[])
    for i, mode in enumerate(['continuous', 'selected', 'clip']):
        rig['cameras'].append(dict(id='camera/'+chr(65+i), position=[1.8+i*.3, -4, 5.2],
            yaw=math.atan2(-1.8-i*.3, -4), pitch=-.63, fov_y=55, zoom=1,
            clock=dict(mode=mode, offset=ratio(0), speed=ratio(1), duration_frames=48, end_behavior='hold')))
    for i in range(6):
        rig['shots'].append(dict(id='shot_'+str(i), camera_id='camera/'+chr(65+i%3),
            start_frame=8*i, frame_count=8, clip_start=ratio(1 if i==5 else 0)))
    author['camera_rig'] = rig
    originals = author['scene_timeline']['tracks']
    tracks=[]
    for i in range(3):
        for original in originals:
            track=copy.deepcopy(original)
            track['target_id']='camera/'+chr(65+i)
            track['id']=chr(65+i)+'_'+track['id']
            if i==0 and track['property_id']=='camera/position':
                track['enabled']=False
            if track['property_id']=='camera/position':
                for key in track['keys']:
                    key['value'][0]+=i*.3
            tracks.append(track)
    progress=copy.deepcopy(originals[1]);progress.update(id='A_route', target_id='camera/A',property_id='camera/route_progress',unit='unitless')
    progress['keys'][0]['value']=0;progress['keys'][1]['value']=1
    tracks.append(progress)
    author['scene_timeline']['tracks']=tracks
    points=[]
    for i, pos in enumerate([[1.8,-4,5.2],[.5,-4,5.2]]):
        points.append(dict(id='point_'+str(i),position=pos,incoming=[.3,-.3,0],outgoing=[-.3,.3,0],segment='cubic'))
    author['motion_paths']=dict(schema='ray_motion_paths_v1',paths=[dict(id='A_dolly',name='A curved dolly',points=points)],bindings=[dict(target_id='camera/A',path_id='A_dolly',enabled=True,placement='on_path',restore_position_tracks=['A_camera_position'],orientation_frame=dict(start_up=[0,0,1],start_roll=0,end_roll=5,end_enabled=True,camera_mode=1))])
    scene_path.write_text(json.dumps(scene,indent=2)+'\n')
    pins={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in root.rglob('*') if f.is_file()}
    logs=a.out/'logs';logs.mkdir()
    program=a.out/'program';program.mkdir()
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(program),XDG_CONFIG_HOME=str(a.out/'config'),RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT=str(a.out/'cache'),PYTHONDONTWRITEBYTECODE='1')
    def run(name,cmd,ok=True):
        result=subprocess.run(list(map(str,cmd)),env=env,cwd=program,capture_output=True,text=True,timeout=300)
        (logs/(name+'.log')).write_text(result.stdout+result.stderr)
        assert (result.returncode==0)==ok,(name,result.returncode,result.stderr[-1200:])
        return result
    order=list(range(48));random.Random(72).shuffle(order);order += [8,16,24,32,40,0,47]
    comparisons=0
    for view in ['active','camera/A','camera/B','camera/C']:
        result=run(view.replace('/','_'),[a.camera_probe,scene_path,view,*order])
        rows=[json.loads(s[7:]) for s in result.stdout.splitlines() if s.startswith('CAMERA ')]
        assert len(rows)==len(order)
        for row in rows:
            f=row['frame'];expected=expected_sample(rig,f,None if view=='active' else view)
            assert row['id']==expected['id'] and row['shot']==expected['shot'] and row['selected']==expected['selected']
            assert Fraction(*row['time'])==expected['time']
            tick=f*10 if f<15 else 150 if f<33 else 150+(f-33)*10
            assert row['source_tick']==tick
            if row['id']!='camera/A':
                t=float(expected['time'])*24/47
                offset=.3 if row['id']=='camera/B' else .6
                assert abs(row['position'][0]-(1.8-1.3*t+offset))<1e-12
            else:
                assert row['route']
            comparisons+=1
    saved=root/'saved-camera.json'
    run('save',[a.probe,scene_path,'--sequence-save',saved])
    reopened=json.loads(saved.read_text())
    assert reopened['extensions']['ray_tracing']['authoring']['camera_rig']==rig
    run('reopen',[a.camera_probe,saved,'active',0,8,16,24,32,40,47])
    mutations={}
    def mutate(name,edit):
        changed=copy.deepcopy(scene);edit(changed['extensions']['ray_tracing']['authoring']['camera_rig']);mutations[name]=changed
    mutate('gap',lambda r:r['shots'][1].update(start_frame=9))
    mutate('overlap',lambda r:r['shots'][1].update(start_frame=7))
    mutate('unknown-camera',lambda r:r['shots'][1].update(camera_id='camera/missing'))
    mutate('duplicate-camera',lambda r:r['cameras'][1].update(id='camera/A'))
    mutate('duplicate-shot',lambda r:r['shots'][1].update(id='shot_0'))
    mutate('unknown-policy',lambda r:r['cameras'][0]['clock'].update(mode='automatic'))
    mutate('ignored-clip-start',lambda r:r['shots'][0].update(clip_start=ratio(1)))
    mutate('unknown-field',lambda r:r.update(unknown=True))
    mutate('wrong-rate',lambda r:r.update(rate=ratio(25)))
    mutate('wrong-range',lambda r:r['range'].update(start_frame=0))
    mutate('zero-speed',lambda r:r['cameras'][0]['clock'].update(speed=ratio(0)))
    mutate('unknown-end',lambda r:r['cameras'][0]['clock'].update(end_behavior='loop'))
    changed=copy.deepcopy(scene);changed['extensions']['ray_tracing']['authoring']['camera_rig']=None;mutations['null-rig']=changed
    for name,changed in mutations.items():
        path=root/('rig-'+name+'.json');path.write_text(json.dumps(changed))
        run('refuse_'+name,[a.camera_probe,path,'active',0],False)
        run('preserve_'+name,[a.probe,scene_path,'--replacement',path])
    run('missing-view',[a.camera_probe,scene_path,'camera/missing',0],False)
    # Camera routes cannot hide an object claim from the retained physics reader.
    for name, edit in [
        ('dual-owner', lambda a: a['motion_paths']['bindings'][0].update(object_id=scene['objects'][0]['object_id'])),
        ('undeclared-route', lambda a: a['motion_paths']['bindings'][0].update(target_id='camera/missing')),
    ]:
        changed=copy.deepcopy(scene);edit(changed['extensions']['ray_tracing']['authoring'])
        path=root/('rig-'+name+'.json');path.write_text(json.dumps(changed))
        run('refuse_'+name,[a.camera_probe,path,'active',0],False)
        run('preserve_'+name,[a.probe,scene_path,'--replacement',path])
    # Two aimed paths exercise the target-keyed orientation cache in alternating shots.
    aimed=copy.deepcopy(scene);aa=aimed['extensions']['ray_tracing']['authoring']
    aa['camera_focus_target']=dict(x=0,y=0,z=2)
    aa['motion_paths']['bindings'][0]['orientation_frame']['camera_mode']=2
    path_b=copy.deepcopy(aa['motion_paths']['paths'][0]);path_b['id']='B_dolly'
    for point in path_b['points']:point['position'][0]+=.3
    aa['motion_paths']['paths'].append(path_b)
    binding=copy.deepcopy(aa['motion_paths']['bindings'][0]);binding.update(target_id='camera/B',path_id='B_dolly',restore_position_tracks=['B_camera_position'])
    aa['motion_paths']['bindings'].append(binding)
    for track in aa['scene_timeline']['tracks']:
        if track['id']=='B_camera_position':track['enabled']=False
    progress_b=copy.deepcopy(progress);progress_b.update(id='B_route',target_id='camera/B');aa['scene_timeline']['tracks'].append(progress_b)
    path=root/'two-aimed-routes.json';path.write_text(json.dumps(aimed))
    aim_order=[0,8,24,32,47,8,0,32]
    result=run('two-aimed-routes',[a.camera_probe,path,'active',*aim_order])
    aim_rows=[json.loads(s[7:]) for s in result.stdout.splitlines() if s.startswith('CAMERA ')]
    for row in aim_rows:
        if row['id'] in ['camera/A','camera/B']:
            x,y,z=row['position']
            assert abs(row['yaw']-math.atan2(-x,y))<1e-10
            assert abs(row['pitch']-math.atan2(2-z,math.hypot(x,y)))<1e-10
    assert aim_rows[0]==aim_rows[6] and aim_rows[1]==aim_rows[5] and aim_rows[3]==aim_rows[7]
    # Rig-only evaluation also works without any numerical source adapter.
    camera_only=copy.deepcopy(scene);camera_only['extensions'].pop('rigid_scene');camera_only['extensions']['ray_tracing'].pop('rigid_result');camera_only['extensions']['ray_tracing']['authoring'].pop('sequence')
    path=root/'camera-only.json';path.write_text(json.dumps(camera_only))
    run('camera-only',[a.camera_probe,path,'active',0,8,24,47])
    # Explicit final motion hold is separate from simulation and shot duration.
    held=copy.deepcopy(scene);held['extensions']['ray_tracing']['authoring']['camera_rig']['cameras'][0]['clock']['duration_frames']=5
    path=root/'camera-end-hold.json';path.write_text(json.dumps(held));result=run('camera-end-hold',[a.camera_probe,path,'camera/A',4,20,47])
    rows=[json.loads(s[7:]) for s in result.stdout.splitlines() if s.startswith('CAMERA ')]
    assert all(Fraction(*r['time'])==Fraction(1,6) for r in rows)
    assert len({tuple(r['position']) for r in rows})==1
    request=json.loads((a.base.parent/'request.json').read_text())
    request['scene']=dict(runtime_scene_path=str(scene_path))
    def render(name,view=None):
        out=a.out/name
        req=copy.deepcopy(request)
        if view:req['scene']['camera_id']=view
        req['output']['root']=str(out);req['progress']={k:str(out/Path(v).name) for k,v in req['progress'].items()}
        path=a.out/(name+'-request.json');path.write_text(json.dumps(req,indent=2))
        run(name,[a.renderer,'--request',path,'--render','--summary',a.out/(name+'-summary.json'),'--summary-file-only'])
        video=a.out/(name+'.mp4');assemble(scene_path,out/'frames',video,camera_id=view)
        return out
    rendered=render('shots')
    # Full fixed-view render proves view selection leaves global PTS and source ticks intact.
    fixed=render('fixed-B','camera/B')
    for f in range(48):
        active=json.loads((rendered/'frames'/f'frame_{f+12:04d}.bmp.sequence.json').read_text())
        other=json.loads((fixed/'frames'/f'frame_{f+12:04d}.bmp.sequence.json').read_text())
        assert all(k in active for k in ['camera_yaw','camera_pitch','camera_fov_y','camera_zoom','camera_frame'])
        assert active['source_tick']==other['source_tick'] and active['presentation_time']==other['presentation_time'] and active['bodies']==other['bodies']
    # Video assembler refuses a camera clock or selected-view mismatch.
    try:assemble(scene_path,fixed/'frames',a.out/'wrong-view.mp4')
    except ValueError:pass
    else:raise AssertionError('accepted wrong camera selection')
    receipt=rendered/'frames/frame_0020.bmp.sequence.json';before=receipt.read_text();bad=json.loads(before);bad['camera_motion_time'][0]+=1;receipt.write_text(json.dumps(bad))
    try:assemble(scene_path,rendered/'frames',a.out/'wrong-time.mp4')
    except ValueError:pass
    else:raise AssertionError('accepted forged camera time')
    receipt.write_text(before)
    assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in pins.items())
    result=dict(ok=True,native_camera_samples=comparisons,independent_views=3,shots=6,native_frames=96,simulation_identity_across_views=True,exact_pts=True,save_reopen=True,invalid_cases=len(mutations)+2,two_aimed_routes=True,camera_only=True,camera_end_hold=True,forged_camera_snapshot_refused=True,source_unchanged=True)
    (a.out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
