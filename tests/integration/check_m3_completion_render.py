#!/usr/bin/env python3
"""M3 conversion tolerance and exact baked combined-follower render proof."""
import argparse, copy, hashlib, json, math, os, struct, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--cli',type=Path,required=True);a=p.parse_args()
root,cli=a.root.resolve(),a.cli.resolve()
def author(s):return s['extensions']['ray_tracing']['authoring']
def render(s,frame,name):
    source=root/f'{name}.scene.json';source.write_text(json.dumps(s))
    request={'schema_version':'ray_tracing_agent_render_request_v1','run_id':name,'scene':{'runtime_scene_path':str(source)},'volume':{'enabled':False},'render':{'start_frame':frame,'frame_count':1,'width':160,'height':100,'temporal_frames':1,'integrator_3d':'direct_light'},'output':{'root':str(root/name),'overwrite':True}}
    req,summary=root/f'{name}.request.json',root/f'{name}.summary.json';req.write_text(json.dumps(request))
    with (root/f'{name}.log').open('w') as log:
        subprocess.run([str(cli),'--request',str(req),'--render','--summary',str(summary),'--summary-file-only'],cwd=root,env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(root)),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
    return json.loads(summary.read_text()),(root/name/'frames'/f'frame_{frame:04d}.bmp').read_bytes()
def values(s):
    c,l=s['evaluated_camera'],s['evaluated_light'];return c['position']+[c['yaw'],c['pitch'],c['fov_y']]+l['position']+[l['intensity']]
def track(prop,typ,unit,value):
    return {'id':'baked-'+prop,'target_id':'camera/main','property_id':'camera/'+prop,'value_type':typ,'unit':unit,'source':'authored','enabled':True,'keys':[{'frame':0,'value':value,'interpolation':'linear','incoming_handle':{'frame_offset':0,'value_offset':0},'outgoing_handle':{'frame_offset':0,'value_offset':0}}]}
def baked(scene,v):
    s=copy.deepcopy(scene);au=author(s);scale=s['world_scale'];au['motion_paths']['bindings']=[];au.pop('camera_focus_target',None)
    tracks=[t for t in au['scene_timeline']['tracks'] if t['target_id']!='camera/main' and t['property_id']!='object/path_progress']
    for prop,typ,unit,value in [('position','vec3','world_distance',[x/scale for x in v[:3]]),('yaw','scalar','radians',v[3]),('pitch','scalar','radians',v[4]),('fov_y','scalar','degrees',v[5])]:tracks.append(track(prop,typ,unit,value))
    for t in tracks:
        if t['property_id']=='light/route_progress':t['enabled']=False
        if t['property_id']=='light/path_progress':
            t['enabled']=True
            for k in t['keys']:
                k['value']=0;k['interpolation']='linear';k['incoming_handle']={'frame_offset':0,'value_offset':0};k['outgoing_handle']={'frame_offset':0,'value_offset':0}
    au['scene_timeline']['tracks']=tracks
    x,y,z=[x/scale for x in v[6:9]]
    au['light_timeline']['spatial_path']={'path':{'mode':'BEZIER_CUBIC','points':[{'x':x,'y':y,'rotation':0},{'x':x+1,'y':y,'rotation':0}]},'depth':{'points':[{'z':z,'lookPitch':0},{'z':z,'lookPitch':0}]}}
    next(o for o in s['objects'] if o['object_id']=='obj_sphere_medium')['transform']['position']=dict(zip('xyz',[x/scale for x in v[10:13]]))
    return s
before=json.loads((root/'dm3_conversion_before.scene.json').read_text());converted=json.loads((root/'dm3_converted.scene.json').read_text());combined=json.loads((root/'scene_runtime.json').read_text());expected=json.loads((root/'dm3_combined_expected.json').read_text());report=[]
assert before['objects']==converted['objects'] and before['lights']==converted['lights'], 'conversion changed scene identities/base geometry'
for i,frame in enumerate((0,23,60,119)):
    b,bi=render(before,frame,f'legacy_{frame}');c,ci=render(converted,frame,f'converted_{frame}')
    bv,cv=values(b),values(c)
    for k in range(10):assert abs(bv[k]-cv[k])<(.003*before['world_scale'] if k in (0,1,2,6,7,8) else 1e-8),(frame,k,bv[k],cv[k])
    # BMP byte comparison excludes header/padding; fixed renderer/format. Alpha is constant.
    offset=struct.unpack_from('<I',bi,10)[0];assert bi[:offset]==ci[:offset]
    diffs=[abs(x-y) for x,y in zip(bi[offset:],ci[offset:])]
    mean=sum(diffs)/len(diffs);fraction=sum(d>4 for d in diffs)/len(diffs)
    assert mean<=.5 and fraction<=.01,(frame,mean,fraction)
    s,pixels=render(combined,frame,f'combined_{frame}');v=values(s)
    v+=next(o['position'] for o in s['evaluated_objects'] if o['object_id']=='obj_sphere_medium')
    assert all(math.isclose(x,y,abs_tol=1e-8) for x,y in zip(v,expected[i])),(frame,v,expected[i])
    _,reference=render(baked(combined,expected[i]),frame,f'baked_{frame}')
    assert pixels==reference,(frame,'combined baked pixels differ')
    report.append({'frame':frame,'conversion_mean_byte_delta':mean,'conversion_fraction_over_4':fraction,'combined_baked_exact':True,'sha256':hashlib.sha256(pixels).hexdigest()})
(root/'dm3_completion_render_verification.json').write_text(json.dumps(report,indent=2))
print('D-M3 completion render PASS: conversion pose/pixel tolerance and combined focus/camera/light/mesh exact baked parity')
