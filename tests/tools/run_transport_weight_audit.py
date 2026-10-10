"""Emit a bounded physical qualification report; failed checks exit nonzero."""
import argparse
import json
import math
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--binary', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args()
rows = []
for ior in (1.45, 1.333, 1.52):
    result = subprocess.run([args.binary, str(ior)], capture_output=True, text=True, check=True)
    rows.extend(dict(json.loads(line), run_ior=ior) for line in result.stdout.splitlines())
checks = []
def check(name, passed, evidence):
    checks.append({'check': name, 'passed': bool(passed), 'evidence': evidence})
for ior in (1.45, 1.333, 1.52):
    group = [r for r in rows if r['run_ior'] == ior]
    interface = [r for r in group if r['case'] == 'interface']
    check(f'{ior}: Snell directions', all(r['snell_error'] < 1e-12 for r in interface), interface)
    check(f'{ior}: sampled reflection PDF and weight identity',
          all(r['pdf_error'] < 1e-7 and r['weight_identity_error'] < 1e-7 for r in interface), interface)
    mixture = [r for r in group if r['case'] == 'mixture_pdf']
    check(f'{ior}: reflection-side mixture PDF', all(r['relative_error'] < 1e-7 for r in mixture), mixture)
    slab = next(r for r in group if r['case'] == 'parallel_slab')
    check(f'{ior}: explicit-interface slab', slab['direction_error'] < 1e-12, slab)
    slab = next(r for r in group if r['case'] == 'camera_slab_exit')
    check(f'{ior}: camera slab exit',
          abs(slab['eta_from'] - ior) < 1e-12 and abs(slab['eta_to'] - 1) < 1e-12 and slab['direction_error'] < 1e-12, slab)
    tir = next(r for r in group if r['case'] == 'tir')
    check(f'{ior}: total internal reflection', tir['tir'] and abs(tir['reported_fresnel'] - 1) < 1e-12
          and tir['wrong_reflection_direction'] == 0 and tir['specular_invalid'] == 0, tir)
    budget = next(r for r in group if r['case'] == 'tir_depth_budget')
    check(f'{ior}: TIR retains dielectric depth budget', budget['wrong_budget'] == 0 and budget['reflections'] > 0, budget)
    radiance = next(r for r in group if r['case'] == 'clear_radiance_reference')
    check(f'{ior}: clear radiance transmission convention',
          abs(radiance['weighted_transmission'] - radiance['expected_radiance_transmission']) < 1e-7, radiance)
    roulette = next(r for r in group if r['case'] == 'roulette')
    # Bernoulli compensation: Var[(0.1/q)*I] = 0.1^2*(1/q-1).
    stderr = math.sqrt(.01 * (1 / roulette['survival'] - 1) / 200000)
    check(f'{ior}: compensated roulette mean', abs(roulette['compensated_mean'] - .1) < 5 * stderr, roulette)
for ior in (1.45, 1.333, 1.52):
    group = [r for r in rows if r['run_ior'] == ior]
    nested = next(r for r in group if r['case'] == 'nested_camera_stack')
    check(f'{ior}: nested camera media and reciprocal radiance scales', nested['passed'] and nested['final_depth'] == 0 and abs(nested['radiance_scale_product'] - 1) < 1e-12, nested)
    tir = next(r for r in group if r['case'] == 'camera_tir_stack')
    check(f'{ior}: camera TIR retains medium', tir['passed'] and tir['depth_unchanged'], tir)
    mismatch = next(r for r in group if r['case'] == 'camera_medium_mismatch')
    check(f'{ior}: mismatched exit fails without stack mutation', mismatch['rejected'] and mismatch['depth_unchanged'], mismatch)
    intersection = next(r for r in group if r['case'] == 'intersection_orientation')
    check(f'{ior}: actual intersection retains boundary orientation', intersection['passed'], intersection)
report = {'qualified': all(c['passed'] for c in checks), 'checks': checks, 'observations': rows,
          'scope': 'Numerical local camera-transport audit; no denoising, photon population, full-frame ground truth or candidate sampler',
          'convention': 'Camera interface weights use radiance transport; photons retain their flux convention.'}
Path(args.output).write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'qualified': report['qualified'], 'checks_passed': sum(c['passed'] for c in checks),
                  'checks_failed': [c['check'] for c in checks if not c['passed']]}))
raise SystemExit(0 if report['qualified'] else 1)
