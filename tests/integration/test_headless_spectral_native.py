#!/usr/bin/env python3
"""Ordinary V3 route, saved-profile replay and observational diagnostics gates."""
import argparse
import copy
import json
import math
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import spectral_room_testbench as bench


def run(root, cli, name, request, scene, *, render=True, valid=True):
    d = root / name
    d.mkdir()
    (d / 'config').symlink_to(bench.REPO / 'config', target_is_directory=True)
    q = copy.deepcopy(request)
    q['scene'] = {'runtime_scene_path': 'scene.json'}
    q['output'] = {'root': 'output', 'overwrite': False}
    (d / 'scene.json').write_text(json.dumps(scene))
    (d / 'request.json').write_text(json.dumps(q))
    result = subprocess.run([str(cli), '--request', 'request.json',
        *(['--render'] if render else []), '--summary', 'summary.json', '--summary-file-only'],
        cwd=d, capture_output=True, text=True, timeout=90)
    (d / 'log.txt').write_text(result.stdout + result.stderr)
    assert (result.returncode == 0) == valid, (name, result.returncode, result.stdout, result.stderr)
    if not valid:
        return None, None
    summary = json.loads((d / 'summary.json').read_text())
    pixels = (d / 'output/frames/frame_0000.bmp').read_bytes() if render else None
    return summary, pixels


def save_profile(scene, profile):
    sc = copy.deepcopy(scene)
    sc.setdefault('extensions', {}).setdefault('ray_tracing', {})['spectral_transport_v1'] = profile
    return sc


