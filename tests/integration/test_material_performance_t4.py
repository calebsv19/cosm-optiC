#!/usr/bin/env python3
"""Fixed 640x480 material preview baseline; separate triangle and instance axes.

The runner measures the selected binary without reducing quality. Each case runs
in a fresh native process; input OBJ
contains named per-corner UVs so actual submitted geometry cannot silently LOD
away the requested workload. Reuse --compare after an independently proven change.
"""
import argparse
import copy
import hashlib
import json
import math
import os
import platform
import statistics
import subprocess
from pathlib import Path
import test_surface_mapping_m4 as m4
from test_surface_mapping_m5 import png, channel
from test_surface_composition_t3 import graph, config
ROOT=m4.ROOT
CASES={'triangles_8k':(8000,1),'triangles_100k':(100000,1),'triangles_1m':(1000000,1),
       'instances_10':(8000,10),'instances_64':(8000,64),'instances_100':(8000,100)}
DEFAULT_CASES=[name for name in CASES if name!='instances_100']

def write(path,value):
    path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')

def run(command,log,env,timeout=1800):
    with log.open('w') as output:
        result=subprocess.run([str(x) for x in command],stdout=output,stderr=subprocess.STDOUT,env=env,timeout=timeout)
    assert result.returncode==0,(result.returncode,log)

def grid(path,triangles):
    nx,ny={8000:(80,50),100000:(250,200),1000000:(1000,500)}[triangles]
    with path.open('w') as out:
        for y in range(ny+1):
            for x in range(nx+1):out.write(f'v {2*x/nx-1:.12g} {2*y/ny-1:.12g} 0\n')
        for y in range(ny+1):
            for x in range(nx+1):out.write(f'vt {x/nx:.12g} {y/ny:.12g}\n')
        out.write('vn 0 0 1\n')
        for y in range(ny):
            for x in range(nx):
                a=y*(nx+1)+x+1;b=a+1;c=a+nx+2;d=c-1
                out.write(f'f {a}/{a}/1 {b}/{b}/1 {c}/{c}/1\nf {a}/{a}/1 {c}/{c}/1 {d}/{d}/1\n')

def digest_file(path):
    digest=hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda:source.read(1024*1024),b''):digest.update(chunk)
    return digest.hexdigest()

def prepare_mesh(out,triangles,binary,env):
    folder=out/'meshes'/str(triangles);folder.mkdir(parents=True)
    source=folder/'source.obj';grid(source,triangles)
    author=json.loads((ROOT/'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json').read_text())
    author['authoring']['imported_mesh'].update(source_format='obj',source_uri=str(source),uv_set_id='paint_uv',source_to_asset_scale=1,preserve_source_normals=True,normal_mode='none',crease_angle_degrees=60,source_unit_system='meter',topology_closed_volume_observed=False)
    author_path=folder/'authoring.json';write(author_path,author)
    assets=folder/'assets/mesh_assets';assets.mkdir(parents=True);asset=assets/'uv_asset.runtime.json'
    print(f'compile {triangles} triangles',flush=True)
    run([binary/'tools/smooth_mesh_reflection/compile_runtime_fixture',author_path,folder,'uv_asset',asset],folder/'compile.log',env)
    return assets

