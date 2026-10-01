#!/usr/bin/env python3
"""Extend the saved --camera-orientation native fixture for multi-follower proof."""
import argparse,copy,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
d=json.loads(args.source.read_text());a=d['extensions']['ray_tracing']['authoring'];paths=a['motion_paths']['paths'];assert len(paths)==2
paths[1]['points'][0]['position']=[.8,1.5,.7];paths[1]['points'][1]['position']=[2.4,1.5,.7]
for point in paths[1]['points']:point['incoming']=[-1,0,0];point['outgoing']=[1,0,0]
camera_path=copy.deepcopy(paths[1]);camera_path.update(id='path-3',name='Camera clearance');camera_path['points'][0]['position']=[-1,-8,4];camera_path['points'][1]['position']=[2,-8,4];paths.append(camera_path)
for b in a['motion_paths']['bindings']:
    if b.get('target_id')=='camera/main':b['path_id']='path-3'
    if b.get('target_id')=='light/light_key':b['path_id']='path-2'
a['camera_focus_target']=d['cameras'][0]['target'];a['environment'].update(ambient_brightness=.5,ambient_strength=.5)
base=next(o for o in d['objects'] if o['object_id']=='obj_sphere_medium');template=next(t for t in a['scene_timeline']['tracks'] if t['target_id']=='object/obj_sphere_medium');binding=next(b for b in a['motion_paths']['bindings'] if b.get('object_id')=='obj_sphere_medium')
for i in range(2):
    obj=copy.deepcopy(base);obj['object_id']='stress_object_'+str(i);obj['name']='Stress follower '+str(i);d['objects'].append(obj)
    b=copy.deepcopy(binding);b['object_id']=obj['object_id'];b['path_id']='path-'+str(1+i);b['forward_axis']=2+i*2;b['orientation_frame']['start_roll']=i*40;b['orientation_frame']['end_roll']=180+i*360;a['motion_paths']['bindings'].append(b)
    t=copy.deepcopy(template);t['id']='stress_progress_'+str(i);t['target_id']='object/'+obj['object_id']
    if i==1:
        for key in t['keys']:key['value']=1-key['value']
    a['scene_timeline']['tracks'].append(t)
args.output.write_text(json.dumps(d,indent=2)+'\n')
