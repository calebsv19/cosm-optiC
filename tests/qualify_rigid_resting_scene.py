#!/usr/bin/env python3
"""Native resting/dynamic-impact qualification; immutable inputs and fixed gates.

No renderer or simulation is substituted by the independent source-mesh SAT
and energy measurements here. Output is create-only and keeps failures.
"""
import argparse
import copy
import itertools
import json
import math
from pathlib import Path
import subprocess

from rigid_scene_qualification_math import (dot, norm, penetration, rotation,
                                           quaternion_from_matrix, world_vertices)


def load(path):
    return json.loads(path.read_bytes())


def write(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def box(path, size):
    x, y, z = (v / 2 for v in size)
    vertices = [(-x,-y,-z),(x,-y,-z),(x,y,-z),(-x,y,-z),
                (-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z)]
    faces = [(0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),
             (1,2,6),(1,6,5),(2,3,7),(2,7,6),(3,0,4),(3,4,7)]
    lines = ['solid resting']
    for face in faces:
        lines += ['facet normal 0 0 0', 'outer loop']
        lines += ['vertex ' + ' '.join(format(v, '.17g') for v in vertices[i]) for i in face]
        lines += ['endloop', 'endfacet']
    path.write_text('\n'.join(lines + ['endsolid resting', '']))


def kinetic(body, binding):
    r = rotation(body['orientation_wxyz'])
    omega = body['angular_velocity_radps']
    principal = [sum(r[k][i]*omega[k] for k in range(3)) for i in range(3)]
    return (binding['mass_kg'] * dot(body['velocity_mps'], body['velocity_mps']) +
            sum(i*w*w for i,w in zip(binding['principal_inertia_kgm2'], principal))) / 2


def measure(cache, meshes, limits, impact):
    bindings = {b['object_id']: b for b in cache['physical_input']['bindings']}
    frames = cache['frames']; initial = {b['object_id']:b for b in frames[0]['bodies']}
    samples = []; worst = 0.; worst_at = None; qerror = 0.; first_hit = None
    contact_pairs = set()
    for frame in frames:
        poses = {b['object_id']:b for b in frame['bodies']}
        vertices = {oid:world_vertices(meshes[oid], bindings[oid], b) for oid,b in poses.items()}
        for a,b in itertools.combinations(poses, 2):
            depth = penetration(vertices[a], meshes[a]['triangles'], vertices[b], meshes[b]['triangles'])
            if depth > worst:
                worst, worst_at = depth, [frame['tick'], a, b]
        qerror = max(qerror, max(abs(dot(b['orientation_wxyz'],b['orientation_wxyz'])-1) for b in poses.values()))
        energy = sum(kinetic(b, bindings[oid]) + bindings[oid]['mass_kg']*9.81*b['position_m'][2]
                     for oid,b in poses.items() if not b['static'])
        for c in frame['contacts']:
            pair = tuple(sorted((c['body_a_id'],c['body_b_id'])))
            contact_pairs.add(pair)
            if (104 in pair and any(i in pair for i in (46,75)) and
                    c['normal_impulse_kg_mps']>1e-6 and first_hit is None):
                first_hit = frame['tick']
        samples.append((frame, poses, energy))
    statics = all(b == initial[b['object_id']] for f in frames for b in f['bodies'] if b['static'])
    rest = {}
    for oid in ('block_lower', 'block_upper'):
        if oid not in initial: continue
        window = [(f,p,e) for f,p,e in samples if f['tick'] >= limits['rest_window_start_tick']]
        def distance(a,b):return norm([x-y for x,y in zip(a,b)])
        rest[oid] = {
            'total_drift_m': max(distance(p[oid]['position_m'], initial[oid]['position_m']) for _,p,_ in samples),
            'window_drift_m': max(distance(p[oid]['position_m'], window[0][1][oid]['position_m']) for _,p,_ in window),
            'window_linear_speed_mps': max(norm(p[oid]['velocity_mps']) for _,p,_ in window),
            'window_angular_speed_radps': max(norm(p[oid]['angular_velocity_radps']) for _,p,_ in window),
            'orientation_change_rad': max(2*math.acos(min(1,abs(dot(p[oid]['orientation_wxyz'],initial[oid]['orientation_wxyz'])))) for _,p,_ in samples),
            'window_kinetic_energy_j': max(kinetic(p[oid],bindings[oid]) for _,p,_ in window),
            'peak_angular_speed_radps': max(norm(p[oid]['angular_velocity_radps']) for _,p,_ in samples),
            'preimpact_drift_m': max((distance(p[oid]['position_m'], initial[oid]['position_m']) for f,p,_ in samples if first_hit is None or f['tick']<first_hit), default=0)
        }
    checks = {'complete_time_range':len(frames)==limits['ticks']+1 and frames[-1]['tick']==limits['ticks'],
              'accepted_mesh_overlap':worst<=limits['accepted_mesh_penetration_m'],
              'static_exact':statics, 'quaternion_norm':qerror<=limits['quaternion_norm_error'],
              'bounded_energy':max(e for _,_,e in samples)<=samples[0][2]+limits['energy_growth_j']}
    if not impact:
        for oid,m in rest.items():
            for field,gate in [('total_drift_m','rest_total_drift_m'),('window_drift_m','rest_window_drift_m'),
                               ('window_linear_speed_mps','rest_linear_speed_mps'),('window_angular_speed_radps','rest_angular_speed_radps'),
                               ('orientation_change_rad','rest_orientation_change_rad'),('window_kinetic_energy_j','rest_kinetic_energy_j')]:
                checks[oid+' '+field] = m[field]<=limits[gate]
    else:
        checks['projectile_contacts_dynamic_block'] = first_hit is not None
        for oid,m in rest.items():
            checks[oid+' initially_stable'] = m['preimpact_drift_m']<=limits['rest_total_drift_m']
            checks[oid+' responds'] = m['total_drift_m']>=limits['impact_displacement_m']
        checks['offset_impact_generates_rotation'] = max(m['peak_angular_speed_radps'] for m in rest.values())>=limits['impact_angular_speed_radps']
    return {'ok':all(checks.values()),'checks':checks,'worst_overlap_m':worst,'worst_overlap_at':worst_at,
            'quaternion_norm_error':qerror,'first_projectile_hit_tick':first_hit,
            'contact_pairs':[list(p) for p in sorted(contact_pairs)],'rest':rest,
            'initial_energy_j':samples[0][2],'maximum_energy_j':max(e for _,_,e in samples)}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--backend',type=Path,required=True)
    ap.add_argument('--parent',type=Path,required=True,help='Retained BR6 scene, for camera/light only')
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--cases',nargs='+',choices=('single-rest','single-impact','stack-rest','stack-impact'))
    ap.add_argument('--stack-offset',nargs=2,type=float,default=(0.,0.),metavar=('X','Y'))
    a=ap.parse_args();root=a.out.resolve();root.mkdir(parents=True,exist_ok=False)
    if not all(math.isfinite(v) and abs(v)<=.15 for v in a.stack_offset):
        ap.error('stack offset must be finite and within 0.15 m on each axis')
    limits=load(Path(__file__).with_name('rigid_resting_limits.json'));write(root/'declared-limits.json',limits)
    def call(argv, name):
        p=subprocess.run(list(map(str,argv)),capture_output=True,text=True,timeout=180)
        (root/(name+'.log')).write_text(p.stderr)
        result=json.loads(p.stdout)
        if p.returncode:
            write(root/(name+'-failure.json'), result)
            raise RuntimeError((name,p.returncode,result.get('error')))
        return result
    assets=root/'assets';assets.mkdir()
    for name,size in [('floor',(12,8,.4)),('block',(.8,.6,.5)),('projectile',(.4,.4,.4))]:
        box(assets/(name+'.stl'),size)
        call([a.backend,'prepare',assets/(name+'.stl'),name,assets/name],'prepare-'+name)
    aset=assets/'asset_set.json'
    write(aset,{'schema':'ball_rigid_asset_set_v1','assets':[{'asset_id':n,'root':n} for n in ('floor','block','projectile')]})
    meta={x['asset_id']:x for x in call([a.backend,'catalog',aset],'catalog')['assets']};write(root/'catalog.json',meta)
    parent=load(a.parent)
    summary={}
    for stacked,impact in itertools.product((False,True),(False,True)):
        name=('stack' if stacked else 'single')+('-impact' if impact else '-rest')
        if a.cases and name not in a.cases:continue
        scene={k:copy.deepcopy(v) for k,v in parent.items() if k not in ('objects','extensions')}
        scene.update(scene_id='resting_'+name,objects=[],extensions={})
        rigid={'schema':'core_rigid_scene_v1','coordinate_system':'right_handed_z_up_meters',
               'rate_numerator':240,'rate_denominator':1,'origin_tick':0,'end_tick':limits['ticks'],
               'gravity_mps2':[0,0,-9.81],'contact_policy':'min_restitution_geometric_friction_v1','bindings':[]}
        scene['extensions']['rigid_scene']=rigid
        items=[('ground','floor',17,[0,0,-.2],0),('block_lower','block',46,[0,0,.25],2)]
        if stacked:items.append(('block_upper','block',75,[*a.stack_offset,.75],2))
        if impact:items.append(('projectile','projectile',104,[-1.5,.12,.28],8))
        meshes={}
        for oid,asset,bid,pos,mass in items:
            m=meta[asset];meshpath=assets/asset/'visible.json';meshes[oid]=load(meshpath)['mesh']
            scene['objects'].append({'object_id':oid,'object_type':'mesh_asset_instance',
                'geometry_ref':{'kind':'mesh_asset','id':m['mesh_asset_id']},
                'transform':{'position':dict(zip('xyz',pos)),'rotation':dict.fromkeys('xyz',0),'scale':dict.fromkeys('xyz',1)},
                'flags':{'visible':True},'extensions':{'line_drawing':{'runtime_mesh_path':str(meshpath)}}})
            b={k:copy.deepcopy(v) for k,v in m.items() if k not in ('asset_id','shape','child_count','inertia_per_kg')}
            b.update(object_id=oid,body_id=bid,role='dynamic' if mass else 'static',
                motion_owner='simulation' if mass else 'authored',collides=True,collision_asset_id=asset,
                orientation_wxyz=quaternion_from_matrix(m['principal_to_source']),linear_velocity_mps=[3,0,0] if oid=='projectile' else [0,0,0],
                angular_velocity_radps=[0,0,0],mass_kg=mass,principal_inertia_kgm2=[mass*v for v in m['inertia_per_kg']],restitution=.05,friction=.6 if oid!='projectile' else .15)
            rigid['bindings'].append(b)
        path=root/(name+'-input.json');write(path,scene)
        cache=call([a.backend,'record',path,aset,limits['ticks']],name)
        write(root/(name+'-cache.json'),cache)
        result=measure(cache,meshes,limits,impact)
        repeat=call([a.backend,'record',path,aset,limits['ticks']],name+'-repeat')
        result['checks']['exact_native_replay']=repeat==cache
        result['ok']=all(result['checks'].values());summary[name]=result
        write(root/(name+'-metrics.json'),result)
        write(root/'verification.json',{'ok':all(x['ok'] for x in summary.values()),'cases':summary})
        print(name, result['ok'], 'overlap',result['worst_overlap_m'],flush=True)
    # Independent two-finite-mass restitution/momentum/energy oracle. This is
    # deliberately isolated from floor support, friction and offset torque.
    source=root/'single-impact-input.json'
    if source.exists():
        scene=load(source);scene['scene_id']='resting_head_on_oracle'
        scene['extensions']['rigid_scene']['gravity_mps2']=[0,0,0]
        for obj,b in zip(scene['objects'],scene['extensions']['rigid_scene']['bindings']):
            b['friction']=0
            if b['role']=='dynamic':obj['transform']['position'].update(y=0,z=2)
        path=root/'head-on-input.json';write(path,scene)
        cache=call([a.backend,'record',path,aset,120],'head-on')
        write(root/'head-on-cache.json',cache)
        frame=next(f for f in cache['frames'] if any(c['body_a_id']==46 and c['body_b_id']==104 for c in f['contacts']))
        poses={b['object_id']:b for b in frame['bodies']}
        target=poses['block_lower'];shot=poses['projectile']
        expected_target=(1+.05)*8*3/(8+2)
        expected_shot=(8-.05*2)*3/(8+2)
        momentum_error=max(abs(8*shot['velocity_mps'][i]+2*target['velocity_mps'][i]-expected)
                           for i,expected in enumerate((24,0,0)))
        bindings={b['object_id']:b for b in cache['physical_input']['bindings']}
        energy=sum(kinetic(poses[oid],bindings[oid]) for oid in ('projectile','block_lower'))
        expected_energy=4*expected_shot**2+expected_target**2
        checks={'finite_mass_restitution':max(abs(target['velocity_mps'][0]-expected_target),abs(shot['velocity_mps'][0]-expected_shot))<=limits['head_on_velocity_error_mps'],
                'momentum':momentum_error<=limits['head_on_momentum_error_kg_mps'],
                'kinetic_energy_loss':abs(energy-expected_energy)<=limits['head_on_energy_error_j'],
                'centered_impact_no_artificial_spin':max(norm(b['angular_velocity_radps']) for b in (target,shot))<=limits['head_on_angular_speed_radps']}
        oracle={'ok':all(checks.values()),'checks':checks,'impact_tick':frame['tick'],
                'target_velocity_mps':target['velocity_mps'],'projectile_velocity_mps':shot['velocity_mps'],
                'expected_target_vx_mps':expected_target,'expected_projectile_vx_mps':expected_shot,
                'momentum_error_kg_mps':momentum_error,'kinetic_energy_j':energy,'expected_kinetic_energy_j':expected_energy}
        write(root/'head-on-metrics.json',oracle);summary['head-on']=oracle
    write(root/'verification.json',{'ok':all(x['ok'] for x in summary.values()),'cases':summary})
    return 0 if all(x['ok'] for x in summary.values()) else 2


if __name__=='__main__':raise SystemExit(main())
