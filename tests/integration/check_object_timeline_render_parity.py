#!/usr/bin/env python3
"""After native --object-timeline, prove saved move/hold/resume in fresh renders."""
import argparse
import json
import math
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('scratch', type=Path)
p.add_argument('--scene', required=True, type=Path)
p.add_argument('--cli', required=True, type=Path)
a = p.parse_args()
root = a.scratch.resolve()
expected = json.loads((root / 'object_expected.json').read_text())
for frame, offset in [(10, 5), (20, 10), (30, 10), (40, 10), (50, 15), (60, 20)]:
    request = {
        'schema_version': 'ray_tracing_agent_render_request_v1',
        'run_id': f'object_timeline_{frame}',
        'scene': {'runtime_scene_path': str(a.scene.resolve())},
        'volume': {'enabled': False},
        'render': {'start_frame': frame, 'frame_count': 1, 'width': 64, 'height': 40, 'temporal_frames': 1},
        'output': {'root': str(root / f'render_{frame}'), 'overwrite': True},
    }
    path = root / f'request_{frame}.json'
    path.write_text(json.dumps(request, indent=2) + '\n')
    summary = root / f'summary_{frame}.json'
    with (root / f'render_{frame}.log').open('w') as log:
        subprocess.run([str(a.cli.resolve()), '--request', str(path), '--render',
                        '--summary', str(summary), '--summary-file-only'],
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
    result = json.loads(summary.read_text())
    objects = [o for o in result['evaluated_objects'] if o['object_id'] == expected['object_id']]
    assert len(objects) == 1
    assert math.isclose(objects[0]['position'][0], (expected['base_x'] + offset) * expected['scale'], abs_tol=1e-8)
    assert result['frames_rendered'] == 1 and result['evaluated_scene_last_frame'] == frame
    print(f'Object timeline native -> fresh headless render frame {frame}: PASS')
