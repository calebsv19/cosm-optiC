#!/usr/bin/env python3
from pathlib import Path
import json,copy,subprocess,hashlib,math
import argparse
p=argparse.ArgumentParser(description="Compare linear object timeline renders with explicitly positioned references; isolate the mesh for visibility.")
p.add_argument('--scene',type=Path,required=True)
p.add_argument('--cli',type=Path,required=True)
p.add_argument('--scratch',type=Path,required=True)
p.add_argument('--object-id',required=True)
p.add_argument('--camera',type=float,nargs=3,required=True)
p.add_argument('--look-at',type=float,nargs=3,required=True)
args=p.parse_args()
root=args.scratch.resolve();root.mkdir(parents=True,exist_ok=True)
cli=args.cli.resolve()
scene=json.loads(args.scene.read_text())
author=scene['extensions']['ray_tracing']['authoring'];tracks=author['scene_timeline']['tracks']
object_id=args.object_id;target='object/'+object_id
obj=next(o for o in scene['objects'] if o['object_id']==object_id)
# Isolate the authored mesh for an unoccluded image-level proof.
scene['objects']=[obj]
scene['constraints']=[]
scene['hierarchy']=[]
def value(t,f):
    k=t['keys']
    for a,b in zip(k,k[1:]):
        if a['frame']<=f<=b['frame']:
            if a['interpolation']=='step':return a['value']
            assert a['interpolation']=='linear', 'Fixture requires linear or hold segments'
            return a['value']+(b['value']-a['value'])*(f-a['frame'])/(b['frame']-a['frame'])
    return k[-1]['value'] if f>=k[-1]['frame'] else k[0]['value']
def render(s,frame,name):
    sp=root/(name+'.scene.json');sp.write_text(json.dumps(s))
    req={'schema_version':'ray_tracing_agent_render_request_v1','run_id':name,'scene':{'runtime_scene_path':str(sp)},'volume':{'enabled':False},'inspection':{'camera_position':dict(zip('xyz',args.camera)),'camera_look_at':dict(zip('xyz',args.look_at)),'camera_zoom':1.0},'render':{'start_frame':frame,'frame_count':1,'width':96,'height':64,'temporal_frames':1},'output':{'root':str(root/name),'overwrite':True}}
    rp=root/(name+'.request.json');rp.write_text(json.dumps(req));summary=root/(name+'.summary.json')
    with (root/(name+'.log')).open('w') as log:subprocess.run([str(cli),'--request',str(rp),'--render','--summary',str(summary),'--summary-file-only'],stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
    result=json.loads(summary.read_text());assert result['frames_rendered']==1
    image=next((root/name).rglob('*.bmp'));return result,image,hashlib.sha256(image.read_bytes()).hexdigest()
results=[]
for frame in [0,20,60,80]:
    expected={t['property_id'][-1]:value(t,frame) for t in tracks if t['target_id']==target}
    baked=copy.deepcopy(scene);baked['extensions']['ray_tracing']['authoring']['scene_timeline']['tracks']=[t for t in tracks if t['target_id']!=target]
    next(o for o in baked['objects'] if o['object_id']==object_id)['transform']['position'].update(expected)
    a,im,ah=render(scene,frame,f'animated_{frame}');b,bim,bh=render(baked,frame,f'baked_{frame}')
    actual=next(o for o in a['evaluated_objects'] if o['object_id']==object_id)['position']
    assert all(math.isclose(actual[i],expected[axis]*scene['world_scale'],abs_tol=1e-8) for i,axis in enumerate('xyz'))
    assert ah==bh,(frame,ah,bh)
    results.append({'frame':frame,'position':actual,'animated_image':str(im),'baked_image':str(bim),'sha256':ah,'pixel_exact_match':True})
    print('PASS animated vs explicitly positioned render',frame,flush=True)
(root/'verification.json').write_text(json.dumps(results,indent=2))
frozen=copy.deepcopy(scene);frozen['extensions']['ray_tracing']['authoring']['scene_timeline']['tracks']=[t for t in tracks if t['target_id']!=target]
f,fi,fh=render(frozen,60,'frozen_60')
assert fh!=next(r['sha256'] for r in results if r['frame']==60),'Object motion did not affect rendered pixels'
print('PASS negative control: frozen object produces different pixels at frame 60',flush=True)
(root/'negative_control.json').write_text(json.dumps({'frame':60,'frozen_image':str(fi),'frozen_sha256':fh,'differs_from_animated':True},indent=2))

# Exercise changing geometry within a single process/job, not only fresh starts.
request=json.loads((root/'animated_20.request.json').read_text())
request['run_id']='consecutive';request['render']['frame_count']=3
request['output']['root']=str(root/'consecutive')
rp=root/'consecutive.request.json';rp.write_text(json.dumps(request))
summary=root/'consecutive.summary.json'
with (root/'consecutive.log').open('w') as log:
    subprocess.run([str(cli),'--request',str(rp),'--render','--summary',str(summary),'--summary-file-only'],stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
assert json.loads(summary.read_text())['frames_rendered']==3
for frame in [20,21,22]:
    baked=copy.deepcopy(frozen)
    expected={t['property_id'][-1]:value(t,frame) for t in tracks if t['target_id']==target}
    baked['objects'][0]['transform']['position'].update(expected)
    _,_,expected_hash=render(baked,frame,f'consecutive_reference_{frame}')
    actual_image=root/'consecutive'/'frames'/f'frame_{frame:04d}.bmp'
    assert hashlib.sha256(actual_image.read_bytes()).hexdigest()==expected_hash
print('PASS consecutive frame render 20-22 matches baked references',flush=True)
