#!/usr/bin/env python3
"""Compare edited direct-XYZ native samples against headless and baked geometry."""
import argparse,copy,hashlib,json,math,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--cli',type=Path,required=True);a=p.parse_args()
root,cli=a.root.resolve(),a.cli.resolve()
scene=json.loads((root/'scene_runtime.json').read_text());expected=json.loads((root/'dm4_trail_expected.json').read_text())
def render(s,frame,name):
    source=root/f'{name}.scene.json';source.write_text(json.dumps(s))
    req={'schema_version':'ray_tracing_agent_render_request_v1','run_id':name,'scene':{'runtime_scene_path':str(source)},'volume':{'enabled':False},'inspection':{'camera_position':{'x':2,'y':-10,'z':6},'camera_look_at':{'x':2,'y':0,'z':1.4},'camera_zoom':1},'render':{'start_frame':frame,'frame_count':1,'width':160,'height':100,'temporal_frames':1,'integrator_3d':'direct_light'},'output':{'root':str(root/name),'overwrite':True}}
    request,summary=root/f'{name}.request.json',root/f'{name}.summary.json';request.write_text(json.dumps(req))
    with (root/f'{name}.log').open('w') as log:
        subprocess.run([str(cli),'--request',str(request),'--render','--summary',str(summary),'--summary-file-only'],cwd=root,env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(root)),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
    return json.loads(summary.read_text()),(root/name/'frames'/f'frame_{frame:04d}.bmp').read_bytes()
report=[]
for frame,position in zip((0,15,30,45,60,90,119),expected):
    summary,pixels=render(scene,frame,f'trail_{frame}')
    actual=next(o['position'] for o in summary['evaluated_objects'] if o['object_id']=='obj_sphere_medium')
    assert all(math.isclose(x,y,abs_tol=1e-8) for x,y in zip(actual,position)),(frame,actual,position)
    baked=copy.deepcopy(scene);author=baked['extensions']['ray_tracing']['authoring']
    author['scene_timeline']['tracks']=[t for t in author['scene_timeline']['tracks'] if t['target_id']!='object/obj_sphere_medium']
    next(o for o in baked['objects'] if o['object_id']=='obj_sphere_medium')['transform']['position']=dict(zip('xyz',[v/baked['world_scale'] for v in position]))
    _,reference=render(baked,frame,f'trail_baked_{frame}');assert pixels==reference,(frame,'baked pixel mismatch')
    report.append({'frame':frame,'position':position,'baked_exact':True,'sha256':hashlib.sha256(pixels).hexdigest()})
assert len({r['sha256'] for r in report})>1,'rendered motion must change pixels'
(root/'dm4_trail_render_verification.json').write_text(json.dumps(report,indent=2))
print('D-M4 XYZ trail render PASS: native/headless positions and seven exact baked images')
