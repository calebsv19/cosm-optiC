#!/usr/bin/env python3
"""Check rendered contact against retained physics, triangle rays and raw pixels.

The controlled camera/material fixture is intentional. This does not qualify
arbitrary materials from RGB alone. No simulation, pose adjustment or renderer
appearance change is performed. Output is create-only.
"""
import argparse
import array
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

from rigid_scene_qualification_math import cross, dot, norm, sub, world_vertices


def load(path):return json.loads(path.read_bytes())
def write(path,value):path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def intersect(origin,direction,triangle):
    """Independent, two-sided Moller-Trumbore reference on source vertices."""
    a,b,c=triangle;e1=sub(b,a);e2=sub(c,a);h=cross(direction,e2);det=dot(e1,h)
    if abs(det)<1e-12:return None
    s=sub(origin,a);u=dot(s,h)/det
    if u < -1e-10 or u > 1+1e-10:return None
    q=cross(s,e1);v=dot(direction,q)/det
    if v < -1e-10 or u+v > 1+1e-10:return None
    t=dot(e2,q)/det
    return t if t>=.1 else None


def matches_material(oid,rgb):
    r,g,b=rgb
    if oid=='block_lower':return b>r+4 and g>r+2
    if oid=='block_upper':return r>b+8 and r>g+3
    return True


