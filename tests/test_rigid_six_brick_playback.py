#!/usr/bin/env python3
"""Verify six-brick immutable results through ordinary native optiC playback.

Checks every tick's poses and every source vertex; selected views additionally
compare flattened/TLAS hits, independent camera/source rays and material pixels.
"""
import argparse
import array
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

from qualify_rigid_resting_scene import load, write
from qualify_rigid_six_brick_scene import BRICKS
from rigid_scene_qualification_math import cross, dot, norm, sub, world_vertices
from test_rigid_contact_presentation import intersect


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('renderer','consumer','pixel-probe','pack-io','parent','out'):
        ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--end-tick',type=int,default=1200)
    ap.add_argument('--rest-ticks',nargs='+',type=int,default=(0,600,1200))
    ap.add_argument('--cases',nargs='+',choices=('rest','impact'),default=('rest','impact'))
    ap.add_argument('--camera-position',nargs=3,type=float,default=(-2.8,4,2.4))
    ap.add_argument('--camera-look-at',nargs=3,type=float,default=(0,0,.3))
    ap.add_argument('--impact-ticks',nargs='+',type=int,default=(0,100,180,360,1200))
    ap.add_argument('--views-only',action='store_true',help='Additional camera review; omits tick/vertex qualification')
    ap.add_argument('--brick-count',type=int,default=6,choices=(6,12))
    a=ap.parse_args();bricks=tuple('brick_'+str(i) for i in range(a.brick_count));root=a.out.resolve();root.mkdir(parents=True,exist_ok=False)
    write(root/'verification.json',{'ok':False,'status':'running','cases':{}})
    env=dict(os.environ,XDG_CONFIG_HOME=str(root/'config'),PYTHONDONTWRITEBYTECODE='1',
             RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled')
    def call(argv,name):
        p=subprocess.run(list(map(str,argv)),capture_output=True,env=env,timeout=180)
        (root/(name+'.log')).write_bytes(p.stderr)
        assert p.returncode==0,(name,p.returncode,p.stderr[-1000:])
        return p.stdout
    def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
    results={};parents={}
    for case in a.cases:
        parent=a.parent/case;scene=load(parent/'scene_review.json')
        parents[str(parent/'scene_review.json')]=sha(parent/'scene_review.json')
        parents[str(parent/'result/poses.pack')]=sha(parent/'result/poses.pack')
        cache=json.loads(call([a.pack_io,'read',parent/'result/poses.pack'],case+'-decode'))
        assert len(cache['frames'])==a.end_tick+1 and cache['frames'][-1]['tick']==a.end_tick,'end tick must cover the whole accepted cache'
        bindings={b['object_id']:b for b in cache['physical_input']['bindings']}
        assets={r['asset_id']:r['root'] for r in load(parent/'result/input/assets/asset_set.json')['assets']}
        meshes={oid:load(parent/'result/input/assets'/assets[b['collision_asset_id']]/'visible.json')['mesh'] for oid,b in bindings.items()}
        maximum=0.;samples=0;vertices_checked=0
        # Bounded chunks avoid retaining a large raw geometry dump.
        for start in (() if a.views_only else range(0,a.end_tick+1,100)):
            ticks=range(start,min(a.end_tick+1,start+100))
            raw=call([a.consumer,parent/'scene_review.json',*ticks],case+'-poses-'+str(start))
            rows=[json.loads(line[4:]) for line in raw.splitlines() if line.startswith(b'BR4 ')]
            assert len(rows)==len(ticks)
            for row,tick in zip(rows,ticks):
                assert row['frame']==tick
                poses={b['object_id']:b for b in cache['frames'][tick]['bodies']}
                assert len(row['poses'])==a.brick_count+2
                for p in row['poses']:
                    q=poses[p['id']]
                    assert p['tick']==tick and p['body_id']==bindings[p['id']]['body_id']
                    assert p['position']==q['position_m'] and p['q']==q['orientation_wxyz']
                expected={oid:world_vertices(mesh,bindings[oid],poses[oid]) for oid,mesh in meshes.items()}
                assert len(row['triangles'])==12*(a.brick_count+2)
                for t in row['triangles']:
                    oid=t['id'];tri=meshes[oid]['triangles'][t['local']]
                    for actual,k in zip(t['vertices'],('a','b','c')):
                        error=max(abs(x-y) for x,y in zip(actual,expected[oid][tri[k]]))
                        maximum=max(maximum,error);vertices_checked+=1
                samples+=1
            assert maximum<=2e-12,(case,maximum)
        views=[]
        ticks=a.rest_ticks if case=='rest' else a.impact_ticks
        for tick in ticks:
            name=case+'-'+str(tick);out=root/name;width,height=384,288
            origin=list(a.camera_position);target=list(a.camera_look_at)
            req={'schema_version':'ray_tracing_agent_render_request_v1','run_id':'six_'+name,
                 'scene':{'runtime_scene_path':str(parent/'scene_review.json')},'volume':{'enabled':False},
                 'render':{'start_frame':tick,'frame_count':1,'width':width,'height':height,
                           'integrator_3d':'direct_light','temporal_frames':1,'normalized_t':tick/a.end_tick},
                 'resources':{'cpu_percent':50,'max_workers':2,'reserve_cpu_count':1},
                 'inspection':{'camera_position':dict(zip('xyz',origin)), 'camera_look_at':dict(zip('xyz',target)),
                               'environment_light_mode':'ambient','ambient_strength':.3},
                 'output':{'root':str(out),'overwrite':False},
                 'progress':{'summary_path':str(root/(name+'-summary.json'))}}
            request=root/(name+'-request.json');write(request,req)
            raw=call([a.pixel_probe,request,root/name],name+'-pixels')
            meta=json.loads(next(r[8:] for r in raw.splitlines() if r.startswith(b'CONTACT ')))
            assert meta['route_mismatches']==0 and meta['route_comparisons']==width*height
            native=array.array('i');native.frombytes((root/(name+'.ids')).read_bytes())
            depths=array.array('d');depths.frombytes((root/(name+'.depths')).read_bytes())
            assert meta['native_int_bytes']==native.itemsize and meta['native_double_bytes']==depths.itemsize
            assert len(native)==len(depths)==width*height
            objects={i:o['object_id'] for i,o in enumerate(scene['objects'])}
            poses={b['object_id']:b for b in cache['frames'][tick]['bodies']};triangles=[]
            for oid,mesh in meshes.items():
                vs=world_vertices(mesh,bindings[oid],poses[oid])
                triangles += [(oid,[vs[t[k]] for k in ('a','b','c')]) for t in mesh['triangles']]
            def unit(v):return [x/norm(v) for x in v]
            forward=unit(sub(target,origin));right=unit(cross([0,0,1],forward));up=unit(cross(forward,right))
            fov=math.tan(math.radians(55)/2);reference={};error=0.
            # Independently calculated reference rays; never copied from the native projector.
            for y in range(height//4,height*3//4,2):
                for x in range(width//8,width*7//8,2):
                    nx=(2*(x+.5)/width-1)*fov*width/height;ny=(1-2*(y+.5)/height)*fov
                    d=unit([forward[k]+nx*right[k]+ny*up[k] for k in range(3)])
                    hits=[(t,oid) for oid,tri in triangles if (t:=intersect(origin,d,tri)) is not None]
                    closest=min(hits,default=(None,None));i=y*width+x
                    assert objects.get(native[i])==closest[1],(name,x,y,objects.get(native[i]),closest)
                    if closest[0] is not None:error=max(error,abs(depths[i]-closest[0]))
                    reference[i]=closest[1]
            assert error<=2e-9,(name,error)
            call([a.renderer,'--request',request,'--render'],name+'-render')
            bmp=out/'frames'/f'frame_{tick:04d}.bmp'
            rgb=call(['ffmpeg','-v','error','-nostdin','-i',bmp,'-f','rawvideo','-pix_fmt','rgb24','-'],name+'-rgb')
            assert len(rgb)==width*height*3
            counts={oid:0 for oid in bricks};bad=[]
            for i,oid in reference.items():
                if oid not in bricks:continue
                counts[oid]+=1;r,g,b=rgb[3*i:3*i+3]
                ok=(b>r+4 and g>r+2) if int(oid.split('_')[-1])<a.brick_count//2 else (r>b+8 and r>g+3)
                if not ok:bad.append([i,oid,[r,g,b]])
            assert not bad,(name,bad[:5])
            assert sum(counts.values())>50,(name,counts)
            call(['ffmpeg','-v','error','-nostdin','-i',bmp,'-frames:v','1',root/(name+'.png')],name+'-png')
            views.append({'tick':tick,'route_comparisons':meta['route_comparisons'],'route_mismatches':0,
                          'reference_pixels':len(reference),'source_depth_error_m':error,
                          'brick_material_pixels':counts,'material_mismatches':0})
            print(name,'source rays and pixels verified',flush=True)
        results[case]={'samples':samples,'vertices_checked':vertices_checked,'maximum_vertex_error_m':maximum,'views':views}
        write(root/'verification.json',{'ok':False,'status':'running','cases':results})
    assert all(sha(Path(p))==v for p,v in parents.items())
    write(root/'verification.json',{'ok':True,'status':'passed','proof_scope':'selected_views' if a.views_only else 'all_ticks_and_views','cases':results,'parent_hashes_unchanged':parents})
    return 0


if __name__=='__main__':raise SystemExit(main())
