"""Frozen-fixture denoise metrics against disjoint, finite noisy references.

Feature windows/thresholds belong to a saved experiment contract. These image
measurements are preservation screens, not scene-wide perceptual certification.
"""
import numpy as np
from scipy.ndimage import gaussian_filter
from spectral_error_budget import ROIS


def crop(a, box):
    x, y, w, h = box
    return a[y:y+h, x:x+w]


def uv(xyz):
    x, y, z = np.asarray(xyz)
    d = x+15*y+3*z
    return np.array([4*x/d, 9*y/d]) if d > 0 else np.zeros(2)


def relative(a, b):
    return abs(np.asarray(a)-b)/np.maximum(abs(np.asarray(b)), 1e-12)


def moments(y):
    # Fixed local low-background removal, applied identically to each image.
    weight = np.maximum(y-np.quantile(y, .10), 0)
    gy, gx = np.indices(y.shape); s = weight.sum()
    if s <= 0: raise ValueError('Feature has no measurable contrast')
    center = np.array([(gx*weight).sum()/s, (gy*weight).sum()/s])
    width = np.sqrt(np.array([((gx-center[0])**2*weight).sum()/s,
                              ((gy-center[1])**2*weight).sum()/s]))
    return center, width


def feature_metrics(xyz, reference, windows):
    y = xyz[..., 1]
    caustic_center, caustic_width = moments(crop(y, windows['caustic']))
    _, beam_width = moments(crop(y, windows['beam']))
    glint = crop(y, windows['glint'])
    glass = crop(y, windows['glass'])
    ref_glass = crop(reference[..., 1], windows['glass'])
    def gradient(a):
        gy, gx = np.gradient(gaussian_filter(a, 1.0))
        return np.hypot(gx, gy)
    ref_gradient = gradient(ref_glass)
    # Reference defines the stable high-gradient mask, never a candidate.
    mask = ref_gradient >= np.quantile(ref_gradient, .80)
    return dict(caustic_center=caustic_center.tolist(), caustic_width=caustic_width.tolist(),
                beam_width=beam_width.tolist(), glint_peak=float(glint.max()),
                glint_contrast=float(glint.max()-np.quantile(glint, .25)),
                glass_edge_gradient=float(gradient(glass)[mask].sum()))


