#!/usr/bin/env python3
"""M5 native filtering, declared color, tangent response and bounded cost proof."""
import argparse, copy, hashlib, importlib.util, json, os, platform, struct, sys, zlib
from pathlib import Path
import test_surface_mapping_m4 as m4
ROOT=m4.ROOT
sys.path.insert(0,str(ROOT/'tools'))
from smooth_mesh_reflection.prepare_reflection_matrix import build_request

def png(path,w,h,pixel):
    def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    raw=b''.join(b'\0'+b''.join(bytes(pixel(x,y)) for x in range(w)) for y in range(h))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))

def channel(path,encoding):return {'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'color_space':encoding}

def grid(n):
    lines=[]
    for y in range(n+1):
        for x in range(n+1):lines.extend([f'v {x/n} {y/n} 0',f'vt {x/n} {y/n}'])
    for y in range(n):
        for x in range(n):
            a=y*(n+1)+x+1;b=a+1;c=a+n+2;d=a+n+1
            lines.extend([f'f {a}/{a} {b}/{b} {c}/{c}',f'f {a}/{a} {c}/{c} {d}/{d}'])
    return '\n'.join(lines)+'\n'

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output-root',type=Path,required=True)
    parser.add_argument('--cases',nargs='+',default=['normal','mirrored','rotated','degenerate','linear','alpha','normal_zero','normal_variance','height','procedural','large'])
    args=parser.parse_args();out=args.output_root.resolve();out.mkdir(parents=True,exist_ok=False)
    binary=ROOT/f'build/toolchains/clang/{platform.machine()}'
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'))
    template=json.loads((ROOT/'tests/fixtures/mesh_asset_runtime_spheres/scene_runtime.json').read_text())
    author_template=json.loads((ROOT/'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json').read_text())
    results={}
    for name in args.cases:
        folder=out/name;runtime=folder/'data/runtime';runtime.mkdir(parents=True)
        (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(folder/'videos')}))
        (runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
        obj=folder/'source.obj';obj.write_text(grid(64) if name=='large' else m4.OBJ.replace('f 1/4/1 3/5/1 4/6/1','f 1/4/1 3/4/1 4/4/1') if name=='degenerate' else m4.OBJ)
        author=copy.deepcopy(author_template);author['authoring']['imported_mesh'].update(source_format='obj',source_uri=str(obj),uv_set_id='paint_uv',source_to_asset_scale=1,preserve_source_normals=True,normal_mode='none',crease_angle_degrees=60,source_unit_system='meter',topology_closed_volume_observed=False)
        author_path=folder/'authoring.json';author_path.write_text(json.dumps(author));dest=folder/'assets/mesh_assets';dest.mkdir(parents=True);asset=dest/'uv_asset.runtime.json'
        m4.run([binary/'tools/smooth_mesh_reflection/compile_runtime_fixture',author_path,folder,'uv_asset',asset],folder/'compile.log',env)
        scene=copy.deepcopy(template);scene['scene_id']='m5_'+name
        mesh=m4.m0.mesh_instance('surface','uv_asset','mat_sphere_high',0,0,0)
        mesh['transform']['scale']={'x':-2 if name=='mirrored' else 2,'y':1.3,'z':.7};mesh['transform']['rotation']={'x':17,'y':23,'z':31}
        mapping=copy.deepcopy(m4.MAP);mapping.update(uv_scale=[1,1],uv_offset=[0,0],rotation_rad=1.5707963267948966 if name=='rotated' else 0)
        sampling={'version':1,'required_capability':'optic.surface_sampling_v1','filter':'trilinear','address':'repeat','color_space':'linear','period_tiles':[1,1],'normal_strength':0 if name=='normal_zero' else 1,'producer_note':{'preserve':True}}
        color=folder/'color.png';rough=folder/'rough.png';normal=folder/'normal.png';height=folder/'height.png'
        png(color,2,2,lambda x,y:(255,0,0,0) if name=='alpha' and x==0 else (128,128,128,255));png(rough,64,64,lambda x,y:((x+y)%2*255,0,0,255));png(normal,64 if name in ('normal_variance','normal_zero') else 2,2,lambda x,y:(192 if x%2 else 63,128,230,255) if name in ('normal_variance','normal_zero') else (192,160,238,255));png(height,64,64,lambda x,y:(round(x/63*255),0,0,255))
        sampling['channels']={'base_color':channel(color,'linear' if name=='linear' else 'srgb'),'roughness':channel(rough,'data')}
        if name=='procedural':sampling['channels'].pop('base_color')
        if name=='height':sampling['channels']['height']=channel(height,'data');sampling['height_m']=.1
        else:sampling['channels']['normal']=channel(normal,'data')
        mesh.setdefault('extensions',{}).setdefault('ray_tracing',{}).update(surface_mapping=mapping,surface_sampling=sampling);scene['objects']=[mesh]
        scene.setdefault('extensions',{}).setdefault('ray_tracing',{})['authoring']={'object_materials':[{'object_id':'surface','material_id':0,'object_color':12756864,'roughness':.8,'reflectivity':.02,'producer_note':'M5 retained source','material_texture_stack':{'layers':[{'id':'brick','kind':'brick','placement':{'scale':1,'strength':1}}]}}]}
        if name=='alpha':scene['extensions']['ray_tracing']['authoring']['object_materials'][0]['material_texture_stack']['layers'][0]['enabled']=False
        path=folder/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2));case_env=dict(env,OPTIC_M5_CASE=name)
        for pass_name in ('native','reopen'):
            m4.run([binary/'tests/scene_editor_workspace_visual_test',folder,path,'--mapping-m5'],folder/(pass_name+'.log'),case_env)
        if name=='normal':
            m4.run([binary/'tests/scene_editor_workspace_visual_test',folder,path,'--sampling-ray-motion-m5'],folder/'ray-motion.log',case_env)
        saved=json.loads(path.read_text());assert saved['objects'][0]['extensions']['ray_tracing']['surface_sampling']==sampling
        assert saved['objects'][0]['extensions']['ray_tracing']['surface_mapping']==mapping
        for capture in ('m5-material.ppm','m5-orbit.ppm','m5-distant.ppm'):
            pixels=(folder/capture).read_bytes().split(b'\n',3)[3];assert not any(pixels[i:i+3]==b'\xff\x00\xff' for i in range(0,len(pixels),3))
        results[name]=json.loads((folder/'mapping_m5.json').read_text());hashes=[]
        for route in ('flattened','tlas_blas'):
            req=build_request(folder,'direct',route);req['scene']['runtime_scene_path']=str(path);req['render'].update(width=240,height=180)
            req['inspection'].update(camera_position={'x':2,'y':-3,'z':3},camera_look_at={'x':0,'y':0,'z':0},camera_zoom=1.2)
            request=folder/(route+'.json');request.write_text(json.dumps(req));m4.run([binary/'tools/cli/ray_tracing_render_headless','--request',request,'--render'],folder/(route+'.log'),env)
            frames=list((folder/'renders'/('direct_'+route)/'frames').glob('*.bmp'));assert len(frames)==1;hashes.append(hashlib.sha256(frames[0].read_bytes()).hexdigest())
        assert hashes[0]==hashes[1];results[name]['flattened_tlas_sha256']=hashes[0];print(name,results[name],flush=True)
    folder=out/args.cases[0];scene=json.loads((folder/'scene_runtime.json').read_text());negatives=[]
    for name in ('unknown_capability','data_srgb','missing_image','stale_hash','non_pot','bad_period','conflicting_normal_height','unsupported_binding','negative_strength','negative_normal_z','oversized_image','preparation_budget'):
        candidate=copy.deepcopy(scene);sampling=candidate['objects'][0]['extensions']['ray_tracing']['surface_sampling']
        if name=='unknown_capability':sampling['required_capability']='future'
        elif name=='data_srgb':sampling['channels']['roughness']['color_space']='srgb'
        elif name=='missing_image':sampling['channels']['base_color']['path']=str(out/'missing.png')
        elif name=='stale_hash':sampling['channels']['base_color']['sha256']='0'*64
        elif name=='non_pot':
            path=out/'nonpot.png';png(path,3,2,lambda x,y:(128,128,128,255));sampling['channels']['base_color']=channel(path,'srgb')
        elif name=='bad_period':sampling['period_tiles']=[1.5,1]
        elif name=='conflicting_normal_height':sampling['channels']['normal']=channel(folder/'normal.png','data');sampling['channels']['height']=channel(folder/'height.png','data')
        elif name=='unsupported_binding':candidate['extensions']['ray_tracing']['authoring']['object_materials'][0]['surface_material_binding']={'version':1,'required_capability':'optic.surface_material_v3','mappings':[]}
        elif name=='negative_strength':sampling['normal_strength']=-1
        elif name=='negative_normal_z':
            image=out/'negative-normal.png';png(image,2,2,lambda x,y:(128,128,0,255));sampling['channels']['normal']=channel(image,'data')
        elif name=='oversized_image':
            image=out/'oversized.png';png(image,2048,1,lambda x,y:(128,128,128,255));sampling['channels']['base_color']=channel(image,'srgb')
        else:
            for i in range(1,49):
                obj=copy.deepcopy(candidate['objects'][0]);obj['object_id']='copy_'+str(i);candidate['objects'].append(obj)
                row=copy.deepcopy(candidate['extensions']['ray_tracing']['authoring']['object_materials'][0]);row['object_id']=obj['object_id'];candidate['extensions']['ray_tracing']['authoring']['object_materials'].append(row)
        bad=folder/(name+'.json');bad.write_text(json.dumps(candidate));req=build_request(out/name,'direct','flattened');req['scene']['runtime_scene_path']=str(bad);request=out/(name+'-request.json');request.write_text(json.dumps(req));m4.run([binary/'tools/cli/ray_tracing_render_headless','--request',request,'--preflight'],out/(name+'.log'),env,False);negatives.append(name)
    if (out/'normal/mapping_m5_ray_motion.json').exists():results['camera_ray_motion']=json.loads((out/'normal/mapping_m5_ray_motion.json').read_text())
    results['negative_preflights']=negatives;(out/'acceptance.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
