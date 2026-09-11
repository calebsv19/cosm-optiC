#!/usr/bin/env python3
"""Opt-in native E0/E1 acceptance. All writes stay in a new supplied output root."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from smooth_mesh_reflection.prepare_reflection_matrix import build_request


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ppm(path):
    with path.open('rb') as file:
        assert file.readline().strip() == b'P6'
        line = file.readline()
        while line.startswith(b'#'):
            line = file.readline()
        width, height = map(int, line.split())
        assert file.readline().strip() == b'255'
        pixels = file.read()
    assert len(pixels) == width * height * 3
    return width, height, pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root', type=Path, required=True)
    args = parser.parse_args()
    out = args.output_root.resolve()
    out.mkdir(parents=True, exist_ok=False)
    project = out / 'scene'
    shutil.copytree(ROOT / 'tests/fixtures/mesh_asset_runtime_spheres', project)
    scene = project / 'scene_runtime.json'
    before = json.loads(scene.read_text())
    probe = {'owner': 'acceptance', 'nested': [7, {'keep': 'unchanged'}]}
    before['extensions']['e0_preservation_probe'] = probe
    scene.write_text(json.dumps(before, indent=2))
    runtime = out / 'data/runtime'
    runtime.mkdir(parents=True)
    (runtime / 'animation_config.json').write_text(json.dumps({
        'editorMode': 1, 'spaceMode': 1, 'windowWidth': 1280, 'windowHeight': 800,
        'inputRoot': str(ROOT / 'config'), 'outputRoot': str(runtime),
        'videoOutputRoot': str(out / 'videos')}))
    (runtime / 'scene_config.json').write_text(json.dumps({'window': {'width': 1280, 'height': 800}}))
    binary_root = ROOT / f'build/toolchains/clang/{platform.machine()}'
    binary = binary_root / 'tests/scene_editor_workspace_visual_test'
    source = ROOT / 'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/imports/tetrahedron_ascii.stl'
    source_hash = digest(source)
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(ROOT))
    for stage, argument in [('edit', str(source)), ('fresh-reopen', '--reopen')]:
        with (out / f'{stage}.log').open('w') as log:
            subprocess.run([str(binary), str(out), str(scene), argument], env=env,
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
    after = json.loads(scene.read_text())
    assert after['extensions']['e0_preservation_probe'] == probe
    assert len(after['objects']) == len(before['objects']) + 1
    for original, retained in zip(before['objects'], after['objects']):
        assert original == retained, original['object_id']
    imported = after['objects'][-1]
    assert abs(imported['transform']['position']['x'] - 0.25) < 1e-6
    recipe = imported['extensions']['ray_tracing']['managed_mesh']['compiled']['recipe']
    assert recipe['import']['source_to_asset_scale'] == 0.001
    assert recipe['source_sha256'] == source_hash
    assert digest(source) == source_hash
    # The existing header must not be overdrawn by scrolled sidebar labels.
    width, height, initial = ppm(out / 'workspace_library_top.ppm')
    width2, height2, scrolled = ppm(out / 'workspace_library_scrolled.ppm')
    assert (width, height) == (width2, height2)
    header_bytes = width * round(72 * width / 1024) * 3  # Compact header, normal text scale.
    assert initial[:header_bytes] == scrolled[:header_bytes], 'Scrolled pane escaped its clipping boundary'
    request = build_request(project, 'e01', 'tlas_blas_parity')
    request['render'].update(width=320, height=200)
    request_path = project / 'request_e01.json'
    request_path.write_text(json.dumps(request, indent=2))
    with (out / 'render.log').open('w') as log:
        subprocess.run([str(binary_root / 'tools/cli/ray_tracing_render_headless'),
                        '--request', str(request_path), '--render'],
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
    frame = project / 'renders/e01_tlas_blas_parity/frames/frame_0000.bmp'
    assert frame.exists() and len(set(frame.read_bytes()[122:])) > 4
    report = {'status': 'passed', 'source_stl_sha256': source_hash,
              'unknown_fields_preserved': True, 'original_objects_preserved': True,
              'source_unit_scale': recipe['import']['source_to_asset_scale'],
              'imported_object_id': imported['object_id'], 'fresh_process_reopen': True,
              'header_clip_pixels_equal': True, 'render_sha256': digest(frame),
              'scene_sha256': digest(scene)}
    (out / 'acceptance.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
