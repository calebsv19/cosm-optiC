#!/usr/bin/env python3
"""Native animation MCP acceptance with legacy objects and atmosphere."""
import argparse
import copy
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-root', required=True, type=Path)
    parser.add_argument('--binary', required=True, type=Path, help='Contained native acceptance binary')
    args = parser.parse_args()
    out = args.output_root.resolve()
    out.mkdir(parents=True, exist_ok=False)
    runtime = out / 'data/runtime'
    runtime.mkdir(parents=True)
    (runtime / 'animation_config.json').write_text(json.dumps({
        'editorMode': 1, 'spaceMode': 1, 'windowWidth': 1280, 'windowHeight': 800,
        'inputRoot': str(ROOT / 'config'), 'outputRoot': str(runtime),
        'videoOutputRoot': str(out / 'videos')}))
    (runtime / 'scene_config.json').write_text(json.dumps({'window': {'width': 1280, 'height': 800}}))
    scene = json.loads((ROOT / 'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    scene['scene_id'] = 'timeline_controls_acceptance'
    scene['world_scale'] = 2
    source = copy.deepcopy(scene['objects'][0])
    scene['objects'] = []
    for index, name in enumerate(('surface', 'recipient', 'rowless')):
        obj = copy.deepcopy(source)
        obj['object_id'] = name
        obj['object_type'] = obj['primitive']['kind'] = 'rect_prism_primitive'
        obj['primitive'].update(width=2, height=2, depth=.5)
        obj['primitive']['frame']['origin'] = {'x': index * 2.5, 'y': 0, 'z': 0}
        obj['transform']['position'] = {'x': 0, 'y': 0, 'z': 0}
        obj['transform']['rotation'] = {'x': 0, 'y': 0, 'z': 0}
        obj['transform']['scale'] = {'x': 1, 'y': 1, 'z': 1}
        obj['flags'] = {'visible': True, 'locked': False, 'selectable': True}
        obj.pop('extensions', None)
        scene['objects'].append(obj)
    authoring = scene.setdefault('extensions', {}).setdefault('ray_tracing', {}).setdefault('authoring', {})
    authoring['object_materials'] = [
        {'object_id': name, 'material_id': 0, 'object_color': 12756864, 'roughness': .65,
         'reflectivity': .02, 'glass_ior': 1.8 if name == 'surface' else 1.1,
         't1_unknown_producer_metadata': {'preserve': name}}
        for name in ('surface', 'recipient')]
    scene['extensions']['t1_provenance'] = {'preserve': ['unrelated', 123]}
    scene['objects'][2].pop('flags') # Legacy omission remains omitted on reads/save.
    for obj in scene['objects']:
        obj['producer_metadata']={'preserve':obj['object_id']}
    import shutil
    shutil.copyfile(ROOT/'tests/fixtures/scene_project_worker_snapshot/assets/vf3d/active/frame_000000.vf3d',out/'attached.vf3d')
    authoring['atmosphere_attachment']={'schema_version':'ray_atmosphere_attachment_v1','id':'preserved-volume','source_path':'attached.vf3d','source_kind':'vf3d','enabled':False,'visible':False,'affects_lighting':False}
    # Retained carriers give the legacy camera an explicit visible static pose,
    # and an unambiguous two-point light path exercises shared light setup.
    authoring['camera_path']={'mode':'BEZIER_CUBIC','points':[{'x':.625,'y':-1.25,'rotation':3.141592653589793,'handleLink':False}]}
    authoring['camera_path_depth']={'points':[{'z':1.875,'lookPitch':-.7853981633974483}]}
    authoring['light_path']={'mode':'BEZIER_CUBIC','points':[{'x':.82,'y':-.92,'rotation':0.,'velocity1':{'vx':.1,'vy':0},'handleLink':False},{'x':1.12,'y':-.92,'rotation':0.,'velocity2':{'vx':-.1,'vy':0},'handleLink':False}]}
    authoring['light_path_depth']={'points':[{'z':.34,'lookPitch':0},{'z':.34,'lookPitch':0}]}
    authoring['light_settings']={'intensity':7.5,'radius':0.,'producer_metadata':{'preserve':'legacy-light'}}
    scene['lights'][0]['radius']=0.
    def channel(id,target,prop,unit,keys,enabled=True):
        return dict(id=id,target_id=target,property_id=prop,value_type='scalar',unit=unit,source='authored',enabled=enabled,
                    producer_metadata={'preserve':id},keys=[dict(frame=f,value=v,interpolation='linear',tangent_mode='broken',incoming_handle={'frame_offset':0.,'value_offset':0.},outgoing_handle={'frame_offset':0.,'value_offset':0.},key_producer={'preserve':f}) for f,v in keys])
    tracks=[channel('lens','camera/main','camera/fov_y','degrees',[(0,60),(24,70),(39,55)]),
            channel('yaw','camera/main','camera/yaw','radians',[(0,3.141592653589793),(24,3.291592653589793)]),
            channel('light-progress','light/mirror_key_light','light/path_progress','unitless',[(0,0),(39,1)]),
            channel('intensity','light/mirror_key_light','light/intensity','relative_intensity',[(0,7.5),(24,1.5)])]
    tracks += [channel('position-'+axis,'object/surface','object/transform/position_'+axis,'world_distance',[(0,0),(24,.8 if axis=='x' else 0)]) for axis in ['x','y','z']]
    tracks += [channel('dormant-camera','camera/main','camera/pitch','radians',[(60,.2)],False)]
    authoring['scene_timeline']=dict(version=1,rate={'numerator':30,'denominator':1,'producer_rate':'keep'},range={'start_frame':0,'frame_count':80,'producer_range':'keep'},tracks=tracks,producer_timeline={'preserve':['old',1]})
    progress=copy.deepcopy(tracks[2]);progress.pop('target_id');progress.pop('source')
    authoring['light_timeline']=dict(version=2,rate={'numerator':30,'denominator':1},range={'start_frame':0,'frame_count':40},target_id='light/mirror_key_light',spatial_path={'path':copy.deepcopy(authoring['light_path']),'depth':copy.deepcopy(authoring['light_path_depth'])},tracks=[progress],producer_legacy={'preserve':'light timing'})
    path=out/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2))
    (out/'original_scene.json').write_text(json.dumps(scene,indent=2))
    binary=args.binary.resolve()
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'),OPTIC_TIMELINE_CONTROLS_TEST=str(ROOT/'tests/integration/check_timeline_controls_mcp.py'))
    for mode,log in (('--timeline-controls-mcp','native.log'),('--timeline-controls-mcp-reopen','reopen.log')):
        with (out/log).open('w') as stream:
            result=subprocess.run([str(binary),str(out),str(path),mode],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        assert result.returncode==0,(result.returncode,out/log)
    saved=json.loads(path.read_text());assert saved['extensions']['t1_provenance']==scene['extensions']['t1_provenance']
    assert saved['extensions']['ray_tracing']['authoring']['atmosphere_attachment']==authoring['atmosphere_attachment']
    assert saved['objects']==scene['objects'] and saved['cameras']==scene['cameras']
    receipt=json.loads((out/'timeline-controls-receipt.json').read_text());receipt.update(fresh_process_reopen=True,fixed_sampling_direct_disney_fresh_reopen=True,ui_timeline_settings=True,legacy_direct_light_restore=True,legacy_source_and_attachment_preserved=True)
    (out/'acceptance.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))

if __name__=='__main__':main()
