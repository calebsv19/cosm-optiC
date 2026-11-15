"""Measured joint-cost planning with absolute linear noise and seed uncertainty.

This diagnostic selects only measured budgets. Whole independent render seeds,
never pixels, are the replication units. Bounds combine seed resampling with a
conservative Gaussian reference, not a distribution-free convergence certificate.
"""
import argparse
import copy
import itertools
import json
from pathlib import Path

import numpy as np
from scipy.stats import chi2

from spectral_budget_planner import controlled_scene, digest, pilot_pixels, region_spec
from spectral_error_budget import ROIS


def nnls_small(a, b):
    """Exhaustive active-set least squares for at most four cost coefficients."""
    choices = []
    for n in range(1, a.shape[1] + 1):
        for cols in itertools.combinations(range(a.shape[1]), n):
            c = np.zeros(a.shape[1])
            c[list(cols)] = np.linalg.lstsq(a[:, cols], b, rcond=None)[0]
            if np.all(c >= 0):
                choices.append((float(np.sum((a @ c - b)**2)), c))
    return min(choices, key=lambda v: v[0])[1]


def joint_cost(budgets, times):
    """Fit setup+c+p+c*p; leave whole budget cells out to test interaction cost.

    times has shape [matched seed block, budget]. Prediction errors describe
    measured cell means; they are not wall-time guarantees or full-frame costs.
    """
    b = np.asarray(budgets, float)
    t = np.asarray(times, float)
    if b.ndim != 2 or b.shape[1] != 2 or t.ndim != 2 or t.shape[1] != len(b):
        raise ValueError('Expected budgets [cell,2] and times [seed,cell]')
    if t.shape[0] < 8 or len(b) < 6 or not np.isfinite(t).all() or np.any(t <= 0):
        raise ValueError('Need at least eight positive timing replicas and six budget cells')
    if not np.isfinite(b).all() or np.any(b < 1) or len(set(map(tuple,b))) != len(b):
        raise ValueError('Invalid or duplicate budget cells')
    a = np.column_stack((np.ones(len(b)), b, b[:,0]*b[:,1]))
    if np.linalg.matrix_rank(a) != 4:
        raise ValueError('Joint cost requires independent combined-budget calibration')
    means = t.mean(0)
    result = {}
    for name, design in [('additive', a[:,:3]), ('joint', a)]:
        c = nnls_small(design, means)
        cv = []
        for k in range(len(b)):
            keep = np.arange(len(b)) != k
            if np.linalg.matrix_rank(design[keep]) != design.shape[1]:
                raise ValueError('Insufficient grid for held-out budget-cell validation')
            cv.append(float(design[k] @ nnls_small(design[keep], means[keep])))
        errors = (np.array(cv)-means)/means
        result[name] = dict(coefficients=c.tolist(), predictions=(design@c).tolist(),
                            heldout_predictions=cv, heldout_relative_errors=errors.tolist(),
                            heldout_relative_rmse=float(np.sqrt(np.mean(errors**2))),
                            maximum_absolute_relative_error=float(np.max(np.abs(errors))))
    result.update(measured_cpu_mean=means.tolist(),
                  measured_cpu_standard_error=(t.std(0,ddof=1)/np.sqrt(len(t))).tolist(),
                  timing_replicas=len(t), scope='CPU seconds for the captured pixel domain only',
                  interaction_improves_holdout=result['joint']['heldout_relative_rmse'] < result['additive']['heldout_relative_rmse'])
    result['preferred_model'] = min(('additive','joint'),key=lambda k:result[k]['heldout_relative_rmse'])
    result['heldout_20_percent_gate'] = result[result['preferred_model']]['maximum_absolute_relative_error'] <= .20
    return result


