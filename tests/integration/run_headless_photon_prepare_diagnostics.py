#!/usr/bin/env python3
"""Contained preparation diagnostics, transport invariance and normal-render proof."""
import argparse
import copy
import json
from pathlib import Path
import subprocess


def run(cli, request, args, expected=0):
    p = subprocess.run([str(cli), '--request', str(request), '--summary-file-only', *args],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert p.returncode == expected, (args, p.returncode, p.stdout[-3000:])
    return p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cli', type=Path, required=True)
    ap.add_argument('--output-root', type=Path, required=True)
    a = ap.parse_args()
    root = a.output_root.resolve()
    root.mkdir(parents=True, exist_ok=False)
    repo = Path(__file__).resolve().parents[2]
    fixture = repo / 'tests/fixtures/caustic_probe_glass_sphere/request_disney_v2.json'
    base = json.loads(fixture.read_text())
    scene = json.loads((fixture.parent / 'scene_runtime.json').read_text())
    for obj in scene['objects']:
        ld = obj.get('extensions', {}).get('line_drawing', {})
        if 'runtime_mesh_path' in ld:
            ld['runtime_mesh_path'] = str((fixture.parent / ld['runtime_mesh_path']).resolve())
    glass = scene['extensions']['ray_tracing']['authoring']['object_materials'][1]
    glass.update(glass_transport_override=True, glass_thin_walled=False,
                 glass_ior=1.45, glass_transmission=0.97, glass_absorption_distance=12)
    scene_path = root / 'solid_glass_scene.json'
    scene_path.write_text(json.dumps(scene))
    base['scene']['runtime_scene_path'] = str(scene_path)
    base['render'].update(width=32, height=24, temporal_frames=4, denoise_enabled=False)
    base['inspection'].update(
        caustic_sidecar_enabled=False, caustic_product_mode='photon_map',
        caustic_transport_engine='photon_map', caustic_photon_consumer='direct_map',
        caustic_surface_query_enabled=True, caustic_volume_query_enabled=False,
        caustic_photon_render_prep_population_enabled=True,
        caustic_photon_render_contribution_enabled=True,
        caustic_photon_surface_diagnostics_enabled=True,
        caustic_photon_sample_budget=1024, caustic_photon_emission_seed=1592594996,
        caustic_photon_max_path_depth=10, caustic_photon_surface_radiance_scale=1.0,
        object_audit_enabled=False)
    def request(name):
        q = copy.deepcopy(base)
        out = root / name / 'output'
        out.parent.mkdir()
        q['output'].update(root=str(out), overwrite=False)
        q['progress'] = {'summary_path': str(out.parent / 'summary.json'),
                         'progress_path': str(out / 'progress.json')}
        p = out.parent / 'request.json'
        p.write_text(json.dumps(q))
        return p, out
    invalid, invalid_out = request('invalid')
    for ids in ('', '-1', '1,1', '1,', 'abc', '18446744073709551616',
                ','.join(map(str, range(1, 66)))):
        run(a.cli, invalid, ['--preflight', '--photon-prepare-diagnostics', '--photon-path-ids', ids], 2)
    run(a.cli, invalid, ['--photon-prepare-diagnostics', '--photon-path-ids', '1', '--render'], 2)
    run(a.cli, invalid, ['--preflight', '--photon-path-ids', '1'], 2)
    assert not list(invalid_out.glob('*.jsonl'))
    captures = []
    for name, ids in [('capture_a', '1,2'), ('capture_b', '2,3')]:
        p, out = request(name)
        run(a.cli, p, ['--preflight', '--photon-prepare-diagnostics', '--photon-path-ids', ids])
        s = json.loads((out.parent / 'summary.json').read_text())
        assert s['prepared_frame'] and not s['rendered_frames'] and s['frames_rendered'] == 0
        assert not list(out.rglob('*.bmp'))
        rows = [json.loads(x) for x in (out / 'photon_transport_paths_0000.jsonl').read_text().splitlines()]
        assert sorted(x['photon_id'] for x in rows) == list(map(int, ids.split(',')))
        assert all('reject_reason' in x for x in rows)
        termination = json.loads((out / 'photon_termination_aggregate_0000.json').read_text())
        assert termination['observed_paths'] == 1024
        assert sum(termination['terminations'].values()) == 1024
        assert termination['unmatched_exports_omitted'] == max(0, termination['unmatched_full_paths'] - 64)
        unmatched = (out / 'photon_unmatched_paths_0000.jsonl').read_text().splitlines()
        assert len(unmatched) == termination['exported_unmatched_paths'] <= 64
        records = (out / 'photon_surface_records_0000.jsonl').read_bytes()
        assert records, 'fixture must have deposits for invariance proof'
        captures.append(records)
    assert captures[0] == captures[1], 'observer selection changed the photon map'
    p = root / 'capture_a/request.json'
    captured_path = root / 'capture_a/output/photon_transport_paths_0000.jsonl'
    before = captured_path.read_bytes()
    run(a.cli, p, ['--preflight', '--photon-prepare-diagnostics', '--photon-path-ids', '1,2'], 2)
    assert captured_path.read_bytes() == before, 'overwrite refusal changed retained paths'
    p, out = request('occupied_records')
    out.mkdir()
    occupied = out / 'photon_surface_records_0000.jsonl'
    occupied.write_text('retained evidence\n')
    run(a.cli, p, ['--preflight', '--photon-prepare-diagnostics', '--photon-path-ids', '1'], 2)
    assert occupied.read_text() == 'retained evidence\n'
    assert not list(out.glob('photon_transport_paths*'))
    for stem, suffix in [('photon_termination_aggregate', 'json'), ('photon_unmatched_paths', 'jsonl'), ('photon_surface_live_queries', 'jsonl'), ('photon_receiver_accounting', 'json')]:
        p, out = request('occupied_' + stem)
        out.mkdir()
        occupied = out / (stem + '_0000.' + suffix)
        occupied.write_text('retained termination evidence\n')
        run(a.cli, p, ['--preflight', '--photon-prepare-diagnostics', '--photon-path-ids', '1'], 2)
        assert occupied.read_text() == 'retained termination evidence\n'
        assert not list(out.glob('photon_transport_paths*'))
    p, out = request('occupied_aggregate')
    out.mkdir()
    occupied = out / 'photon_surface_query_aggregate_0000.json'
    occupied.write_text('retained aggregate evidence\n')
    run(a.cli, p, ['--preflight', '--photon-prepare-diagnostics', '--photon-path-ids', '1'], 2)
    assert occupied.read_text() == 'retained aggregate evidence\n'
    assert not list(out.glob('photon_transport_paths*'))
    p, out = request('missing_id')
    run(a.cli, p, ['--preflight', '--photon-prepare-diagnostics', '--photon-path-ids', '2048'], 7)
    s = json.loads((out.parent / 'summary.json').read_text())
    assert s['prepared_frame'] and not s['rendered_frames'] and 'diagnostics failed' in s['diagnostics']
    p, out = request('ordinary_render')
    run(a.cli, p, ['--render'])
    s = json.loads((out.parent / 'summary.json').read_text())
    assert s['rendered_frames'] and s['frames_rendered'] == 1
    assert len(list(out.rglob('*.bmp'))) == 1
    assert not list(out.glob('photon_transport_paths*'))
    assert (out / 'photon_surface_records_0000.jsonl').read_bytes() == captures[0]
    aggregate = json.loads((out / 'photon_surface_query_aggregate_0000.json').read_text())
    histogram = aggregate['effective_sample_histogram']
    assert aggregate['capture_stage'] == 'before_synthetic_diagnostic_grid'
    assert aggregate['domain'] == 'query_calls_since_last_consumer_reset'
    assert sum(histogram) == aggregate['query_count']
    assert sum(histogram[1:]) == aggregate['positive_query_count']
    assert aggregate['query_count'] > 0  # The tiny camera fixture may miss all deposits.
    assert 0 <= aggregate['positive_query_count'] <= aggregate['query_count']
    assert aggregate['undersampled_positive_query_count'] <= aggregate['positive_query_count']
    accounting = json.loads((out / 'photon_receiver_accounting_0000.json').read_text())
    assert accounting['domain'] == 'live_query_calls_before_synthetic_grid'
    assert accounting['overflow_query_count'] == 0
    assert sum(b['query_count'] for b in accounting['receivers']) == aggregate['query_count']
    live = (out / 'photon_surface_live_queries_0000.jsonl').read_text().splitlines()
    assert len(live) == min(8192, aggregate['query_count'])
    for name in ('capture_a', 'capture_b'):
        prep = json.loads((root / name / 'output/photon_receiver_accounting_0000.json').read_text())
        assert prep['receivers'] == [], 'preparation grid must not masquerade as render queries'
    result = {'schema': 'photon_prepare_tests_v1', 'passed': True,
              'negative_cases': 17, 'selected_path_sets': 2,
              'map_byte_equivalence': True, 'preparation_writes_no_frames': True,
              'ordinary_render_completed': True, 'live_aggregate_checked': True}
    (root / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
