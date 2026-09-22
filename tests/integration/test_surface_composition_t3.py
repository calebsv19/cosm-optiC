#!/usr/bin/env python3
"""Independent T3 linear composition/RMS, chart frames, regions and render proof."""
import argparse
import copy
import hashlib
import json
import math
import os
import platform
import subprocess
import struct
from pathlib import Path
import test_surface_mapping_m4 as m4
from test_surface_mapping_m5 import png, channel
from test_surface_mapping_m1 import MAPPING as PLANAR
from smooth_mesh_reflection.prepare_reflection_matrix import build_request

ROOT = m4.ROOT
COLOR = [64 / 255, 128 / 255, 192 / 255]
BASE = [.8, .2, .1]
MASK = 128 / 255
ROUGH = math.sqrt((1 - MASK) * .2**2 + MASK * (153 / 255)**2)
EXPECTED = [(1 - MASK) * a + MASK * b for a, b in zip(BASE, COLOR)]


def graph(alpha=False, reference=False):
    if reference:
        nodes = [{'id': 'finish', 'kind': 'color', 'value': EXPECTED},
                 {'id': 'rough_mix', 'kind': 'scalar', 'value': ROUGH}]
    else:
        nodes = [
            {'id': 'image', 'kind': 'image_color', 'resource': 'base_color'},
            {'id': 'rough_image', 'kind': 'image_scalar', 'resource': 'roughness'},
            {'id': 'base', 'kind': 'color', 'value': BASE},
            {'id': 'low', 'kind': 'scalar', 'value': .2},
            {'id': 'mask', 'kind': 'image_scalar', 'resource': 'base_color_alpha'} if alpha else
            {'id': 'mask', 'kind': 'scalar', 'value': MASK},
            {'id': 'finish', 'kind': 'mix', 'inputs': ['base', 'image', 'mask']},
            {'id': 'rough_mix', 'kind': 'roughness_mix', 'inputs': ['low', 'rough_image', 'mask']},
            {'id': 'region_color', 'kind': 'color', 'value': [.1, .7, .3]},
            {'id': 'region_rough', 'kind': 'scalar', 'value': .9}]
    return {'version': 2, 'required_capability': 'optic.surface_composition_v1',
            'color_space': 'linear', 'producer': {'t3_preserve': {'unknown': [1, 'proof']}},
            'nodes': nodes, 'outputs': {'base_color': 'finish', 'roughness': 'rough_mix'}}


