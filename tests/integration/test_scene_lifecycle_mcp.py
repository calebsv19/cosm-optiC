#!/usr/bin/env python3
"""Native static-lighting MCP acceptance with legacy objects and atmosphere."""
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
    scene['scene_id'] = 'render_mcp_acceptance'
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
    authoring['atmosphere_attachment']={'schema_version':'ray_atmosphere_attachment_v1','id':'preserved-volume','source_path':'attached.vf3d','source_kind':'vf3d','enabled':True,'visible':True,'affects_lighting':True,'optics':{'profile':'physical_smoke_v1','sampling_layout':'cell_centered','reference_concentration_kg_m3':.001,'mass_extinction_m2_kg':1000,'scattering_albedo':.5,'anisotropy':0.,'sky_illumination':False,'sky_error_budget':0.}}
    # Legacy zero-radius runtime carrier differs from the authored seed radius.
    # Derive the ordinary single-point carrier from the public camera/path fixture.
    authoring['light_path']={'mode':'BEZIER_CUBIC','points':[{'x':.82,'y':-.92,'rotation':0.,'handleLink':False}]}
    authoring['light_path_depth']={'points':[{'z':.34,'lookPitch':0}]}
    authoring['light_settings']={'intensity':7.5,'radius':0.,'producer_metadata':{'preserve':'light-settings'}}
    scene['lights'][0]['radius']=0.
    authoring['camera_path']={'mode':'BEZIER_CUBIC','points':[{'x':.625,'y':-1.25,'rotation':3.141592653589793,'handleLink':False}]}
    authoring['camera_path_depth']={'points':[{'z':1.875,'lookPitch':-.7853981633974483}]}
    scene['lights'][0]['producer_metadata']={'preserve':'primary'}
    authoring['environment']['producer_metadata']={'preserve':'environment'}
    # A second ordinary light proves the primary command preserves other sources.
    second=copy.deepcopy(scene['lights'][0]);second['light_id']='preserved_fill';second['position']['x']=8;scene['lights'].append(second)
    path=out/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2))
    (out/'original_scene.json').write_text(json.dumps(scene,indent=2))
    binary=args.binary.resolve()
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'),OPTIC_LIFECYCLE_MCP_TEST=str(ROOT/'tests/integration/check_scene_lifecycle_mcp.py'),OPTIC_LIGHTING_TIMELINE_FIXTURE=str(ROOT/'config/samples/light_timeline_editor_demo_runtime.json'))
    with (out/'native.log').open('w') as stream:
        result=subprocess.run([str(binary),str(out),str(path),'--scene-lifecycle'],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
    assert result.returncode==0,(result.returncode,out/'native.log')
    with (out/'fresh-reopen.log').open('w') as stream:
        result=subprocess.run([str(binary),str(out),str(out/'mcp-created.json'),'--scene-lifecycle-reopen'],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
    assert result.returncode==0,(result.returncode,out/'fresh-reopen.log')
    saved=json.loads(path.read_text());assert saved['extensions']['t1_provenance']==scene['extensions']['t1_provenance']
    assert saved['extensions']['ray_tracing']['authoring']['atmosphere_attachment']==authoring['atmosphere_attachment']
    assert saved['objects']==scene['objects'] and saved['cameras']==scene['cameras']
    assert saved['lights'][1]==scene['lights'][1] and saved['lights'][0]['producer_metadata']==scene['lights'][0]['producer_metadata']
    assert saved['extensions']['ray_tracing']['authoring']['environment']['producer_metadata']==authoring['environment']['producer_metadata']
    assert saved['extensions']['ray_tracing']['authoring']['light_settings']['producer_metadata']==authoring['light_settings']['producer_metadata']
    receipt=json.loads((out/'lifecycle-receipt.json').read_text());receipt.update(native_ui_create_open_save_as=True,fresh_process_created_scene_reopen=True,save_as_shared_undo_redo=True,new_scene_material_camera_lighting=True)
    assert json.loads((out/'ui-save-as.json').read_text())['objects']
    assert not json.loads((out/'ui-created.json').read_text())['objects']
    (out/'acceptance.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))

if __name__=='__main__':main()
