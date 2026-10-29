#!/usr/bin/env python3
"""BR4 native consumer qualification against a caller-selected accepted BR3 bundle.
No solver/backend is required or invoked. Writes only a fresh --out directory.
"""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import random
import shutil
import subprocess


def encode(value):
    return (json.dumps(value,sort_keys=True,separators=(',',':'),allow_nan=False)+'\n').encode()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('probe','profile','renderer','pack_io','bundle','out'):
        p.add_argument('--'+name.replace('_','-'),type=Path,required=True)
    a=p.parse_args();a.out.mkdir(exist_ok=False)
    ray=Path(__file__).resolve().parents[1]
    env=dict(os.environ,XDG_CONFIG_HOME=str(a.out/'config'),PYTHONDONTWRITEBYTECODE='1',RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled',RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT=str(a.out/'mesh-cache'))
    checks=[]
    def run(command,expect=True):
        r=subprocess.run(list(map(str,command)),env=env,cwd=ray,capture_output=True,text=True,timeout=60)
        if (r.returncode==0)!=expect:
            raise AssertionError((command,r.returncode,r.stdout[-1000:],r.stderr[-1000:]))
        return [json.loads(line[4:]) for line in r.stdout.splitlines() if line.startswith('BR4 ')]
    root=a.out/'pilot';root.mkdir();shutil.copytree(a.bundle,root/'result')
    revision=json.loads((a.bundle/'manifest.json').read_text())['result_revision']
    payload=subprocess.run([str(a.pack_io),'read',str(a.bundle/'poses.pack')],capture_output=True,check=True).stdout
    cache=json.loads(payload);(a.out/'payload.json').write_bytes(payload)
    run([a.profile,a.out/'payload.json']);checks.append('native shared payload admission and checked rational tick arithmetic')
    scene=json.loads((a.bundle/'input/scene.json').read_text());scene['schema_variant']='scene_runtime_v1'
    aset=json.loads((a.bundle/'input/assets/asset_set.json').read_text())
    meshes={}
    for item in aset['assets']:
        path=Path('result/input/assets')/item['root']/'visible.json'
        mesh=json.loads((root/path).read_text());meshes[mesh['asset_id']]=(path,mesh)
    for obj in scene['objects']:
        obj.setdefault('extensions',{})['line_drawing']={'runtime_mesh_path':str(meshes[obj['geometry_ref']['id']][0])}
    visual=json.loads((ray/'tests/fixtures/caustic_probe_glass_sphere/scene_runtime.json').read_text())
    scene['lights']=visual['lights'];scene['cameras']=visual['cameras']
    scene['cameras'][0].update(position={'x':0,'y':7,'z':3.6},yaw=0,look_pitch=-0.25)
    scene['extensions']['ray_tracing']={'rigid_result':{'schema':'ray_rigid_result_binding_v1','bundle_path':'result','result_revision':revision}}
    def save(name,s):
        path=root/(name+'.json');path.write_text(json.dumps(s,indent=2));return path
    baseline=save('scene',scene);source_hash=hashlib.sha256(baseline.read_bytes()).hexdigest()
    ticks=list(range(cache['endpoint_tick']+1));random.Random(20261020).shuffle(ticks);ticks=[0,154,30,180,0]+ticks+[180,0]
    rows=run([a.probe,baseline,*ticks]);assert len(rows)==len(ticks)
    def expected_vertex(binding,pose,v):
        m=binding['principal_to_source'];c=binding['source_center_m']
        x=[sum(m[k*3+i]*(v[k]-c[k]) for k in range(3)) for i in range(3)]
        w,a,b,d=pose['orientation_wxyz']
        R=[[1-2*(b*b+d*d),2*(a*b-d*w),2*(a*d+b*w)], [2*(a*b+d*w),1-2*(a*a+d*d),2*(b*d-a*w)], [2*(a*d-b*w),2*(b*d+a*w),1-2*(a*a+b*b)]]
        return [pose['position_m'][i]+sum(R[i][k]*x[k] for k in range(3)) for i in range(3)]
    worst=0;by_tick={}
    for row in rows:
        t=row['frame'];source=cache['frames'][t]['bodies'];bindings=cache['physical_input']['bindings'];assert len(row['poses'])==len(source)
        for actual,expected in zip(row['poses'],source):
            assert actual['id']==expected['object_id'] and actual['tick']==t and actual['q']==expected['orientation_wxyz'] and actual['position']==expected['position_m'] and actual['revision']==revision and actual['body_id']==expected['body_id']
        assert len(row['triangles'])==sum(len(meshes[b['mesh_asset_id']][1]['mesh']['triangles']) for b in bindings)
        for tri in row['triangles']:
            b=next(b for b in bindings if b['object_id']==tri['id']);pose=next(s for s in source if s['object_id']==tri['id']);mesh=meshes[b['mesh_asset_id']][1]['mesh'];indices=mesh['triangles'][tri['local']]
            for actual,key in zip(tri['vertices'],('a','b','c')):
                v=mesh['vertices'][indices[key]];expected=expected_vertex(b,pose,[v[k] for k in ('x','y','z')]);error=max(abs(x-y) for x,y in zip(actual,expected));worst=max(worst,error);assert error<2e-12,(t,tri['id'],error)
        if t in by_tick:assert row==by_tick[t]
        by_tick[t]=row
    checks.append(f'all {cache["endpoint_tick"]+1} ticks: exact preview/headless poses, shuffled/reverse/repeated seeks, independent source COM/principal mapping; max vertex error {worst:.3g}m')
    assert hashlib.sha256(baseline.read_bytes()).hexdigest()==source_hash
    checks.append('prepared snapshots reject forged poses; flattened BVH and TLAS/BLAS hit the current moving body at every seek')
    checks.append('authored initial conditions and selected immutable result preserved')
    mutations={
        'gravity':lambda s:s['extensions']['rigid_scene']['gravity_mps2'].__setitem__(2,-8),
        'velocity':lambda s:s['extensions']['rigid_scene']['bindings'][-1]['linear_velocity_mps'].__setitem__(0,0.5),
        'friction':lambda s:s['extensions']['rigid_scene']['bindings'][-1].__setitem__('friction',0.123),
        'position':lambda s:s['objects'][-1]['transform']['position'].__setitem__('z',2.6),
        'source_digest':lambda s:s['extensions']['rigid_scene']['bindings'][-1].__setitem__('source_sha256','0'*64),
        'principal_frame':lambda s:s['extensions']['rigid_scene']['bindings'][-1]['principal_to_source'].__setitem__(0,1),
        'missing_binding':lambda s:s['extensions']['rigid_scene']['bindings'].pop(),
        'missing_result':lambda s:s['extensions']['ray_tracing'].pop('rigid_result'),
        'wrong_revision':lambda s:s['extensions']['ray_tracing']['rigid_result'].__setitem__('result_revision','0'*64),
        'other_owner':lambda s:s['extensions']['rigid_scene']['bindings'][-1].__setitem__('motion_owner','authored'),
    }
    for name,mutate in mutations.items():
        s=copy.deepcopy(scene);mutate(s);path=save(name,s);run([a.probe,path,0],False);run([a.probe,baseline,'--replacement',path]);checks.append(name+' refuses and retains previous admitted scene/result')
    def mass(s):
        b=s['extensions']['rigid_scene']['bindings'][-1];b['mass_kg']*=2;b['principal_inertia_kgm2']=[2*x for x in b['principal_inertia_kgm2']]
    s=copy.deepcopy(scene);mass(s);run([a.probe,save('mass',s),0],False);checks.append('self-consistent mass/inertia edit invalidates accepted physical identity')
    for name,change in (
        ('camera',lambda s:s['cameras'][0]['position'].__setitem__('x',1)),
        ('light',lambda s:s['lights'][0].__setitem__('intensity',2)),
        ('material',lambda s:s.__setitem__('materials',[{'material_id':'appearance','kind':'lambert','albedo':[0.4,0.2,0.9]}])),
        ('reorder',lambda s:(s['objects'].reverse(),s['extensions']['rigid_scene']['bindings'].reverse())),
    ):
        s=copy.deepcopy(scene);change(s);read=run([a.probe,save(name,s),154]);assert read[0]['poses']==by_tick[154]['poses'];checks.append(name+' reuses exact immutable poses without solver execution')
    changed_mesh=root/'presentation_mesh.json';changed_mesh.write_text(json.dumps(meshes['asymmetric_mesh'][1],indent=4)+'\n')
    s=copy.deepcopy(scene);s['objects'][-1]['extensions']['line_drawing']['runtime_mesh_path']=changed_mesh.name
    s['extensions']['rigid_scene']['bindings'][-1]['mesh_sha256']=hashlib.sha256(changed_mesh.read_bytes()).hexdigest()
    read=run([a.probe,save('mesh_serialization',s),154]);assert read[0]['poses']==by_tick[154]['poses'];checks.append('verified equivalent visible serialization with updated mesh digest reuses physics')
    s=copy.deepcopy(scene);s['objects'][-1]['flags']['visible']=False
    hidden=run([a.probe,save('hidden',s),154])[0];assert hidden['poses']==by_tick[154]['poses'] and not any(t['id']=='projectile' for t in hidden['triangles'])
    checks.append('hiding a render instance reuses physical poses and removes only its visible geometry')
    s=copy.deepcopy(scene);s['extensions']['ray_tracing']['authoring']={'scene_timeline':{'version':1,'rate':{'numerator':30,'denominator':1},'range':{'start_frame':10,'frame_count':23},'tracks':[]}}
    read=run([a.probe,save('clock30',s),0,19,22,1,0]);assert [r['poses'][0]['tick'] for r in read]==[0,152,176,8,0];checks.append('30fps presentation, nonzero timeline origin maps exactly to 240Hz result ticks')
    for name,rate,count in [('fractional_clock',29,20),('uncovered_clock',30,24)]:
        wrong=copy.deepcopy(s);wrong['extensions']['ray_tracing']['authoring']['scene_timeline']['rate']['numerator']=rate;wrong['extensions']['ray_tracing']['authoring']['scene_timeline']['range']['frame_count']=count;run([a.probe,save(name,wrong),0],False);checks.append(name+' refuses before scene mutation')
    # Payload mutations recompute only the physical canonical hash when needed,
    # so shape/state checks cannot accidentally pass by testing just a hash error.
    profiles={
        'unknown_field':lambda c:c.__setitem__('future',1),
        'missing_frame':lambda c:c['frames'].pop(),
        'duplicate_tick':lambda c:c['frames'][2].__setitem__('tick',1),
        'wrong_time':lambda c:c['frames'][2].__setitem__('time_s',2),
        'quaternion':lambda c:c['frames'][2]['bodies'][-1]['orientation_wxyz'].__setitem__(0,3),
        'static_drift':lambda c:c['frames'][2]['bodies'][0]['position_m'].__setitem__(0,9),
        'initial_drift':lambda c:c['frames'][0]['bodies'][-1]['position_m'].__setitem__(0,9),
        'body_order':lambda c:c['frames'][2]['bodies'].reverse(),
        'contact_capacity':lambda c:c['frames'][2].__setitem__('constraints_solved',121),
        'negative_impulse':lambda c:c['frames'][2].__setitem__('impulse_kg_mps',-1),
        'canonical_mismatch':lambda c:c.__setitem__('physical_input_canonical','{}'),
        'engine_digest':lambda c:c.__setitem__('engine_sha256','not_a_digest'),
    }
    for name,mutate in profiles.items():
        c=copy.deepcopy(cache);mutate(c);path=a.out/('profile_'+name+'.json');path.write_bytes(encode(c));run([a.profile,path],False);checks.append('shared profile refuses '+name)
    for name,raw in [('duplicate_key',payload.replace(b'"schema":',b'"schema":"bad","schema":',1)),('trailing',payload+b'{}'),('nul',payload+b'\0'),('nan',payload.replace(b'"endpoint_tick":180',b'"endpoint_tick":NaN'))]:
        path=a.out/('profile_'+name+'.json');path.write_bytes(raw);run([a.profile,path],False);checks.append('shared strict JSON refuses '+name)
    # Immutable bundle and current visible mesh corruption are different gates.
    meshpath=root/'bad_mesh.json';badmesh=copy.deepcopy(meshes['asymmetric_mesh'][1]);badmesh['mesh']['vertices'][0]['x']+=0.01;meshpath.write_text(json.dumps(badmesh));s=copy.deepcopy(scene);s['objects'][-1]['extensions']['line_drawing']['runtime_mesh_path']='bad_mesh.json';path=save('bad_mesh_scene',s);run([a.probe,path,0],False);run([a.probe,baseline,'--replacement',path]);checks.append('current visible mesh corruption refuses and preserves previous scene')
    for name,action in [('corrupt',lambda b:(b/'poses.pack').write_bytes((b/'poses.pack').read_bytes()+b'x')),('unlisted',lambda b:(b/'surprise').write_text('x')),('symlink',lambda b:((b/'input/scene.json').unlink(),(b/'input/scene.json').symlink_to(root/'scene.json')))]:
        bundle=root/('bundle_'+name);shutil.copytree(root/'result',bundle);action(bundle);s=copy.deepcopy(scene);s['extensions']['ray_tracing']['rigid_result']['bundle_path']=bundle.name;run([a.probe,save('bundle_scene_'+name,s),0],False);checks.append('bundle '+name+' refused')
    # The ordinary CLI must show the same exact provenance and produce pixels.
    for tick in (0,154):
        req=json.loads((ray/'tests/fixtures/agent_render_preflight_request.json').read_text());req['run_id']='br4_'+str(tick);req['scene']={'runtime_scene_path':str(baseline)};req['render'].update(start_frame=tick,frame_count=1,width=96,height=64,normalized_t=tick/180,temporal_frames=1);req['output']={'root':str(a.out/f'render{tick}'),'overwrite':False};req['progress']={'summary_path':str(a.out/f'summary{tick}.json'),'progress_path':str(a.out/f'progress{tick}.json')};request=a.out/f'request{tick}.json';request.write_text(json.dumps(req));run([a.renderer,'--request',request,'--render']);summary=json.loads((a.out/f'summary{tick}.json').read_text());assert summary['rendered_frames'] and summary['frames_rendered']==1
        assert next(row for row in summary['object_audit'] if row['object_id']=='projectile')['primary_hit_pixels']>0
        for actual,expected in zip(summary['evaluated_objects'],by_tick[tick]['poses']):assert actual['source_tick']==expected['tick'] and actual['result_revision']==revision and actual['orientation_wxyz']==expected['q'] and actual['position']==expected['position']
    assert (a.out/'render0/frames/frame_0000.bmp').read_bytes()!=(a.out/'render154/frames/frame_0154.bmp').read_bytes()
    checks.append('ordinary headless renders at ticks0/154 show moving-body pixels, changed images and exact pose/result provenance')
    report={'status':'PASS','checks':checks,'check_count':len(checks),'revision':revision,'max_vertex_error_m':worst};(a.out/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
