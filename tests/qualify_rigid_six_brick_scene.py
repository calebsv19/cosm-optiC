#!/usr/bin/env python3
"""Six finite-mass crossed bricks: fixed rest/impact gates, no sleeping or pinning.

Uses the native Ball producer and independently measures source-mesh SAT,
energy, individual rest/response, and impulse propagation. Create-only output.
"""
import argparse
import copy
import itertools
import json
import math
from pathlib import Path
import subprocess

from qualify_rigid_resting_scene import box, kinetic, load, write
from rigid_scene_qualification_math import (dot, norm, penetration,
                                           quaternion_from_matrix, world_vertices)


BRICKS = tuple('brick_'+str(i) for i in range(6))


def multiply(a, b):
    w,x,y,z=a;v,i,j,k=b
    return [w*v-x*i-y*j-z*k,w*i+x*v+y*k-z*j,
            w*j-x*k+y*v+z*i,w*k+x*j-y*i+z*v]


def measure(cache, meshes, limits, impact, bricks=BRICKS):
    bindings={b['object_id']:b for b in cache['physical_input']['bindings']}
    initial={b['object_id']:b for b in cache['frames'][0]['bodies']}
    ids={b['body_id']:b['object_id'] for b in bindings.values()}
    first=None;edges=set();worst=0.;where=None;qerror=0.;samples=[];static_exact=True
    for f in cache['frames']:
        poses={b['object_id']:b for b in f['bodies']}
        vertices={oid:world_vertices(meshes[oid],bindings[oid],p) for oid,p in poses.items()}
        bounds={oid:([min(v[k] for v in vs) for k in range(3)],
                     [max(v[k] for v in vs) for k in range(3)]) for oid,vs in vertices.items()}
        for a,b in itertools.combinations(poses,2):
            # Conservative broadphase only: every overlapping AABB gets full source SAT.
            aa,ab=bounds[a];ba,bb=bounds[b]
            if any(ab[k]<=ba[k] or bb[k]<=aa[k] for k in range(3)):continue
            depth=penetration(vertices[a],meshes[a]['triangles'],vertices[b],meshes[b]['triangles'])
            if depth>worst:worst,where=depth,[f['tick'],a,b]
        for c in f['contacts']:
            if c['normal_impulse_kg_mps']<=1e-6:continue
            pair=tuple(sorted((ids[c['body_a_id']],ids[c['body_b_id']])))
            if 'projectile' in pair and any(v in bricks for v in pair) and first is None:first=f['tick']
            if first is not None:edges.add(pair)
        for oid,p in poses.items():
            if p['static']:static_exact &= p==initial[oid]
            qerror=max(qerror,abs(dot(p['orientation_wxyz'],p['orientation_wxyz'])-1))
        energy=sum(kinetic(p,bindings[oid])+bindings[oid]['mass_kg']*9.81*p['position_m'][2]
                   for oid,p in poses.items() if not p['static'])
        samples.append((f['tick'],poses,energy))
    def distance(a,b):return norm([x-y for x,y in zip(a,b)])
    rest={};checks={
        'complete_time_range':len(samples)==limits['ticks']+1 and samples[-1][0]==limits['ticks'],
        ('eight_bodies_all_ticks' if len(bricks)==6 else 'expected_bodies_all_ticks'):all(len(p)==len(bricks)+2 for _,p,_ in samples),
        ('six_positive_mass_bricks' if len(bricks)==6 else 'positive_mass_bricks'):all(bindings[o]['mass_kg']>0 and not initial[o]['static'] for o in bricks),
        'static_exact':static_exact,'source_mesh_overlap':worst<=limits['accepted_mesh_penetration_m'],
        'quaternion_norm':qerror<=limits['quaternion_norm_error'],
        'bounded_energy':max(e for _,_,e in samples)<=samples[0][2]+limits['energy_growth_j']}
    window=[row for row in samples if row[0]>=limits['rest_window_start_tick']]
    for oid in bricks:
        m={
            'total_drift_m':max(distance(p[oid]['position_m'],initial[oid]['position_m']) for _,p,_ in samples),
            'window_drift_m':max(distance(p[oid]['position_m'],window[0][1][oid]['position_m']) for _,p,_ in window),
            'window_linear_speed_mps':max(norm(p[oid]['velocity_mps']) for _,p,_ in window),
            'window_angular_speed_radps':max(norm(p[oid]['angular_velocity_radps']) for _,p,_ in window),
            'window_kinetic_energy_j':max(kinetic(p[oid],bindings[oid]) for _,p,_ in window),
            'orientation_change_rad':max(2*math.acos(min(1,abs(dot(p[oid]['orientation_wxyz'],initial[oid]['orientation_wxyz'])))) for _,p,_ in samples),
            'peak_angular_speed_radps':max(norm(p[oid]['angular_velocity_radps']) for _,p,_ in samples),
            'preimpact_drift_m':max(distance(p[oid]['position_m'],initial[oid]['position_m']) for t,p,_ in samples if first is None or t<first)}
        rest[oid]=m
        if impact:
            checks[oid+' initially_stable']=m['preimpact_drift_m']<=limits['rest_total_drift_m']
            checks[oid+' moves']=m['total_drift_m']>=limits['impact_displacement_m']
        else:
            for field,gate in [('total_drift_m','rest_total_drift_m'),('window_drift_m','rest_window_drift_m'),
                               ('window_linear_speed_mps','rest_linear_speed_mps'),('window_angular_speed_radps','rest_angular_speed_radps'),
                               ('orientation_change_rad','rest_orientation_change_rad'),('window_kinetic_energy_j','rest_kinetic_energy_j')]:
                checks[oid+' '+field]=m[field]<=limits[gate]
    if impact:
        reached={'projectile'}
        # Floor support is not impact transmission evidence.
        for _ in range(len(bricks)+1):
            for a,b in edges:
                if 'ground' not in (a,b) and (a in reached or b in reached):reached.update((a,b))
        checks['delayed_projectile_hit']=first is not None and first>=limits['minimum_preimpact_ticks']
        checks['impulse_path_to_all_six']=all(o in reached for o in bricks)
        checks['rotational_response']=max(m['peak_angular_speed_radps'] for m in rest.values())>=limits['impact_angular_speed_radps']
        shot=next(p['projectile'] for t,p,_ in samples if t==first) if first is not None else initial['projectile']
        incoming=next(p['projectile'] for t,p,_ in samples if t==first-1) if first is not None else shot
        # Same-tick horizontal loss isolates impact from the preceding slide/drop.
        checks['finite_projectile_reacts']=incoming['velocity_mps'][0]-shot['velocity_mps'][0]>limits['impact_projectile_velocity_loss_mps']
        projectile_response={'incoming_velocity_mps':incoming['velocity_mps'],
                             'outgoing_velocity_mps':shot['velocity_mps']}

    return {'ok':all(checks.values()),'checks':checks,'worst_overlap_m':worst,'worst_overlap_at':where,
            'quaternion_norm_error':qerror,'first_projectile_hit_tick':first,'rest':rest,
            'impact_contact_edges':[list(e) for e in sorted(edges)],
            'initial_energy_j':samples[0][2],'maximum_energy_j':max(e for _,_,e in samples),
            'projectile_response':projectile_response if impact else None}


