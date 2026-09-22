#!/usr/bin/env python3
"""M6 producer -> compile -> native typed editing -> ray/preview acceptance."""
import argparse,copy,hashlib,json,os,platform,subprocess,sys
from pathlib import Path
import test_surface_mapping_m4 as m4
ROOT=m4.ROOT
sys.path.insert(0,str(ROOT/'tools'))
from surface_material_m6 import default_graph
from surface_uv_unwrap import unwrap
from smooth_mesh_reflection.prepare_reflection_matrix import build_request

def main():
    p=argparse.ArgumentParser();p.add_argument('--output-root',type=Path,required=True);p.add_argument('--sculpts-tool',type=Path);a=p.parse_args()
    out=a.output_root.resolve();out.mkdir(parents=True,exist_ok=False);binary=ROOT/f'build/toolchains/clang/{platform.machine()}';renderer=binary/'tools/cli/ray_tracing_render_headless'
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'))
    capabilities=json.loads(subprocess.check_output([renderer,'--surface-capabilities']));assert capabilities['capability']=='optic.surface_graph_v1'
    template=json.loads((ROOT/'tests/fixtures/mesh_asset_runtime_spheres/scene_runtime.json').read_text())
    author_template=json.loads((ROOT/'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json').read_text())
    results={'capabilities':capabilities}
    for name,source,space in [('checker_rest','triplanar_checker','object_rest'),('noise_world','noise3d','world'),('checker_world','triplanar_checker','world'),('noise_rest','noise3d','object_rest')]:
        folder=out/name;runtime=folder/'data/runtime';runtime.mkdir(parents=True)
        (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(folder/'videos')}));(runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
        original=folder/'source.obj';original.write_text(m4.OBJ);obj=folder/'unwrap.obj'
        m4.run([sys.executable,ROOT/'tools/surface_uv_unwrap.py','--source',original,'--expected-sha256',hashlib.sha256(original.read_bytes()).hexdigest(),'--output',obj,'--method','box','--uv-set-id','generated_uv'],folder/'unwrap.log',env)
        metadata=json.loads(obj.with_suffix('.uv.json').read_text());assert len(metadata['triangle_charts'])==2
        author=copy.deepcopy(author_template);author['authoring']['imported_mesh'].update(source_format='obj',source_uri=str(obj),uv_set_id=metadata['uv_set_id'],source_to_asset_scale=1,preserve_source_normals=True,normal_mode='none',crease_angle_degrees=60,source_unit_system='meter',topology_closed_volume_observed=False)
        author_path=folder/'authoring.json';author_path.write_text(json.dumps(author));dest=folder/'assets/mesh_assets';dest.mkdir(parents=True);asset=dest/'uv_asset.runtime.json'
        m4.run([binary/'tools/smooth_mesh_reflection/compile_runtime_fixture',author_path,folder,'uv_asset',asset],folder/'compile.log',env)
        compiled=json.loads(asset.read_text());assert compiled['mesh']['surface_attributes']['uv_set_id']=='generated_uv'
        scene=copy.deepcopy(template);scene['scene_id']='m6_'+name;mesh=m4.m0.mesh_instance('surface','uv_asset','mat_sphere_high',0,0,0)
        mesh['transform']['scale']={'x':-2,'y':1.3,'z':.7};mesh['transform']['rotation']={'x':17,'y':23,'z':31};mesh['transform']['position']={'x':.3,'y':.2,'z':.1};scene['objects']=[mesh]
        graph=default_graph(source);graph['nodes'][0]['space']=space;graph['producer']['unknown_metadata']={'preserve':[1,2,3]}
        scene.setdefault('extensions',{}).setdefault('ray_tracing',{})['authoring']={'object_materials':[{'object_id':'surface','material_id':0,'object_color':12756864,'roughness':.8,'reflectivity':.02,'surface_graph':graph}]}
        path=folder/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2))
        for pass_name in ('native','reopen'):m4.run([binary/'tests/scene_editor_workspace_visual_test',folder,path,'--graph-m6'],folder/(pass_name+'.log'),env)
        saved=json.loads(path.read_text());assert saved['extensions']['ray_tracing']['authoring']['object_materials'][0]['surface_graph']==graph
        results[name]=json.loads((folder/'mapping_m6.json').read_text());hashes=[]
        for route in ('flattened','tlas_blas'):
            req=build_request(folder,'direct',route);req['scene']['runtime_scene_path']=str(path);req['render'].update(width=240,height=180);req['inspection'].update(camera_position={'x':2,'y':-3,'z':3},camera_look_at={'x':0,'y':0,'z':0},camera_zoom=1.2)
            request=folder/(route+'.json');request.write_text(json.dumps(req));m4.run([renderer,'--request',request,'--render'],folder/(route+'.log'),env)
            frames=list((folder/'renders'/('direct_'+route)/'frames').glob('*.bmp'));assert len(frames)==1;hashes.append(hashlib.sha256(frames[0].read_bytes()).hexdigest())
        assert hashes[0]==hashes[1];results[name]['render_sha256']=hashes[0];print(name,results[name],flush=True)
    negatives={}
    for name in ('unknown_capability','cycle','wrong_type','missing_ref','duplicate_id','bad_scale','extra_output','fractional_seed','oversize','mixed_source','missing_object','null_graph','missing_node_id'):
        bad=copy.deepcopy(scene);row=bad['extensions']['ray_tracing']['authoring']['object_materials'][0];g=row['surface_graph']
        if name=='unknown_capability':g['required_capability']='future'
        elif name=='cycle':g['nodes'][5]['inputs'][0]='finish'
        elif name=='wrong_type':g['nodes'][5]['inputs'][0]='rough'
        elif name=='missing_ref':g['nodes'][5]['inputs'][0]='missing'
        elif name=='duplicate_id':g['nodes'][1]['id']='position'
        elif name=='bad_scale':g['nodes'][0]['scale_m']=0
        elif name=='extra_output':g['outputs']['displacement']='rough'
        elif name=='fractional_seed':g['nodes'][1]['seed']=.5
        elif name=='oversize':g['nodes']*=6
        elif name=='mixed_source':row['material_texture_stack']={'layers':[]}
        elif name=='missing_object':row['object_id']='absent'
        elif name=='null_graph':row['surface_graph']=None
        else:del g['nodes'][2]['id']
        path=folder/(name+'.json');path.write_text(json.dumps(bad));req=build_request(out/name,'direct','flattened');req['scene']['runtime_scene_path']=str(path);request=out/(name+'-request.json');request.write_text(json.dumps(req));m4.run([renderer,'--request',request,'--preflight'],out/(name+'.log'),env,False);assert 'surface_graph:' in (out/(name+'.log')).read_text();negatives[name]=True
    results['negative_preflights']=negatives
    for malformed in (None, {}, {'version':1,'required_capability':'optic.surface_graph_v1','color_space':'linear','nodes':[{},{}],'outputs':{}}):
        malformed_path=out/'malformed-graph.json';malformed_path.write_text(json.dumps(malformed))
        result=subprocess.run([renderer,'--validate-surface-graph',malformed_path],capture_output=True)
        assert result.returncode==2,(result.returncode,result.stderr)
    for invalid in ('v 0 0 0\nf 1 1 1\n','v 0 0 0\nf 1 1 1 1\n'):
        try:unwrap(invalid,'box',1)
        except ValueError:pass
        else:raise AssertionError('invalid unwrap accepted')
    # Producer graph validation and metadata-preserving typed edit use the same runtime parser.
    graph_path=out/'agent-graph.json';m4.run([sys.executable,ROOT/'tools/surface_material_m6.py','--renderer',renderer,'--output',graph_path],out/'agent-create.log',env)
    edited=out/'agent-edited.json';m4.run([sys.executable,ROOT/'tools/surface_material_m6.py','--renderer',renderer,'--graph',graph_path,'--expected-sha256',hashlib.sha256(graph_path.read_bytes()).hexdigest(),'--node','finish','--property','inputs','--value','["light","dark","pattern"]','--output',edited],out/'agent-edit.log',env)
    assert json.loads(edited.read_text())['producer']==json.loads(graph_path.read_text())['producer']
    # Projection determinism and planar independent coordinate oracle.
    projected,charts=unwrap(m4.OBJ,'planar_xy',2);assert projected==unwrap(m4.OBJ,'planar_xy',2)[0];assert 'vt 0.5 0.5' in projected
    if a.sculpts_tool:
        req={'schema':'line_drawing_agent_scene_request_v1','scene_id':'m6_sculpts','grid_size':.1,'construction_plane':{'axis':'xy','offset':0},'objects':[{'id':'panel','kind':'plane','axis':'xy','position':{'x':0,'y':0,'z':0},'width':2,'height':2,'lock_to_construction_plane':False,'lock_to_bounds':False,'surface_graph':default_graph()}]}
        req['objects'].extend([
            {'id':'block','kind':'rect_prism','axis':'xy','position':{'x':2,'y':0,'z':.5},'width':1,'height':1,'depth':1,'lock_to_bounds':False,'surface_graph':default_graph('noise3d')},
            {'id':'mesh','kind':'mesh_asset_instance','asset_id':'uv_asset','asset_source_path':str(asset),'variant':'runtime_default','material_id':'mat_default','position':{'x':-2,'y':0,'z':0},'scale':{'x':1,'y':1,'z':1},'lock_to_bounds':False,'surface_graph':default_graph()}])
        request=out/'sculpts-request.json';request.write_text(json.dumps(req));dest=out/'sculpts';m4.run([a.sculpts_tool.resolve(),'--request',request,'--out',dest,'--determinism-check'],out/'sculpts.log',env)
        generated=json.loads((dest/'scene_runtime.json').read_text());rows=generated['extensions']['ray_tracing']['authoring']['object_materials'];assert len(rows)==3
        for item in req['objects']:assert next(row for row in rows if row['object_id']==item['id'])['surface_graph']==item['surface_graph']
        render_req=build_request(out/'sculpts-render','direct','flattened');render_req['scene']['runtime_scene_path']=str(dest/'scene_runtime.json');render_req['render'].update(width=160,height=120);render_req['inspection'].update(camera_position={'x':2,'y':-3,'z':3},camera_look_at={'x':0,'y':0,'z':0})
        path=out/'sculpts-render.json';path.write_text(json.dumps(render_req));m4.run([renderer,'--request',path,'--render'],out/'sculpts-render.log',env);results['sculpts']='deterministic producer contract preserved and rendered'
    (out/'acceptance.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