def noise_bounds(values, exposure, confidence=.95, resamples=8192, seed=9173):
    """Simultaneous two-sided approximate band over ALL candidate/region/XYZ cells.

    Input [independent matched seed,candidate,region,pixel,XYZ]. Delete-one-render
    jackknife estimates uncertainty of spatially averaged per-pixel variance.
    Gaussian multiplier maximum-t resamples the same seed block across every
    candidate and channel, preserving both spatial and cross-budget dependence.
    """
    x = np.asarray(values, float)
    if x.ndim != 5 or x.shape[-1] != 3 or x.shape[0] < 16 or min(x.shape[1:]) < 1:
        raise ValueError('Need at least 16 independent paired-seed XYZ render replicas')
    if not np.isfinite(x).all() or not np.isfinite(exposure) or exposure <= 0:
        raise ValueError('Finite films and positive fixed exposure required')
    if not .8 <= confidence < 1 or type(resamples) is not int or resamples < 1000:
        raise ValueError('Invalid confidence or insufficient multiplier draws')
    n = len(x)
    x = x * exposure
    # Center first to avoid subtracting large squared means in bright patches.
    centered = x-x.mean(0)
    sum_squares = np.sum(centered**2,axis=0)
    variance = (sum_squares/(n-1)).mean(-2)
    leave_out = ((sum_squares[None]-n/(n-1)*centered**2)/(n-2)).mean(-2)
    if np.any(variance <= 0) or np.any(leave_out <= 0):
        raise ValueError('Unresolved zero variance; cannot certify unobserved noise')
    # Log scale keeps bounds positive and avoids an implausibly small upper
    # allowance when a small pilot underestimates a positive variance.
    pseudo = n*np.log(variance[None])-(n-1)*np.log(leave_out)
    influence = pseudo-pseudo.mean(0)
    se = influence.std(0,ddof=1)/np.sqrt(n)
    if np.any(se <= 0):
        raise ValueError('Unresolved zero variance/uncertainty; cannot certify unobserved noise')
    # Scaling makes each multiplier statistic unit variance conditional on data.
    weights = influence.reshape(n,-1)/(np.sqrt(n*(n-1))*se.reshape(1,-1))
    rng = np.random.default_rng(seed)
    maxima = []
    for start in range(0,resamples,256):
        draws = rng.standard_normal((min(256,resamples-start),n)) @ weights
        maxima.extend(np.max(np.abs(draws),axis=1))
    critical = float(np.quantile(maxima,confidence,method='higher'))
    multiplier_lower = variance*np.exp(-critical*se)
    multiplier_upper = variance*np.exp(critical*se)
    # A deliberately conservative Gaussian-reference envelope: simultaneous
    # per-pixel chi-square intervals, then average their variance limits. This
    # does NOT assume pixels are independent; Bonferroni handles dependence.
    # Non-Gaussian path tails can still violate this conditional reference.
    pixel_family = int(variance.size*x.shape[-2])
    tail = (1-confidence)/(2*pixel_family)
    gaussian_lower = variance*(n-1)/chi2.isf(tail,n-1)
    gaussian_upper = variance*(n-1)/chi2.ppf(tail,n-1)
    lower = np.minimum(multiplier_lower,gaussian_lower)
    upper = np.maximum(multiplier_upper,gaussian_upper)
    if not np.isfinite(upper).all():raise ValueError('Unresolved unbounded noise uncertainty')
    seed_energy=(centered**2).mean(-2)
    return dict(variance=variance.tolist(), log_variance_standard_error=se.tolist(),
                largest_seed_variance_share=(seed_energy.max(0)/seed_energy.sum(0)).tolist(),
                rms=np.sqrt(variance).tolist(), rms_lower=np.sqrt(lower).tolist(),
                rms_upper=np.sqrt(upper).tolist(), confidence=confidence,
                multiplier_rms_upper=np.sqrt(multiplier_upper).tolist(),
                gaussian_reference_pixel_family=pixel_family,
                gaussian_reference_rms_factor=float(np.sqrt((n-1)/chi2.ppf(tail,n-1))),
                critical_value=critical, seed_replicas=n, resamples=resamples,
                method='log-variance seed multiplier band enveloped by simultaneous per-pixel Gaussian-reference intervals',
                family_cells=int(variance.size), exposure=exposure,
                units='exposure-scaled linear XYZ; not display-code or perceptual error',
                limits=['Gaussian-reference coverage is conditional; heavy-tail coverage is not guaranteed.',
                        'Bounds concern sampling variance, not reconstruction bias or absolute physical accuracy.',
                        'Unmeasured pixels and unseen rare paths remain outside the claim.'])


def choose_absolute(budgets, cost, bounds, target_rms):
    if not np.isfinite(target_rms) or target_rms <= 0:
        raise ValueError('Absolute RMS target must be positive and finite')
    rows = []
    for i,(c,p) in enumerate(budgets):
        rms = np.array(bounds['rms'][i]); upper = np.array(bounds['rms_upper'][i])
        rows.append(dict(camera_multiplier=c, photon_multiplier=p,
                         predicted_cpu_seconds=cost[cost.get('preferred_model','joint')]['predictions'][i],
                         measured_cpu_seconds=cost['measured_cpu_mean'][i],
                         measured_region_XYZ_rms=rms.tolist(), upper_region_XYZ_rms=upper.tolist(),
                         worst_rms=float(rms.max()), worst_upper_rms=float(upper.max()),
                         meets_target=bool(np.all(upper <= target_rms))))
    passing = [r for r in rows if r['meets_target']]
    selected = min(passing,key=lambda r:(r['predicted_cpu_seconds'],r['worst_upper_rms'])) if passing else None
    point_pass=any(r['worst_rms']<=target_rms for r in rows)
    return dict(selected=selected,candidates=rows,target_rms=target_rms,
                status='measured_bound_target_met' if selected else ('uncertainty_limited' if point_pass else 'budget_cap_insufficient'))


