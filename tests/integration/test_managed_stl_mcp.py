#!/usr/bin/env python3
"""Native managed STL UI/MCP acceptance in a fresh contained project."""
import argparse,copy,json,os,subprocess,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output-root',required=True,type=Path);parser.add_argument('--binary',required=True,type=Path);parser.add_argument('--compiler',required=True,type=Path);args=parser.parse_args()
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
    # Closed tetrahedron, millimeter source coordinates. Includes ASCII and binary.
    v=[(0,0,0),(1000,0,0),(0,1000,0),(0,0,1000)]
    faces=[(0,2,1),(0,1,3),(0,3,2),(1,2,3)]
    text='solid test\n'
    for f in faces:
        text+='facet normal 0 0 0\nouter loop\n'+''.join('vertex %s %s %s\n'%v[i] for i in f)+'endloop\nendfacet\n'
    (out/'mesh.stl').write_text(text+'endsolid test\n')
    data=b'binary test'.ljust(80,b'\0')+struct.pack('<I',len(faces))
    for f in faces:data+=struct.pack('<12fH',0,0,0,*(x for i in f for x in v[i]),0)
    (out/'binary.stl').write_bytes(data);(out/'bad.stl').write_text('invalid STL')
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'),RAY_TRACING_MANAGED_MESH_HELPER=str(ROOT/'tools/managed_mesh_assets.py'),RAY_TRACING_MANAGED_MESH_COMPILER=str(args.compiler.resolve()),OPTIC_MESH_MCP_TEST=str(ROOT/'tests/integration/check_managed_stl_mcp.py'))
    for mode,log in (('--mesh-mcp','native.log'),('--mesh-mcp-reopen','reopen.log')):
        with (out/log).open('w') as stream:r=subprocess.run([str(args.binary.resolve()),str(out),str(path),mode],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        assert r.returncode==0,(r.returncode,out/log)
    receipt=json.loads((out/'mesh-mcp-receipt.json').read_text());receipt.update(fresh_process_reopen=True,ui_mcp_import_geometry_parity=True,legacy_pixels_restored=True)
    (out/'acceptance.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
