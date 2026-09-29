#!/usr/bin/env python3
"""Check M3 native camera poses and pixels against explicit baked camera channels."""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--root', type=Path, required=True)
p.add_argument('--cli', type=Path, required=True)
a = p.parse_args()
root, cli = a.root.resolve(), a.cli.resolve()
scene = json.loads((root / 'scene_runtime.json').read_text())
expected = json.loads((root / 'dm3_camera_expected.json').read_text())

def author(s):
    return s['extensions']['ray_tracing']['authoring']

def render(s, frame, name):
    source = root / f'{name}.scene.json'
    source.write_text(json.dumps(s))
    request = {'schema_version': 'ray_tracing_agent_render_request_v1', 'run_id': name,
               'scene': {'runtime_scene_path': str(source)}, 'volume': {'enabled': False},
               'render': {'start_frame': frame, 'frame_count': 1, 'width': 160, 'height': 100,
                          'temporal_frames': 1, 'integrator_3d': 'direct_light'},
               'output': {'root': str(root / name), 'overwrite': True}}
    req, summary = root / f'{name}.request.json', root / f'{name}.summary.json'
    req.write_text(json.dumps(request))
    with (root / f'{name}.log').open('w') as log:
        subprocess.run([str(cli), '--request', str(req), '--render', '--summary', str(summary),
                        '--summary-file-only'], cwd=root,
                       env=dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(root)),
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
    digest = hashlib.sha256((root / name / 'frames' / f'frame_{frame:04d}.bmp').read_bytes()).hexdigest()
    return json.loads(summary.read_text()), digest

def baked(values):
    result = copy.deepcopy(scene)
    au = author(result)
    au['motion_paths']['bindings'] = []
    tracks = au['scene_timeline']['tracks']
    au['scene_timeline']['tracks'] = tracks = [t for t in tracks if t['target_id'] != 'camera/main']
    props = [('position', 'vec3', 'world_distance', [v / result['world_scale'] for v in values[:3]]),
             ('yaw', 'scalar', 'radians', values[3]), ('pitch', 'scalar', 'radians', values[4]),
             ('fov_y', 'scalar', 'degrees', values[5])]
    for prop, typ, unit, value in props:
        tracks.append({'id': 'baked-' + prop, 'target_id': 'camera/main', 'property_id': 'camera/' + prop,
                       'value_type': typ, 'unit': unit, 'source': 'authored', 'enabled': True,
                       'keys': [{'frame': 0, 'value': value, 'interpolation': 'linear',
                                 'incoming_handle': {'frame_offset': 0, 'value_offset': 0},
                                 'outgoing_handle': {'frame_offset': 0, 'value_offset': 0}}]})
    return result

records = []
for frame, values in zip((0, 30, 60, 119), expected):
    summary, digest = render(scene, frame, f'route_{frame}')
    c = summary['evaluated_camera']
    actual = c['position'] + [c['yaw'], c['pitch'], c['fov_y']]
    assert all(math.isclose(x, y, rel_tol=1e-9, abs_tol=1e-8) for x, y in zip(actual, values)), (frame, actual, values)
    _, reference = render(baked(values), frame, f'baked_camera_{frame}')
    assert digest == reference, (frame, 'baked camera pixel mismatch')
    records.append({'frame': frame, 'sha256': digest, 'pixel_exact_match': True})
assert len({r['sha256'] for r in records}) > 1, 'camera output must change'
(root / 'dm3_camera_render_verification.json').write_text(json.dumps(records, indent=2))
print('D-M3 camera render PASS: native/headless pose and exact baked pixels at 0/30/60/119')
