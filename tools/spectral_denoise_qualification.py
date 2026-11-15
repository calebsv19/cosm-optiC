"""Run frozen tungsten development, then one unchanged LED denoise holdout.

Requires retained V3 batch/absolute-budget roots, an already frozen contract,
and an explicit OIDN library. Never starts renderer jobs or changes scenes.
"""
import argparse
import gzip
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
from spectral_batch_accumulate import accumulate, digest, load_records
from spectral_budget_planner import controlled_scene
from spectral_denoise_metrics import evaluate


def write(path, value):
    with Path(path).open('x') as f: json.dump(value, f, indent=2, allow_nan=False)


def prepare(batch_root, regional_root, output, source):
    records = [batch_root/'batches'/source/f'batch-{i:02}'/'result.json' for i in range(8)]
    checked = load_records(records)  # checks disjoint camera AND map seeds and full fixture identity
    rawroot = batch_root/'accumulated'/source/'batches-4'
    m = json.loads((rawroot/'manifest.json').read_text())
    if [Path(r['path']).resolve() for r in m['input_records']] != [r.resolve() for r in records[:4]]:
        raise ValueError('Declared input split differs from frozen first four batches')
    reference_root = output/source/'independent-reference'
    if reference_root.exists(): raise ValueError('Reference exists; reconcile the same run')
    accumulate(records[4:], reference_root)
    scene = controlled_scene(json.loads((records[0].parent/'scene.json').read_text()))
    cs = {r['data']['camera_seed'] for r in checked}; ms = {r['data']['map_seed'] for r in checked}
    regional = []; identities = []
    stem = 'original-tungsten' if source == 'tungsten' else 'heldout-led'
    for i in range(16):
        p = regional_root/'validation'/f'{stem}-planned-{i}'/'roi.f64.gz'
        d = json.loads((p.parent/'result.json').read_text())
        if digest(p) != d['roi_sha256'] or digest(p.parent/'scene.json') != d['scene_sha256']:
            raise ValueError('Regional reference hash drift')
        if d['camera_seed'] in cs or d['map_seed'] in ms:
            raise ValueError('Input/reference or reference/reference seed overlap')
        cs.add(d['camera_seed']); ms.add(d['map_seed'])
        if controlled_scene(json.loads((p.parent/'scene.json').read_text())) != scene or d['passes'] != 16:
            raise ValueError('Regional reference scene/reconstruction mismatch')
        a = np.frombuffer(gzip.decompress(p.read_bytes()), dtype='<f8').reshape(6, 64, 11, 3)
        if not np.isfinite(a).all(): raise ValueError('Nonfinite reference')
        regional.append(a[..., 0, :]); identities.append(dict(path=str(p), sha256=digest(p), camera_seed=d['camera_seed'], map_seed=d['map_seed']))
    write(output/source/'split.json', dict(input=[str(p) for p in records[:4]], independent_reference=[str(p) for p in records[4:]], regional_reference=identities, seeds_disjoint=True))
    return (rawroot/'manifest.json', np.load(rawroot/'components.npy')[..., 0, :],
            np.load(reference_root/'components.npy')[..., 0, :],
            np.stack([r['film'][..., 0, :] for r in checked[:4]]), np.stack(regional), m['exposure'])


def candidate(output, source, name, manifest, library, settings=None):
    target = output/source/name
    cmd = [sys.executable, str(Path(__file__).with_name('spectral_denoise.py')),
           '--manifest', str(manifest), '--output', str(target), '--mode', 'component' if settings else 'oidn']
    if settings:
        setting_path = output/(name+'-settings.json')
        if not setting_path.exists(): write(setting_path, settings)
        if json.loads(setting_path.read_text()) != settings: raise ValueError('Frozen settings drift')
        cmd += ['--settings', str(setting_path)]
    else: cmd += ['--oidn-library', str(library)]
    with (output/source/(name+'.log')).open('x') as log:
        subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
    return np.load(target/'beauty_xyz.npy')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--batch-root', type=Path, required=True)
    p.add_argument('--regional-root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--oidn-library', type=Path, required=True)
    p.add_argument('--stage', choices=('development', 'holdout'), required=True)
    args = p.parse_args(); output = args.output.resolve()
    contract_path = output/'contract.json'; contract = json.loads(contract_path.read_text())
    source = 'tungsten' if args.stage == 'development' else 'led'
    selection = None
    if source == 'led':
        selection = json.loads((output/'selection.json').read_text())
        if selection['contract_sha256'] != digest(contract_path): raise ValueError('Contract drift')
        if selection['development_sha256'] != digest(output/'tungsten/metrics.json'): raise ValueError('Development evidence drift')
    manifest, raw, reference, input_batches, regional, exposure = prepare(args.batch_root, args.regional_root, output, source)
    presets = contract['component_presets'] if source == 'tungsten' else [selection['settings']]
    results = {}
    for name, settings in [('oidn', None)] + [(s['name'], s) for s in presets]:
        beauty = candidate(output, source, name, manifest, args.oidn_library, settings)
        results[name] = evaluate(raw, beauty, reference, input_batches, regional, contract, exposure)
        print(source, name, json.dumps({k:results[name][k] for k in ('aggregate_RMSE_reduction', 'full_RMSE_reduction', 'gates', 'accepted')}), flush=True)
    write(output/source/'metrics.json', results)
    if source == 'tungsten':
        passing = [s for s in presets if results[s['name']]['gates']['preservation']]
        selected = min(passing or presets, key=lambda s:results[s['name']]['aggregate_candidate_RMSE'])
        write(output/'selection.json', dict(settings=selected, preservation_pass=bool(passing),
              development_accepted=results[selected['name']]['accepted'],
              contract_sha256=digest(contract_path), development_sha256=digest(output/source/'metrics.json'),
              reason='Lowest regional error among preservation-passing candidates; best diagnostic if none pass. Frozen before LED.'))


if __name__ == '__main__': main()