def check_pixels(rgb,ids,width,height):
    bad=[];counts={oid:0 for oid in ('block_lower','block_upper')}
    for y in range(height*3//10,height*7//10):
        for x in range(width*3//10,width*7//10):
            i=y*width+x;oid=ids.get(i)
            if oid not in counts:continue
            counts[oid]+=1
            if not matches_material(oid,rgb[3*i:3*i+3]):bad.append([x,y,oid,list(rgb[3*i:3*i+3])])
    return bad,counts


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('renderer','pixel-probe','pack-io','parent','out'):
        ap.add_argument('--'+name,type=Path,required=True)
    a=ap.parse_args();root=a.out.resolve();root.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,XDG_CONFIG_HOME=str(root/'config'),
             RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled',PYTHONDONTWRITEBYTECODE='1')
    def call(args,name,binary=False):
        p=subprocess.run(list(map(str,args)),capture_output=True,env=env,timeout=180)
        (root/(name+'.log')).write_bytes(p.stderr)
        assert p.returncode==0,(name,p.returncode,p.stderr[-1000:])
        if binary:return p.stdout
        if name.endswith('-pixels'):
            rows=[line[8:] for line in p.stdout.splitlines() if line.startswith(b'CONTACT ')]
            assert len(rows)==1,(name,p.stdout[-1000:])
            return json.loads(rows[0])
        return json.loads(p.stdout)
    checks=[];parents={}
    for case,ticks in [('stack-rest',(0,600,1200)),('stack-impact',(0,84,180))]:
        parent=a.parent/case;scene_path=parent/'scene_review.json';scene=load(scene_path)
        parents[str(scene_path)]=digest(scene_path)
        parents[str(parent/'result/poses.pack')]=digest(parent/'result/poses.pack')
        cache=call([a.pack_io,'read',parent/'result/poses.pack'],case+'-cache')
        bindings={b['object_id']:b for b in cache['physical_input']['bindings']}
        objects={i:o['object_id'] for i,o in enumerate(scene['objects'])}
        assetset=load(parent/'result/input/assets/asset_set.json')
        assets={row['asset_id']:row['root'] for row in assetset['assets']}
        meshes={oid:load(parent/'result/input/assets'/assets[b['collision_asset_id']]/'visible.json')['mesh'] for oid,b in bindings.items()}
        for tick in ticks:
            name=case+'-'+str(tick);out=root/name
            req={'schema_version':'ray_tracing_agent_render_request_v1','run_id':'contact_'+name,
                 'scene':{'runtime_scene_path':str(scene_path)},'volume':{'enabled':False},
                 'render':{'start_frame':tick,'frame_count':1,'width':384,'height':288,
                           'integrator_3d':'direct_light','temporal_frames':1,'normalized_t':tick/1200},
                 'resources':{'cpu_percent':50,'max_workers':2,'reserve_cpu_count':1},
                 'inspection':{'camera_position':{'x':0,'y':3.5,'z':1.8},
                               'camera_look_at':{'x':0,'y':0,'z':.45},
                               'environment_light_mode':'ambient','ambient_strength':.3},
                 'output':{'root':str(out),'overwrite':False},
                 'progress':{'summary_path':str(root/(name+'-summary.json'))}}
            request=root/(name+'-request.json');write(request,req)
            meta=call([a.pixel_probe,request,root/name],name+'-pixels')
            assert meta['route_mismatches']==0 and meta['route_comparisons']==384*288
            native=array.array('i');native.frombytes((root/(name+'.ids')).read_bytes())
            depths=array.array('d');depths.frombytes((root/(name+'.depths')).read_bytes())
            assert meta['native_int_bytes']==native.itemsize and meta['native_double_bytes']==depths.itemsize
            assert len(native)==len(depths)==384*288
            poses={b['object_id']:b for b in cache['frames'][tick]['bodies']}
            triangles=[]
            for oid,mesh in meshes.items():
                vertices=world_vertices(mesh,bindings[oid],poses[oid])
                triangles += [(oid,[vertices[t[k]] for k in ('a','b','c')]) for t in mesh['triangles']]
            # Camera formula is independent of the C projector, with explicit
            # fixture origin/target/FOV rather than copying native ray vectors.
            origin=[0,3.5,1.8];pitch=math.atan2(-1.35,3.5);cp=math.cos(pitch);sp=math.sin(pitch)
            fov=math.tan(math.radians(55)/2);expected={};max_error=0
            for y in range(288*3//10,288*7//10):
                for x in range(384*3//10,384*7//10):
                    nx=(2*(x+.5)/384-1)*fov*384/288;ny=(1-2*(y+.5)/288)*fov
                    d=[nx,-cp+ny*sp,sp+ny*cp];length=norm(d);d=[v/length for v in d]
                    hits=[(t,oid) for oid,tri in triangles if (t:=intersect(origin,d,tri)) is not None]
                    i=y*384+x;actual=objects.get(native[i]);closest=min(hits,default=(None,None))
                    assert actual==closest[1],(name,x,y,actual,closest)
                    if closest[0] is not None:max_error=max(max_error,abs(depths[i]-closest[0]))
                    expected[i]=actual
            assert max_error<2e-9,(name,max_error)
            call([a.renderer,'--request',request,'--render'],name+'-render',True)
            bmp=out/'frames'/f'frame_{tick:04d}.bmp'
            rgb=call(['ffmpeg','-v','error','-nostdin','-i',bmp,'-f','rawvideo','-pix_fmt','rgb24','-'],name+'-decode',True)
            assert len(rgb)==384*288*3
            bad,counts=check_pixels(rgb,expected,384,288)
            assert not bad,(name,bad[:5])
            assert min(counts.values())>100,(name,counts)
            # An injected one-row light/background band must fail the exact
            # same detector; this guards the suspected regression, not code shape.
            corrupted=bytearray(rgb)
            for i,oid in expected.items():
                if oid=='block_lower' and i//384==149:corrupted[3*i:3*i+3]=bytes((190,190,190))
            negative_bad,_=check_pixels(corrupted,expected,384,288)
            assert negative_bad,'injected contact band escaped detection'
            png=root/(name+'.png')
            call(['ffmpeg','-v','error','-nostdin','-i',bmp,'-frames:v','1',png],name+'-png',True)
            row={'case':case,'tick':tick,'route_comparisons':meta['route_comparisons'],
                 'route_mismatches':0,'reference_ray_pixels':len(expected),'maximum_reference_depth_error_m':max_error,
                 'material_pixels':counts,'material_mismatches':0,'negative_control_mismatches':len(negative_bad),
                 'png_sha256':digest(png)}
            checks.append(row);write(root/'verification.json',{'ok':True,'checks':checks})
            print(name,'source rays and rendered material pixels agree',flush=True)
    assert all(digest(Path(p))==sha for p,sha in parents.items())
    write(root/'verification.json',{'ok':True,'checks':checks,'parent_hashes_unchanged':parents})
    return 0


if __name__=='__main__':raise SystemExit(main())
