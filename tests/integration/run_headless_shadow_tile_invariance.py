#!/usr/bin/env python3
"""Real soft-shadow renders must not change with tile partition or denoising.

Uses a fresh caller-owned output root. The fixed-budget raw image is an
independent control for premature convergence of the stratified light sequence.
"""
import argparse
import copy
import json
import os
from pathlib import Path
import struct
import subprocess


def pixels(path):
    data = Path(path).read_bytes()
    assert data[:2] == b'BM'
    offset = struct.unpack_from('<I', data, 10)[0]
    return data[offset:]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cli', type=Path, required=True)
    ap.add_argument('--output-root', type=Path, required=True)
    a = ap.parse_args()
    root = a.output_root.resolve()
    root.mkdir(parents=True, exist_ok=False)
    repo = Path(__file__).resolve().parents[2]
    fixture = repo / 'tests/fixtures/caustic_probe_glass_sphere'
    scene = json.loads((fixture / 'scene_runtime.json').read_text())
    for obj in scene['objects']:
        ld = obj.get('extensions', {}).get('line_drawing', {})
        if 'runtime_mesh_path' in ld:
            ld['runtime_mesh_path'] = str((fixture / ld['runtime_mesh_path']).resolve())
    material = scene['extensions']['ray_tracing']['authoring']['object_materials'][1]
    material.update(material_id=0, alpha=1, roughness=.82,
                    reflectivity=0, object_color=10066329)
    scene['lights'] = [dict(light_id='shadow_rect', kind='rect',
        position=dict(x=-1, y=0, z=3.4), axis_u=dict(x=1, y=0, z=0),
        axis_v=dict(x=0, y=1, z=0), normal=dict(x=0, y=0, z=-1),
        width=2, height=1.5, intensity=2, falloff_distance=8,
        color=dict(r=1, g=1, b=1), enabled=True,
        emission_profile='one_sided', radiometric_mode='legacy_intensity')]
    scene_path = root / 'scene.json'
    scene_path.write_text(json.dumps(scene))
    base = json.loads((fixture / 'request_disney_v2.json').read_text())
    base['scene']['runtime_scene_path'] = str(scene_path)
    base['render'].update(width=160, height=104, temporal_frames=32,
                          use_tiled_renderer=True)
    base['inspection'].update(caustic_sidecar_enabled=False, caustic_mode='off',
        object_audit_enabled=False, secondary_diffuse_samples_3d=1,
        transmission_samples_3d=1)
    base['inspection'].pop('light_intensity', None)
    base['inspection'].pop('light_radius', None)
    base['resources'] = dict(max_workers=3, cpu_percent=75, reserve_cpu_count=2)
    env = {k: v for k, v in os.environ.items() if not k.startswith('RAY_TRACING_')}
    images = {}
    stats = {}
    for name, tile, dn, adaptive in [('fixed', 16, False, False),
            ('raw16', 16, False, True), ('raw32', 32, False, True),
            ('raw23', 23, False, True), ('filtered16', 16, True, True),
            ('filtered32', 32, True, True), ('filtered23', 23, True, True)]:
        case = root / name
        case.mkdir()
        q = copy.deepcopy(base)
        q['run_id'] = name
        q['render'].update(tile_size=tile, denoise_enabled=dn,
                           adaptive_sampling_enabled=adaptive)
        q['output'].update(root=str(case / 'output'), overwrite=False)
        q['progress'] = dict(summary_path=str(case / 'summary.json'),
                            progress_path=str(case / 'output/progress.json'))
        request = case / 'request.json'
        request.write_text(json.dumps(q))
        with (case / 'run.log').open('w') as log:
            subprocess.run([str(a.cli.resolve()), '--request', str(request),
                '--summary-file-only', '--render'], cwd=repo, env=env,
                stdout=log, stderr=subprocess.STDOUT, check=True)
        summary = json.loads((case / 'summary.json').read_text())
        assert summary['diagnostics'] == 'ok' and summary['frames_rendered'] == 1
        images[name] = pixels(summary['outputs']['first_frame_path'])
        stats[name] = summary['render_stats']['temporal_pixels_skipped']
    checks = {name: images[name] == images['fixed'] for name in ('raw16', 'raw32', 'raw23')}
    checks.update({name: images[name] == images['filtered16']
                   for name in ('filtered32', 'filtered23')})
    result = dict(passed=all(checks.values()), checks=checks,
                  skipped_pixel_samples=stats, photon_contribution=False)
    (root / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result))
    assert result['passed'], 'Tile-dependent shadow output or incomplete surface budget'


if __name__ == '__main__':
    main()
