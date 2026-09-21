#!/usr/bin/env python3
"""M5 planar primitive and axial mesh sampling adapters."""
import argparse,copy,json,os,platform
from pathlib import Path
import test_surface_mapping_m5 as m5
import test_surface_mapping_m2 as m2
import test_surface_mapping_m1 as m1
ROOT=m5.ROOT

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output-root',type=Path,required=True);args=parser.parse_args();out=args.output_root.resolve();m2.m0.generate(out)
    binary=ROOT/f'build/toolchains/clang/{platform.machine()}';env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT));results={}
    for name in ['plane_diagonal_a','cylinder_high','sphere_high']:
        folder=out/name;runtime=folder/'data/runtime';runtime.mkdir(parents=True)
        (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(folder/'videos')}))
        (runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
        path=folder/'scene_runtime.json';scene=json.loads(path.read_text())
        if name.startswith('plane'):
            template=json.loads((ROOT/'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text());obj=copy.deepcopy(template['objects'][0]);obj['object_id']='surface';obj['transform']['position']={'x':0,'y':0,'z':0};obj['transform']['rotation']={'x':0,'y':0,'z':0};obj['primitive']['frame']['origin']={'x':0,'y':0,'z':0};obj['primitive'].update(width=4,height=2);scene['objects']=[obj]
        mapping=copy.deepcopy(m1.MAPPING if name.startswith('plane') else m2.MAP)
        sampling={'version':1,'required_capability':'optic.surface_sampling_v1','filter':'trilinear','address':'repeat','color_space':'linear','period_tiles':[1 if mapping['version']==1 else 13,1]}
        color=folder/'color.png';rough=folder/'rough.png';normal=folder/'normal.png'
        m5.png(color,2,2,lambda x,y:(128,128,128,255));m5.png(rough,64,64,lambda x,y:((x+y)%2*255,0,0,255));m5.png(normal,2,2,lambda x,y:(192,160,238,255))
        sampling['channels']={'base_color':m5.channel(color,'srgb'),'roughness':m5.channel(rough,'data'),'normal':m5.channel(normal,'data')}
        scene['objects'][0].setdefault('extensions',{}).setdefault('ray_tracing',{}).update(surface_mapping=mapping,surface_sampling=sampling)
        path.write_text(json.dumps(scene));
        for phase in ['native','reopen']:m2.run([binary/'tests/scene_editor_workspace_visual_test',folder,path,'--sampling-curved-m5'],folder/(phase+'.log'),env)
        saved=json.loads(path.read_text());assert saved['objects'][0]['extensions']['ray_tracing']['surface_sampling']==sampling
        results[name]=json.loads((folder/'mapping_m5.json').read_text());print(name,results[name],flush=True)
        req=m5.build_request(folder,'direct','flattened');req['scene']['runtime_scene_path']=str(path);req['render'].update(width=240,height=180);request=folder/'request.json';request.write_text(json.dumps(req));m2.run([binary/'tools/cli/ray_tracing_render_headless','--request',request,'--render'],folder/'render.log',env)
    (out/'acceptance.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
