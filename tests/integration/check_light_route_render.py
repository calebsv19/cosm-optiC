#!/usr/bin/env python3
"""Check M3 native light poses and pixels against explicit baked light channels."""
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
expected = json.loads((root / 'dm3_light_expected.json').read_text())

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
    for track in tracks:
        if track['property_id'] == 'light/route_progress':
            track['enabled'] = False
        if track['property_id'] == 'light/path_progress':
            track['enabled'] = True
            for key in track['keys']:
                key['value'] = 0
                key['interpolation'] = 'linear'
                key['incoming_handle'] = {'frame_offset': 0, 'value_offset': 0}
                key['outgoing_handle'] = {'frame_offset': 0, 'value_offset': 0}
    # Independent reference: legacy spatial carrier at the captured world position.
    x, y, z = [v / result['world_scale'] for v in values[:3]]
    au['light_timeline']['spatial_path'] = {
        'path': {'mode': 'LINEAR', 'points': [
            {'x': x, 'y': y, 'rotation': 0, 'handleLink': False},
            {'x': x + 1, 'y': y, 'rotation': 0, 'handleLink': False}]},
        'depth': {'points': [{'z': z, 'lookPitch': 0}, {'z': z, 'lookPitch': 0}]}}
    return result

records = []
for frame, values in zip((0, 30, 60, 119), expected):
    summary, digest = render(scene, frame, f'route_{frame}')
    c = summary['evaluated_light']
    actual = c['position'] + [c['intensity']]
    assert all(math.isclose(x, y, rel_tol=1e-9, abs_tol=1e-8) for x, y in zip(actual, values)), (frame, actual, values)
    _, reference = render(baked(values), frame, f'baked_light_{frame}')
    assert digest == reference, (frame, 'baked light pixel mismatch')
    records.append({'frame': frame, 'sha256': digest, 'pixel_exact_match': True})
assert len({r['sha256'] for r in records}) > 1, 'light output must change'
(root / 'dm3_light_render_verification.json').write_text(json.dumps(records, indent=2))
print('D-M3 light render PASS: native/headless pose and exact baked pixels at 0/30/60/119')