def qualify(root, cli):
    q, sc = bench.room()
    # Primitive-only fixture removes asset-path changes from the routing proof.
    sc['objects'] = sc['objects'][:6]
    p = q['spectral']['surface_path_v1']
    p.update(samples=16, max_depth=4, probes=[{'name': 'center', 'x': 16, 'y': 12}])
    p['materials'] = p['materials'][:6]
    q['render'].update(width=32, height=24)
    explicit, image = run(root, cli, 'explicit', q, sc)
    saved_q = copy.deepcopy(q)
    profile = saved_q.pop('spectral')
    saved_sc = save_profile(sc, profile)
    saved, saved_image = run(root, cli, 'saved', saved_q, saved_sc)
    assert image == saved_image
    assert explicit['spectral_transport'] == saved['spectral_transport']
    assert explicit['spectral_surface_path'] == saved['spectral_surface_path']
    pre, _ = run(root, cli, 'preflight', saved_q, saved_sc, render=False)
    assert pre['spectral_transport']['status'] == 'prepared'
    assert saved['spectral_transport']['status'] == 'completed'
    assert pre['spectral_transport']['termination']['camera_paths'] == 0

    traced_q = copy.deepcopy(q)
    traced_q['spectral']['surface_path_v1']['path_trace'] = {'x': 16, 'y': 12, 'sample': 0}
    traced, traced_image = run(root, cli, 'trace', traced_q, sc)
    assert traced_image == image
    assert traced['spectral_surface_path'] == explicit['spectral_surface_path']
    tr = traced['spectral_transport']['path_trace']
    events = tr['events']
    assert events and not tr['truncated'] and events[0]['event'] == 'camera'
    nm = events[0]['vacuum_nm']
    assert all(e['vacuum_nm'] == nm for e in events)
    for e in events:
        if e['event'] == 'surface':
            assert math.isclose(e['beta_out'], e['beta_in'] * e['continuation_weight'], rel_tol=1e-12)
        if e['event'] == 'light_sample' and e['reason'] == 'visible':
            replay = e['beta_in'] * e['transmittance'] * e['bsdf_f_cos_or_phase'] / e['light_pdf'] * e['mis_weight'] * e['source_weight']
            assert math.isclose(replay, e['scalar_contribution'], rel_tol=1e-12, abs_tol=1e-15)
    for result in (explicit, saved, traced):
        report = result['spectral_transport']
        counts = report['termination']
        assert counts['all_started_paths_classified']
        assert sum(v for k, v in counts.items() if k not in ('camera_paths', 'measure', 'all_started_paths_classified')) == counts['camera_paths']
        c = report['film_components_xyz']
        assert c['committed_pixels'] == 32 * 24
        for i in range(3):
            assert math.isclose(sum(c[k][i] for k in ('direct', 'surface_indirect', 'volume')), c['total'][i], rel_tol=1e-14)

    fog_q = copy.deepcopy(traced_q)
    fog_q['spectral']['surface_path_v1']['medium'] = {
        'bounds_min_m': [-.99, .01, .001], 'bounds_max_m': [.99, 1.99, 1.99],
        'sigma_a_per_m': [[380, .015], [780, .015]],
        'sigma_s_per_m': [[380, 2], [780, 2]], 'g': .25}
    fog, fog_image = run(root, cli, 'fog-trace', fog_q, sc)
    for event in fog['spectral_transport']['path_trace']['events']:
        if event['event'] == 'light_sample':
            assert event['object_or_source_id'] == 'area'
    fog_q['spectral']['surface_path_v1'].pop('path_trace')
    fog_plain, fog_plain_image = run(root, cli, 'fog-no-trace', fog_q, sc)
    assert fog_image == fog_plain_image
    assert fog['spectral_surface_path'] == fog_plain['spectral_surface_path']
    assert fog['spectral_transport']['termination']['all_started_paths_classified']

    # Unprofiled old scenes and an explicit pinned default must be identical.
    tiny = copy.deepcopy(saved_q)
    tiny['render'].update(width=4, height=3)
    absent, a = run(root, cli, 'native-default', tiny, sc)
    pinned, b = run(root, cli, 'native-pinned', tiny, save_profile(sc, {'quality_profile': 'surface_path_v1'}))
    assert a == b and absent['spectral_transport'] == pinned['spectral_transport']
    native = absent['spectral_transport']
    assert native['material_representation'] == 'native_rgb_basis_approximation'
    assert native['sources'] and native['sources'][0]['representation'] == 'native_rgb_basis_approximation'
    assert native['quality']['samples_per_pixel'] == 512 and native['quality']['max_surface_interactions'] == 12
    assert native['film_components_xyz']['surface_indirect'][1] > 0
    legacy, _ = run(root, cli, 'legacy', tiny, save_profile(sc, {'quality_profile': 'legacy_direct_v1'}))
    assert legacy['spectral_transport']['route'] == 'legacy_direct_v1'
    assert legacy['spectral_transport']['explicit_legacy_optout'] and not legacy['spectral_transport']['ordinary_indirect']

    # Scene-native incident adapter remains linear in source power (same paths).
    native_q = copy.deepcopy(q)
    native_q['spectral']['surface_path_v1'].pop('light')
    sc_light = copy.deepcopy(sc)
    sc_light['lights'] = [{'light_id': 'key', 'kind': 'point', 'position': bench.v(0, .75, 1.8),
        'color': {'r': 1, 'g': .8, 'b': .6}, 'intensity': 2, 'falloff_distance': 8, 'enabled': True}]
    once, _ = run(root, cli, 'native-source', native_q, sc_light)
    sc_light['lights'][0]['intensity'] = 4
    twice, _ = run(root, cli, 'native-source-double', native_q, sc_light)
    for key in ('direct', 'surface_indirect', 'volume', 'total'):
        for a, b in zip(once['spectral_transport']['film_components_xyz'][key], twice['spectral_transport']['film_components_xyz'][key]):
            assert math.isclose(2*a, b, rel_tol=1e-12, abs_tol=1e-14), (key, a, b)
    assert once['spectral_transport']['sources'][0]['id'] == 'key'

    # Uniform sky is a passive furnace: an opaque normalized native mixture
    # cannot return more spectral radiance than it receives. This exercises the
    # RGB material adapter while source energy stays a known spectral line.
    furnace_scene = bench.scene_root([bench.plane('receiver', (0, 0, 0),
        (0, 1, 0), (0, 0, 1), (1, 0, 0), 2, 2)])
    furnace_q = {'schema_version': 'ray_tracing_agent_render_request_v2',
        'render': {'integrator_3d': 'disney_v3_spectral', 'width': 1, 'height': 1},
        'inspection': {'camera_position': bench.v(.35, 0, 0),
            'camera_look_at': bench.v(0, 0, 0), 'camera_zoom': 1000},
        'spectral': {'surface_path_v1': {'samples': 8192, 'max_depth': 1,
            'probes': [{'name': 'receiver', 'x': 0, 'y': 0}],
            'light': {'type': 'sky_cap', 'axis': [1, 0, 0], 'half_angle_deg': 180,
                'spectrum': {'lines': [[530, 1]]}}}}}
    furnace, _ = run(root, cli, 'native-material-furnace', furnace_q, furnace_scene)
    import test_headless_spectral_surface_path as physical
    y = furnace['spectral_surface_path']['probes'][0]['total_xyz'][1] / physical.cie(530)[1]
    assert 0 < y <= 1.02, y

    # Point-source distance response must follow the existing native legacy
    # radiometry (the declared fixture config uses quadratic falloff), rather
    # than silently claiming inverse-square point watts.
    point_q = copy.deepcopy(furnace_q)
    point_q['spectral']['surface_path_v1'].pop('light')
    point_q['spectral']['surface_path_v1']['materials'] = [bench.material('receiver', .8)]
    point_scene = copy.deepcopy(furnace_scene)
    point_scene['lights'] = [{'light_id': 'point', 'kind': 'point', 'position': bench.v(1, 0, 0),
        'color': {'r': 1, 'g': 1, 'b': 1}, 'intensity': 1, 'falloff_distance': 1, 'enabled': True}]
    near, _ = run(root, cli, 'point-distance-one', point_q, point_scene)
    point_scene['lights'][0]['position']['x'] = 2
    far, _ = run(root, cli, 'point-distance-two', point_q, point_scene)
    n = near['spectral_surface_path']['probes'][0]['total_xyz'][1]
    f = far['spectral_surface_path']['probes'][0]['total_xyz'][1]
    assert math.isclose(n/f, 2.5, rel_tol=2e-6), n/f

    for name, mutate in (
        ('null-profile', lambda x: x['spectral'].update(quality_profile=None)),
        ('null-light', lambda x: x['spectral']['surface_path_v1'].update(light=None)),
        ('missing-material', lambda x: x['spectral']['surface_path_v1']['materials'].pop()),
        ('probe-outside-film', lambda x: x['spectral']['surface_path_v1'].update(path_trace={'x': 32, 'y': 0, 'sample': 0})),
    ):
        invalid = copy.deepcopy(q)
        mutate(invalid)
        run(root, cli, name, invalid, sc, valid=False)
    print('PASS ordinary V3 defaults, saved replay, native source scaling, terminal accounting, trace replay and strict admission')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cli', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='optic-native-spectral-') as scratch:
        root = (args.output or Path(scratch)).resolve()
        root.mkdir(parents=True, exist_ok=True)
        qualify(root, args.cli.resolve())
