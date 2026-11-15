"""Saved V3 smooth dielectric and disjoint caustic transport regression."""
import argparse
import copy
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools'))
from spectral_room_testbench import material, plane, scene_root, v
from spectral_response_fixture import cie
from spectral_components import verify, read as read_components


def qualify(root, cli, *, package_config=None):
    asset = 'asset_sphere_64x32'
    path = Path(f'assets/mesh_assets/{asset}.runtime.json')
    (root / path.parent).mkdir(parents=True)
    shutil.copy2(REPO / 'tests/fixtures/mesh_asset_runtime_spheres' / path, root / path)
    if package_config is None:
        (root / 'config').symlink_to(REPO / 'config', target_is_directory=True)
    else:
        shutil.copytree(package_config, root / 'config')
    sphere = {
        'object_id': 'lens', 'object_type': 'mesh_asset_instance', 'dimensional_mode': 'full_3d',
        'space_mode_intent': '3d',
        'transform': {'position': v(0,0,.28), 'rotation': v(0,0,0), 'scale': v(.183,.183,.183)},
        'geometry_ref': {'kind': 'mesh_asset', 'id': asset, 'variant': 'runtime_default'},
        'extensions': {'line_drawing': {'runtime_mesh_path': str(path)}},
        'flags': {'visible': True, 'locked': False, 'selectable': True}}
    scene = scene_root([
        plane('floor',(0,.14,0),(1,0,0),(0,1,0),(0,0,1),1.7,1.8),
        plane('wall',(0,.435,.37),(1,0,0),(0,0,1),(0,-1,0),1.55,.74),
        sphere])
    settings = {
        'samples': 48, 'max_depth': 10, 'seed': 19,
        'materials': [material('floor',.72),material('wall',.48),
                      {'object_id':'lens','model':'smooth_glass'}],
        'light': {'type':'area','origin_m':[-.17,-.18,.88],'axis_u':[1,0,0],
                  'axis_v':[0,-1,0],'width_m':.19,'height_m':.25,
                  'spectrum':{'lines':[[450,1],[550,1],[650,1]]}}}
    scene['extensions'] = {'ray_tracing': {'spectral_transport_v1': {
        'quality_profile': 'surface_path_v1', 'surface_path_v1': settings}}}

    def run(name, configure=None, valid=True, view=None):
        document = copy.deepcopy(scene)
        p = document['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
        if configure:
            configure(document,p)
        (root/f'{name}.scene.json').write_text(json.dumps(document))
        q = {'schema_version':'ray_tracing_agent_render_request_v2','run_id':name,
             'scene':{'runtime_scene_path':f'{name}.scene.json'},
             'render':{'integrator_3d':'disney_v3_spectral','width':64,'height':48},
             'inspection':{'camera_position':v(.52,-1.28,.51),'camera_look_at':v(0,.06,.225),'camera_zoom':2},
             'output':{'root':name,'overwrite':False}}
        if view:
            q['inspection'].update(view)
        req = root/f'{name}.request.json'
        req.write_text(json.dumps(q))
        summary = root/f'{name}.summary.json'
        proc = subprocess.run([str(cli),'--request',str(req),'--render','--summary',str(summary),
                               '--summary-file-only'], cwd=root, capture_output=True, timeout=90)
        assert (proc.returncode == 0) == valid, (proc.stderr,summary.read_text() if summary.exists() else '')
        if not valid:
            assert not (root/name/'frames/frame_0000.bmp').exists()
            return
        report = json.loads(summary.read_text())['spectral_transport']
        image = root/name/'frames/frame_0000.bmp'
        if p.get('linear_components',True):
            verify(report,image)
        else:
            assert not report['linear_components']['written']
            assert not Path(str(image)+'.components.npy').exists()
        return report

    full = run('default')
    g = full['smooth_glass']
    f = full['film_components_xyz']
    assert full['route'] == 'surface_path_v1' and full['photon_maps_used']
    assert g['launched'] == 65536 and g['deposits'] > 0 and g['radius_m'] > 0
    assert g['camera_transmissions'] > 0 and g['camera_source_hits_owned_by_map'] > 0
    assert f['caustic'][1] > 0 and f['caustic_bounce'][1] > 0
    for b in g['bands']:
        assert math.isclose(b['launched_w'], sum(b[k] for k in (
            'all_receiver_w','escaped_w','glass_absorbed_w','depth_limited_w','optical_loss_w')),
            rel_tol=1e-9, abs_tol=1e-11)
    if package_config is not None:
        print('PASS staged worker launcher ordinary V3 glass, photon map and verified raw XYZ film')
        return
    disabled = run('export_disabled', lambda _,p:p.update(linear_components=False))
    assert (root/'default/frames/frame_0000.bmp').read_bytes() == (root/'export_disabled/frames/frame_0000.bmp').read_bytes()
    assert full['film_components_xyz'] == disabled['film_components_xyz']
    assert all(group['xyz']==[0,0,0] for group in full['scattering_orders']['groups'])
    run('invalid_export',lambda _,p:p.update(linear_components='false'),valid=False)
    pure = run('camera_only', lambda _,p: p.update(caustics={'passes':0}))
    assert not pure['photon_maps_used'] and pure['smooth_glass']['launched'] == 0
    for i,x in enumerate(pure['film_components_xyz']['total']):
        expected = sum(f[k][i] for k in ('direct','surface_indirect','volume')) + g['camera_replaced_source_xyz'][i]
        assert math.isclose(x, expected, rel_tol=2e-10, abs_tol=1e-10)
    black = run('black_receiver', lambda _,p: p['materials'][0].update(reflectance=[[380,0],[780,0]]))
    assert black['smooth_glass']['map_key'] == g['map_key']
    assert black['film_components_xyz']['caustic_bounce'][1] < .1*f['caustic_bounce'][1]

    guided = run('guided', lambda _,p: p.update(caustics={'photon_launch':'glass_guided'}))
    split = run('split', lambda _,p: p.update(caustics={'camera_fresnel':'split_first'},
                path_trace={'x':32,'y':24,'sample':0}))
    both = run('both', lambda _,p: p.update(caustics={'photon_launch':'glass_guided','camera_fresnel':'split_first'}))
    base_map=full['smooth_glass']
    guided_map=guided['smooth_glass']
    split_map=split['smooth_glass']
    both_map=both['smooth_glass']
    assert base_map['photon_launch']=='cosine' and base_map['camera_fresnel']=='roulette'
    assert guided_map['photon_launch']=='glass_guided' and guided_map['launch_power_measure']=='importance_weighted_estimate'
    assert split_map['camera_fresnel']=='split_first' and both_map['camera_fresnel']=='split_first'
    assert base_map['map_key']==split_map['map_key'] and base_map['bands']==split_map['bands']
    assert guided_map['map_key']==both_map['map_key'] and guided_map['bands']==both_map['bands']
    assert base_map['map_key']!=guided_map['map_key']
    assert guided_map['deposits']>2*base_map['deposits']
    for key in ('launched_w','deposited_w'):
        baseline_power=sum(b[key] for b in base_map['bands'])
        guided_power=sum(b[key] for b in guided_map['bands'])
        assert math.isclose(guided_power,baseline_power,rel_tol=.05)
    for result in (full,guided,split,both):
        a=result['smooth_glass']
        assert a['camera_primary_paths']==64*48*48
        assert a['camera_primary_paths']+a['camera_first_interface_splits']==result['termination']['camera_paths']
        assert result['termination']['all_started_paths_classified']
    assert split_map['camera_first_interface_splits']>0 and both_map['camera_first_interface_splits']>0
    assert base_map['camera_first_interface_splits']==0 and guided_map['camera_first_interface_splits']==0
    branches={e['reason']:e for e in split['path_trace']['events'] if e['reason'] in (
        'split_reflection','split_transmission')}
    assert set(branches)=={'split_reflection','split_transmission'}
    r,t=(branches[k] for k in ('split_reflection','split_transmission'))
    assert math.isclose(r['beta_out'],r['reflectance'],rel_tol=1e-12)
    assert math.isclose(r['continuation_pdf']+t['continuation_pdf'],1,abs_tol=1e-12)
    assert r['vacuum_nm']==t['vacuum_nm']
    run('bad_launch',lambda _,p: p.update(caustics={'photon_launch':'guided'}),valid=False)
    run('bad_fresnel',lambda _,p: p.update(caustics={'camera_fresnel':True}),valid=False)
    print('PASS saved glass guidance, exact map identity for camera splitting, branch weights and work accounting')

    def open_lens(doc,p):
        doc['objects'][-1] = plane('lens',(0,0,.4),(1,0,0),(0,1,0),(0,0,1),1,1)
    run('open_lens',open_lens,valid=False)
    run('over_budget',lambda _,p: p.update(caustics={'passes':64,'photons_per_pass':262144}),valid=False)
    print('PASS saved V3 glass defaults, area photon ledger, receiver bounce and exactly-once path handoff')

    def fog_box(absorption, scattering, lo=(-.5,-.5,0), hi=(.5,.5,.95)):
        return {'bounds_min_m':lo,'bounds_max_m':hi,'sigma_a_per_m':absorption,
                'sigma_s_per_m':scattering,'g':.2}

    def constant(x):
        return [[380,x],[780,x]]

    def scattering_setup(doc, p):
        p['medium'] = fog_box(constant(.04),constant(.65))
        p['caustics'] = {'photon_launch':'glass_guided','camera_fresnel':'split_first'}
        p['probes'] = [{'name':'orb','x':32,'y':24}]
    observed_orders = set()
    for trace_seed in (19,43,79,123,307,509,1237,104729):
        def order_setup(doc,p):
            p.update(samples=1,seed=trace_seed,caustics={'passes':0},
                medium=fog_box(constant(.04),constant(3)),
                path_trace={'x':32,'y':24,'sample':0})
        name = f'order_trace_{trace_seed}'
        traced = run(name,order_setup)
        events = traced['path_trace']['events']
        assert not traced['path_trace']['truncated']
        first = events[0]
        color = [v*first['wavelength_weight'] for v in cie(first['vacuum_nm'])]
        scalar = [0.]*4
        for event in events:
            if event['component'] == 'volume' and event['scalar_contribution']:
                order = event['volume_events']; assert order > 0
                group = 0 if order == 1 else 1 if order == 2 else 2 if order <= 4 else 3
                scalar[group] += event['scalar_contribution']; observed_orders.add(order)
        width,height,data = read_components(root/name/'frames/frame_0000.bmp.components.npy')
        offset = (24*width+32)*33
        for group in range(4):
            for c in range(3):
                expected = scalar[group]*color[c]
                assert math.isclose(expected,data[offset+(7+group)*3+c],rel_tol=1e-10,abs_tol=1e-12)
    assert 1 in observed_orders and any(order>1 for order in observed_orders),observed_orders
    print('PASS independent trace-event scattering-order reconstruction',sorted(observed_orders))
    zero = run('zero_fog', lambda _,p: p.update(medium=fog_box(constant(0),constant(0))))
    assert (root/'zero_fog/frames/frame_0000.bmp').read_bytes() == (root/'default/frames/frame_0000.bmp').read_bytes()
    assert zero['smooth_glass']['ballistic_launch_key'] == g['map_key']
    assert zero['smooth_glass']['map_key'] == g['map_key']
    assert zero['film_components_xyz']['volume_caustic_subset'] == [0,0,0]
    assert all(zero['film_components_xyz'][k] == value for k,value in f.items())

    def axial_setup(doc, p):
        doc['objects'][0] = plane('floor',(0,0,0),(1,0,0),(0,1,0),(0,0,1),.03,.03)
        doc['objects'].pop(1)
        p['materials'].pop(1)
        p['materials'][-1].update(glass='constant',ior=1.0001)
        p['light'].update(origin_m=[0,0,.88],width_m=.02,height_m=.02)
        p['caustics'] = {'passes':16,'photons_per_pass':16384,'photon_launch':'glass_guided'}
        p['samples'] = 8

    def axial_absorption(doc,p):
        axial_setup(doc,p)
        p['medium'] = fog_box([[380,.6],[450,.6],[550,1.3],[650,2.1],[780,2.1]],
                              constant(0),(-1,-1,.62),(1,1,.82))

    reference = run('axial_dry',axial_setup)['smooth_glass']
    attenuated = run('axial_absorption',axial_absorption)['smooth_glass']
    assert reference['map_key'] == attenuated['ballistic_launch_key']
    assert reference['map_key'] != attenuated['map_key']
    # Nearly index-matched glass (IOR=1.0001) and small axial apertures
    # bound the slab chord to 0.200–0.201 m for this control.
    chord_max = .201
    ratios = []
    for dry,wet,sigma in zip(reference['bands'],attenuated['bands'],(.6,1.3,2.1),strict=True):
        assert dry['deposits'] == wet['deposits'] > 30
        ratio = wet['deposited_w']/dry['deposited_w']
        ratios.append(ratio)
        assert math.exp(-sigma*chord_max)-1e-12 <= ratio <= math.exp(-sigma*.2)+1e-12
        assert wet['medium_absorbed_w'] > 0
        assert wet['medium_scattered_w'] == wet['volume_caustic_scattered_w'] == 0

    def inside_solid(doc,p):
        scattering_setup(doc,p)
        p['medium'] = fog_box(constant(3),constant(2),(-.01,-.01,.27),(.01,.01,.29))
    enclosed = run('box_inside_glass',inside_solid)
    assert (root/'box_inside_glass/frames/frame_0000.bmp').read_bytes() == (root/'both/frames/frame_0000.bmp').read_bytes()
    assert enclosed['film_components_xyz']['volume_caustic_subset'] == [0,0,0]
    assert all(b['medium_absorbed_w']==b['medium_scattered_w']==0 for b in enclosed['smooth_glass']['bands'])
    print('PASS exterior fog does not enter the solid dielectric')

    fog = run('scatter_box',scattering_setup)
    optical = fog['smooth_glass']
    film = fog['film_components_xyz']
    assert optical['ballistic_launch_key'] == both_map['map_key']
    assert optical['map_key'] != both_map['map_key']
    assert optical['deposits'] == both_map['deposits']
    assert 0 < film['volume_caustic_subset'][1] < film['volume'][1]
    assert 0 < film['caustic'][1] < both['film_components_xyz']['caustic'][1]
    assert film['caustic_bounce'][1] > 0
    assert fog['termination']['all_started_paths_classified']
    for band in optical['bands']:
        assert band['volume_caustic_deposits'] > 0
        assert 0 < band['volume_caustic_scattered_w'] < band['medium_scattered_w']
        assert math.isclose(band['launched_w'],sum(band[key] for key in (
            'all_receiver_w','escaped_w','glass_absorbed_w','depth_limited_w','optical_loss_w',
            'medium_absorbed_w','medium_scattered_w')),rel_tol=1e-10,abs_tol=1e-11)
    for i,channel in enumerate(film['total']):
        assert math.isclose(channel,sum(film[key][i] for key in (
            'direct','surface_indirect','volume','caustic','caustic_bounce')),rel_tol=1e-12)

    def component_setup(doc,p):
        scattering_setup(doc,p)
        p['output_component'] = 'volume_caustic'
    component = run('volume_caustic_only',component_setup)
    assert component['film_components_xyz'] == film and component['smooth_glass'] == optical
    assert (root/'volume_caustic_only/frames/frame_0000.bmp').read_bytes() != (root/'scatter_box/frames/frame_0000.bmp').read_bytes()
    run('bad_fog_cell',lambda _,p: p.update(medium=dict(
        fog_box(constant(0),constant(1)),caustic_cell_m=0)),valid=False)
    print('PASS dry/zero fog replay, three Beer-Lambert slab bands, fog energy, source ownership and disjoint outputs',ratios)

    # An almost index-matched lens shadows the small fog box from direct
    # next-event sampling. For the centered, side-looking probe, compare
    # the resulting volume caustic with independent single-scatter quadrature.
    def axial_scatter(doc,p):
        axial_setup(doc,p)
        p['materials'][0]['reflectance'] = constant(0)
        p['light'].update(width_m=.06,height_m=.06,spectrum={'lines':[[550,1]]})
        p['medium'] = fog_box(constant(.05),constant(.25),(-.12,-.12,.02),(.12,.12,.10))
        p['medium']['g'] = 0
        p['probes'] = [{'name':'axis','x':32,'y':24}]
        p['samples'] = 96
    view = {'camera_position':v(0,-.8,.06),'camera_look_at':v(0,0,.06),'camera_zoom':8}
    run('axial_scatter',axial_scatter,view=view)
    summary = json.loads((root/'axial_scatter.summary.json').read_text())
    measured = summary['spectral_surface_path']['probes'][0]['volume_caustic_xyz'][1] / cie(550)[1]
    # Rectangle solid-angle quadrature along a 24 cm camera chord. Every
    # light ray enters the fog through its top plane 4 cm above the probe.
    expected = 0.
    for iy in range(160):
        y = -.12 + .24*(iy+.5)/160
        for iu in range(16):
            u = -.03 + .06*(iu+.5)/16
            for iv in range(16):
                v_light = -.03 + .06*(iv+.5)/16
                distance = math.sqrt(.82**2 + u*u + (v_light-y)**2)
                tr = math.exp(-.30*(y+.12 + .04*distance/.82))
                expected += tr * .25/(4*math.pi) * .82/distance**3
    expected *= .24/160 * (.06/16)**2
    assert abs(measured/expected-1) < .18, (measured,expected)
    print('PASS mapped fog single-scatter quadrature',measured,expected,measured/expected)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cli', required=True, type=Path)
    parser.add_argument('--package-config', type=Path,
                        help='Run the packaged foreground V3 smoke with this config directory')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='optic-ordinary-glass-') as directory:
        qualify(Path(directory), args.cli.resolve(),
                package_config=args.package_config.resolve() if args.package_config else None)
