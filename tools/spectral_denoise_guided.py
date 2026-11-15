"""Experimental batch-variance-guided detail restoration after signed HDR OIDN.

Guides are image statistics from raw independent batches, not albedo/normals.
No reference image is accepted by the filter. This nonlinear heuristic has no
unbiasedness, variance-reduction or feature-preservation guarantee.
"""
import argparse
import json
from pathlib import Path
import time

import numpy as np
from scipy.ndimage import gaussian_filter
from spectral_batch_accumulate import digest, load_records
from spectral_denoise import load_accumulation, save_preview
from spectral_denoise_backend import oidn_signed_hdr

POLICY = dict(version=1, guide_sigma_pixels=0.8, variance_multiplier=4.0,
              maximum_raw_restore=0.9, relative_floor=0.005)


def restore_detail(batches, baseline):
    """Restore spatially coherent residuals using one shared XYZ weight.

    Blur each batch BEFORE estimating guide variance to include observed spatial
    correlations (e.g. shared photon maps). The weight uses squared discrepancy
    between blurred raw and blurred OIDN divided by discrepancy plus four times
    empirical guide variance and a relative floor. OIDN uncertainty/covariance
    is not modeled, so this is a heuristic confidence weight, not a probability.
    """
    batches = np.asarray(batches, dtype=np.float64)
    baseline = np.asarray(baseline, dtype=np.float64)
    if (batches.ndim != 4 or batches.shape[0] < 4 or batches.shape[-1] != 3
            or baseline.shape != batches.shape[1:] or min(baseline.shape[:2]) < 1
            or np.prod(baseline.shape[:2]) > 1048576
            or not np.isfinite(batches).all() or not np.isfinite(baseline).all()):
        raise ValueError('Four finite equal-budget HxWx3 independent batches required')
    raw = batches.mean(0)
    sigma = POLICY['guide_sigma_pixels']
    guides = gaussian_filter(batches, sigma=(0, sigma, sigma, 0), mode='reflect')
    guide_mean = guides.mean(0)
    guide_variance = guides.var(0, ddof=1)/len(batches)
    base_guide = gaussian_filter(baseline, sigma=(sigma, sigma, 0), mode='reflect')
    discrepancy = np.mean((guide_mean-base_guide)**2, axis=-1)
    uncertainty = np.mean(guide_variance, axis=-1)
    floor = POLICY['relative_floor']**2*np.mean(guide_mean**2, axis=-1)
    denom = discrepancy + POLICY['variance_multiplier']*uncertainty + floor
    confidence = np.divide(discrepancy, denom, out=np.zeros_like(denom), where=denom > 0)
    weight = POLICY['maximum_raw_restore']*confidence
    beauty = baseline + weight[..., None]*(raw-baseline)
    if not np.isfinite(beauty).all():
        raise ValueError('Nonfinite guided result')
    return beauty, weight


def run(manifest, output, library):
    output = Path(output).resolve()
    if output.exists():
        raise ValueError('Output exists; no overwrite or uncertain resume')
    started = time.monotonic()
    a, _, m, bound = load_accumulation(manifest)
    records = load_records([r['path'] for r in m['input_records']])
    batches = np.stack([r['film'][..., 0, :] for r in records])
    baseline, backend = oidn_signed_hdr(a[..., 0, :], library, m['exposure'])
    beauty, weight = restore_detail(batches, baseline)
    output.mkdir(parents=True, exist_ok=False)
    for name, data in [('beauty_xyz', beauty), ('baseline_xyz', baseline),
                       ('restore_weight', weight), ('difference_xyz', beauty-a[..., 0, :])]:
        np.save(output/(name+'.npy'), data)
    save_preview(output/'preview.png', beauty, m['exposure'], m['display_transform'])
    # Revalidate the complete record/resource bindings after processing.
    load_accumulation(manifest)
    if any(digest(p) != h for p, h in bound.items()):
        raise ValueError('Raw input changed during filtering')
    meta = dict(schema='optic_v3_guided_detail_v1', complete=True,
                experimental=True, policy=POLICY, backend=backend,
                raw_manifest=str(Path(manifest).resolve()), raw_sha256=bound,
                guide='spatially smoothed independent raw batch mean and variance',
                albedo_or_normal_guides=False, reference_used=False,
                raw_preserved=True, filtered_variance_claim=False,
                feature_preservation_certified=False,
                mean_restore_weight=float(weight.mean()),
                maximum_restore_weight=float(weight.max()),
                elapsed_seconds=time.monotonic()-started,
                source_sha256={str(Path(__file__).resolve()):digest(__file__),
                               str(Path(__file__).with_name('spectral_denoise_backend.py')):
                               digest(Path(__file__).with_name('spectral_denoise_backend.py'))},
                outputs={p.name:dict(sha256=digest(p), bytes=p.stat().st_size)
                         for p in output.iterdir()})
    (output/'manifest.json').write_text(json.dumps(meta, indent=2, allow_nan=False)+'\n')
    return meta


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--oidn-library', type=Path, required=True)
    args = p.parse_args()
    result = run(args.manifest, args.output, args.oidn_library)
    print(json.dumps({k:result[k] for k in ('complete', 'mean_restore_weight', 'elapsed_seconds')}))


if __name__ == '__main__':
    main()
