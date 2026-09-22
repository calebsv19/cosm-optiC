#!/usr/bin/env python3
"""T4 production integrator wiring: visible source detail, transport ledger, route parity.

This complements (does not replace) the float receiver supersampling budgets.
Image metrics here are normalized display-encoded RGB integration-effect checks.
"""
import argparse
import copy
import hashlib
import json
import math
import os
import platform
import subprocess
import sys
from pathlib import Path
from test_secondary_footprint_t4 import fixture, ROI, WIDTH, HEIGHT
from test_surface_composition_t3 import write
from test_surface_mapping_m5 import png, channel

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from smooth_mesh_reflection.prepare_reflection_matrix import build_request
from vf3d_initial_state_preset_tool import read_bmp, write_rgb_png


def run(renderer, scene, root, route, kind, env, workers):
    root.mkdir(parents=True)
    request = build_request(root, 'direct', route)
    request['scene']['runtime_scene_path'] = str(scene)
    # The synchronized ledger must preserve exact accounting with parallel workers.
    request['resources'] = {'max_workers': workers}
    request['render'].update(width=WIDTH, height=HEIGHT, temporal_frames=1, denoise_enabled=False)
    request['inspection'].update(camera_position={'x': .013123, 'y': -.180629, 'z': 1},
        camera_look_at={'x': .013123, 'y': .019371, 'z': 0}, camera_zoom=1.5,
        ambient_strength=.12 if kind == 'mirror' else .3,
        top_fill_strength=.08 if kind == 'mirror' else .2,
        light_intensity=2 if kind == 'mirror' else 8, light_radius=.1,
        secondary_diffuse_samples_3d=1, transmission_samples_3d=1,
        render_trace_cost_ledger_enabled=True)
    path = root / 'request.json'; write(path, request)
    for mode in ('--preflight', '--render'):
        with (root / (mode[2:] + '.log')).open('w') as log:
            result = subprocess.run([str(renderer), '--request', str(path), mode], env=env,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=300)
        assert result.returncode == 0, (kind, route, mode, root)
    summary = json.loads(Path(request['progress']['summary_path']).read_text())
    assert summary['frames_rendered'] == 1
    frames = list((Path(request['output']['root']) / 'frames').glob('*.bmp')); assert len(frames) == 1
    width, height, pixels = read_bmp(frames[0]); assert (width, height) == (WIDTH, HEIGHT)
    write_rgb_png(root / 'frame.png', width, height, pixels)
    ledger = summary['render_trace_cost_ledger']; assert ledger['enabled']
    counts = ledger['ray_class_counts']; stats = summary['render_stats']
    transport = {'primary_rays': counts['primary'], 'reflection_rays': counts['reflection_specular'],
                 'transmission_rays': counts['transmission']}
    transport['primary_diagnostic_coverage'] = counts['primary'] / (WIDTH * HEIGHT)
    transport['requested_max_workers'] = workers
    assert counts['primary'] == WIDTH * HEIGHT
    assert stats['temporal_pixels_rendered'] >= WIDTH * HEIGHT
    if kind == 'mirror':
        transport['geometry_reflection_pixels'] = stats['mirror_geometry_reflection_pixels']
        transport['reflection_radiance_total'] = stats['total_mirror_specular_reflection_radiance']
        assert transport['reflection_rays'] > 0 and transport['geometry_reflection_pixels'] > WIDTH * HEIGHT * .5
        assert transport['reflection_radiance_total'] > 0
    else:
        policy = ledger['transmission_path_policy']; diagnostics = policy['ior_diagnostics']
        transport.update(receiver_hits=policy['receiver_hits'], contributing_samples=policy['contributing_samples'],
            primary_source_samples=policy['source_sample_counts']['primary'],
            refraction_events=diagnostics['refraction_event_count'],
            direction_changed=diagnostics['direction_changed_count'],
            thin_wall_events=diagnostics['thin_walled_straight_through_count'],
            receiver_object_hits=policy['receiver_object_hits'])
        assert transport['transmission_rays'] > 0 and transport['primary_source_samples'] > 0
        assert transport['receiver_hits'] > WIDTH * HEIGHT * .5 and transport['contributing_samples'] > 0
        assert transport['refraction_events'] > 0 and transport['direction_changed'] > 0
        assert transport['thin_wall_events'] == 0
        assert any(row.get('object_id') == 'receiver' for row in policy['receiver_object_hits'])
    return pixels, {'bmp_sha256': hashlib.sha256(frames[0].read_bytes()).hexdigest(),
                    'scene_sha256': hashlib.sha256(scene.read_bytes()).hexdigest(),
                    'request_sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'transport': transport}


def crop(pixels):
    x0, y0, x1, y1 = ROI
    return [v / 255 for y in range(y0, y1) for x in range(x0, x1) for v in pixels[y * WIDTH + x]]


def deviation(values):
    mean = sum(values) / len(values)
    return math.sqrt(sum((x - mean) ** 2 for x in values) / len(values))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root', required=True, type=Path)
    parser.add_argument('--renderer', type=Path, default=ROOT / f'build/toolchains/clang/{platform.machine()}/tools/cli/ray_tracing_render_headless')
    parser.add_argument('--cases', nargs='+', choices=['mirror', 'refraction'], default=['mirror', 'refraction'])
    parser.add_argument('--fixtures-only', action='store_true')
    parser.add_argument('--workers', type=int, choices=range(1, 9), default=1)
    args = parser.parse_args(); out = args.output_root.resolve(); out.mkdir(parents=True, exist_ok=False)
    paths = fixture(out / 'fixtures', ['direct', 'mirror', 'refraction'])
    flat = out / 'flat.png'; png(flat, 64, 64, lambda x, y: (128, 128, 128, 255))
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(ROOT), RAY_TRACING_RENDER_TRACE_COST_LEDGER='1',
               RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out / 'cache'))
    cases = {}
    for kind in args.cases:
        pattern = json.loads(Path(paths[kind]).read_text())
        # Light is on the receiver's viewed side, behind the interface relative
        # to its incident camera ray. The source plane itself remains unchanged.
        pattern['lights'] = [{'light_id': 'receiver_key', 'kind': 'point',
            'position': {'x': -.4, 'y': -.3, 'z': 2 if kind == 'mirror' else -2},
            'intensity': 2 if kind == 'mirror' else 8, 'radius': .1}]
        if kind == 'mirror':
            # Use the product's mirror BSDF, not an opaque white diffuse source
            # with only its reflectivity changed. Keep its receiver below saturation.
            interface = pattern['extensions']['ray_tracing']['authoring']['object_materials'][0]
            interface.update(material_id=1, reflectivity=1, roughness=0, transparency=0)
        if kind == 'refraction':
            # The production integrator chooses physical transmission from the
            # transparent BSDF preset; alpha alone on opaque preset0 is not it.
            interface = pattern['extensions']['ray_tracing']['authoring']['object_materials'][0]
            interface.update(material_id=5, transparency=1, glass_ior=1.5,
                             glass_thin_walled=False, roughness=0, reflectivity=.04)
        control = copy.deepcopy(pattern)
        control['objects'][1]['extensions']['ray_tracing']['surface_sampling']['channels']['base_color'] = channel(flat, 'linear')
        case = {}; images = {}
        for variation, scene in [('pattern', pattern), ('flat_control', control)]:
            scene_path = out / (kind + '-' + variation + '.json'); write(scene_path, scene)
            if args.fixtures_only:
                continue
            for route in ('flattened', 'tlas_blas'):
                image, receipt = run(args.renderer.resolve(), scene_path, out / f'{kind}-{variation}-{route}', route, kind, env, args.workers)
                case[variation + '_' + route] = receipt
                if route == 'flattened': images[variation] = image
            assert case[variation + '_flattened']['bmp_sha256'] == case[variation + '_tlas_blas']['bmp_sha256'], (kind, variation, 'route mismatch')
        if args.fixtures_only:
            continue
        a, b = crop(images['pattern']), crop(images['flat_control'])
        delta = [x-y for x, y in zip(a,b)]
        channel_mae = [sum(abs(v) for v in delta[c::3]) / len(delta[c::3]) for c in range(3)]
        metrics = {'display_rgb_channel_difference_mae': channel_mae,
                   'display_rgb_difference_rms': math.sqrt(sum(x*x for x in delta)/len(delta)),
                   'difference_spatial_stddev': deviation(delta),
                   'pattern_mean': sum(a)/len(a), 'flat_control_mean': sum(b)/len(b)}
        # A changed digest alone can pass on one pixel; these fixed full-crop
        # predicates require an observable spatial source response over the image.
        checks = {'source_effect': max(channel_mae) > .005,
                  'spatial_source_effect': metrics['difference_spatial_stddev'] > .003,
                  'lit_receiver': metrics['pattern_mean'] > .02 and metrics['flat_control_mean'] > .02,
                  'route_parity': True, 'production_transport_evidence': True}
        case.update(metrics=metrics, checks=checks, passes=all(checks.values())); cases[kind] = case
    if args.fixtures_only:
        return
    receipt = {'schema': 'optic_secondary_integrator_acceptance_v1', 'cases': cases,
        'passes': all(case['passes'] for case in cases.values()), 'roi_xyxy': ROI,
        'renderer_sha256': hashlib.sha256(args.renderer.read_bytes()).hexdigest(),
        'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'signal': 'normalized display-encoded RGB; integration-effect proof only',
        'numerical_supersampling_budget_proof': 'separate test_secondary_footprint_t4.py'}
    write(out / 'acceptance.json', receipt); print(json.dumps(receipt, indent=2))
    assert receipt['passes'], out / 'acceptance.json'


if __name__ == '__main__':
    main()
