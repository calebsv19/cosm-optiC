#!/usr/bin/env python3
"""Pixel and evaluated-position parity for the native --dm2 disposable scene."""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--cli', type=Path)
parser.add_argument('--prepare', action='store_true')
args = parser.parse_args()
root = args.root.resolve()
if args.prepare:
    from check_path_authoring_render import prepare
    prepare(root)
    source = root / 'scene_runtime.json'
    scene = json.loads(source.read_text())
    second = copy.deepcopy(next(o for o in scene['objects'] if o['object_id'] == 'obj_sphere_medium'))
    second['object_id'], second['name'] = 'obj_sphere_second', 'Second sphere'
    scene['objects'].append(second)
    source.write_text(json.dumps(scene, indent=2))
    raise SystemExit(0)
if not args.cli:
    parser.error('--cli is required for render verification')
cli = args.cli.resolve()
scene = json.loads((root / 'scene_runtime.json').read_text())
expected = json.loads((root / 'dm2_expected.json').read_text())
positions = dict(zip((0, 20, 40, 60, 80, 119, 10, 11, 12, 40), expected))
# Isolate the first follower and keep camera/light stationary for clear visual proof.
scene['objects'] = [o for o in scene['objects'] if o['object_id'] != 'obj_sphere_second']
author = scene['extensions']['ray_tracing']['authoring']
author['motion_paths']['bindings'] = [b for b in author['motion_paths']['bindings'] if b['object_id'] != 'obj_sphere_second']
author['scene_timeline']['tracks'] = [t for t in author['scene_timeline']['tracks'] if t['target_id'] != 'object/obj_sphere_second']
for t in author['scene_timeline']['tracks']:
    if not t['target_id'].startswith('object/'):
        for k in t['keys']:
            k['value'] = t['keys'][0]['value']
            k['interpolation'] = 'linear'
env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(root))

def render(data, frame, name, count=1):
    source = root / f'{name}.scene.json'
    source.write_text(json.dumps(data))
    request = {'schema_version': 'ray_tracing_agent_render_request_v1', 'run_id': name,
               'scene': {'runtime_scene_path': str(source)}, 'volume': {'enabled': False},
               'inspection': {'camera_position': {'x': 0, 'y': -7, 'z': 3.5},
                              'camera_look_at': {'x': 0, 'y': 0, 'z': 1.4}, 'camera_zoom': 1},
               'render': {'start_frame': frame, 'frame_count': count, 'width': 160,
                          'height': 100, 'temporal_frames': 1, 'integrator_3d': 'direct_light'},
               'output': {'root': str(root / name), 'overwrite': True}}
    request_path, summary_path = root / f'{name}.request.json', root / f'{name}.summary.json'
    request_path.write_text(json.dumps(request))
    with (root / f'{name}.log').open('w') as log:
        subprocess.run([str(cli), '--request', str(request_path), '--render', '--summary',
                        str(summary_path), '--summary-file-only'], cwd=root, env=env,
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
    summary = json.loads(summary_path.read_text())
    assert summary['frames_rendered'] == count
    return summary

def digest(name, frame):
    return hashlib.sha256((root / name / 'frames' / f'frame_{frame:04d}.bmp').read_bytes()).hexdigest()

def baked(frame):
    result = copy.deepcopy(scene)
    a = result['extensions']['ray_tracing']['authoring']
    a['motion_paths']['bindings'] = []
    a['scene_timeline']['tracks'] = [t for t in a['scene_timeline']['tracks'] if not t['target_id'].startswith('object/')]
    obj = next(o for o in result['objects'] if o['object_id'] == 'obj_sphere_medium')
    obj['transform']['position'] = dict(zip('xyz', [v / result['world_scale'] for v in positions[frame]]))
    return result

report = []
for frame in (0, 20, 40, 60):
    a, b = f'path_{frame}', f'baked_{frame}'
    summary = render(scene, frame, a)
    actual = next(o for o in summary['evaluated_objects'] if o['object_id'] == 'obj_sphere_medium')['position']
    assert all(math.isclose(v, expected, abs_tol=1e-8) for v, expected in zip(actual, positions[frame]))
    render(baked(frame), frame, b)
    assert digest(a, frame) == digest(b, frame), (frame, 'pixel mismatch')
    report.append({'frame': frame, 'position': actual, 'pixel_exact_match': True, 'sha256': digest(a, frame)})
    print('PASS native sample and baked pixel parity', frame, flush=True)
assert report[1]['sha256'] == report[2]['sha256'], 'hold must render identically'
assert report[0]['sha256'] != report[1]['sha256'] != report[3]['sha256'], 'motion must be visible'
render(scene, 10, 'consecutive', 3)
for frame in (10, 11, 12):
    name = f'baked_{frame}'
    render(baked(frame), frame, name)
    assert digest('consecutive', frame) == digest(name, frame)
(root / 'dm2_render_verification.json').write_text(json.dumps(report, indent=2))
print('PASS consecutive 10-12 frames, visible motion and frozen hold', flush=True)
