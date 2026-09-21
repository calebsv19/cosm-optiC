#!/usr/bin/env python3
"""Native M2 axial UI/runtime acceptance; outputs are create-only."""
import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import shutil
import csv
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from smooth_mesh_reflection.prepare_reflection_matrix import build_request
spec=importlib.util.spec_from_file_location('m0',ROOT/'tests/fixtures/surface_material_m0/generate.py')
m0=importlib.util.module_from_spec(spec);spec.loader.exec_module(m0)
MAP={'version':2,'required_capability':'optic.axial_surface_v2','method':'axial_height',
     'space':'object_rest','source_domain':'brick_cells_v1','scale_policy':'stretch_with_object',
     'origin_m':[0,0,0],'axis_u':[1,0,0],'axis_v':[0,0,1],'tile_m':[.5,.25],
     'offset_m':[0,0],'pivot_m':[0,0],'rotation_rad':0,'seed':1729,'reference_radius_m':1,
     'seam_rad':0,'pole_radius_m':.12,'height_range_m':[-1,1],
     'repeat_policy':'integer_circumference','pole_policy':'fade_to_base',
     'producer_note':{'source':'m2_generated_curved_fixture','preserve':True}}
def run(command,log,env,ok=True):
    with log.open('w') as f:r=subprocess.run([str(a) for a in command],stdout=f,stderr=subprocess.STDOUT,env=env,timeout=240)
    assert (r.returncode==0)==ok,(command,r.returncode,log)