def main(profile=None):
    ap=argparse.ArgumentParser(description=__doc__ if profile is None else profile.__doc__)
    for name in ('backend','parent','out'):ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--cases',nargs='+',choices=('rest','impact'),default=('rest','impact'))
    if profile is None:ap.add_argument('--projectile-side',type=float,choices=(.4,.8),default=.8,help='0.8m broad structure strike; 0.4m narrow offset control')
    ap.add_argument('--projectile-offset-y',type=float,default=0.,help='Explicit initial lateral offset in meters; does not alter any acceptance gate')
    ap.add_argument('--wall-seconds',type=float,default=300,help='Per native call, at most 600 seconds for sanitizer builds')
    if profile is not None:ap.set_defaults(projectile_side=profile.asset_sizes[-1][1][0])
    a=ap.parse_args()
    if not 0<a.wall_seconds<=600:ap.error('wall seconds must be in (0,600]')
    if not math.isfinite(a.projectile_offset_y):ap.error('projectile offset must be finite')
    root=a.out.resolve();root.mkdir(parents=True,exist_ok=False)
    write(root/'run-config.json',{'native_wall_seconds':a.wall_seconds,'profile':'six_brick' if profile is None else profile.name,
         'projectile_side_m':a.projectile_side if profile is None else profile.asset_sizes[-1][1][0],
         'asset_sizes':None if profile is None else profile.asset_sizes,
         'projectile_offset_y_m':a.projectile_offset_y,
         'projectile_velocity_mps':[4,0,0] if profile is None else profile.projectile_velocity})
    if profile is not None:write(root/'fixture.json',{'items':profile.items(),'projectile_velocity_mps':profile.projectile_velocity})
    limits=load(Path(__file__).with_name('rigid_six_brick_limits.json' if profile is None else getattr(profile,'limits_file','rigid_toppling_limits.json')));write(root/'declared-limits.json',limits)
    write(root/'verification.json',{'ok':False,'status':'running','expected_cases':a.cases,'cases':{}})
    def call(argv,name):
        p=subprocess.run(list(map(str,argv)),capture_output=True,text=True,timeout=a.wall_seconds)
        (root/(name+'.log')).write_text(p.stderr)
        result=json.loads(p.stdout)
        if p.returncode:
            write(root/(name+'-failure.json'),result)
            status=load(root/'verification.json')
            status.update(ok=False,status='failed',failed_call=name,error=result.get('error'))
            write(root/'verification.json',status)
            raise RuntimeError((name,p.returncode,result.get('error')))
        return result
    assets=root/'assets';assets.mkdir()
    sizes=[('floor',(12,8,.4)),('brick',(.9,.28,.3)),('projectile',(a.projectile_side,)*3)] if profile is None else profile.asset_sizes
    for name,size in sizes:
        box(assets/(name+'.stl'),size)
        call([a.backend,'prepare',assets/(name+'.stl'),name,assets/name],'prepare-'+name)
    aset=assets/'asset_set.json';write(aset,{'schema':'ball_rigid_asset_set_v1',
        'assets':[{'asset_id':n,'root':n} for n in ('floor','brick','projectile')]})
    meta={m['asset_id']:m for m in call([a.backend,'catalog',aset],'catalog')['assets']}
    write(root/'catalog.json',meta);summary={}
    for case in a.cases:
        scene={k:copy.deepcopy(v) for k,v in load(a.parent).items() if k not in ('objects','extensions')}
        scene.update(scene_id=('six_brick_' if profile is None else getattr(profile,'scene_id_prefix','six_brick_toppling_'))+case,objects=[],extensions={})
        rigid={'schema':'core_rigid_scene_v1','coordinate_system':'right_handed_z_up_meters',
               'rate_numerator':240,'rate_denominator':1,'origin_tick':0,'end_tick':limits['ticks'],
               'gravity_mps2':[0,0,-9.81],'contact_policy':'min_restitution_geometric_friction_v1','bindings':[]}
        scene['extensions']['rigid_scene']=rigid
        items=[('ground','floor',17,[0,0,-.2],0,0)]
        for i in range(6):
            upper=i>=3;side=(i%3-1)*.285
            items.append((BRICKS[i],'brick',46+29*i,[side,0,.45] if upper else [0,side,.15],2,math.pi/2 if upper else 0))
        items.append(('projectile','projectile',220,[-2,.12,a.projectile_side/2+.02],8,0))
        if profile is not None:items=profile.items()
        for oid,asset,bid,pos,mass,yaw in items:
            if oid=='projectile':pos[1]+=a.projectile_offset_y
        write(root/(case+'-fixture.json'),{'items':items})
        meshes={}
        for oid,asset,bid,pos,mass,yaw in items:
            m=meta[asset];path=assets/asset/'visible.json';meshes[oid]=load(path)['mesh']
            scene['objects'].append({'object_id':oid,'object_type':'mesh_asset_instance',
                'geometry_ref':{'kind':'mesh_asset','id':m['mesh_asset_id']},
                'transform':{'position':dict(zip('xyz',pos)),'rotation':dict.fromkeys('xyz',0),'scale':dict.fromkeys('xyz',1)},
                'flags':{'visible':True},'extensions':{'line_drawing':{'runtime_mesh_path':str(path)}}})
            b={k:copy.deepcopy(v) for k,v in m.items() if k not in ('asset_id','shape','child_count','inertia_per_kg')}
            b.update(object_id=oid,body_id=bid,role='dynamic' if mass else 'static',motion_owner='simulation' if mass else 'authored',
                     collides=True,collision_asset_id=asset,orientation_wxyz=multiply([math.cos(yaw/2),0,0,math.sin(yaw/2)],quaternion_from_matrix(m['principal_to_source'])),
                     linear_velocity_mps=[4,0,0] if oid=='projectile' and case=='impact' else [0,0,0],angular_velocity_radps=[0,0,0],mass_kg=mass,
                     principal_inertia_kgm2=[mass*v for v in m['inertia_per_kg']],restitution=.05,friction=.15 if oid=='projectile' else .6)
            if profile is not None and oid=='projectile' and case=='impact':b['linear_velocity_mps']=profile.projectile_velocity
            rigid['bindings'].append(b)
        path=root/(case+'-input.json');write(path,scene)
        cache=call([a.backend,'record',path,aset,limits['ticks']],case);write(root/(case+'-cache.json'),cache)
        result=(measure if profile is None else profile.measure)(cache,meshes,limits,case=='impact')
        result['checks']['exact_native_replay']=call([a.backend,'record',path,aset,limits['ticks']],case+'-repeat')==cache
        result['ok']=all(result['checks'].values());summary[case]=result;write(root/(case+'-metrics.json'),result)
        write(root/'verification.json',{'ok':False,'status':'running','expected_cases':a.cases,'cases':summary})
        print(case,result['ok'],'overlap',result['worst_overlap_m'],flush=True)
    ok=len(summary)==len(a.cases) and all(v['ok'] for v in summary.values())
    write(root/'verification.json',{'ok':ok,'status':'passed' if ok else 'failed','expected_cases':a.cases,'cases':summary})
    return 0 if ok else 2


if __name__=='__main__':raise SystemExit(main())
