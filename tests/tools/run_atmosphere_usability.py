#!/usr/bin/env python3
"""Native atmosphere acceptance in disposable scene copies, never the input."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scene', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--regression-fixture', type=Path)
    args = parser.parse_args()
    scene = args.scene.resolve()
    before = hashlib.sha256(scene.read_bytes()).hexdigest()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    working = out / 'complex'
    shutil.copytree(scene.parent, working / 'scene', ignore=shutil.ignore_patterns('*.optic-agent.json'))
    shutil.copytree(ROOT / 'config', working / 'config')
    matrix = [(working, working / 'scene' / scene.name, '--atmosphere-usability'),
              (working, working / 'scene' / scene.name, '--atmosphere-usability-reopen')]
    if args.regression_fixture:
        fixture = args.regression_fixture.resolve()
        small = out / 'regression'
        small.mkdir()
        shutil.copytree(ROOT / 'config', small / 'config')
        shutil.copy2(fixture / 'scene_runtime.json', small / 'scene_runtime.json')
        shutil.copytree(fixture / 'assets', small / 'assets')
        for mode in ('--atmosphere', '--atmosphere-reopen', '--atmosphere-mcp',
                     '--atmosphere-water', '--atmosphere-water-reopen'):
            matrix.append((small, small / 'scene_runtime.json', mode))
    results = []
    for work, copied_scene, mode in matrix:
        env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(work),
                   RAY_TRACING_ATMOSPHERE_HELPER=str(ROOT / 'tools/scene_atmosphere_assets.py'),
                   OPTIC_ATMOSPHERE_MCP_TEST=str(ROOT / 'tests/integration/check_atmosphere_mcp.py'))
        log = out / (mode.removeprefix('--') + '.log')
        with log.open('w') as stream:
            result = subprocess.run([str(args.native.resolve()), str(work), str(copied_scene), mode],
                                    env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=600)
        results.append(dict(mode=mode, exit_code=result.returncode, log=str(log)))
        print(json.dumps(results[-1]), flush=True)
        (out / 'native_results.json').write_text(json.dumps(results, indent=2) + '\n')
        if result.returncode:
            print(log.read_text()[-2000:])
            raise SystemExit(result.returncode)
    after = hashlib.sha256(scene.read_bytes()).hexdigest()
    assert before == after, 'Original scene changed during isolated verification'
    (out / 'input_preservation.json').write_text(json.dumps(dict(scene=str(scene), before=before, after=after), indent=2) + '\n')


if __name__ == '__main__':
    main()
