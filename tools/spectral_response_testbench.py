#!/usr/bin/env python3
"""Run matched visible spectral controls through ordinary saved-profile V3.

Fresh case directories and sealed requests prevent accidental overwrite/retry.
This tool never changes renderer source, reconstruction or installed settings.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import tempfile
import time

from optical_comparison_artifacts import bmp_rgba, png_bytes
import spectral_response_fixture as fixture


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, obj):
    path = Path(path)
    with tempfile.NamedTemporaryFile(mode='w', dir=path.parent, delete=False) as f:
        temporary = Path(f.name)
        try:
            json.dump(obj, f, indent=2)
            f.write('\n')
            f.flush()
            temporary.replace(path)
        finally:
            if temporary.exists():
                temporary.unlink()


def render(root, cli, name, request, scene, timeout=900):
    d = root/name
    request = copy.deepcopy(request)
    request['scene'] = {'runtime_scene_path': 'scene.json'}
    request['output'] = {'root': 'output', 'overwrite': False}
    recipe = hashlib.sha256(json.dumps([request, scene], sort_keys=True).encode()).hexdigest()
    if d.exists():
        receipt = json.loads((d/'receipt.json').read_text()) if (d/'receipt.json').is_file() else {}
        if receipt.get('recipe') == recipe and receipt.get('status') == 'completed':
            assert receipt['image_sha256'] == digest(d/'output/frames/frame_0000.bmp')
            assert receipt['summary_sha256'] == digest(d/'summary.json')
            assert receipt['scene_sha256'] == digest(d/'scene.json')
            assert receipt['request_sha256'] == digest(d/'request.json')
            assert receipt['runtime_sha256'] == digest(cli)
            return json.loads((d/'summary.json').read_text())
        raise RuntimeError(f'Existing uncertain/different case: inspect {d}; no resubmission')
    d.mkdir(parents=True)
    (d/'config').symlink_to(fixture.REPO/'config', target_is_directory=True)
    write(d/'scene.json', scene)
    write(d/'request.json', request)
    receipt = dict(status='preparing', recipe=recipe, runtime_sha256=digest(cli),
        scene_sha256=digest(d/'scene.json'), request_sha256=digest(d/'request.json'))
    write(d/'receipt.json', receipt)
    t = time.monotonic()
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
    with (d/'log.txt').open('w') as log:
        p = subprocess.Popen([str(cli), '--request', 'request.json', '--render',
            '--summary', 'summary.json', '--summary-file-only'], cwd=d, env=env,
            stdout=log, stderr=subprocess.STDOUT)
        receipt.update(status='running', pid=p.pid)
        write(d/'receipt.json', receipt)
        try:
            result = p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            p.terminate()
            try:
                p.wait(timeout=10)
            except subprocess.TimeoutExpired:
                p.kill(); p.wait()
            receipt.update(status='timed_out', seconds=time.monotonic()-t)
            write(d/'receipt.json', receipt)
            raise
    if result:
        receipt.update(status='failed', exit_code=result)
        write(d/'receipt.json', receipt)
        raise RuntimeError(f'Render failed: {d}/log.txt')
    summary = json.loads((d/'summary.json').read_text())
    transport = summary['spectral_transport']
    assert transport['route'] == 'surface_path_v1' and transport['status'] == 'completed'
    assert transport['material_representation'] == 'explicit_spectral_reflectance'
    assert transport['termination']['all_started_paths_classified']
    assert not transport['photon_maps_used']
    w, h, rgba = bmp_rgba(d/'output/frames/frame_0000.bmp')
    (d/'image.png').write_bytes(png_bytes(w, h, rgba))
    receipt.update(status='completed', seconds=time.monotonic()-t,
        image_sha256=digest(d/'output/frames/frame_0000.bmp'), summary_sha256=digest(d/'summary.json'))
    write(d/'receipt.json', receipt)
    print(name, round(receipt['seconds'], 2), flush=True)
    return summary


def initialize(root):
    root.mkdir(parents=True, exist_ok=True)
    path = root/'plan.json'
    plan = dict(schema='optic_visible_response_bench_v1', source_normalization='equal_incident_CIE_Y',
        camera_samples=1024, max_surface_interactions=12, exposure=3,
        width=320, height=240, seeds=[19, 104729],
        analytic_relative_L2_limit=.10, analytic_quadrature_refinement_limit=.0005,
        seed_display_RMS_limit_255=5,
        spectral_models='synthetic_declared_curves_not_measured_lamps_or_pigments',
        source_curves=fixture.sources(), reflectances=fixture.reflectances(),
        room_parent='spectral_room_transport_20261110 explicit fixture',
        source_kind='area', medium='vacuum_for_surface_isolation',
        emitted_support_nm=[400,780],
        rejection='invalid framing, nonzero direct analytic transfer, failed reference, unresolved noise hiding color differences')
    if path.exists():
        assert json.loads(path.read_text()) == plan, 'Plan changed; use a fresh root'
    else:
        write(path, plan)
    return plan


def previews(root, cli):
    plan = initialize(root)
    jobs = []
    for name in ('white_broad', 'green_broad', 'violet_405', 'white_three_peak'):
        q, sc = fixture.fixture(plan['source_curves'][name], width=128, height=96, samples=128)
        jobs.append((name, q, sc))
    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(lambda job: render(root/'preview', cli, *job), jobs))


def accept_previews(root, cli):
    """Record review after inspecting the four preview images; no scene promotion."""
    initialize(root)
    records = []
    for name in ('white_broad', 'green_broad', 'violet_405', 'white_three_peak'):
        d = root/'preview'/name
        receipt = json.loads((d/'receipt.json').read_text())
        assert receipt['status'] == 'completed' and receipt['runtime_sha256'] == digest(cli)
        assert receipt['request_sha256'] == digest(d/'request.json')
        assert receipt['scene_sha256'] == digest(d/'scene.json')
        assert receipt['image_sha256'] == digest(d/'output/frames/frame_0000.bmp')
        records.append(dict(case=name, image_sha256=digest(d/'image.png'),
            request_sha256=digest(d/'request.json'), scene_sha256=digest(d/'scene.json')))
    value = dict(reviewed=True, promotion_eligible=False, previews=records,
        plan_sha256=digest(root/'plan.json'), runtime_sha256=digest(cli))
    path = root/'preview-reviewed.json'
    if path.exists():
        assert json.loads(path.read_text()) == value, 'Prior review differs; inspect before replacing'
    else:
        write(path, value)


def references(root, cli, precision=False):
    plan = initialize(root)
    factor = fixture.geometry_reference()
    refined = fixture.geometry_reference(40, 8)
    assert abs(refined/factor-1) < .001
    results = []
    for source_name, source in plan['source_curves'].items():
        if precision and source_name not in ('white_broad', 'white_three_peak'):
            continue
        materials = ('neutral', 'amber', 'blue') if precision else ('neutral', 'orange', 'amber', 'green', 'blue', 'black')
        for material_name in materials:
            rho = plan['reflectances'][material_name]
            expected = [.8*factor*x for x in fixture.xyz(source, rho)]
            finer = [.8*factor*x for x in fixture.xyz(source, rho, .05)]
            norm = math.sqrt(sum(x*x for x in expected))
            refinement = math.sqrt(sum((a-b)**2 for a, b in zip(expected, finer)))/max(norm, 1e-30)
            assert refinement < plan['analytic_quadrature_refinement_limit']
            values = []
            seeds = [19] if material_name == 'black' else [19, 104729, 1237, 65537]
            if precision:
                seeds += [16807, 7919, 373, 99991, 524287, 8191, 65521, 32749, 1009, 42043, 131071, 31]
            for seed in seeds:
                q, sc = fixture.transfer_fixture(source, rho, seed)
                summary = render(root/'reference', cli,
                    f'{source_name}-{material_name}-{seed}', q, sc, timeout=60)
                probe = summary['spectral_surface_path']['probes'][0]
                assert probe['direct_xyz'] == [0, 0, 0]
                assert probe['volume_xyz'] == [0, 0, 0]
                values.append(probe['total_xyz'])
            actual = [statistics.mean(v[i] for v in values) for i in range(3)]
            error = math.sqrt(sum((a-b)**2 for a, b in zip(actual, expected)))/max(norm, 1e-30)
            assert error <= plan['analytic_relative_L2_limit'], (source_name, material_name, actual, expected, error)
            results.append(dict(source=source_name, material=material_name, expected_xyz=expected,
                actual_xyz=actual, relative_L2_error=error, quadrature_refinement=refinement, seeds=len(values),
                seed_mean_standard_error=[statistics.stdev(v[i] for v in values)/math.sqrt(len(values))
                    if len(values)>1 else 0 for i in range(3)]))
    write(root/('precision-results.json' if precision else 'reference-results.json'), dict(geometry_factor=factor,
        geometry_refinement=abs(refined/factor-1), results=results))
    print('PASS independent source→selective panel→neutral receiver references', flush=True)


def finals(root, cli):
    plan = initialize(root)
    reference = json.loads((root/'reference-results.json').read_text())['results']
    assert len(reference)==48 and all(r['relative_L2_error'] <= plan['analytic_relative_L2_limit']
        for r in reference), 'Independent reference gate required'
    review = json.loads((root/'preview-reviewed.json').read_text())
    assert review['reviewed'] and review['plan_sha256'] == digest(root/'plan.json')
    assert review['runtime_sha256'] == digest(cli)
    for record in review['previews']:
        d = root/'preview'/record['case']
        for field, name in [('image_sha256','image.png'), ('request_sha256','request.json'), ('scene_sha256','scene.json')]:
            assert digest(d/name) == record[field], 'Preview changed after review'
    jobs = []
    for source_name, source in plan['source_curves'].items():
        q, sc = fixture.fixture(source)
        jobs.append(('room-'+source_name, q, sc))
    for source_name in ('white_broad', 'white_three_peak', 'green_broad', 'blue_broad'):
        q, sc = fixture.fixture(plan['source_curves'][source_name], component='surface_indirect')
        jobs.append(('indirect-'+source_name, q, sc))
    for source_name in ('white_broad', 'white_three_peak', 'green_broad'):
        q, sc = fixture.fixture(plan['source_curves'][source_name], seed=104729)
        jobs.append(('seed-'+source_name, q, sc))
    for wall in ('orange', 'neutral', 'green', 'black', 'amber'):
        q, sc = fixture.fixture(plan['source_curves']['white_broad'], wall=wall,
            view='shadow', width=256, height=192, samples=1536)
        jobs.append(('shadow-'+wall, q, sc))
    for wall in ('neutral', 'amber'):
        q, sc = fixture.fixture(plan['source_curves']['white_three_peak'], wall=wall,
            view='shadow', width=256, height=192, samples=1536)
        jobs.append(('shadow-metamer-'+wall, q, sc))
    q, sc = fixture.fixture(plan['source_curves']['white_broad'], view='shadow',
        component='direct', width=256, height=192, samples=1536)
    jobs.append(('shadow-direct', q, sc))
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda job: render(root/'final', cli, *job), jobs))


def shadow_probes(root, cli):
    """High-sample point diagnostics at the center of the fixed shadow view.

    These use a narrow field of view around the same target, not a full-sensor
    crop or a replacement for the fixed-camera images.
    """
    plan = initialize(root)
    seeds = (19, 104729, 1237, 65537, 16807, 7919, 373, 99991)
    results = []
    for source_name, wall in [('white_broad', n) for n in ('orange', 'neutral', 'green', 'black', 'amber')] + [
            ('white_three_peak', 'neutral'), ('white_three_peak', 'amber')]:
        values = []
        for seed in seeds:
            q, sc = fixture.fixture(plan['source_curves'][source_name], wall=wall,
                view='shadow', width=1, height=1, samples=8192, seed=seed)
            q['inspection']['camera_zoom'] = 1000
            s = render(root/'shadow-probe', cli, f'{source_name}-{wall}-{seed}', q, sc, timeout=60)
            p = s['spectral_surface_path']['probes'][0]
            assert p['direct_xyz'] == [0, 0, 0] and p['volume_xyz'] == [0, 0, 0]
            values.append(p['total_xyz'])
        mean = [statistics.mean(v[i] for v in values) for i in range(3)]
        se = [statistics.stdev(v[i] for v in values)/math.sqrt(len(values)) for i in range(3)]
        results.append(dict(source=source_name, wall=wall, mean_xyz=mean, seed_mean_standard_error=se,
            seeds=list(seeds), samples_per_seed=8192, direct_xyz=[0, 0, 0],
            scope='narrow_FOV_point_diagnostic_same_shadow_target_not_full_sensor_crop'))
    write(root/'shadow-probe-results.json', dict(results=results))
    print('PASS shadow-point indirect-only controls', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--stage', required=True, choices=('preview', 'accept-preview', 'reference', 'precision', 'final', 'shadow-probe'))
    args = parser.parse_args()
    functions = {'preview': previews, 'accept-preview': accept_previews, 'reference': references,
        'precision': lambda r,c: references(r,c,True), 'final': finals, 'shadow-probe': shadow_probes}
    functions[args.stage](args.output.resolve(), args.cli.resolve())
