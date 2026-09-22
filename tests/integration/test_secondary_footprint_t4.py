#!/usr/bin/env python3
"""Fixed-budget T4 secondary-footprint baseline and independent point-reference images."""
import argparse
import copy
import hashlib
import json
import math
import os
import platform
import struct
import subprocess
from pathlib import Path
from test_surface_mapping_m5 import png, channel
from test_surface_mapping_m1 import MAPPING
from test_surface_composition_t3 import config, write

ROOT = Path(__file__).resolve().parents[2]
WIDTH, HEIGHT = 160, 120
ROI = (16, 12, 144, 108)
CONVERGENCE_ROI = (60, 45, 100, 75)
BUDGETS = {'linear_rgb_mae_max': .04, 'mean_rgb_bias_max': .01,
           'contrast_ratio_min': .85, 'contrast_ratio_max': 1.15,
           'motion_change_residual_mae_max': .04, 'reference_convergence_rms_max': .005}


def fixture(out, cases):
    config(out)
    texture = out / 'receiver.png'
    png(texture, 64, 64, lambda x, y: (
        round(255 * (.5 + .32 * math.sin(2 * math.pi * 8 * (x + .5) / 64))),
        round(255 * (.5 + .25 * math.cos(2 * math.pi * 6 * (y + .5) / 64))),
        round(255 * (.5 + .18 * math.sin(2 * math.pi * 4 * (x + y + 1) / 64))), 255))
    template = json.loads((ROOT / 'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    def plane(name, z):
        obj = copy.deepcopy(template['objects'][0])
        obj['object_id'] = name
        obj['primitive'].update(width=20, height=20)
        obj['primitive']['frame']['origin'] = {'x': 0, 'y': 0, 'z': z}
        obj['transform'] = {'position': {'x': 0, 'y': 0, 'z': z}, 'scale': {'x': 1, 'y': 1, 'z': 1}}
        return obj
    paths = {}
    for name, z in [('direct', 0), ('mirror', 4), ('refraction', -4)]:
        scene = copy.deepcopy(template); scene['scene_id'] = 't4_' + name
        target = plane('receiver', z)
        mapping = copy.deepcopy(MAPPING)
        mapping.update(origin_m=[0, 0, 0], tile_m=[.6, .6])
        target.setdefault('extensions', {}).setdefault('ray_tracing', {}).update(
            surface_mapping=mapping,
            surface_sampling={'version': 1, 'required_capability': 'optic.surface_sampling_v1',
                              'filter': 'trilinear', 'address': 'repeat', 'color_space': 'linear',
                              'period_tiles': [1, 1], 'channels': {'base_color': channel(texture, 'linear')}})
        row = {'object_id': 'receiver', 'material_id': 0, 'roughness': .65, 'reflectivity': .02,
               'surface_graph': {'version': 2, 'required_capability': 'optic.surface_composition_v1',
                'color_space': 'linear', 'nodes': [{'id': 'image', 'kind': 'image_color', 'resource': 'base_color'},
                {'id': 'rough', 'kind': 'scalar', 'value': .65},
                {'id': 'position', 'kind': 'coordinate', 'space': 'object_rest', 'scale_m': 1.3, 'offset': [.07, -.11, .03]},
                {'id': 'noise', 'kind': 'noise3d', 'inputs': ['position'], 'seed': 19},
                {'id': 'weight', 'kind': 'scalar', 'value': .15},
                {'id': 'fraction', 'kind': 'multiply', 'inputs': ['noise', 'weight']},
                {'id': 'tint', 'kind': 'color', 'value': [.2, .35, .6]},
                {'id': 'finish', 'kind': 'mix', 'inputs': ['image', 'tint', 'fraction']}],
                'outputs': {'base_color': 'finish', 'roughness': 'rough'}}}
        objects, rows = [target], [row]
        if name != 'direct':
            objects.insert(0, plane('interface', 0))
            rows.insert(0, {'object_id': 'interface', 'material_id': 0, 'object_color': 16777215,
                           'roughness': 0, 'reflectivity': 1 if name == 'mirror' else .04,
                           'transparency': 0 if name == 'mirror' else 1, 'glass_ior': 1.5, 'glass_thin_walled': False})
        scene['objects'] = objects
        scene['extensions']['ray_tracing']['authoring']['object_materials'] = rows
        path = out / (name + '.json'); write(path, scene); paths[name] = str(path)
    # Separate eligibility scenes leave the frozen numerical routes unchanged.
    neutral = out / 'neutral-normal.png'
    png(neutral, 2, 2, lambda x, y: (128, 128, 255, 255))
    for name, strength in [('fallback_normal', 1), ('fallback_disabled', 0)]:
        scene = json.loads(Path(paths['mirror']).read_text())
        surface = scene['objects'][0]
        surface.setdefault('extensions', {}).setdefault('ray_tracing', {}).update(
            surface_mapping=copy.deepcopy(mapping),
            surface_sampling={'version': 1, 'required_capability': 'optic.surface_sampling_v1',
                'filter': 'trilinear', 'address': 'repeat', 'color_space': 'linear',
                'period_tiles': [1, 1], 'normal_strength': strength,
                'channels': {'normal': channel(neutral, 'data')}})
        row = scene['extensions']['ray_tracing']['authoring']['object_materials'][0]
        row['surface_graph'] = {'version': 2, 'required_capability': 'optic.surface_composition_v1',
            'color_space': 'linear', 'nodes': [{'id': 'white', 'kind': 'color', 'value': [1, 1, 1]},
            {'id': 'smooth', 'kind': 'scalar', 'value': 0}],
            'outputs': {'base_color': 'white', 'roughness': 'smooth'}}
        path = out / (name + '.json'); write(path, scene); paths[name] = str(path)
    paths['_cases'] = cases
    write(out / 'manifest.json', paths)
    write(out / 'declared-budgets.json', {'budgets': BUDGETS, 'resolution': [WIDTH, HEIGHT],
          'interior_roi_xyxy': ROI, 'convergence_roi_xyxy': CONVERGENCE_ROI,
          'camera_distances_m': [1, 2], 'camera_base_xy_m': [.013123, .019371], 'camera_motion_x_m': [0, .007],
          'reference_subsamples': [4, 4], 'convergence_subsamples': [8, 8],
          'receiver_image_sha256': hashlib.sha256(texture.read_bytes()).hexdigest()})
    return paths


def image(path):
    data = path.read_bytes()
    assert len(data) == WIDTH * HEIGHT * 3 * 4, path
    result = struct.unpack('<' + 'f' * (WIDTH * HEIGHT * 3), data)
    assert all(math.isfinite(v) and 0 <= v <= 1 for v in result), path
    return result


def values(pixels, roi=ROI):
    x0, y0, x1, y1 = roi
    return [pixels[(y * WIDTH + x) * 3 + c] for y in range(y0, y1) for x in range(x0, x1) for c in range(3)]


def contrast(pixels):
    # Luminance standard deviation distinguishes retained detail from chart average.
    luminance = [.2126 * pixels[i] + .7152 * pixels[i + 1] + .0722 * pixels[i + 2] for i in range(0, len(pixels), 3)]
    mean = sum(luminance) / len(luminance)
    return math.sqrt(sum((v - mean) ** 2 for v in luminance) / len(luminance))


def preview(path, pixels):
    path.write_bytes(f'P6\n{WIDTH} {HEIGHT}\n255\n'.encode() + bytes(round(v * 255) for v in pixels))


def analyze(out, routes):
    cases, motions = {}, {}
    for route in routes:
        for distance in range(2):
            for pose in range(2):
                key = f'{route}-d{distance}-p{pose}'
                actual = image(out / (key + '-actual.f32'))
                reference = image(out / (key + '-reference.f32'))
                high = image(out / (key + '-convergence.f32'))
                a, r = values(actual), values(reference)
                mae = sum(abs(x - y) for x, y in zip(a, r)) / len(a)
                channel_mae = [sum(abs(x-y) for x,y in zip(a[c::3], r[c::3])) / len(a[c::3]) for c in range(3)]
                biases = [abs(sum(a[c::3]) / len(a[c::3]) - sum(r[c::3]) / len(r[c::3])) for c in range(3)]
                rc = contrast(r); assert rc > .02, (key, 'reference has no measurable detail')
                ratio = contrast(a) / rc
                ref_small, high_small = values(reference, CONVERGENCE_ROI), values(high, CONVERGENCE_ROI)
                convergence = math.sqrt(sum((x-y)**2 for x, y in zip(ref_small, high_small)) / len(ref_small))
                checks = {'linear_rgb_mae': max(channel_mae) <= BUDGETS['linear_rgb_mae_max'],
                          'mean_rgb_bias': max(biases) <= BUDGETS['mean_rgb_bias_max'],
                          'contrast_ratio': BUDGETS['contrast_ratio_min'] <= ratio <= BUDGETS['contrast_ratio_max'],
                          'reference_convergence': convergence <= BUDGETS['reference_convergence_rms_max']}
                cases[key] = {'linear_rgb_mae': mae, 'linear_channel_mae': channel_mae, 'mean_rgb_bias': biases, 'contrast_ratio': ratio,
                              'reference_contrast': rc, 'reference_convergence_rms': convergence,
                              'checks': checks, 'passes': all(checks.values())}
                preview(out / (key + '-actual.ppm'), actual); preview(out / (key + '-reference.ppm'), reference)
            stem = f'{route}-d{distance}'
            a0, a1 = (values(image(out / (f'{stem}-p{p}-actual.f32'))) for p in range(2))
            r0, r1 = (values(image(out / (f'{stem}-p{p}-reference.f32'))) for p in range(2))
            error = sum(abs((b-a)-(d-c)) for a,b,c,d in zip(a0,a1,r0,r1)) / len(a0)
            motions[stem] = {'motion_change_residual_mae': error, 'passes': error <= BUDGETS['motion_change_residual_mae_max']}
    receipt = {'schema': 'optic_secondary_footprint_acceptance_v1', 'budgets': BUDGETS,
               'signal': 'normalized linear receiver base color before lighting or tone mapping',
               'reference': 'independent analytic planes/reflection/Snell; traced receiver; test-only point sampling',
               'reference_valid': all(c['checks']['reference_convergence'] for c in cases.values()),
               'cases': cases, 'motion': motions,
               'passes': all(c['passes'] for c in cases.values()) and all(m['passes'] for m in motions.values()),
               'headless_integrator_image_proof': False,
               'provenance': json.loads((out / 'provenance.json').read_text()),
               'native_proof': json.loads((out / 'native-proof.json').read_text()),
               'interior_roi_xyxy': ROI, 'fixed_mask_pixels': (ROI[2]-ROI[0])*(ROI[3]-ROI[1]),
               'mask_policy': 'same fixed predeclared interior rectangle for actual/reference and both motion poses; no thresholded or inferred validity masks'}
    write(out / 'acceptance.json', receipt)
    print(json.dumps(receipt, indent=2))
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--fixtures-only', action='store_true')
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--analyze-only', action='store_true')
    parser.add_argument('--require-budgets', action='store_true')
    parser.add_argument('--cases', nargs='+', choices=['direct', 'mirror', 'refraction'], default=['direct', 'mirror', 'refraction'])
    args = parser.parse_args(); out = args.output_root.resolve()
    if not args.analyze_only:
        out.mkdir(parents=True, exist_ok=False)
        paths = fixture(out, args.cases)
        source_paths = ['tests/scene_editor_secondary_footprint_t4.h', 'tests/integration/test_secondary_footprint_t4.py',
            'src/render/runtime_ray_3d.c', 'include/render/runtime_ray_3d.h', 'src/render/runtime_camera_3d_rays.c',
            'src/render/runtime_specular_reflection_3d.c', 'src/render/runtime_dielectric_transport_3d.c',
            'src/render/runtime_disney_v2_transmission_3d.c', 'src/render/runtime_disney_v2_transport_3d.c',
            'src/render/materials/runtime_surface_sampling.inc']
        provenance = {'source_sha256': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in source_paths},
                      'scene_sha256': {name: hashlib.sha256(Path(paths[name]).read_bytes()).hexdigest() for name in args.cases},
                      'auxiliary_scene_sha256': {name: hashlib.sha256(Path(paths[name]).read_bytes()).hexdigest() for name in ('fallback_normal', 'fallback_disabled')}}
        write(out / 'provenance.json', provenance)
        if args.fixtures_only:
            return
        binary = args.binary.resolve() if args.binary else ROOT / f'build/toolchains/clang/{platform.machine()}/tests/scene_editor_workspace_visual_test'
        provenance['native_binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
        write(out / 'provenance.json', provenance)
        env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(ROOT), OPTIC_T4_MANIFEST=str(out / 'manifest.json'),
                   RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out / 'cache'))
        with (out / 'native.log').open('w') as log:
            result = subprocess.run([str(binary), str(out), paths['direct'], '--secondary-footprint-t4'],
                                    stdout=log, stderr=subprocess.STDOUT, env=env, timeout=1200)
        assert result.returncode == 0, (result.returncode, out / 'native.log')
    receipt = analyze(out, args.cases)
    if args.require_budgets:
        assert receipt['passes'], out / 'acceptance.json'


if __name__ == '__main__':
    main()
