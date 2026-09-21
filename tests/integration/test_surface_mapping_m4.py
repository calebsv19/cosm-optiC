#!/usr/bin/env python3
"""M4 real OBJ -> compiled UV asset -> pack/LOD/native/ray acceptance."""
import argparse, copy, hashlib, importlib.util, json, os, platform, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from smooth_mesh_reflection.prepare_reflection_matrix import build_request
spec=importlib.util.spec_from_file_location('m0',ROOT/'tests/fixtures/surface_material_m0/generate.py')
m0=importlib.util.module_from_spec(spec);spec.loader.exec_module(m0)
MAP={'version':3,'required_capability':'optic.authored_uv_v1','method':'authored_uv','source_domain':'brick_cells_v1',
     'uv_set_id':'paint_uv','uv_scale':[3,3],'uv_offset':[.1,.2],'rotation_rad':.17,'seed':1729}
OBJ='v 0 0 0\nv 1 0 .2\nv 1 1 1\nv 0 1 .8\nvt 0 0\nvt 1 0\nvt 1 1\nvt 2 0\nvt 1 1\nvt 2 1\nvn -.2 -.8 1\nf 1/1/1 2/2/1 3/3/1\nf 1/4/1 3/5/1 4/6/1\n'
def run(cmd,log,env,ok=True):
    with log.open('w') as f:r=subprocess.run([str(c) for c in cmd],stdout=f,stderr=subprocess.STDOUT,env=env,timeout=240)
    assert (r.returncode==0)==ok,(r.returncode,log)
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output-root',type=Path,required=True);a=parser.parse_args()
    out=a.output_root.resolve();out.mkdir(parents=True,exist_ok=False)
    binary=ROOT/f'build/toolchains/clang/{platform.machine()}';env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'))
    template=json.loads((ROOT/'tests/fixtures/mesh_asset_runtime_spheres/scene_runtime.json').read_text())
    author_template=json.loads((ROOT/'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json').read_text())
    results={}
    for name in ('seam','mirrored_transform','degenerate_uv'):
        folder=out/name;runtime=folder/'data/runtime';runtime.mkdir(parents=True)
        (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(folder/'videos')}))
        (runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
        obj=folder/'source.obj';obj.write_text(OBJ if name!='degenerate_uv' else OBJ.replace('f 1/4/1 3/5/1 4/6/1','f 1/4/1 3/4/1 4/4/1'))
        author=copy.deepcopy(author_template)
        # The existing importer schema is retained, with additive OBJ format/UV set.
        source=author['authoring']['imported_mesh']
        source.update(source_format='obj',source_uri=str(obj),uv_set_id='paint_uv',source_to_asset_scale=1,
                      preserve_source_normals=True,normal_mode='none',crease_angle_degrees=60,source_unit_system='meter',topology_closed_volume_observed=False)
        author_path=folder/'authoring.json';author_path.write_text(json.dumps(author))
        dest=folder/'assets/mesh_assets';dest.mkdir(parents=True);asset=dest/'uv_asset.runtime.json'
        run([binary/'tools/smooth_mesh_reflection/compile_runtime_fixture',author_path,folder,'uv_asset',asset],folder/'compile.log',env)
        compiled=json.loads(asset.read_text());assert compiled['mesh']['surface_attributes']['uv_set_id']=='paint_uv'
        scene=copy.deepcopy(template);scene['scene_id']='m4_'+name
        mesh=m0.mesh_instance('surface','uv_asset','mat_sphere_high',0,0,0)
        mesh['transform']['scale']={'x':-2 if name=='mirrored_transform' else 2,'y':1.3,'z':.7}
        mesh['transform']['rotation']={'x':17,'y':23,'z':31}
        mesh.setdefault('extensions',{}).setdefault('ray_tracing',{})['surface_mapping']=copy.deepcopy(MAP)
        scene['objects']=[mesh]
        scene.setdefault('extensions',{}).setdefault('ray_tracing',{})['authoring']={'object_materials':[
            {'object_id':'surface','material_id':0,'object_color':12756864,'roughness':.8,'reflectivity':.02,
             'producer_note':'M4 retained source',
             'material_texture_stack':{'layers':[{'id':'brick','kind':'brick','placement':{'scale':1,'strength':1}}]}}]}
        path=folder/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2))
        run([binary/'tests/scene_editor_workspace_visual_test',folder,path,'--mapping-m4'],folder/'native.log',env)
        run([binary/'tests/scene_editor_workspace_visual_test',folder,path,'--mapping-m4'],folder/'reopen.log',env)
        for capture in ('m4-uv-material.ppm','m4-geometry-field-material.ppm'):
            pixels=(folder/capture).read_bytes().split(b'\n',3)[3]
            assert not any(pixels[i:i+3]==b'\xff\x00\xff' for i in range(0,len(pixels),3)),capture
        saved=json.loads(path.read_text());assert saved['objects'][0]['extensions']['ray_tracing']['surface_mapping']==MAP
        results[name]=json.loads((folder/'mapping_m4.json').read_text());results[name]['geometry_field_preview']=json.loads((folder/'mapping_m4_graph.json').read_text());print(name,results[name],flush=True)
        hashes=[]
        for route in ('flattened','tlas_blas'):
            req=build_request(folder,'direct',route);req['scene']['runtime_scene_path']=str(path)
            req['render'].update(width=240,height=180)
            req['inspection'].update(camera_position={'x':2,'y':-3,'z':3},camera_look_at={'x':0,'y':0,'z':0},camera_zoom=1.2)
            request=folder/(route+'.json');request.write_text(json.dumps(req))
            run([binary/'tools/cli/ray_tracing_render_headless','--request',request,'--render'],folder/(route+'.log'),env)
            frames=list((folder/'renders'/('direct_'+route)/'frames').glob('*.bmp'));assert len(frames)==1
            hashes.append(hashlib.sha256(frames[0].read_bytes()).hexdigest())
            if route=='tlas_blas':
                summary=json.loads(Path(req['progress']['summary_path']).read_text())
                assert summary['prepared_acceleration']['blas_persistent_cache_hits']>=1
                results[name]['persistent_blas_cache_hit']=True
        assert hashes[0]==hashes[1],hashes;results[name]['flattened_tlas_bmp_sha256']=hashes[0]
    folder=out/'seam';path=folder/'scene_runtime.json';scene=json.loads(path.read_text())
    negatives=[]
    for name in ('missing_set','zero_scale','unknown_capability','numeric_set','named_missing_set'):
        candidate=copy.deepcopy(scene);mapping=candidate['objects'][0]['extensions']['ray_tracing']['surface_mapping']
        if name=='missing_set':mapping['uv_set_id']='absent'
        elif name=='zero_scale':mapping['uv_scale']=[0,1]
        elif name=='unknown_capability':mapping['required_capability']='unknown'
        elif name=='numeric_set':mapping['uv_set_id']=123
        else:
            chart=copy.deepcopy(MAP);chart['uv_set_id']='missing'
            candidate['extensions']['ray_tracing']['authoring']['object_materials'][0]['surface_material_binding']={'version':1,'required_capability':'optic.surface_material_v3','mappings':[{'id':'bad-chart','definition':chart}]}
        bad=folder/(name+'.json');bad.write_text(json.dumps(candidate));req=build_request(out/name,'direct','flattened');req['scene']['runtime_scene_path']=str(bad)
        request=out/(name+'-request.json');request.write_text(json.dumps(req))
        run([binary/'tools/cli/ray_tracing_render_headless','--request',request,'--preflight'],out/(name+'.log'),env,False);negatives.append(name)
    asset=folder/'assets/mesh_assets/uv_asset.runtime.json';original=asset.read_bytes()
    try:
        for name in ('attribute_version','missing_corner','bad_tangent','nonfinite_uv','absent_attributes'):
            document=json.loads(original);attrs=document['mesh']['surface_attributes']
            if name=='attribute_version':attrs['version']=2
            elif name=='missing_corner':attrs['corners'].pop()
            elif name=='bad_tangent':attrs['corners'][0]['handedness']=0
            elif name=='nonfinite_uv':attrs['corners'][0]['uv'][0]=float('inf')
            else:document['mesh'].pop('surface_attributes')
            asset.write_text(json.dumps(document))
            req=build_request(out/name,'direct','flattened');req['scene']['runtime_scene_path']=str(path)
            request=out/(name+'-request.json');request.write_text(json.dumps(req))
            run([binary/'tools/cli/ray_tracing_render_headless','--request',request,'--preflight'],out/(name+'.log'),env,False);negatives.append(name)
    finally:asset.write_bytes(original)
    results['negative_preflights']=negatives
    (out/'acceptance.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