def fixture(folder,triangles,instances,assets):
    config(folder);(folder/'assets').mkdir();(folder/'assets/mesh_assets').symlink_to(assets,target_is_directory=True)
    scene=json.loads((ROOT/'tests/fixtures/mesh_asset_runtime_spheres/scene_runtime.json').read_text())
    scene['scene_id']=folder.name;scene['objects']=[];rows=[]
    color=folder/'color.png';rough=folder/'rough.png';normal=folder/'normal.png'
    png(color,64,64,lambda x,y:(64+((x//8+y//8)%2)*96,80+y,128+x,128))
    png(rough,64,64,lambda x,y:(64+(x//8)%2*128,0,0,255))
    png(normal,64,64,lambda x,y:(128+((x//8)%2)*32,128,250,255))
    sampling={'version':1,'required_capability':'optic.surface_sampling_v1','filter':'trilinear','address':'repeat','color_space':'linear','period_tiles':[1,1],'normal_strength':1,
              'channels':{'base_color':channel(color,'linear'),'roughness':channel(rough,'data'),'normal':channel(normal,'data')}}
    mapping=copy.deepcopy(m4.MAP);mapping.update(uv_scale=[1,1],uv_offset=[0,0],rotation_rad=0)
    side=math.ceil(math.sqrt(instances))
    for i in range(instances):
        identity=f'surface_{i}';obj=m4.m0.mesh_instance(identity,'uv_asset','mat_sphere_high',(i%side-(side-1)/2)*2.3,(i//side-(side-1)/2)*2.3,0)
        obj.setdefault('extensions',{}).setdefault('ray_tracing',{}).update(surface_mapping=copy.deepcopy(mapping),surface_sampling=copy.deepcopy(sampling))
        scene['objects'].append(obj)
        g=graph();g['nodes'].extend([{'id':'position','kind':'coordinate','space':'object_rest','scale_m':.2,'offset':[.03,.07,0]}, {'id':'pattern','kind':'noise3d','inputs':['position'],'seed':17}])
        next(n for n in g['nodes'] if n['id']=='finish')['inputs'][2]='pattern'
        rows.append({'object_id':identity,'material_id':0,'roughness':.8,'reflectivity':.02,'object_color':12756864,'surface_graph':g})
    scene.setdefault('extensions',{}).setdefault('ray_tracing',{})['authoring']={'object_materials':rows}
    path=folder/'scene_runtime.json';write(path,scene);return path

def summarize(receipt):
    result={}
    for phase in ('orbit','cache_hit','edit','production_interactive'):
        samples=[s for s in receipt['samples'] if s['phase']==phase]
        metrics={k:[s[k] for s in samples] for k in ('frame_ms','document_command_ms','preview_refresh_ms')}
        metrics['edit_total_ms']=[s['frame_ms']+s['document_command_ms']+s['preview_refresh_ms'] for s in samples]
        metrics.update({name:[s['stage_ms'][i] for s in samples] for i,name in enumerate(receipt['stage_names'])})
        result[phase]={k:{'p50':statistics.median(v),'p95':sorted(v)[math.ceil(.95*len(v))-1]} for k,v in metrics.items()}
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root',type=Path,required=True)
    parser.add_argument('--cases',nargs='+',choices=CASES,default=DEFAULT_CASES)
    parser.add_argument('--samples',type=int,default=20)
    parser.add_argument('--fixtures-only',action='store_true')
    parser.add_argument('--compare',type=Path)
    parser.add_argument('--require-budgets',action='store_true',help='Fail when any requested comparison gate fails')
    parser.add_argument('--binary',type=Path,help='Exact native host binary; compiler tool remains the standard build')
    parser.add_argument('--analyze-existing',action='store_true',help='Aggregate existing per-case native receipts without compiling or running')
    args=parser.parse_args();assert 5<=args.samples<=100
    if args.require_budgets and not args.compare:parser.error('--require-budgets requires --compare')
    out=args.output_root.resolve();out.mkdir(parents=True,exist_ok=args.analyze_existing)
    binary=ROOT/f'build/toolchains/clang/{platform.machine()}'
    native_binary=args.binary.resolve() if args.binary else binary/'tests/scene_editor_workspace_visual_test'
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'))
    meshes={};results={'matrix':'triangle axis at1instance; supported instance axis1/10/64 at8K; requested100 unsupported' ,'samples_per_phase':args.samples,'machine':platform.platform(),'native_binary_sha256':digest_file(native_binary),'unsupported_boundaries':{'instances_100':{'requested_instances':100,'supported':False,'runtime_instance_limit':64,'reason':'RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES in include/import/runtime_mesh_asset_loader.h'}},'cases':{}}
    for name in args.cases:
        triangles,instances=CASES[name]
        if instances>64:continue
        folder=out/name;scene=folder/'scene_runtime.json'
        if not args.analyze_existing:
            if triangles not in meshes:meshes[triangles]=prepare_mesh(out,triangles,binary,env)
            scene=fixture(folder,triangles,instances,meshes[triangles])
            if args.fixtures_only:continue
            native_env=dict(env,OPTIC_T4_TRIANGLES=str(triangles),OPTIC_T4_INSTANCES=str(instances),OPTIC_T4_SAMPLES=str(args.samples))
            print(f'native {name} {args.samples} samples',flush=True)
            run([native_binary,folder,scene,'--material-performance-t4'],folder/'native.log',native_env)
        receipt=json.loads((folder/'material_performance_t4.json').read_text())
        assert receipt['actual_asset_triangles']==triangles and receipt['attribute_protected']
        for sample in receipt['samples']:
            if sample['phase']=='production_interactive':assert (sample['width'],sample['height'])==(480,360)
            else:assert (sample['width'],sample['height'])==(640,480) and not sample['interactive']
            if sample['phase'] in ('orbit','edit'):assert sample['submitted_triangles']==triangles*instances and sample['rasterized']
        receipt['summary']=summarize(receipt);receipt['input_scene_sha256']=hashlib.sha256(scene.read_bytes()).hexdigest();receipt['geometry_source_sha256']=digest_file(out/'meshes'/str(triangles)/'source.obj')
        write(folder/'material_performance_t4.json',receipt);results['cases'][name]=receipt
    if args.compare:
        before=json.loads(args.compare.read_text());comparison={}
        for name,after in results['cases'].items():
            old=before['cases'][name]
            same_pixels=[s['pixel_fnv64'] for s in old['samples']]==[s['pixel_fnv64'] for s in after['samples']]
            peak_ratio=after['peak_rss_bytes']/old['peak_rss_bytes']
            dominant=max(old['stage_names'],key=lambda stage:old['summary']['orbit'][stage]['p50'])
            before_ms=old['summary']['orbit'][dominant]['p50'];after_ms=after['summary']['orbit'][dominant]['p50']
            p95_ok=all(after['summary'][phase][metric]['p95']<=old['summary'][phase][metric]['p95']*1.1 for phase,metric in (('orbit','frame_ms'),('cache_hit','frame_ms'),('edit','edit_total_ms')))
            comparison[name]={'pixels_match':same_pixels,'source_geometry_match':old['geometry_source_sha256']==after['geometry_source_sha256'],'peak_rss_ratio':peak_ratio,'peak_rss_budget_met':peak_ratio<=1.05,'dominant_stage':dominant,'dominant_stage_p50_reduction':1-after_ms/before_ms if before_ms else None,'dominant_stage_target_met':before_ms>0 and after_ms<=before_ms*.8,'other_p95_budget_met':p95_ok,'summary_before':old['summary'],'summary_after':after['summary']}
        results['comparison']=comparison
    if args.analyze_existing and (out/'baseline.json').exists():
        results['native_binary_sha256']=json.loads((out/'baseline.json').read_text())['native_binary_sha256']
    write(out/'baseline.json',results)
    if args.require_budgets:
        gates=('pixels_match','source_geometry_match','peak_rss_budget_met','dominant_stage_target_met','other_p95_budget_met')
        assert results['comparison'] and all(all(c[g] for g in gates) for c in results['comparison'].values()), out/'baseline.json'
if __name__=='__main__':main()
