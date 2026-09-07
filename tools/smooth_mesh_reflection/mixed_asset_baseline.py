#!/usr/bin/env python3
"""Capture a managed dragon/wrench/dresser baseline from supplied authoring files.

Source assets stay private and unchanged. Output must be a new directory.
"""
import argparse
import json
from pathlib import Path
import platform
import subprocess
import sys
import os

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from managed_mesh_assets import update, digest, status
from smooth_mesh_reflection.prepare_reflection_matrix import build_scene, build_request, mesh_object


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    for name in ('dragon', 'wrench', 'dresser'):
        p.add_argument('--' + name, type=Path, required=True, help='Existing mesh authoring JSON')
    p.add_argument('--compiler', type=Path, default=ROOT / f'build/toolchains/clang/{platform.machine()}/tools/smooth_mesh_reflection/compile_runtime_fixture')
    p.add_argument('--renderer', type=Path, default=ROOT / f'build/toolchains/clang/{platform.machine()}/tools/cli/ray_tracing_render_headless')
    a=p.parse_args(); out=a.out.resolve();out.mkdir(parents=True, exist_ok=False)
    a.compiler=a.compiler.resolve();a.renderer=a.renderer.resolve()
    scene=build_scene({f: f for f in ('crease', 'analytic_sphere', 'icosphere', 'organic_blob')})
    scene['objects']=scene['objects'][:6]
    # Reflective floor makes direct and reflected surfaces visible together.
    scene['extensions']['ray_tracing']['authoring']['object_materials'][0].update(material_id=1, roughness=0.015, reflectivity=0.98, object_color=15132390)
    path=out/'scene_runtime.json';path.write_text(json.dumps(scene, indent=2))
    cube=out/'floor.stl'
    subprocess.run([sys.executable,str(ROOT/'tools/smooth_mesh_reflection/generate_fixtures.py'),'--family','crease','--tier','unit','--output',str(cube)],check=True,capture_output=True)
    update(path,a.compiler,source=cube,asset_id='floor',object_id='matte_floor',default_mode='flat')
    for obj in scene['objects'][1:]:
        update(path,a.compiler,asset_id='floor',object_id=obj['object_id'])
    sources={}
    for name,x,mode in [('dragon',-2.8,'smooth'),('wrench',0,'flat'),('dresser',2.8,'crease_aware')]:
        author_path=getattr(a,name).resolve();author=json.loads(author_path.read_text())
        imp=author['authoring']['imported_mesh'];source=Path(imp['source_uri'])
        if not source.is_absolute(): source=author_path.parent/source
        obj=mesh_object(name,name,x,1.2,'mat_metal',y=0,scale=(1,1,1))
        update(path,a.compiler,source=source,asset_id=name,spawn=obj,default_mode=mode,
               scale=imp.get('source_to_asset_scale',1),weld_tolerance=imp.get('weld_tolerance',1e-6))
        scene=json.loads(path.read_text());obj=next(o for o in scene['objects'] if o['object_id']==name)
        runtime=out/obj['extensions']['line_drawing']['runtime_mesh_path'];mesh=json.loads(runtime.read_text())['mesh']
        low={k:min(v[k] for v in mesh['vertices']) for k in 'xyz'};high={k:max(v[k] for v in mesh['vertices']) for k in 'xyz'}
        scale=2.15/max(high[k]-low[k] for k in 'xyz')
        obj['transform']['scale']={k:scale for k in 'xyz'}
        obj['transform']['position']={'x':x-(low['x']+high['x'])*scale/2,'y':-(low['y']+high['y'])*scale/2,'z':0.25-low['z']*scale}
        scene['extensions']['ray_tracing']['authoring']['object_materials'].append({'object_id':name,'material_id':2,'object_color':12619069,'roughness':0.16,'reflectivity':0.72})
        path.write_text(json.dumps(scene,indent=2))
        sources[name]={'source_sha256':digest(source),'authoring_sha256':digest(author_path),'triangle_count':mesh['triangle_count'], 'normal_provenance':mesh.get('normal_provenance','none'),'runtime_sha256':digest(runtime),'mode':mode}
    report={'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            'compiler_sha256':digest(a.compiler),'renderer_sha256':digest(a.renderer),'assets':sources,'managed':status(path,a.compiler),'runs':{}}
    for mode in ('configured','flat','configured_repeat'):
        request=build_request(out,mode,'tlas_blas_parity')
        request['render'].update(width=640,height=400)
        request['inspection'].update(camera_position={'x':0,'y':-7,'z':5},camera_look_at={'x':0,'y':0,'z':0.65})
        request_path=out/(mode+'.request.json');request_path.write_text(json.dumps(request,indent=2))
        env=os.environ.copy()
        env.pop('RAY_TRACING_MESH_SHADING_MODE',None)
        if mode=='flat':env['RAY_TRACING_MESH_SHADING_MODE']='flat'
        summary=out/(mode+'.summary.json')
        subprocess.run([str(a.renderer),'--request',str(request_path),'--render','--summary',str(summary)],env=env,check=True,capture_output=True)
        result=json.loads(summary.read_text());frame=out/f'renders/{mode}_tlas_blas_parity/frames/frame_0000.bmp'
        assert result['rendered_frames']
        report['runs'][mode]={'image_sha256':digest(frame),'scene_triangle_count':result['bvh_summary']['triangle_count'],'route_parity_mismatches':result['prepared_acceleration']['route_parity_mismatches']}
    report['baseline_repeated_exactly']=report['runs']['configured']['image_sha256']==report['runs']['configured_repeat']['image_sha256']
    report['configured_differs_from_flat']=report['runs']['configured']['image_sha256']!=report['runs']['flat']['image_sha256']
    report['route_parity_passed']=all(r['route_parity_mismatches']==0 for r in report['runs'].values())
    (out/'baseline_report.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))
    if not all(report[k] for k in ('route_parity_passed', 'baseline_repeated_exactly', 'configured_differs_from_flat')):
        raise SystemExit(1)

if __name__=='__main__':main()
