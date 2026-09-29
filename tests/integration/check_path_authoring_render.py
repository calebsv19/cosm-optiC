#!/usr/bin/env python3
"""Check saved D-M1 native samples against real headless images and controls."""
import argparse
import copy
import hashlib
import json
import math
import os
import shutil
from pathlib import Path
import subprocess


def prepare(root):
    """Create a new disposable native fixture without touching user scenes."""
    repo = Path(__file__).resolve().parents[2]
    root.mkdir(parents=True, exist_ok=False)
    fixture = repo / 'tests/fixtures/mesh_asset_runtime_spheres'
    scene = json.loads((fixture / 'scene_runtime.json').read_text())
    scene['objects'] = [o for o in scene['objects'] if o['object_id'] in ('obj_floor', 'obj_sphere_medium')]
    shutil.copytree(fixture / 'assets', root / 'assets')
    author = scene['extensions']['ray_tracing']['authoring']
    paths = {'camera': [(-1, -6.2, 2.6), (0, -6.2, 2.8), (1, -6.2, 3)],
             'light': [(-2, -2, 4), (0, -2, 4.5), (2, -2, 5)]}
    for subject, positions in paths.items():
        points, depths = [], []
        for i, (x, y, z) in enumerate(positions):
            point = {'x': x, 'y': y, 'rotation': math.pi + i*.03 if subject == 'camera' else 0, 'handleLink': False}
            depth = {'z': z, 'lookPitch': -.23 + i*.02 if subject == 'camera' else 0}
            if i < 2:
                point['velocity1'] = {'vx': .45, 'vy': .2}
                depth['velocity1'] = {'vz': .1}
            if i > 0:
                point['velocity2'] = {'vx': -.45, 'vy': -.2}
                depth['velocity2'] = {'vz': -.1}
            points.append(point)
            depths.append(depth)
        author[subject + '_path'] = {'mode': 'BEZIER_CUBIC', 'points': points}
        author[subject + '_path_depth'] = {'points': depths}
    (root / 'scene_runtime.json').write_text(json.dumps(scene, indent=2))
    (root / 'config').mkdir()
    (root / 'data/runtime').mkdir(parents=True)
    shutil.copy2(repo / 'config/scene_config.json', root / 'config/scene_config.json')
    settings = {'inputRoot': str(root / 'config'), 'outputRoot': str(root / 'data/runtime'),
                'meshAssetRoot': str(root / 'assets/mesh_assets'), 'framesForTravel': 120,
                'frameLimit': 120, 'fps': 24, 'frameDir': str(root / 'data/runtime/frames')}
    (root / 'data/runtime/animation_config.json').write_text(json.dumps(settings))
    print('Prepared isolated D-M1 fixture:', root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True, help='Isolated --dm1 native test directory')
    parser.add_argument('--cli', type=Path, help='Required for render verification')
    parser.add_argument('--prepare', action='store_true', help='Create a new disposable native test fixture only')
    args = parser.parse_args()
    root = args.root.resolve()
    if args.prepare:
        prepare(root)
        return
    if not args.cli:
        parser.error('--cli is required for render verification')
    cli = args.cli.resolve()
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(root),
               RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT=str(root / 'data/mesh_cache'))
    expected = json.loads((root / 'dm1_expected.json').read_text())
    before = json.loads((root / 'dm1_before.json').read_text())
    after = json.loads((root / 'scene_runtime.json').read_text())
    records = []

    def render(scene, frame, name):
        scene_path = root / (name + '.scene.json')
        scene_path.write_text(json.dumps(scene))
        request = {'schema_version': 'ray_tracing_agent_render_request_v1', 'run_id': name,
                   'scene': {'runtime_scene_path': str(scene_path)}, 'volume': {'enabled': False},
                   'render': {'start_frame': frame, 'frame_count': 1, 'width': 160, 'height': 100,
                              'temporal_frames': 1, 'integrator_3d': 'direct_light'},
                   'output': {'root': str(root / name), 'overwrite': False}}
        request_path = root / (name + '.request.json')
        request_path.write_text(json.dumps(request))
        summary_path = root / (name + '.summary.json')
        with (root / (name + '.log')).open('w') as log:
            subprocess.run([str(cli), '--request', str(request_path), '--render', '--summary',
                            str(summary_path), '--summary-file-only'], cwd=root, env=env,
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
        summary = json.loads(summary_path.read_text())
        assert summary['frames_rendered'] == 1
        image = root / name / 'frames' / f'frame_{frame:04d}.bmp'
        return summary, hashlib.sha256(image.read_bytes()).hexdigest()

    for i, frame in enumerate((0, 30, 60)):
        result, digest = render(after, frame, f'changed_{frame}')
        camera, light = result['evaluated_camera'], result['evaluated_light']
        actual = camera['position'] + [camera['yaw'], camera['pitch'], camera['fov_y']] + light['position'] + [light['progress'], light['intensity']]
        assert all(math.isclose(a, b, rel_tol=1e-9, abs_tol=1e-8) for a, b in zip(actual, expected[i])), (frame, actual, expected[i])
        records.append({'frame': frame, 'native_headless_sample_match': True, 'sha256': digest})
    _, baseline = render(before, 30, 'before_30')
    assert baseline != records[1]['sha256'], 'Edited paths did not change the rendered image'
    # Each edited path must affect actual pixels independently, not just summary data.
    for subject, fields in [('camera', ('camera_path', 'camera_path_depth')), ('light', ('light_timeline', 'light_path', 'light_path_depth'))]:
        scene = copy.deepcopy(before)
        author = scene['extensions']['ray_tracing']['authoring']
        edited = after['extensions']['ray_tracing']['authoring']
        for field in fields:
            if field in edited:
                author[field] = copy.deepcopy(edited[field])
        _, digest = render(scene, 30, subject + '_only_30')
        assert digest != baseline, subject + ' path edits did not affect pixels'
        records.append({'subject': subject, 'changed_pixels_independently': True, 'sha256': digest})
    proof = {'frames': records, 'baseline_sha256': baseline,
             'renderer_sha256': hashlib.sha256(cli.read_bytes()).hexdigest()}
    (root / 'dm1_render_verification.json').write_text(json.dumps(proof, indent=2) + '\n')
    print('D-M1 render PASS: native/headless samples at 0/30/60; camera and light edits independently change pixels')


if __name__ == '__main__':
    main()
