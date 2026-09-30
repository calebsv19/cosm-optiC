#!/usr/bin/env python3
"""Compare a saved oriented follower against explicitly baked mesh transforms."""
import argparse,copy,json,math,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--cli',type=Path,required=True);a=p.parse_args()
root,cli=a.root.resolve(),a.cli.resolve();scene=json.loads((root/'scene_runtime.json').read_text())
def render(s,frame,name):
    source=root/(name+'.scene.json');source.write_text(json.dumps(s))
    req={'schema_version':'ray_tracing_agent_render_request_v1','run_id':name,'scene':{'runtime_scene_path':str(source)},'volume':{'enabled':False},'render':{'start_frame':frame,'frame_count':1,'width':160,'height':100,'temporal_frames':1,'integrator_3d':'direct_light'},'output':{'root':str(root/name),'overwrite':True}}
    request=root/(name+'.request.json');request.write_text(json.dumps(req));summary=root/(name+'.summary.json')
    with (root/(name+'.log')).open('w') as log:
        subprocess.run([str(cli),'--request',str(request),'--render','--summary',str(summary),'--summary-file-only'],cwd=root,env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(root)),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
    return json.loads(summary.read_text()),(root/name/'frames'/f'frame_{frame:04d}.bmp').read_bytes()
for frame in [0,37,119]:
    report,pixels=render(scene,frame,f'orientation_{frame}')
    sample=next(x for x in report['evaluated_objects'] if x['object_id']=='obj_sphere_medium');assert sample['has_rotation']
    baked=copy.deepcopy(scene);author=baked['extensions']['ray_tracing']['authoring']
    for b in author['motion_paths']['bindings']:
        if b.get('object_id')=='obj_sphere_medium':b['enabled']=False
    for t in author['scene_timeline']['tracks']:
        if t['target_id']=='object/obj_sphere_medium':t['enabled']=False
    obj=next(x for x in baked['objects'] if x['object_id']=='obj_sphere_medium')
    obj['transform']['position']=dict(zip('xyz',[v/baked['world_scale'] for v in sample['position']]))
    obj['transform']['rotation']=dict(zip('xyz',[math.degrees(v) for v in sample['rotation_radians']]))
    _,reference=render(baked,frame,f'orientation_baked_{frame}')
    assert pixels==reference,(frame,'oriented/baked pixels differ')
print('Path orientation render PASS: three exact baked transform images')
