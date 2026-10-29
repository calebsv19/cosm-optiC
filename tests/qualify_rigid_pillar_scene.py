#!/usr/bin/env python3
"""BR6 bounded native physical + Ray geometry qualification.

Creates explicit named test fixtures from the retained BR5 parent. No existing
scene/store is edited. Limits are declared separately before simulation.
"""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
from rigid_scene_qualification_math import (dot,norm,penetration,prism_mass_oracle,
    prism_stl,quaternion_from_matrix,world_vertices)


def write(path,value):path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')
def load(path):return json.loads(path.read_bytes())


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('parent','backend','pack-io','ball-tool','renderer','probe','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);root=a.out.resolve()
    ray=Path(__file__).resolve().parents[1]
    limits=load(Path(__file__).with_name('rigid_scene_qualification_limits.json'))
    write(root/'declared-limits.json',limits)
    env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',XDG_CONFIG_HOME=str(root/'config'),
             RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled',
             RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT=str(root/'mesh-cache'))
    checks=[];failures=[]
    def check(name,value,details=None):
        checks.append({'name':name,'pass':bool(value),'details':details})
        if not value:failures.append(name)
    def call(argv,name,timeout=90):
        r=subprocess.run(list(map(str,argv)),capture_output=True,text=True,cwd=ray,env=env,timeout=timeout)
        (root/(name+'.log')).write_text(r.stdout+r.stderr)
        if r.returncode:raise RuntimeError((name,r.returncode,(r.stdout+r.stderr)[-2000:]))
        return json.loads(r.stdout)
    parent=load(a.parent/'scene.json');assets=root/'assets';assets.mkdir()
    existing=load(a.parent/'result/input/assets/asset_set.json')['assets']
    for item in existing:
        name=item['asset_id'];source=a.parent/'result/input/assets'/item['root']/'source.stl'
        copied=assets/(name+'.stl');shutil.copy2(source,copied)
        call([a.backend,'prepare',copied,name,assets/name],'prepare-'+name)
    prism_stl(assets/'prism.stl');call([a.backend,'prepare',assets/'prism.stl','prism',assets/'prism'],'prepare-prism')
    aset=assets/'asset_set.json'
    # The current native profile permits eight assets; all four are verified.
    write(aset,{'schema':'ball_rigid_asset_set_v1','assets':[{'asset_id':n,'root':n} for n in ('floor','pillar','asymmetric','prism')]})
    catalog=call([a.backend,'catalog',aset],'catalog');metadata={m['asset_id']:m for m in catalog['assets']}
    oracle=prism_mass_oracle(metadata['prism']);write(root/'independent-prism-mass.json',oracle)
    check('new prism independent analytic COM/inertia',max(oracle['com_error_m'],oracle['inertia_per_kg_error'])<limits['mass_frame_error'],oracle)
    source_hash=hashlib.sha256((a.parent/'scene.json').read_bytes()).hexdigest()
    def fixture(asset,kind):
        scene=copy.deepcopy(parent);scene['scene_id']='br6_'+asset+'_'+kind
        scene['extensions']['ray_tracing'].pop('rigid_result',None)
        scene['extensions']['rigid_scene']['end_tick']=limits['ticks']
        for obj,b in zip(scene['objects'],scene['extensions']['rigid_scene']['bindings']):
            name=asset if b['role']=='dynamic' else b['collision_asset_id'];m=metadata[name]
            for key in ('source_asset_id','mesh_asset_id','source_sha256','mesh_sha256','collision_sha256','source_center_m','principal_to_source'):b[key]=copy.deepcopy(m[key])
            b['collision_asset_id']=name;b['orientation_wxyz']=quaternion_from_matrix(m['principal_to_source'])
            b['principal_inertia_kgm2']=[v*b['mass_kg'] for v in m['inertia_per_kg']]
            obj['geometry_ref']['id']=m['mesh_asset_id']
            obj.setdefault('extensions',{}).setdefault('line_drawing',{})['runtime_mesh_path']=str(assets/name/'visible.json')
            if b['role']=='dynamic' and kind in ('pillar','throw'):
                b['linear_velocity_mps']=[4,0,0];b['angular_velocity_radps']=[0,0,0]
                if kind=='pillar':
                    scene['extensions']['rigid_scene']['gravity_mps2']=[0,0,0]
                    obj['transform']['position']['z']=1.5
        return scene
    cases={}
    for asset in ('asymmetric','prism'):
        for kind in ('drop','pillar','throw'):
            name=asset+'-'+kind;scene=fixture(asset,kind);path=root/(name+'-input.json');write(path,scene)
            prepared=call([sys.executable,'-B',ray/'tools/rigid_scene_workflow.py','run','--scene',path,'--assets',aset,
                '--ball-tool',a.ball_tool,'--backend',a.backend,'--pack-io',a.pack_io,'--renderer',a.renderer,
                '--store',root/(name+'-store'),'--expected-head','none','--ticks',limits['ticks'],'--output',root/name],name+'-prepare')
            cache=call([a.pack_io,'read',root/name/'result/poses.pack'],name+'-cache')
            repeat=call([a.backend,'record',path,aset,limits['ticks']],name+'-repeat')
            check(name+' exact native replay',cache==repeat)
            initial=cache['frames'][0];bindings={b['object_id']:b for b in cache['physical_input']['bindings']}
            static=[b['object_id'] for b in initial['bodies'] if b['static']]
            check(name+' static bodies exact immobility',all([b for b in f['bodies'] if b['object_id'] in static]==[b for b in initial['bodies'] if b['object_id'] in static] for f in cache['frames']))
            mesh_by_id={b['object_id']:load(root/name/'result/input/assets'/next(x['root'] for x in load(root/name/'result/input/assets/asset_set.json')['assets'] if x['asset_id']==b['collision_asset_id'])/'visible.json')['mesh'] for b in bindings.values()}
            worst=0.;worst_tick=0;incoming=0.;qerror=0.;pair_ticks=[];maxomega=0.;minz=math.inf
            for f in cache['frames']:
                poses={b['object_id']:b for b in f['bodies']}
                vertices={oid:world_vertices(mesh_by_id[oid],bindings[oid],pose) for oid,pose in poses.items()}
                minz=min(minz,min(v[2] for v in vertices['projectile']))
                for oid in static:
                    depth=penetration(vertices['projectile'],mesh_by_id['projectile']['triangles'],vertices[oid],mesh_by_id[oid]['triangles'])
                    if depth>worst:worst,worst_tick=depth,f['tick']
                incoming=max(incoming,f['maximum_penetration_m'])
                qerror=max(qerror,max(abs(dot(b['orientation_wxyz'],b['orientation_wxyz'])-1) for b in f['bodies']))
                maxomega=max(maxomega,norm(poses['projectile']['angular_velocity_radps']))
                if any(104 in (c['body_a_id'],c['body_b_id']) and (17 if kind=='drop' else 75) in (c['body_a_id'],c['body_b_id']) for c in f['contacts']):pair_ticks.append(f['tick'])
            metrics={'accepted_mesh_penetration_m':worst,'worst_tick':worst_tick,'incoming_contact_penetration_m':incoming,'quaternion_norm_error':qerror,'contact_ticks':pair_ticks,'maximum_angular_speed_radps':maxomega,'minimum_source_vertex_z_m':minz,'result_revision':prepared['result_revision']}
            write(root/(name+'-metrics.json'),metrics);cases[name]=metrics
            check(name+' accepted source mesh penetration',worst<=limits['accepted_mesh_penetration_m'],metrics)
            check(name+' bounded incoming contact',incoming<=limits['incoming_contact_penetration_m'],incoming)
            check(name+' finite normalized orientations',qerror<=limits['quaternion_norm_error'],qerror)
            check(name+' expected collider contacted',bool(pair_ticks),pair_ticks[:6])
            if kind in ('pillar','throw'):check(name+' collision generates rotation',maxomega>limits['generated_angular_speed_min_radps'],maxomega)
            if kind=='throw':
                check(name+' floor and pillar contacted under gravity',
                    all(any(any(104 in (c['body_a_id'],c['body_b_id']) and collider in (c['body_a_id'],c['body_b_id']) for c in f['contacts']) for f in cache['frames']) for collider in (17,75)))
            # All accepted ticks through the ordinary native Ray evaluated path.
            proc=subprocess.run([str(a.probe),str(root/name/'scene.json'),*map(str,range(limits['ticks']+1))],capture_output=True,text=True,cwd=ray,env=env,timeout=90)
            if proc.returncode:raise RuntimeError((name+'-ray-probe',proc.returncode,proc.stderr[-2000:]))
            rows=[json.loads(line[4:]) for line in proc.stdout.splitlines() if line.startswith('BR4 ')]
            check(name+' all ticks exposed',len(rows)==limits['ticks']+1)
            error=0.
            for row in rows:
                f=cache['frames'][row['frame']];poses={b['object_id']:b for b in f['bodies']}
                for pose in row['poses']:
                    expected=poses[pose['id']]
                    assert pose['position']==expected['position_m'] and pose['q']==expected['orientation_wxyz'] and pose['tick']==row['frame']
                transformed={oid:world_vertices(mesh_by_id[oid],bindings[oid],pose) for oid,pose in poses.items()}
                for tri in row['triangles']:
                    indices=mesh_by_id[tri['id']]['triangles'][tri['local']]
                    for actual,key in zip(tri['vertices'],('a','b','c')):
                        expected=transformed[tri['id']][indices[key]]
                        error=max(error,max(abs(x-y) for x,y in zip(actual,expected)))
            check(name+' independent visible mesh alignment',error<=limits['render_vertex_error_m'],error)
            metrics['ray_vertex_error_m']=error;write(root/(name+'-metrics.json'),metrics)
    # Isolated analytic controls; no contact is allowed in ballistic motion.
    free=fixture('prism','drop');free['objects'][-1]['transform']['position']['z']=100
    b=free['extensions']['rigid_scene']['bindings'][-1];b['angular_velocity_radps']=[0,0,0]
    free_path=root/'ballistic-input.json';write(free_path,free)
    frames=call([a.backend,'record',free_path,aset,60],'ballistic')['frames']
    v_error=z_error=0.
    for f in frames:
        n=f['tick'];body=next(x for x in f['bodies'] if x['body_id']==104)
        v_error=max(v_error,abs(body['velocity_mps'][2]+9.81*n/240))
        z_error=max(z_error,abs(body['position_m'][2]-(100-9.81*n*(n+1)/(2*240**2))))
    check('ballistic no contact',all(not f['contacts'] for f in frames))
    check('gravity velocity oracle',v_error<=limits['ballistic_velocity_error_mps'],v_error)
    check('semi-implicit position oracle',z_error<=limits['semi_implicit_position_error_m'],z_error)
    drop=fixture('prism','drop');drop['extensions']['rigid_scene']['gravity_mps2']=[0,0,0]
    drop['objects'][-1]['transform']['position']['z']=.7
    for b in drop['extensions']['rigid_scene']['bindings']:
        b['friction']=0
        if b['body_id']==17:b['restitution']=.35
        if b['body_id']==104:b.update(restitution=.8,linear_velocity_mps=[0,0,-1],angular_velocity_radps=[0,0,0])
    path=root/'restitution-input.json';write(path,drop);frames=call([a.backend,'record',path,aset,240],'restitution')['frames']
    impact=next(f for f in frames if f['contacts']);vz=next(b for b in impact['bodies'] if b['body_id']==104)['velocity_mps'][2]
    check('isolated min restitution oracle',abs(vz-.35)<=limits['restitution_velocity_error_mps'],{'impact_tick':impact['tick'],'outgoing_vz_mps':vz,'expected_mps':.35})
    check('retained parent unchanged',hashlib.sha256((a.parent/'scene.json').read_bytes()).hexdigest()==source_hash)
    report={'ok':not failures,'checks':checks,'failures':failures,'cases':cases,'parent_sha256':source_hash,'limits':limits}
    write(root/'verification.json',report)
    print(json.dumps({'ok':not failures,'failures':failures,'cases':cases},indent=2))
    return 0 if not failures else 2


if __name__=='__main__':raise SystemExit(main())
