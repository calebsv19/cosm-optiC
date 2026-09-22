#!/usr/bin/env python3
"""Run native lifecycle faults on copied fixtures; never edit archived proof."""
import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def copy_fixture(source, target):
    source = source.resolve(strict=True)
    shutil.copytree(source.parent, target, ignore=shutil.ignore_patterns(
        '*.ppm', '*.bmp', '*.log', 'renders', 'videos', 'cache', 'data', '__pycache__'))
    copied = target / source.name
    # Rebind local absolute resource references while preserving relative asset
    # layout and image bytes/hash pins. Outside dependencies remain read-only.
    copied.write_text(source.read_text().replace(str(source.parent), str(target)))
    return copied


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--graph-scene', required=True, type=Path)
    parser.add_argument('--sampling-scene', required=True, type=Path)
    parser.add_argument('--output-root', required=True, type=Path)
    args = parser.parse_args()
    sources = [args.graph_scene.resolve(strict=True), args.sampling_scene.resolve(strict=True)]
    hashes = [hashlib.sha256(p.read_bytes()).hexdigest() for p in sources]
    out = args.output_root.resolve()
    # Avoid copying a fixture directory into itself recursively.
    assert all(not out.is_relative_to(p.parent) for p in sources)
    out.mkdir(parents=True, exist_ok=False)
    graph = copy_fixture(sources[0], out / 'graph')
    sampling = copy_fixture(sources[1], out / 'sampling')
    runtime = graph.parent / 'data/runtime'
    runtime.mkdir(parents=True)
    (runtime / 'animation_config.json').write_text(json.dumps({
        'editorMode': 1, 'spaceMode': 1, 'windowWidth': 1280, 'windowHeight': 800,
        'inputRoot': str(ROOT / 'config'), 'outputRoot': str(runtime),
        'videoOutputRoot': str(out / 'videos')}))
    (runtime / 'scene_config.json').write_text(json.dumps({'window': {'width': 1280, 'height': 800}}))
    binary = ROOT / f'build/toolchains/clang/{platform.machine()}/tests/scene_editor_workspace_visual_test'
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(ROOT),
               RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out / 'cache'),
               OPTIC_T0_SAMPLING_SCENE=str(sampling))
    with (out / 'native.log').open('w') as log:
        result = subprocess.run([str(binary), str(graph.parent), str(graph), '--surface-lifecycle-t0'],
                                env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
    assert [hashlib.sha256(p.read_bytes()).hexdigest() for p in sources] == hashes
    assert result.returncode == 0, (result.returncode, out / 'native.log')
    receipt = json.loads((graph.parent / 'surface_lifecycle_t0.json').read_text())
    assert receipt['early_failures_preserved'] == 2 and receipt['late_failures_cleared'] == 4
    assert all(value is True for key, value in receipt.items() if key not in (
        'early_failures_preserved', 'late_failures_cleared'))
    receipt['source_scene_sha256'] = dict(zip(('graph', 'sampling'), hashes))
    receipt['archived_scene_files_unchanged'] = True
    (out / 'acceptance.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
