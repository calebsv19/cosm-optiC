#!/usr/bin/env python3
"""Opt-in material diagnostic; records mismatches, not a parity acceptance gate."""
import argparse, copy, json, os, platform, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from smooth_mesh_reflection.prepare_reflection_matrix import build_request

def run(args,log,env):
    with log.open('w') as f: subprocess.run([str(a) for a in args],stdout=f,stderr=subprocess.STDOUT,env=env,check=True,timeout=120)

def pixels(path):
    with path.open('rb') as f:
        assert f.readline().strip()==b'P6'
        line=f.readline()
        while line.startswith(b'#'):line=f.readline()
        w,h=map(int,line.split());assert f.readline().strip()==b'255'
        data=f.read();assert len(data)==w*h*3
        return data

def differences(a,b):
    assert len(a)==len(b)
    return sum(a[i:i+3]!=b[i:i+3] for i in range(0,len(a),3))

def summarize(out,results):
    changes={}
    for case in ['plane_scale','plane_offset_u','plane_offset_v','plane_rotation','prism_faces']:
        base='prism_base' if case=='prism_faces' else 'plane_base'
        faces=range(6) if case=='prism_faces' else range(1)
        entry={}
        for suffix in ['viewport','render']:
            entry[suffix+'_albedo_changed_samples']=[differences(pixels(out/base/f'face_{i}_{suffix}_albedo.ppm'),pixels(out/case/f'face_{i}_{suffix}_albedo.ppm')) for i in faces]
        entry['native_viewport_changed_pixels']=differences(pixels(out/base/'viewport.ppm'),pixels(out/case/'viewport.ppm'))
        if case!='prism_faces':
            assert entry['viewport_albedo_changed_samples'][0]>0 and entry['render_albedo_changed_samples'][0]>0
            assert entry['native_viewport_changed_pixels']>0
        changes[case]=entry
    (out/'diagnostic.json').write_text(json.dumps({'cases':results,'changes':changes,'scope':'Diagnostic, not a parity pass. Albedo metrics compare the ideal pre-cache viewport sampler to actual runtime ray-hit material payloads. Native captures and separate headless direct-light BMPs also provided. Editor save invokes the control mutation adapter, not mouse dragging.'},indent=2))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output-root',type=Path,required=True);args=parser.parse_args()
    out=args.output_root.resolve();out.mkdir(parents=True,exist_ok=False)
    binaries=ROOT/f'build/toolchains/clang/{platform.machine()}'
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT))
    template=json.loads((ROOT/'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    results={}
    for case in ['plane_base','plane_scale','plane_offset_u','plane_offset_v','plane_rotation','prism_base','prism_faces','editor_save']:
        folder=out/case;runtime=folder/'data/runtime';runtime.mkdir(parents=True)
        (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(folder/'videos')}))
        (runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
        scene=copy.deepcopy(template);scene['scene_id']=case
        obj=copy.deepcopy(template['objects'][0]);obj['object_id']='surface';obj['transform']['position']={'x':0,'y':0,'z':0}
        obj['primitive']['frame']['origin']={'x':0,'y':0,'z':0};obj['primitive']['width']=4;obj['primitive']['height']=2
        if case.startswith('prism'):
            obj['object_type']=obj['primitive']['kind']='rect_prism_primitive';obj['primitive']['depth']=1
        scene['objects']=[obj]
        placement={'scale':2 if case=='plane_scale' else 1,'strength':1,'offset_u':.23 if case=='plane_offset_u' else 0,'offset_v':.19 if case=='plane_offset_v' else 0,'rotation':.4 if case=='plane_rotation' else 0}
        material={'object_id':'surface','material_id':0,'object_color':12756864,'roughness':.8,'reflectivity':.02,'material_texture_stack':{'layers':[{'id':'base','name':'Brick','kind':'brick','enabled':True,'placement':placement}]}}
        if case=='prism_faces': material['procedural_texture']={'face_placements':[{'face_group_index':i,'layer_index':0,'layer_id':'base','scale':1+i*.5,'offset_u':i*.13,'offset_v':i*.07,'strength':1} for i in range(6)]}
        scene['extensions']['ray_tracing']['authoring']['object_materials']=[material]
        scene['lights'][0]['position']={'x':-2,'y':-3,'z':5}
        path=folder/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2))
        run([binaries/'tests/scene_editor_workspace_visual_test',folder,path,'--material-edit-proof' if case=='editor_save' else '--material-parity'],folder/'viewport.log',env)
        if case=='editor_save':
            live=json.loads((folder/'parity.json').read_text())
            (folder/'live_parity.json').write_text(json.dumps(live,indent=2))
            run([binaries/'tests/scene_editor_workspace_visual_test',folder,path,'--material-parity'],folder/'reopen.log',env)
            assert live==json.loads((folder/'parity.json').read_text()),'Fresh reopen changed evaluated material'
            assert abs(live['scale']-2)<1e-8 and abs(live['offset_u']-.23)<1e-8 and abs(live['offset_v']-.19)<1e-8,live
        request=build_request(folder,'direct','flattened');request['scene']['runtime_scene_path']=str(path)
        request['render'].update(width=640,height=480,integrator_3d='direct_light')
        request['inspection'].update(camera_position={'x':3,'y':-5,'z':5},camera_look_at={'x':0,'y':0,'z':0},ambient_strength=.08,top_fill_strength=.2,light_intensity=1.5,camera_zoom=1.8)
        request_path=folder/'request.json';request_path.write_text(json.dumps(request,indent=2))
        run([binaries/'tools/cli/ray_tracing_render_headless','--request',request_path,'--render'],folder/'render.log',env)
        summary=json.loads(Path(request['progress']['summary_path']).read_text())
        assert summary['rendered_frames'] and summary['frames_rendered']==1,summary.get('diagnostics')
        results[case]=json.loads((folder/'parity.json').read_text())
        print(case,results[case],flush=True)
    summarize(out,results)
if __name__=='__main__':main()
