#!/usr/bin/env python3
"""Prove the actual imported-water payload through the native headless CLI.

Caller supplies an existing water request and a fresh disposable output root.
No scene, assets, package, installed app or live render is modified.
"""
import argparse
import copy
import hashlib
import json
import math
import os
import subprocess
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cli', required=True, type=Path)
    ap.add_argument('--request', required=True, type=Path)
    ap.add_argument('--output-root', required=True, type=Path)
    a = ap.parse_args()
    source = a.request.resolve()
    original = source.read_bytes()
    q = copy.deepcopy(json.loads(original))
    root = a.output_root.resolve()
    root.mkdir(parents=True, exist_ok=False)
    for section, key in [('scene', 'runtime_scene_path'), ('volume', 'source_path')]:
        path = q.get(section, {}).get(key)
        if path and not Path(path).is_absolute():
            q[section][key] = str((source.parent / path).resolve())
    q['run_id'] = 'clear-water-contract'
    q['output']['root'] = str(root / 'output')
    q['output']['overwrite'] = False
    q['progress'] = dict(summary_path=str(root / 'summary.json'),
                         progress_path=str(root / 'progress.json'))
    request = root / 'request.json'
    request.write_text(json.dumps(q, indent=2))
    env = {k: v for k, v in os.environ.items() if not k.startswith('RAY_TRACING_')}
    with (root / 'run.log').open('w') as log:
        subprocess.run([str(a.cli.resolve()), '--request', str(request),
                        '--preflight', '--summary-file-only'],
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120, env=env)
    s = json.loads((root / 'summary.json').read_text())
    water = s['water_surface']
    p, material = water['payload'], water['material']
    close = lambda x, y: math.isclose(x, y, rel_tol=0, abs_tol=2e-9)
    expected = [math.exp(-c * material['absorption_distance_m'])
                for c in material['absorption_rgb']]
    checks = dict(imported=water['loaded'] and water['mesh_attached'] and water['triangle_count'] > 0,
        interface_neutral=all(close(x, 1) for x in p['tint_rgb']),
        no_opaque_mixture=close(p['transparency'], 1),
        no_diffuse=close(p['diffuse_weight'], 0),
        absorption_separate=all(close(x, y) for x, y in zip(p['absorption_transmittance_rgb'], expected)),
        ior_retained=close(p['ior'], material['ior']),
        roughness_retained=close(p['roughness'], material['roughness']),
        fresnel_owned=close(p['reflectivity'], ((p['ior'] - 1) / (p['ior'] + 1)) ** 2),
        input_unchanged=source.read_bytes() == original)
    result = dict(passed=all(checks.values()), checks=checks,
                  request_sha256=hashlib.sha256(original).hexdigest(), payload=p)
    (root / 'result.json').write_text(json.dumps(result, indent=2))
    assert result['passed'], result
    print(json.dumps(result))


if __name__ == '__main__':
    main()
