#!/usr/bin/env python3
"""Real editor lifecycle, durable V3 routing and lit headless/native parity.

Requires an available native window server. No installed application is changed.
"""
import argparse
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import spectral_room_testbench as bench
from spectral_components import verify
from optical_comparison_artifacts import bmp_rgba, png_bytes


def close(a, b):
    if isinstance(a, dict):
        assert a.keys() == b.keys()
        for k in a:
            close(a[k], b[k])
    elif isinstance(a, list):
        assert len(a) == len(b)
        for x, y in zip(a, b):
            close(x, y)
    elif isinstance(a, float):
        # Camera direction -> look-at -> direction incurs floating roundoff.
        assert math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-12), (a, b)
    else:
        assert a == b, (a, b)


def qualify(root, cli, harness):
    root.mkdir(parents=True)
    shutil.copytree(bench.REPO / 'config', root / 'config')
    (root / 'shaders').symlink_to(bench.REPO /
        'third_party/codework_shared/vk_renderer/shaders', target_is_directory=True)
    q, scene = bench.room()
    scene['objects'] = scene['objects'][:6]
    profile = q['spectral']
    profile['quality_profile'] = 'surface_path_v1'
    p = profile['surface_path_v1']
    p.update(samples=16, max_depth=4, probes=[], path_trace={'x': 16, 'y': 12, 'sample': 0})
    p['materials'] = p['materials'][:6]
    scene['extensions'] = {'ray_tracing': {'spectral_transport_v1': profile,
        'editor_render_integrator_v1': 'disney_v3_spectral'}}
    (root / 'scene.json').write_text(json.dumps(scene, indent=2))
    for mode in ('--v3-integrated', '--v3-integrated-reopen'):
        with (root / (mode[2:] + '.log')).open('w') as log:
            subprocess.run([str(harness), str(root), str(root / 'scene.json'), mode],
                cwd=bench.REPO, env=dict(os.environ, OPTIC_V3_MCP_TEST=str(
                    bench.REPO / 'tests/integration/check_v3_transport_mcp.py')),
                stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
    job = json.loads((root / 'v3-expected.json').read_text())['v3-final']
    report = json.loads(Path(job['spectral_transport_path']).read_text())
    # Read the exact camera from ordinary File Save; re-encoding a look-at
    # vector would introduce a tiny direction round-trip before raw comparison.
    request = {'schema_version': 'ray_tracing_agent_render_request_v2',
        'scene': {'runtime_scene_path': str(root / 'scene.json')},
        'render': {'integrator_3d': 'disney_v3_spectral', 'width': 32, 'height': 24},
        'output': {'root': 'headless-output', 'overwrite': False}}
    (root / 'request.json').write_text(json.dumps(request, indent=2))
    with (root / 'headless.log').open('w') as log:
        subprocess.run([str(cli), '--request', 'request.json', '--render', '--summary',
            'summary.json', '--summary-file-only'], cwd=root, stdout=log,
            stderr=subprocess.STDOUT, check=True, timeout=90)
    summary = json.loads((root / 'summary.json').read_text())
    a, b = bmp_rgba(Path(job['image_path'])), bmp_rgba(root / 'headless-output/frames/frame_0000.bmp')
    assert a == b
    assert report['film_components_xyz']['surface_indirect'][1] > 0
    verify(report, Path(job['image_path']))
    verify(summary['spectral_transport'], root/'headless-output/frames/frame_0000.bmp')
    native_export = report['linear_components']
    headless_export = summary['spectral_transport']['linear_components']
    assert native_export['sha256'] == headless_export['sha256']
    # Distinct job owners choose their existing image basename; payload is identical.
    report['linear_components']['file'] = headless_export['file']
    close(report, summary['spectral_transport'])
    canceled = json.loads((root/'v3-expected.json').read_text())['v3-cancel']
    assert not Path(canceled['image_path']+'.components.npy').exists()
    assert not list(Path(canceled['image_path']).parent.glob('*.components.npy.tmp.*'))
    (root / 'native.png').write_bytes(png_bytes(*a))
    (root / 'result.json').write_text(json.dumps({'exact_rgba': True,
        'report_tolerance': 1e-12, 'profile_save_reopen': True,
        'native_ui_and_job_lifecycle': True, 'actual_stdio_mcp': True,
        'in_flight_cancel_no_final_image': True}, indent=2))
    print('PASS lit native/headless RGBA identity, report parity, profile Save/reopen, UI and cancellation')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cli', required=True, type=Path)
    parser.add_argument('--harness', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    qualify(args.output.resolve(), args.cli.resolve(), args.harness.resolve())