def main():
    p=argparse.ArgumentParser();p.add_argument('--output-root',type=Path,required=True)
    p.add_argument('--cases',nargs='+',default=['sphere_low','sphere_high','cylinder_low','cylinder_high','sphere_subdivided']);a=p.parse_args()
    out=a.output_root.resolve();m0.generate(out)
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT));binary=ROOT/f'build/toolchains/clang/{platform.machine()}'
    if 'sphere_subdivided' in a.cases:
        folder=out/'sphere_subdivided';shutil.copytree(out/'sphere_low',folder)
        asset=folder/'assets/mesh_assets/sphere_low.runtime.json';doc=json.loads(asset.read_text());mesh=doc['mesh'];vertices=mesh['vertices'];triangles=[];edges={}
        def midpoint(a,b):
            key=tuple(sorted((a,b)))
            if key not in edges:
                edges[key]=len(vertices);vertices.append({k:(vertices[a][k]+vertices[b][k])*.5 for k in 'xyz'})
            return edges[key]
        for t in mesh['triangles']:
            a0,b,c=(t[k] for k in 'abc');ab,bc,ca=midpoint(a0,b),midpoint(b,c),midpoint(c,a0)
            triangles.extend(m0.triangle(*indices,t['surface_group_id']) for indices in [(a0,ab,ca),(ab,b,bc),(ca,bc,c),(ab,bc,ca)])
        mesh.update(vertices=vertices,triangles=triangles,vertex_count=len(vertices),triangle_count=len(triangles))
        for group in doc['surface_groups']:
            group['triangle_span']['start']*=4;group['triangle_span']['count']*=4
        asset.write_text(json.dumps(doc,indent=2))
    results={}
    for name in a.cases:
        folder=out/name;runtime=folder/'data/runtime';runtime.mkdir(parents=True)
        (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(folder/'videos')}))
        (runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
        path=folder/'scene_runtime.json';scene=json.loads(path.read_text());scene['world_scale']=2.5 if name=='cylinder_high' else 1
        scene['objects'][0].setdefault('extensions',{}).setdefault('ray_tracing',{})['surface_mapping']=copy.deepcopy(MAP)
        scene['extensions']['m2_source']={'asset':name,'graph_document':'retained producer identity'}
        path.write_text(json.dumps(scene,indent=2))
        run([binary/'tests/scene_editor_workspace_visual_test',folder,path,'--mapping-m2'],folder/'native.log',env)
        saved=json.loads(path.read_text());mapping=saved['objects'][0]['extensions']['ray_tracing']['surface_mapping']
        assert mapping['tile_m'][0]==.4 and mapping['seam_rad']==.23 and mapping['offset_m'][1]==.13
        assert mapping['producer_note']==MAP['producer_note'] and saved['extensions']['m2_source']==scene['extensions']['m2_source']
        run([binary/'tests/scene_editor_workspace_visual_test',folder,path,'--mapping-m2-reopen'],folder/'reopen.log',env)
        req=build_request(folder,'direct','flattened');req['scene']['runtime_scene_path']=str(path)
        req['render'].update(width=480,height=360,integrator_3d='direct_light')
        req['inspection'].update(camera_position={'x':3*scene['world_scale'],'y':-5*scene['world_scale'],'z':2*scene['world_scale']},camera_look_at={'x':0,'y':0,'z':0},ambient_strength=.08,top_fill_strength=.2,light_intensity=1.5,camera_zoom=1.2)
        request=folder/'request.json';request.write_text(json.dumps(req,indent=2))
        run([binary/'tools/cli/ray_tracing_render_headless','--request',request,'--render'],folder/'render.log',env)
        summary=json.loads(Path(req['progress']['summary_path']).read_text());assert summary['frames_rendered']==1
        results[name]=json.loads((folder/'mapping_m2.json').read_text());print(name,results[name],flush=True)
    if 'sphere_subdivided' in results and 'sphere_low' in results:
        # Reopen has identical persisted mapping values in both assets.
        def values(name):
            with (out/name/'mapping_m2_samples.csv').open() as f:return [[float(x) for x in row] for row in csv.reader(f)]
        first,second=values('sphere_low'),values('sphere_subdivided');assert len(first)==len(second)
        delta=max(abs(x-y) for a,b in zip(first,second) for x,y in zip(a,b));assert delta<1e-8,delta
        results['exact_subdivision_max_point_channel_delta']=delta
    reference=json.loads((out/a.cases[0]/'scene_runtime.json').read_text())
    for name,key,value in [('zero_radius','reference_radius_m',0),('bad_axis','axis_v',[1,0,0]),
        ('unknown_pole','pole_policy','guess'),('invalid_height','height_range_m',[1,-1]),
        ('too_many_repeats','tile_m',[.001,.2]),('rotation_breaks_seam','rotation_rad',.3),('pivot_breaks_seam','pivot_m',[1,0])]:
        scene=copy.deepcopy(reference);scene['objects'][0]['extensions']['ray_tracing']['surface_mapping'][key]=value
        path=out/a.cases[0]/f'{name}.json';path.write_text(json.dumps(scene));req=build_request(out/name,'direct','flattened');req['scene']['runtime_scene_path']=str(path)
        request=out/f'{name}-request.json';request.write_text(json.dumps(req));run([binary/'tools/cli/ray_tracing_render_headless','--request',request,'--preflight'],out/f'{name}.log',env,False)
        assert 'surface_mapping' in (out/f'{name}.log').read_text()
    # A plain primitive receives an editable source and mapping through actual UI controls.
    folder=out/'plane_ui';runtime=folder/'data/runtime';runtime.mkdir(parents=True)
    (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(folder/'videos')}))
    (runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
    scene=json.loads((ROOT/'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    obj=scene['objects'][0];obj['object_id']='surface';obj['object_type']=obj['primitive']['kind']='plane_primitive';obj['primitive']['width']=4;obj['primitive']['height']=2
    obj['transform']['position']={'x':0,'y':0,'z':0};obj['transform']['rotation']={'x':0,'y':0,'z':0};scene['objects']=[obj]
    scene['extensions']['ray_tracing']['authoring']['object_materials']=[{'object_id':'surface','material_id':0,'object_color':12756864,'roughness':.8,'producer_note':{'keep':True}}]
    path=folder/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2))
    run([binary/'tests/scene_editor_workspace_visual_test',folder,path,'--mapping-panel'],folder/'native.log',env)
    saved=json.loads(path.read_text());assert saved['objects'][0]['extensions']['ray_tracing']['surface_mapping']['tile_m'][0]==.7
    assert saved['extensions']['ray_tracing']['authoring']['object_materials'][0]['producer_note']=={'keep':True}
    results['plane_inspector_created_binding']=True
    (out/'acceptance.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