def absolute_plan(reference_path, calibration_path, target_rms):
    """Verify a complete matched-seed candidate grid, then freeze one budget.

    Reference supplies camera/scene/layout only. Old variance/cost observations
    are not pooled into the new bounds. Each calibration seed is unique on BOTH
    axes, and is paired across candidates to compare allocations fairly.
    """
    reference_path,calibration_path = map(Path,(reference_path,calibration_path))
    ref=json.loads(reference_path.read_text());m=json.loads(calibration_path.read_text())
    regions=region_spec(ref.get('regions',ROIS)); pairs=[tuple(v) for v in m['seed_pairs']]
    budgets=[tuple(v) for v in m['budgets']]
    if len(pairs)<16 or any(len(v)!=2 for v in pairs) or any(len({v[i] for v in pairs})!=len(pairs) for i in (0,1)):
        raise ValueError('Unique independent camera AND map seeds required')
    if any(len(v)!=2 or any(type(n)is not int or n<1 for n in v) for v in budgets) or len(set(budgets))!=len(budgets):
        raise ValueError('Unique positive integer budget counts required')
    exposure=ref['controlled_scene']['extensions']['ray_tracing']['spectral_transport_v1']['exposure']
    if m['exposure']!=exposure:raise ValueError('Absolute metric exposure differs from frozen scene')
    cells={};times={};identities=[];binary=None;capture_mode=None
    for location in m['records']:
        f=(calibration_path.parent/location).resolve();d=json.loads(f.read_text());root=f.parent
        if digest(root/'scene.json')!=d['scene_sha256'] or digest(root/'roi.f64.gz')!=d['roi_sha256']:
            raise ValueError('Calibration artifact hash mismatch')
        scene=json.loads((root/'scene.json').read_text());q=json.loads((root/'request.json').read_text())
        for key in ('run_id','output'):q.pop(key,None)
        if controlled_scene(scene)!=ref['controlled_scene'] or q!=ref['controlled_request']:
            raise ValueError('Calibration scene/camera/reconstruction drift')
        mode=d.get('regions',True)
        if binary is None:binary=d['binary_sha256'];capture_mode=mode
        if binary!=d['binary_sha256'] or mode!=capture_mode:raise ValueError('Mixed renderer or timing domain')
        p=scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
        counts=(p['samples'],p['caustics']['passes']);seeds=(p['seed'],p['caustics']['seed'])
        if counts!=(d['spp'],d['map']['passes']) or seeds!=(d['camera_seed'],d['map']['seed']):
            raise ValueError('Calibration record/scene mismatch')
        if d.get('nonscattered_indirect',{}).get('surface_nee_pdf_mismatches',0):raise ValueError('Unqualified PDF route')
        if counts not in budgets or seeds not in pairs:raise ValueError('Unexpected budget/seed')
        key=(counts,seeds)
        if key in cells:raise ValueError('Duplicate calibration cell')
        cells[key]=pilot_pixels(root,d,regions)[:,:,0,:];times[key]=d['cpu_seconds']
        identities.append(dict(result=str(f),sha256=digest(f),camera_seed=seeds[0],map_seed=seeds[1]))
    if len(cells)!=len(budgets)*len(pairs):raise ValueError('Incomplete matched-seed budget grid')
    values=np.array([[cells[b,s] for b in budgets] for s in pairs])
    timing=np.array([[times[b,s] for b in budgets] for s in pairs])
    normalized=[(c/ref['baseline_camera_samples'],p/ref['baseline_photon_passes']) for c,p in budgets]
    cost=joint_cost(normalized,timing)
    bounds=noise_bounds(values,exposure,m.get('confidence',.95),m.get('resamples',8192))
    result=choose_absolute(normalized,cost,bounds,target_rms)
    for row,(c,p) in zip(result['candidates'],budgets):
        row.update(camera_samples=c,photon_passes=p,photons=p*ref['controlled_scene']['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']['caustics']['photons_per_pass'])
    result.update(schema='optic_v3_absolute_budget_plan_v1',regions=regions,region_order=list(regions),
                  controlled_scene=ref['controlled_scene'],controlled_request=ref['controlled_request'],
                  baseline_camera_samples=ref['baseline_camera_samples'],baseline_photon_passes=ref['baseline_photon_passes'],
                  forbidden_final_camera_seeds=sorted(set(ref['forbidden_final_camera_seeds'])|{s[0] for s in pairs}),
                  forbidden_final_map_seeds=sorted(set(ref['forbidden_final_map_seeds'])|{s[1] for s in pairs}),
                  renderer_sha256=binary,calibration_records=identities,cost_model=cost,noise_bounds=bounds,
                  reference_sha256=digest(reference_path),calibration_sha256=digest(calibration_path),
                  capture_domain='full_frame' if capture_mode is False else 'fixed_regions',
                  limits=['Only measured candidate budgets may be selected; no noise or timing extrapolation.',
                          'Independent final seeds are required; calibration pixels never enter the final image.',
                          'Nominal uncertainty is approximate; validate its coverage on independent renders.',
                          'A linear XYZ threshold is an engineering criterion, not perceptual clean-still certification.'])
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('reference','calibration','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--target-rms',type=float,required=True)
    a=p.parse_args();result=absolute_plan(a.reference,a.calibration,a.target_rms)
    with a.output.open('x') as stream:json.dump(result,stream,indent=2,allow_nan=False)
    print(json.dumps({'status':result['status'],'selected':result['selected']},indent=2))


if __name__=='__main__':main()