def evaluate(raw, candidate, independent_full, input_batches, regional_replicates,
             contract, exposure):
    """References must be checked for source and seed independence by caller."""
    raw = raw*exposure; candidate = candidate*exposure
    full = independent_full*exposure
    samples = input_batches*exposure
    references = regional_replicates*exposure  # seed, region, pixel, XYZ
    ref = references.mean(0)
    rows = {}; raw_patches = []; candidate_patches = []
    preserve = contract['preservation']; quality = contract['quality']
    for k, (name, box) in enumerate(ROIS.items()):
        r = crop(raw, box).reshape(-1, 3); c = crop(candidate, box).reshape(-1, 3)
        raw_patches.append(r); candidate_patches.append(c)
        means = np.array([crop(a, box).reshape(-1, 3).mean(0) for a in samples])
        refmeans = references[:, k].mean(1)
        allowance = np.maximum(.05*abs(refmeans.mean(0)),
                               3*np.sqrt(means.var(0, ddof=1)/len(means)+refmeans.var(0, ddof=1)/len(refmeans)))
        raw_error = np.sqrt(np.mean((r-ref[k])**2, axis=0))
        error = np.sqrt(np.mean((c-ref[k])**2, axis=0))
        drift = relative(c.mean(0), r.mean(0))
        color_drift = float(np.linalg.norm(uv(c.mean(0))-uv(r.mean(0))))
        noise = np.sqrt(np.array([crop(a, box) for a in samples]).var(0, ddof=1).mean((0, 1))/len(samples))
        ref_noise = np.sqrt(references[:, k].var(0, ddof=1).mean(0)/len(references))
        ratio = float(np.linalg.norm(error)/np.linalg.norm(raw_error))
        rows[name] = dict(raw_XYZ_RMSE=raw_error.tolist(), candidate_XYZ_RMSE=error.tolist(),
                         XYZ_RMSE_ratio=ratio, raw_mean=r.mean(0).tolist(), candidate_mean=c.mean(0).tolist(),
                         reference_mean=ref[k].mean(0).tolist(), mean_allowance=allowance.tolist(),
                         mean_relative_drift=drift.tolist(), uv_drift=color_drift,
                         raw_noise_RMS_estimate=noise.tolist(), reference_noise_RMS_estimate=ref_noise.tolist(),
                         raw_cleanup_point_pass=bool(np.all(noise <= .05)),
                         raw_strict_point_pass=bool(np.all(noise <= .01)),
                         existing_mean_gate=bool(np.all(abs(c.mean(0)-ref[k].mean(0)) <= allowance)),
                         mean_drift_gate=bool(np.all(drift <= preserve['maximum_region_mean_drift_from_raw_relative'])),
                         color_gate=color_drift <= preserve['maximum_region_uv1976_drift'],
                         no_regression_gate=ratio <= 1+quality['maximum_individual_region_XYZ_RMSE_regression'])
    rp = np.array(raw_patches); cp = np.array(candidate_patches)
    aggregate_raw = float(np.sqrt(np.mean((rp-ref)**2)))
    aggregate = float(np.sqrt(np.mean((cp-ref)**2)))
    # Paired loss difference cancels squared reference noise. Bootstrap only
    # quantifies uncertainty from these finite reference seeds, conditional on
    # the fixed input/candidate; it is not a rare-event coverage guarantee.
    rng = np.random.default_rng(73481)
    indices = rng.integers(0, len(references), (1000, len(references)))
    paired = []
    for idx in indices:
        bootstrap_ref = references[idx].mean(0)
        paired.append(float(np.mean((cp-bootstrap_ref)**2-(rp-bootstrap_ref)**2)))
    fraw = feature_metrics(raw, full, contract['feature_windows'])
    fc = feature_metrics(candidate, full, contract['feature_windows'])
    fref = feature_metrics(full, full, contract['feature_windows'])
    shifts = dict(caustic_centroid_pixels=float(np.linalg.norm(np.array(fc['caustic_center'])-fraw['caustic_center'])),
                  caustic_width_relative=float(relative(fc['caustic_width'], fraw['caustic_width']).max()),
                  beam_width_relative=float(relative(fc['beam_width'], fraw['beam_width']).max()),
                  glint_peak_relative=float(relative(fc['glint_peak'], fraw['glint_peak'])),
                  glint_contrast_relative=float(relative(fc['glint_contrast'], fraw['glint_contrast'])),
                  glass_edge_relative=float(relative(fc['glass_edge_gradient'], fraw['glass_edge_gradient'])))
    feature_gates = dict(caustic_centroid=shifts['caustic_centroid_pixels'] <= preserve['floor_caustic_centroid_shift_pixels'],
                         caustic_width=shifts['caustic_width_relative'] <= preserve['floor_caustic_rms_width_drift_relative'],
                         beam_width=shifts['beam_width_relative'] <= preserve['beam_rms_width_drift_relative'],
                         glint_peak=shifts['glint_peak_relative'] <= preserve['glint_peak_and_contrast_drift_relative'],
                         glint_contrast=shifts['glint_contrast_relative'] <= preserve['glint_peak_and_contrast_drift_relative'],
                         glass_edge=shifts['glass_edge_relative'] <= preserve['glass_edge_gradient_energy_drift_relative'])
    shadow_raw = crop(raw, contract['feature_windows']['color_shadow']).mean((0, 1))
    shadow = crop(candidate, contract['feature_windows']['color_shadow']).mean((0, 1))
    shadow_drift = float(np.linalg.norm(uv(shadow)-uv(shadow_raw)))
    feature_gates['shadow_color'] = shadow_drift <= preserve['maximum_region_uv1976_drift']
    feature_gates['shadow_mean'] = bool(np.all(relative(shadow, shadow_raw) <= preserve['maximum_region_mean_drift_from_raw_relative']))
    global_drift = relative(candidate.mean((0, 1)), raw.mean((0, 1)))
    raw_full_rmse = float(np.sqrt(np.mean((raw-full)**2)))
    full_rmse = float(np.sqrt(np.mean((candidate-full)**2)))
    gates = dict(preservation=all(feature_gates.values()) and bool(np.all(global_drift <= preserve['maximum_global_mean_drift_relative'])) and all(r['mean_drift_gate'] and r['color_gate'] and r['existing_mean_gate'] for r in rows.values()),
                 aggregate_improvement=aggregate <= (1-quality['aggregate_regional_XYZ_RMSE_reduction_min'])*aggregate_raw,
                 no_region_regression=all(r['no_regression_gate'] for r in rows.values()),
                 full_frame_improvement=full_rmse < raw_full_rmse)
    return dict(regions=rows, aggregate_raw_RMSE=aggregate_raw, aggregate_candidate_RMSE=aggregate,
                aggregate_RMSE_reduction=1-aggregate/aggregate_raw,
                reference_only_paired_MSE_difference_bootstrap95=np.quantile(paired, [.025, .975]).tolist(),
                full_raw_RMSE=raw_full_rmse, full_candidate_RMSE=full_rmse,
                full_RMSE_reduction=1-full_rmse/raw_full_rmse,
                global_mean_relative_drift=global_drift.tolist(), shadow_uv_drift=shadow_drift,
                features=dict(raw=fraw, candidate=fc, independent_reference=fref, shifts=shifts, gates=feature_gates),
                gates=gates, accepted=all(gates.values()),
                limits='Noisy finite references; bootstrap conditions on fixed input/candidate. Feature windows are image screens, not semantic guides. No filtered variance/convergence certificate.')
