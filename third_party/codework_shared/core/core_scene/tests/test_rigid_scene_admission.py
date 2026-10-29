#!/usr/bin/env python3
"""Cross-host-codec contract admission matrix; never executes physics."""
import argparse
import copy
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', action='append', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    args.out.mkdir(exist_ok=False)
    fixture = Path(__file__).parent / 'fixtures/rigid_scene_v1'
    base = json.loads((fixture / 'scene_authoring.json').read_text())
    catalog = json.loads((fixture / 'asset_metadata.json').read_text())
    cases = []

    def run(name, edit=None, ok=False, raw=None, code=None):
        scene, assets = copy.deepcopy(base), copy.deepcopy(catalog)
        if edit:
            edit(scene, assets)
        payload = raw if raw is not None else json.dumps(scene, allow_nan=False).encode()
        s, a = args.out / 'scene.json', args.out / 'catalog.json'
        s.write_bytes(payload)
        a.write_text(json.dumps(assets, allow_nan=False))
        replies = []
        for tool in args.tool:
            result = subprocess.run([str(tool.resolve()), str(s), str(a)], capture_output=True, text=True, timeout=10)
            assert result.returncode == (0 if ok else 1), (name, tool, result.stdout, result.stderr)
            reply = json.loads(result.stdout)
            assert reply['ok'] is ok and reply['execution_supported'] is False, (name, reply)
            assert reply['proof_layer'] == 'contract_metadata_only'
            if code:
                assert reply['code'] == code, (name, reply)
            replies.append(reply)
        assert all(x == replies[0] for x in replies), (name, replies)
        cases.append(dict(name=name, accepted=ok, diagnostic=replies[0]))
        print('PASS ' + name, flush=True)

    def body(s):
        return s['extensions']['rigid_scene']['bindings'][3]

    def field(key, value):
        return lambda s, a: body(s).__setitem__(key, value)

    run('floor two pillars asymmetric body', ok=True)
    run('negative explicit gravity is valid', lambda s,a:s['extensions']['rigid_scene'].update(gravity_mps2=[0,0,0]), ok=True)
    run('static triangle collider metadata', lambda s,a:a['assets'][0].update(shape='static_triangles'), ok=True)
    run('object order independent', lambda s,a:s['objects'].reverse(), ok=True)
    run('binding order independent', lambda s,a:s['extensions']['rigid_scene']['bindings'].reverse(), ok=True)
    run('catalog order independent', lambda s,a:a['assets'].reverse(), ok=True)
    run('wrong role', field('role','mystery'))
    run('kinematic reserved', field('role','kinematic'), code='kinematic_unsupported')
    run('prescribed owner reserved', field('motion_owner','prescribed'), code='kinematic_unsupported')
    run('dynamic authored conflict', field('motion_owner','authored'), code='role_owner')
    run('collision opt out contradicts role', field('collides',False), code='role_owner')
    run('boolean body id', field('body_id',True))
    run('fractional body id', field('body_id',2.5))
    run('zero body id', field('body_id',0), code='role_owner')
    run('duplicate body id', field('body_id',100), code='duplicate_body')
    run('missing target', field('object_id','missing'), code='missing_object')
    run('duplicate target binding', field('object_id','pillar_left'), code='duplicate_binding')
    run('duplicate object identity', lambda s,a:s['objects'].append(copy.deepcopy(s['objects'][0])), code='duplicate_object')
    run('duplicate catalog identity', lambda s,a:a['assets'].append(copy.deepcopy(a['assets'][0])), code='duplicate_asset')
    run('missing asset', field('collision_asset_id','missing'), code='missing_asset')
    for key in ['source_sha256','collision_sha256','mesh_sha256']:
        run('mutated '+key, field(key,'0'*64), code='asset_mismatch')
    run('wrong source identity', field('source_asset_id','another'), code='asset_mismatch')
    run('wrong mesh identity', field('mesh_asset_id','another'), code='asset_mismatch')
    run('source center mismatch', field('source_center_m',[0,0,0]), code='mass_frame_mismatch')
    run('nonprincipal basis', field('principal_to_source',[1,.1,0,0,1,0,0,0,1]), code='mass_frame_mismatch')
    run('reflected asset basis', lambda s,a:a['assets'][2]['principal_to_source'].__setitem__(0,-1), code='asset_metadata')
    run('source unit inertia invalid', lambda s,a:a['assets'][2]['inertia_per_kg'].__setitem__(0,0), code='inertia')
    run('dynamic triangles refused', lambda s,a:a['assets'][2].update(shape='static_triangles'), code='dynamic_shape_unsupported')
    for mass in [0,-1]:
        run('invalid mass '+str(mass), field('mass_kg',mass), code='inertia')
    run('inertia mismatch', field('principal_inertia_kgm2',[1,1,1]), code='inertia')
    run('tiny mass not hidden by absolute tolerance', lambda s,a:body(s).update(mass_kg=1e-200,principal_inertia_kgm2=[1e-12]*3), code='inertia')
    run('zero quaternion', field('orientation_wxyz',[0,0,0,0]), code='quaternion')
    run('unnormalized quaternion', field('orientation_wxyz',[2,0,0,0]), code='quaternion')
    run('overflow norm quaternion', field('orientation_wxyz',[1e200]*4), code='quaternion')
    run('friction outside initial model', field('friction',1.5), code='material_mass')
    run('restitution outside initial model', field('restitution',-1), code='material_mass')
    run('static velocity', lambda s,a:s['extensions']['rigid_scene']['bindings'][0].update(linear_velocity_mps=[1,0,0]), code='static_state')
    run('static response mass', lambda s,a:s['extensions']['rigid_scene']['bindings'][0].update(mass_kg=1), code='static_state')
    run('hierarchy relation cannot bypass parent check', lambda s,a:s.update(hierarchy=[dict(parent_id='floor',child_id='tumbling_body')]))
    run('constraints cannot bypass motion ownership', lambda s,a:s.update(constraints=[dict(target_id='tumbling_body')]))
    run('physical parent', lambda s,a:s['objects'][3].update(parent_id='floor'), code='unsupported_transform')
    run('physical Euler rotation', lambda s,a:s['objects'][3]['transform']['rotation'].update(z=10), code='unsupported_transform')
    run('nonuniform scale', lambda s,a:s['objects'][3]['transform']['scale'].update(x=2), code='unsupported_transform')
    run('near unit scale still requires baking', lambda s,a:s['objects'][3]['transform']['scale'].update(x=1+1e-12), code='unsupported_transform')
    run('child capacity', lambda s,a:a['assets'][2].update(child_count=65), code='shape_capacity')
    def nine(s,a):
        for i in range(5):
            o=copy.deepcopy(s['objects'][0]);o['object_id']='extra_'+str(i);s['objects'].append(o)
            b=copy.deepcopy(s['extensions']['rigid_scene']['bindings'][0]);b.update(object_id=o['object_id'],body_id=200+i);s['extensions']['rigid_scene']['bindings'].append(b)
    run('nine bodies eight body consumer', nine, code='body_capacity')
    for key,value in [('origin_tick',1),('rate_numerator',0),('rate_denominator',0),('end_tick',0),('end_tick',2**32),('rate_numerator',True),('gravity_mps2',[0,0]),('schema','future'),('contact_policy','unknown')]:
        run('world '+key+' '+str(value), lambda s,a,k=key,v=value:s['extensions']['rigid_scene'].__setitem__(k,v))
    run('unknown physics field', lambda s,a:s['extensions']['rigid_scene'].update(sleep=True))
    run('unknown binding field', field('teleport',True))
    run('bad world scale', lambda s,a:s.update(world_scale=2))
    run('bad coordinate convention', lambda s,a:s['extensions']['rigid_scene'].update(coordinate_system='y_up'))
    def path(s,a,enabled=True,static=False):
        s['extensions']['ray_tracing']={'authoring':{'motion_paths':{'bindings':[{'object_id':'floor' if static else 'tumbling_body','enabled':enabled}]}}}
    run('dynamic authored path', path, code='motion_conflict')
    run('static authored path', lambda s,a:path(s,a,static=True), code='motion_conflict')
    run('disabled path has no ownership', lambda s,a:path(s,a,enabled=False), ok=True)
    run('nonboolean path enable refuses', lambda s,a:path(s,a,enabled=0))
    run('legacy motion track conflict', lambda s,a:s['extensions'].update(ray_tracing={'authoring':{'object_motion_tracks':[{'object_id':'tumbling_body'}]}}), code='motion_conflict')
    run('timeline rotation conflict', lambda s,a:s['extensions'].update(ray_tracing={'authoring':{'scene_timeline':{'tracks':[{'target_id':'object/tumbling_body','property_id':'object/transform/rotation_z','enabled':True}]}}}), code='motion_conflict')
    run('timeline path progress conflict', lambda s,a:s['extensions'].update(ray_tracing={'authoring':{'scene_timeline':{'tracks':[{'target_id':'object/tumbling_body','property_id':'object/path_progress'}]}}}), code='motion_conflict')
    run('other simulation conflict', lambda s,a:s['extensions'].update(ball_bounce_sim={'bodies':[{'object_id':'tumbling_body'}]}), code='motion_conflict')
    run('camera and light paths independent', lambda s,a:s['extensions'].update(ray_tracing={'authoring':{'motion_paths':{'bindings':[{'target_id':'camera/main'},{'target_id':'light/key'}]},'scene_timeline':{'tracks':[{'target_id':'camera/main','property_id':'camera/fov_y'},{'target_id':'light/key','property_id':'light/intensity'}]}}}), ok=True)
    def visual(s,a):
        o=copy.deepcopy(s['objects'][0]);o.update(object_id='decor');s['objects'].append(o)
        s['extensions']['rigid_scene']['bindings'].append(dict(object_id='decor',body_id=0,role='visual_only',motion_owner='authored',collides=False))
    run('visual only no collision', visual, ok=True)
    def visual_conflict(s,a):
        visual(s,a);s['extensions']['ray_tracing']={'authoring':{'object_motion_tracks':[{'object_id':'decor'}],'scene_timeline':{'tracks':[{'target_id':'object/decor','property_id':'object/path_progress'}]}}}
    run('visual duplicate transform owners', visual_conflict, code='motion_conflict')
    raw=json.dumps(base).encode()
    run('duplicate JSON key', raw=raw.replace(b'"world_scale": 1',b'"world_scale": 1, "world_scale": 1'))
    run('escaped duplicate JSON key', raw=raw.replace(b'"world_scale": 1',b'"world_scale": 1, "world_\\u0073cale": 1'))
    run('escaped NUL', raw=raw.replace(b'tumbling_body',b'tumbling_body\\u0000hidden'))
    run('literal NUL', raw=raw+b'\0')
    run('trailing content', raw=raw+b' false')
    run('nonfinite number', raw=raw.replace(b'9.81',b'1e9999'))
    run('NaN token', raw=raw.replace(b'9.81',b'NaN'))
    run('document byte capacity', raw=b' '*(1024*1024+1))
    (args.out/'proof.json').write_text(json.dumps(dict(status='PASS',cases=cases,tools=[str(p.resolve()) for p in args.tool]),indent=2)+'\n')
    print(json.dumps(dict(status='PASS',checks=len(cases),tools=len(args.tool))))


if __name__=='__main__':
    main()
