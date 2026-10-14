#!/usr/bin/env python3
"""Optical response native UI, MCP, persistence and fixed renderer parity."""
import argparse,copy,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output-root',required=True,type=Path);parser.add_argument('--binary',required=True,type=Path);args=parser.parse_args()
    out=args.output_root.resolve();out.mkdir(parents=True,exist_ok=False)
    runtime = out / 'data/runtime'
    runtime.mkdir(parents=True)
    (runtime / 'animation_config.json').write_text(json.dumps({
        'editorMode': 1, 'spaceMode': 1, 'windowWidth': 1280, 'windowHeight': 800,
        'inputRoot': str(ROOT / 'config'), 'outputRoot': str(runtime),
        'videoOutputRoot': str(out / 'videos')}))
    (runtime / 'scene_config.json').write_text(json.dumps({'window': {'width': 1280, 'height': 800}}))
    scene = json.loads((ROOT / 'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    scene['scene_id'] = 'object_mcp_acceptance'
    scene['world_scale'] = 1
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
    path=out/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2))
    (out/'original_scene.json').write_text(json.dumps(scene,indent=2))
    # Complete authored illumination avoids inherited config defaults changing
    # meaning when File Save first projects legacy lighting into the document.
    authoring['light_path']={'mode':'BEZIER_CUBIC','points':[{'x':.82,'y':-.92,'rotation':0.,'handleLink':False}]}
    authoring['light_path_depth']={'points':[{'z':.34,'lookPitch':0.}]}
    authoring['light_settings']={'intensity':.4,'radius':0.}
    authoring.setdefault('environment',{}).update(background_color={'r':1.,'g':1.,'b':1.},ambient_brightness=40.8,background_brightness=.16,environment_preset=1,background_brightness_auto=True)
    authoring['camera_path']={'mode':'BEZIER_CUBIC','points':[{'x':0,'y':-4.4,'rotation':180,'handleLink':False}]}
    authoring['camera_path_depth']={'points':[{'z':.62,'lookPitch':-.08}]}
    graph={'version':1,'required_capability':'optic.surface_graph_v1','color_space':'linear','producer':{'preserve':'optics'},'nodes':[{'id':'color','kind':'color','value':[.8,.9,1.]},{'id':'rough','kind':'scalar','value':.15}],'outputs':{'base_color':'color','roughness':'rough'}}
    authoring['object_materials'][0].update(material_id=5,glass_ior=1.45,glass_transmission=.8,glass_absorption_distance=2.,glass_thin_walled=True,surface_graph=graph)
    scene['objects'][1]['primitive']['frame']['origin']={'x':0,'y':2,'z':0}
    path.write_text(json.dumps(scene,indent=2));(out/'original_scene.json').write_text(json.dumps(scene,indent=2))
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'),OPTIC_OPTICAL_RESPONSE_TEST=str(ROOT/'tests/integration/check_optical_response_mcp.py'))
    for mode,log in (('--optical-mcp','native.log'),('--optical-mcp-reopen','reopen.log')):
        with (out/log).open('w') as stream:r=subprocess.run([str(args.binary.resolve()),str(out),str(path),mode],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=300)
        assert r.returncode==0,(r.returncode,out/log)
    receipt=json.loads((out/'optical-receipt.json').read_text());receipt.update(ui_graph_ior_shared_command=True,legacy_ui_transport_shared_history=True,fresh_process_reopen=True,fixed_sampling_direct_disney_exact=True)
    (out/'acceptance.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