def write(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def config(folder):
    runtime = folder / 'data/runtime'
    runtime.mkdir(parents=True)
    write(runtime / 'animation_config.json', {'editorMode': 1, 'spaceMode': 1,
          'windowWidth': 1280, 'windowHeight': 800, 'inputRoot': str(ROOT / 'config'),
          'outputRoot': str(runtime), 'videoOutputRoot': str(folder / 'videos')})
    write(runtime / 'scene_config.json', {'window': {'width': 1280, 'height': 800}})


def render(renderer, scene, folder, env, route='flattened'):
    folder.mkdir(parents=True, exist_ok=True)
    request = build_request(folder, 'direct', route)
    request['scene']['runtime_scene_path'] = str(scene)
    request['render'].update(width=240, height=180)
    request['inspection'].update(camera_position={'x': 2, 'y': -3, 'z': 3},
                                 camera_look_at={'x': 0, 'y': 0, 'z': 0}, camera_zoom=1.2)
    req = folder / 'request.json'; write(req, request)
    m4.run([renderer, '--request', req, '--preflight'], folder / 'preflight.log', env)
    m4.run([renderer, '--request', req, '--render'], folder / 'render.log', env)
    frames = list((folder / 'renders' / ('direct_' + route) / 'frames').glob('*.bmp'))
    assert len(frames) == 1
    summary = json.loads(Path(request['progress']['summary_path']).read_text())
    assert summary['frames_rendered'] == 1
    return hashlib.sha256(frames[0].read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--fixtures-only', action='store_true')
    parser.add_argument('--skip-native', action='store_true')
    parser.add_argument('--cases', nargs='+', default=['blend', 'alpha', 'flat', 'zero', 'nonflat', 'mirrored', 'regions', 'plane_regions', 'rest', 'world', 'height_constant', 'height_zero', 'height_ramp', 'variance'])
    args = parser.parse_args()
    out = args.output_root.resolve(); out.mkdir(parents=True, exist_ok=False)
    binary = ROOT / f'build/toolchains/clang/{platform.machine()}'
    renderer = binary / 'tools/cli/ray_tracing_render_headless'
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(ROOT),
               RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out / 'cache'))
    template = json.loads((ROOT / 'tests/fixtures/mesh_asset_runtime_spheres/scene_runtime.json').read_text())
    author_template = json.loads((ROOT / 'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json').read_text())
    results = {'oracle': {'linear_rgb': EXPECTED, 'roughness_rms': ROUGH,
                         'mask': MASK, 'tolerance': 2e-6}, 'cases': {}}
    for name in args.cases:
        folder = out / name; config(folder)
        scene = copy.deepcopy(template); scene['scene_id'] = 't3_' + name
        if name in ('regions', 'plane_regions'):
            primitive_scene = json.loads((ROOT / 'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
            obj = copy.deepcopy(primitive_scene['objects'][0])
            obj.update(object_id='surface', object_type='rect_prism_primitive')
            obj['primitive'].update(kind='rect_prism_primitive', width=2, height=2, depth=2)
            obj['primitive']['frame']['origin'] = {'x': 0, 'y': 0, 'z': 0}
            obj['transform'] = {'position': {'x': 0, 'y': 0, 'z': 0}, 'scale': {'x': 1, 'y': 1, 'z': 1}}
            if name == 'plane_regions':
                obj.update(object_type='plane_primitive')
                obj['primitive']['kind'] = 'plane_primitive'
                obj['primitive'].pop('depth')
            mapping = copy.deepcopy(PLANAR)
        else:
            source = folder / 'source.obj'; source.write_text(m4.OBJ)
            author = copy.deepcopy(author_template)
            author['authoring']['imported_mesh'].update(source_format='obj', source_uri=str(source),
                uv_set_id='paint_uv', source_to_asset_scale=1, preserve_source_normals=True,
                normal_mode='none', crease_angle_degrees=60, source_unit_system='meter', topology_closed_volume_observed=False)
            author_path = folder / 'authoring.json'; write(author_path, author)
            dest = folder / 'assets/mesh_assets'; dest.mkdir(parents=True)
            asset = dest / 'uv_asset.runtime.json'
            if not args.fixtures_only:
                m4.run([binary / 'tools/smooth_mesh_reflection/compile_runtime_fixture', author_path, folder, 'uv_asset', asset], folder / 'compile.log', env)
            obj = m4.m0.mesh_instance('surface', 'uv_asset', 'mat_sphere_high', 0, 0, 0)
            obj['transform']['scale'] = {'x': -2 if name == 'mirrored' else 2, 'y': 1.3, 'z': .7}
            obj['transform']['rotation'] = {'x': 17, 'y': 23, 'z': 31}
            mapping = copy.deepcopy(m4.MAP); mapping.update(uv_scale=[1, 1], uv_offset=[0, 0], rotation_rad=0)
        color = folder / 'color.png'; rough = folder / 'rough.png'; normal = folder / 'normal.png'
        png(color, 2, 2, lambda x, y: (64, 128, 192, 128))
        png(rough, 2, 2, lambda x, y: (153, 0, 0, 255))
        png(normal, 2, 2, lambda x, y: (128, 128, 255, 255) if name == 'flat' else (192, 160, 238, 255))
        height = folder / 'height.png'
        png(height, 64, 64, lambda x, y: (153 if name == 'height_constant' else round(x / 63 * 255), 0, 0, 255))
        if name == 'variance':
            png(normal, 64, 2, lambda x, y: (64 if x % 2 else 192, 128, 238, 255))
        sampling = {'version': 1, 'required_capability': 'optic.surface_sampling_v1',
                    'filter': 'trilinear', 'address': 'repeat', 'color_space': 'linear',
                    'period_tiles': [1, 1], 'normal_strength': 0 if name == 'zero' else 1,
                    'channels': {'base_color': channel(color, 'linear'), 'roughness': channel(rough, 'data')}}
        if name in ('flat', 'zero', 'nonflat', 'mirrored', 'variance'):
            sampling['channels']['normal'] = channel(normal, 'data')
        if name.startswith('height_'):
            sampling['channels']['height'] = channel(height, 'data')
            sampling['height_m'] = 0 if name == 'height_zero' else .1
        obj.setdefault('extensions', {}).setdefault('ray_tracing', {}).update(surface_mapping=mapping, surface_sampling=sampling)
        scene['objects'] = [obj]
        g = graph(alpha=name == 'alpha')
        if name in ('rest', 'world'):
            g['nodes'].extend([{'id': 'position', 'kind': 'coordinate', 'space': 'object_rest' if name == 'rest' else 'world', 'scale_m': .2, 'offset': [.03, .07, -.1]}, {'id': 'pattern', 'kind': 'noise3d', 'inputs': ['position'], 'seed': 17}])
            next(n for n in g['nodes'] if n['id'] == 'finish')['inputs'][2] = 'pattern'
        if name in ('regions', 'plane_regions'):
            g['regions'] = [{'face_role': 'front', 'outputs': {'base_color': 'region_color'}}]
            if name == 'regions':
                g['regions'].append({'face_role': 'back', 'outputs': {'roughness': 'region_rough'}})
        row = {'object_id': 'surface', 'material_id': 0, 'roughness': .8, 'reflectivity': .02,
               'object_color': 12756864, 'surface_graph': g, 't3_unknown': [1, 'retained']}
        scene.setdefault('extensions', {}).setdefault('ray_tracing', {})['authoring'] = {'object_materials': [row]}
        scene_path = folder / 'scene_runtime.json'; write(scene_path, scene)
        if args.fixtures_only:
            continue
        case_env = dict(env, OPTIC_T3_CASE=name)
        if not args.skip_native:
            for mode in ('--composition-t3', '--composition-t3-reopen'):
                m4.run([binary / 'tests/scene_editor_workspace_visual_test', folder, scene_path, mode], folder / (mode[2:] + '.log'), case_env)
            results['cases'][name] = json.loads((folder / 'composition_t3.json').read_text())
            results['cases'][name]['fresh_process_reopen'] = json.loads((folder / 'composition_t3_reopen.json').read_text())
            saved = json.loads(scene_path.read_text())
            assert saved['extensions']['ray_tracing']['authoring']['object_materials'][0]['surface_graph'] == g
            assert saved['objects'][0]['extensions']['ray_tracing']['surface_sampling'] == sampling
        else:
            results['cases'][name] = {}
        actual = render(renderer, scene_path, folder / 'actual', env)
        reference = copy.deepcopy(scene)
        reference['extensions']['ray_tracing']['authoring']['object_materials'][0]['surface_graph'] = graph(reference=True)
        if name in ('regions', 'plane_regions', 'rest', 'world'):
            results['cases'][name]['actual_sha256'] = actual
            continue
        if name in ('flat', 'zero'):
            reference['objects'][0]['extensions']['ray_tracing']['surface_sampling']['channels'].pop('normal')
        if name in ('height_constant', 'height_zero'):
            reference['objects'][0]['extensions']['ray_tracing']['surface_sampling']['channels'].pop('height')
            reference['objects'][0]['extensions']['ray_tracing']['surface_sampling']['height_m'] = 0
        reference_path = folder / 'reference.json'; write(reference_path, reference)
        expected = render(renderer, reference_path, folder / 'reference', env)
        if name == 'variance':
            # The image pyramid stores float32 moments, whereas the independent
            # constant graph uses the double-precision byte oracle. A value on
            # an 8-bit rounding boundary can differ by one code value.
            def rgb(folder):
                data = next((folder / 'renders/direct_flattened/frames').glob('*.bmp')).read_bytes()
                offset = struct.unpack_from('<I', data, 10)[0]
                width, height = struct.unpack_from('<ii', data, 18)
                bits = struct.unpack_from('<H', data, 28)[0]
                assert width == 240 and abs(height) == 180 and bits in (24, 32)
                stride = ((width * bits + 31) // 32) * 4
                return bytes(data[offset + y * stride + x * (bits // 8) + c]
                             for y in range(abs(height)) for x in range(width) for c in range(3))
            errors = [abs(a - b) for a, b in zip(rgb(folder / 'actual'), rgb(folder / 'reference'))]
            maximum, mean = max(errors), sum(errors) / len(errors)
            assert maximum <= 1 and mean <= 1e-4, (name, maximum, mean)
            results['cases'][name]['reference_rgb_error_8bit'] = {
                'max': maximum, 'mean': mean, 'max_limit': 1, 'mean_limit': 1e-4}
        else:
            assert actual == expected, (name, 'independent constant graph render differs', actual, expected)
        results['cases'][name].update(actual_sha256=actual, independent_reference_sha256=expected)
        if name in ('nonflat', 'mirrored', 'height_ramp'):
            unperturbed = copy.deepcopy(reference)
            response = unperturbed['objects'][0]['extensions']['ray_tracing']['surface_sampling']
            response['channels'].pop('height' if name == 'height_ramp' else 'normal')
            if name == 'height_ramp': response['height_m'] = 0
            path = folder / 'unperturbed.json'; write(path, unperturbed)
            baseline = render(renderer, path, folder / 'unperturbed', env)
            assert actual != baseline, (name, 'normal response had no directional image effect')
            results['cases'][name]['unperturbed_sha256'] = baseline
        if name == 'mirrored':
            accelerated = render(renderer, scene_path, folder / 'accelerated', env, 'tlas_blas')
            assert actual == accelerated
            results['cases'][name]['tlas_blas_sha256'] = accelerated
    if not args.fixtures_only:
        negatives = {}
        base_folder = next(out / name for name in args.cases if name not in ('regions', 'plane_regions'))
        baseline = json.loads((base_folder / 'scene_runtime.json').read_text())
        for name in ('missing_resource', 'wrong_resource_kind', 'roughness_type', 'roughness_cycle', 'mixed_legacy_stack', 'mesh_regions', 'duplicate_region', 'unknown_face', 'plane_back', 'normal_height_conflict'):
            bad = copy.deepcopy(baseline)
            row = bad['extensions']['ray_tracing']['authoring']['object_materials'][0]
            g = row['surface_graph']
            sampling = bad['objects'][0]['extensions']['ray_tracing']['surface_sampling']
            if name == 'missing_resource':
                sampling['channels'].pop('base_color')
            elif name == 'wrong_resource_kind':
                g['nodes'][0]['resource'] = 'roughness'
            elif name == 'roughness_type':
                next(n for n in g['nodes'] if n['id'] == 'rough_mix')['inputs'][0] = 'base'
            elif name == 'roughness_cycle':
                next(n for n in g['nodes'] if n['id'] == 'rough_mix')['inputs'][0] = 'rough_mix'
            elif name == 'mixed_legacy_stack':
                row['material_texture_stack'] = {'layers': []}
            elif name == 'mesh_regions':
                g['regions'] = [{'face_role': 'front', 'outputs': {'base_color': 'region_color'}}]
            elif name == 'plane_back':
                plane_path = out / 'plane_regions/scene_runtime.json'
                if not plane_path.exists(): continue
                bad = json.loads(plane_path.read_text())
                bad['extensions']['ray_tracing']['authoring']['object_materials'][0]['surface_graph']['regions'][0]['face_role'] = 'back'
            elif name in ('duplicate_region', 'unknown_face'):
                prism = json.loads((out / 'regions/scene_runtime.json').read_text()) if (out / 'regions/scene_runtime.json').exists() else None
                if prism is None:
                    continue
                bad = prism; g = bad['extensions']['ray_tracing']['authoring']['object_materials'][0]['surface_graph']
                if name == 'duplicate_region': g['regions'].append(copy.deepcopy(g['regions'][0]))
                else: g['regions'][0]['face_role'] = 'undefined'
            else:
                sampling['channels']['normal'] = channel(base_folder / 'normal.png', 'data')
                sampling['channels']['height'] = channel(base_folder / 'rough.png', 'data')
                sampling['height_m'] = .1
            path = base_folder / (name + '.json'); write(path, bad)
            req = build_request(out / ('reject-' + name), 'direct', 'flattened')
            req['scene']['runtime_scene_path'] = str(path)
            request = out / (name + '-request.json'); write(request, req)
            with (out / (name + '.log')).open('w') as log:
                result = subprocess.run([str(renderer), '--request', str(request), '--preflight'], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=60)
            assert result.returncode > 0, (name, result.returncode, 'must reject without crashing')
            negatives[name] = {'exit_code': result.returncode}
        results['negative_preflights'] = negatives
        if 'rest' in results['cases'] and 'world' in results['cases']:
            assert results['cases']['rest']['actual_sha256'] != results['cases']['world']['actual_sha256']
    write(out / ('fixtures.json' if args.fixtures_only else 'acceptance.json'), results)
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
