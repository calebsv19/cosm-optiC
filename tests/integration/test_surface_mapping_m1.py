#!/usr/bin/env python3
"""Native M1 planar acceptance. New output directory required; no shared writes."""
import argparse
import copy
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from smooth_mesh_reflection.prepare_reflection_matrix import build_request

MAPPING={'version':1,'required_capability':'optic.planar_surface_v1','method':'planar',
         'space':'object_rest','source_domain':'brick_cells_v1','scale_policy':'stretch_with_object',
         'origin_m':[-2,-1,0],'axis_u':[1,0,0],'axis_v':[0,1,0],
         'tile_m':[.5,.5],'offset_m':[0,0],'pivot_m':[0,0],'rotation_rad':0,'seed':1729}

def run(command,log,env,ok=True):
    with log.open('w') as f:
        result=subprocess.run([str(a) for a in command],env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
    assert (result.returncode==0)==ok,(command,result.returncode,log)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root',type=Path,required=True)
    args=parser.parse_args();out=args.output_root.resolve();out.mkdir(parents=True,exist_ok=False)
    binaries=ROOT/f'build/toolchains/clang/{platform.machine()}'
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT))
    template=json.loads((ROOT/'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    results={}
    for name in ['plane','plane_world_scale','prism']:
        folder=out/name;runtime=folder/'data/runtime';runtime.mkdir(parents=True)
        (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(folder/'videos')}))
        (runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
        scene=copy.deepcopy(template);scene['scene_id']=name;scene['world_scale']=2.5 if name=='plane_world_scale' else 1
        obj=copy.deepcopy(template['objects'][0]);obj['object_id']='surface';obj['transform']['position']={'x':0,'y':0,'z':0}
        obj['transform']['rotation']={'x':0,'y':0,'z':0}
        obj['primitive']['frame']['origin']={'x':0,'y':0,'z':0};obj['primitive']['width']=4;obj['primitive']['height']=2
        if name=='prism':
            obj['object_type']=obj['primitive']['kind']='rect_prism_primitive';obj['primitive']['depth']=1
            obj.setdefault('extensions',{}).setdefault('ray_tracing',{})['surface_mapping']=copy.deepcopy(MAPPING)
        scene['objects']=[obj]
        scene['extensions']['ray_tracing']['authoring']['object_materials']=[{'object_id':'surface','material_id':0,'object_color':12756864,'roughness':.8,'reflectivity':.02,
            'material_texture_stack':{'layers':[{'id':'base','name':'Brick','kind':'brick','enabled':True,'placement':{'scale':1,'strength':1}}]}}]
        scene['extensions']['m1_provenance']={'graph_id':'preserved-source','source_mesh_digest':'a'*64}
        path=folder/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2))
        mode='--material-parity' if name=='prism' else '--mapping-m1'
        run([binaries/'tests/scene_editor_workspace_visual_test',folder,path,mode],folder/'native.log',env)
        if name!='prism':
            saved=json.loads(path.read_text())
            assert saved['extensions']['m1_provenance']==scene['extensions']['m1_provenance']
            mapping=saved['objects'][0]['extensions']['ray_tracing']['surface_mapping']
            assert mapping['seed']==1729 and mapping['producer_note']=={'keep':'editable-source'}
            run([binaries/'tests/scene_editor_workspace_visual_test',folder,path,'--mapping-m1-reopen'],folder/'reopen.log',env)
            results[name]=json.loads((folder/'mapping_m1.json').read_text())
        else:
            metrics=json.loads((folder/'parity.json').read_text())
            assert all(f['hits']==65536 and f['max_channel_error']<=1e-6 and f['cache_rgb_mae']<=.05 for f in metrics['faces'])
            results[name]=metrics
        req=build_request(folder,'direct','flattened');req['scene']['runtime_scene_path']=str(path)
        req['render'].update(width=320,height=240,integrator_3d='direct_light')
        req['inspection'].update(camera_position={'x':3,'y':-5,'z':5},camera_look_at={'x':0,'y':0,'z':0},ambient_strength=.08,top_fill_strength=.2,light_intensity=1.5,camera_zoom=1.2)
        request=folder/'request.json';request.write_text(json.dumps(req,indent=2))
        run([binaries/'tools/cli/ray_tracing_render_headless','--request',request,'--render'],folder/'render.log',env)
        summary=json.loads(Path(req['progress']['summary_path']).read_text());assert summary['rendered_frames'] and summary['frames_rendered']==1
        print(name,'passed',flush=True)
    # Reader rejects required unknown semantics, singular scales, and unsupported sources.
    reference=json.loads((out/'prism/scene_runtime.json').read_text())
    invalid=[]
    for name,key,value in [('unknown_method','method','axial'),('unknown_version','version',2),
                           ('zero_tile','tile_m',[0,.5]),('bad_axis','axis_u',[0,0,0]),
                           ('unknown_capability','required_capability','optic.future'),('missing_seed','seed',None)]:
        doc=copy.deepcopy(reference);doc['objects'][0]['extensions']['ray_tracing']['surface_mapping'][key]=value
        invalid.append((name,doc))
    for name in ['negative_scale','tiny_scale','nonunit_frame','graph_source','duplicate_source','missing_mapping','unsupported_layer']:
        doc=copy.deepcopy(reference);obj=doc['objects'][0]
        row=doc['extensions']['ray_tracing']['authoring']['object_materials'][0]
        if name=='negative_scale':obj['transform']['scale']['x']=-1
        elif name=='tiny_scale':obj['transform']['scale']['x']=.001
        elif name=='nonunit_frame':obj['primitive']['frame']['axis_u']['x']=2
        elif name=='graph_source':row['materialGraph']={'nodes':[]}
        elif name=='duplicate_source':doc['extensions']['ray_tracing']['authoring']['object_materials'].append(copy.deepcopy(row))
        elif name=='missing_mapping':obj['extensions']['ray_tracing']['surface_mapping']=None
        elif name=='unsupported_layer':row['material_texture_stack']['layers'][0]['kind']='wood'
        invalid.append((name,doc))
    for name,doc in invalid:
        path=out/f'{name}.json';path.write_text(json.dumps(doc))
        req=build_request(out/name,'direct','flattened');req['scene']['runtime_scene_path']=str(path)
        request=out/f'{name}-request.json';request.write_text(json.dumps(req))
        run([binaries/'tools/cli/ray_tracing_render_headless','--request',request,'--preflight'],out/f'{name}.log',env,False)
        assert 'surface_mapping' in (out/f'{name}.log').read_text()
    (out/'acceptance.json').write_text(json.dumps(results,indent=2)+'\n')

if __name__=='__main__': main()
