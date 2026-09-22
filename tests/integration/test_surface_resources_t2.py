#!/usr/bin/env python3
"""Native M5 image authoring and independent resource preparation accounting."""
import argparse, copy, hashlib, json, os, platform, shutil, struct, subprocess, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def png(path,kind):
    def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    def pixel(x,y):
        if kind=='normal':return (128,128,255,255)
        if kind=='height':return (x*8,y*8,0,255)
        if kind=='roughness':return (80+x*4,0,0,255)
        if kind=='relink':return (255-x*7,30+y*5,180,255)
        return (x*8,y*8,127,255)
    raw=b''.join(b'\0'+bytes(c for x in range(32) for c in pixel(x,y)) for y in range(32))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',32,32,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))

def fixture(folder,count,sampling):
    folder.mkdir(parents=True)
    runtime=folder/'data/runtime';runtime.mkdir(parents=True)
    (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(folder/'videos')}))
    (runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
    images=folder/'images';images.mkdir()
    for channel in ('base_color','roughness','normal','height','relink'):png(images/(channel+'.png'),channel)
    (images/'alias.png').write_bytes((images/'base_color.png').read_bytes())
    scene=json.loads((ROOT/'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    original=scene['objects'][0];scene['objects']=[];scene['world_scale']=1;rows=[]
    for i in range(count):
        obj=copy.deepcopy(original);obj['object_id']='surface' if i==0 else f'surface_{i}'
        obj['object_type']=obj['primitive']['kind']='rect_prism_primitive';obj['primitive'].update(width=4,height=2,depth=1)
        obj['primitive']['frame']['origin']={'x':0,'y':0,'z':0}
        obj['transform'].update(position={'x':0,'y':0,'z':0},rotation={'x':0,'y':0,'z':0},scale={'x':1,'y':1,'z':1})
        obj['flags']={'visible':True,'locked':False,'selectable':True}
        obj['extensions']={'ray_tracing':{'surface_mapping':{'version':1,'required_capability':'optic.planar_surface_v1','method':'planar','space':'object_rest','source_domain':'brick_cells_v1','scale_policy':'stretch_with_object','origin_m':[0,0,0],'axis_u':[1,0,0],'axis_v':[0,1,0],'tile_m':[.5,.25],'offset_m':[0,0],'pivot_m':[0,0],'rotation_rad':0,'seed':1729}}}
        if sampling:
            obj['extensions']['ray_tracing']['surface_sampling']={'version':1,'required_capability':'optic.surface_sampling_v1','filter':'trilinear','address':'repeat','color_space':'linear','period_tiles':[8,8],'normal_strength':1,'height_m':0,'channels':{'base_color':{'path':'images/alias.png' if i%2 else 'images/base_color.png','sha256':hashlib.sha256((images/'base_color.png').read_bytes()).hexdigest(),'color_space':'srgb'}}}
        rows.append({'object_id':obj['object_id'],'material_id':0,'object_color':12756864,'roughness':.65,'reflectivity':.02,'material_texture_stack':{'layers':[{'id':'brick','kind':'brick','enabled':True,'opacity':1}]},'producer':{'keep':'T2'}})
        scene['objects'].append(obj)
    scene.setdefault('extensions',{}).setdefault('ray_tracing',{})['authoring']={'object_materials':rows}
    scene['extensions']['t2_unknown_metadata']={'preserve':[1,2,3]}
    path=folder/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2));return path

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output-root',required=True,type=Path)
    parser.add_argument('--candidate-project',type=Path,help='Complete reviewed UV project to copy for native adoption proof')
    parser.add_argument('--candidate-only',action='store_true');args=parser.parse_args()
    if args.candidate_only and not args.candidate_project:parser.error('--candidate-only requires --candidate-project')
    out=args.output_root.resolve();out.mkdir(parents=True,exist_ok=False)
    binary=ROOT/f'build/toolchains/clang/{platform.machine()}/tests/scene_editor_workspace_visual_test'
    results={}
    cases=() if args.candidate_only else (('authoring',1,False,('--resources-t2','--resources-t2-reopen')),('cache',100,True,('--resources-t2-cache',)))
    for name,count,sampling,modes in cases:
        folder=out/name;scene=fixture(folder,count,sampling)
        env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(folder/'cache'))
        for channel in ('base_color','roughness','normal','height','relink'):env['OPTIC_T2_'+channel.upper()]=str(folder/'images'/(channel+'.png'))
        env['OPTIC_T2_CACHE_IMAGE']=str(folder/'images/base_color.png')
        outside=out/(name+'-outside.png');shutil.copyfile(folder/'images/base_color.png',outside)
        env['OPTIC_T2_OUTSIDE_IMAGE']=str(outside)
        for mode in modes:
            with (folder/(mode[2:]+'.log')).open('w') as log:
                result=subprocess.run([str(binary),str(folder),str(scene),mode],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
            assert result.returncode==0,(mode,result.returncode,folder)
        results[name]=json.loads((folder/('resources_t2_cache.json' if sampling else 'resources_t2.json')).read_text())
        if not sampling:
            results['reopen']=json.loads((folder/'resources_t2_reopen.json').read_text())
            saved=json.loads(scene.read_text());assert saved['extensions']['t2_unknown_metadata']=={'preserve':[1,2,3]}
    if args.candidate_project:
        folder=out/'candidate';shutil.copytree(args.candidate_project.resolve(),folder)
        runtime=folder/'data/runtime';runtime.mkdir(parents=True,exist_ok=True)
        (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(folder/'videos')}))
        (runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
        env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(folder/'cache'))
        with (folder/'native.log').open('w') as log:
            result=subprocess.run([str(binary),str(folder),str(folder/'scene_runtime.json'),'--surface-candidate-t2'],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
        assert result.returncode==0,('candidate',result.returncode,folder)
        results['candidate']=json.loads((folder/'surface-candidate-t2-receipt.json').read_text())
    (out/'acceptance.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
if __name__=='__main__':main()
