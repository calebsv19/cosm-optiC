"""Immutable offline V3 component denoise experiments; never a renderer default.

Consumes completed spectral_batch_accumulate manifests and writes an exclusive
candidate with raw provenance. Component filtering uses common XYZ/volume-child
weights; direct and surface_indirect (including glass continuation) are exact.
"""
import argparse
import json
from pathlib import Path
import resource
import time

import numpy as np
from spectral_batch_accumulate import digest, load_records, recombine
from spectral_components import LABELS, display


def validate_components(a):
    if a.ndim != 4 or a.shape[2:] != (11, 3) or a.dtype != np.dtype('<f8'):
        raise ValueError('Expected float64 HxWx11x3 XYZ film')
    if min(a.shape[:2]) < 1 or np.prod(a.shape[:2]) > 1048576 or not np.isfinite(a).all():
        raise ValueError('Invalid or oversized component film')
    if not np.array_equal(a[..., 0, :], recombine(a)):
        raise ValueError('Canonical total mismatch')
    residual = abs(a[..., 6:11, :].sum(-2) - a[..., 3, :])
    if np.any(residual > 3e-12*np.maximum(1, abs(a[..., 3, :]))):
        raise ValueError('Volume ownership mismatch')


def load_accumulation(path, minimum_batches=4):
    # Beauty-only OIDN consumes a verified mean, not the empirical variance.
    # Variance-guided callers retain the conservative four-batch default.
    if type(minimum_batches) is not int or minimum_batches not in (2, 4):
        raise ValueError('Supported minimum batch counts are 2 and 4')
    path = Path(path).resolve(strict=True)
    m = json.loads(path.read_text()); root = path.parent
    if m.get('schema') != 'optic_v3_independent_batch_accumulation_v1' or m.get('complete') is not True:
        raise ValueError('Completed independent accumulation required')
    if m.get('variance_of_mean_available') is not True or m.get('equal_camera_budgets') is not True or m['batches'] < minimum_batches:
        count_name = 'two' if minimum_batches == 2 else 'four'
        raise ValueError(f'At least {count_name} equal independent batches with variance required')
    bound = {str(path): digest(path)}
    for name in ('components.npy', 'variance_of_mean.npy'):
        p = root/name; expected = m['outputs'][name]
        if p.stat().st_size != expected['bytes'] or digest(p) != expected['sha256']:
            raise ValueError('Accumulation output hash drift')
        bound[str(p)] = expected['sha256']
    records = load_records([r['path'] for r in m['input_records']])
    if len(records) != m['batches'] or len({r['data']['spp'] for r in records}) != 1:
        raise ValueError('Batch count/budget mismatch')
    for declared, record in zip(m['input_records'], records):
        if declared['files'] != record['bound'] or declared['resources'] != record['resources']:
            raise ValueError('Accumulation input identity drift')
    a = np.load(root/'components.npy', allow_pickle=False)
    v = np.load(root/'variance_of_mean.npy', allow_pickle=False)
    validate_components(a)
    if list(a.shape) != m['shape'] or m['components'] != list(LABELS):
        raise ValueError('Accumulation layout mismatch')
    if v.shape != a.shape or v.dtype != a.dtype or not np.isfinite(v).all() or np.any(v < 0):
        raise ValueError('Invalid variance film')
    report = records[0]['report']['quality']
    if m['exposure'] != report['exposure'] or m['display_transform'] != report['display_transform']:
        raise ValueError('Display policy drift')
    # Independently recompute the mean and variance in strips. Hash validity alone
    # does not establish that the selected records own this accumulation.
    for y in range(0, a.shape[0], 8):
        stack = np.stack([r['film'][y:y+8] for r in records])
        mean = stack.mean(0); mean[..., 0, :] = recombine(mean)
        if not np.allclose(a[y:y+8], mean, rtol=3e-14, atol=3e-13):
            raise ValueError('Accumulation mean differs from bound inputs')
        if not np.allclose(v[y:y+8], stack.var(0, ddof=1)/len(records), rtol=3e-13, atol=3e-13):
            raise ValueError('Accumulation variance differs from bound inputs')
    return a, v, m, bound


def component_filter(a, variance, settings, exposure):
    validate_components(a)
    if variance.shape != a.shape or not np.isfinite(variance).all() or np.any(variance < 0):
        raise ValueError('Invalid variance')
    radius = settings['radius']; sigma = settings['sigma']; range_sigma = settings['range_sigma']
    if type(radius) is not int or not 1 <= radius <= 4:
        raise ValueError('Radius must be 1..4')
    if not all(np.isfinite(x) and 0 < x <= 4 for x in (sigma, range_sigma, exposure)):
        raise ValueError('Invalid filter scale')
    for key in ('volume_blend', 'caustic_blend'):
        if not np.isfinite(settings[key]) or not 0 <= settings[key] <= 1:
            raise ValueError('Invalid family strength')
    result = a.copy(); height, width = a.shape[:2]
    # These are *image* uncertainty weights, not new photon reconstruction.
    for parent, layers, strength in ((3, list(range(6, 11)), settings['volume_blend']),
                                      (4, [4], settings['caustic_blend']),
                                      (5, [5], settings['caustic_blend'])):
        signal = a[..., parent, :]; var = variance[..., parent, :]
        accum = a[..., layers, :].copy(); norm = np.ones((height, width))
        for dy in range(-radius, radius+1):
            for dx in range(-radius, radius+1):
                if dx == dy == 0: continue
                sy = slice(max(0, -dy), min(height, height-dy))
                sx = slice(max(0, -dx), min(width, width-dx))
                ty = slice(max(0, dy), min(height, height+dy))
                tx = slice(max(0, dx), min(width, width+dx))
                if sy.stop <= sy.start or sx.stop <= sx.start: continue
                weight = np.full(signal[sy, sx].shape[:2], np.exp(-(dx*dx+dy*dy)/(2*sigma*sigma)))
                for guide, guide_var in ((signal, var), (a[..., 0, :], variance[..., 0, :])):
                    delta = guide[sy, sx] - guide[ty, tx]
                    floor = (.005*np.maximum(abs(guide[sy, sx]), abs(guide[ty, tx])))**2 + 1e-24
                    distance = np.mean(delta**2/(guide_var[sy, sx]+guide_var[ty, tx]+floor), axis=-1)
                    weight *= np.exp(-distance/(2*range_sigma*range_sigma))
                accum[sy, sx] += weight[..., None, None]*a[ty, tx][..., layers, :]
                norm[sy, sx] += weight
        target = .002/exposure + .005*abs(signal)
        uncertainty = np.mean(var/(var+target*target), axis=-1)
        alpha = strength*uncertainty
        result[..., layers, :] = a[..., layers, :] + alpha[..., None, None]*(accum/norm[..., None, None]-a[..., layers, :])
    result[..., 3, :] = result[..., 6:11, :].sum(-2)
    result[..., 0, :] = recombine(result)
    validate_components(result)
    return result


def save_preview(path, xyz, exposure, transform):
    from PIL import Image
    h, w = xyz.shape[:2]
    pixels = b''.join(display(p, exposure, transform == 'spectral_soft_v1') for p in xyz.reshape(-1, 3))
    Image.frombytes('RGBA', (w, h), pixels).save(path)


def run(manifest, output, mode, settings=None, oidn_library=None):
    output = Path(output).resolve()
    if output.exists(): raise ValueError('Output exists; no overwrite or uncertain resume')
    start = time.monotonic()
    a, v, m, bound = load_accumulation(manifest, minimum_batches=2 if mode in ('oidn', 'oidn_signed') else 4)
    output.mkdir(parents=True, exist_ok=False)
    t = time.monotonic()
    if mode == 'component':
        candidate = component_filter(a, v, settings, m['exposure'])
        beauty = candidate[..., 0, :]
        np.save(output/'components.npy', candidate)
        backend = dict(backend='shared-XYZ variance bilateral', settings=settings,
                       direct_and_surface_indirect_exact=bool(np.array_equal(candidate[..., 1:3, :], a[..., 1:3, :])),
                       volume_subset_residual=float(abs(candidate[..., 6:11, :].sum(-2)-candidate[..., 3, :]).max()))
    elif mode in ('oidn', 'oidn_signed'):
        from spectral_denoise_backend import oidn_hdr, oidn_signed_hdr
        filter_ = oidn_signed_hdr if mode == 'oidn_signed' else oidn_hdr
        beauty, backend = filter_(a[..., 0, :], oidn_library, m['exposure'])
        backend['component_ownership'] = 'beauty-only result; no invented component decomposition'
    else:
        raise ValueError('Unknown denoise mode')
    filter_seconds = time.monotonic()-t
    np.save(output/'beauty_xyz.npy', beauty)
    np.save(output/'difference_xyz.npy', beauty-a[..., 0, :])
    save_preview(output/'preview.png', beauty, m['exposure'], m['display_transform'])
    if any(digest(p) != h for p, h in bound.items()):
        raise ValueError('Raw input changed during filtering')
    meta = dict(schema='optic_v3_offline_denoise_v1', complete=True, mode=mode,
                raw_manifest=str(Path(manifest).resolve()), raw_sha256=bound,
                exposure=m['exposure'], display_transform=m['display_transform'],
                backend=backend, filter_seconds=filter_seconds,
                total_seconds=time.monotonic()-start,
                process_peak_rss_native_units=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
                source_sha256={str(Path(__file__).resolve()): digest(__file__),
                               str(Path(__file__).with_name('spectral_denoise_backend.py').resolve()): digest(Path(__file__).with_name('spectral_denoise_backend.py'))},
                filtered_variance_claim=False, raw_preserved=True,
                outputs={p.name:dict(sha256=digest(p), bytes=p.stat().st_size) for p in output.iterdir()})
    (output/'manifest.json').write_text(json.dumps(meta, indent=2, allow_nan=False)+'\n')
    return meta


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--mode', choices=('component', 'oidn', 'oidn_signed'), required=True)
    p.add_argument('--settings', type=Path)
    p.add_argument('--oidn-library', type=Path)
    args = p.parse_args()
    if args.mode == 'component' and args.settings is None: p.error('--settings required')
    if args.mode in ('oidn', 'oidn_signed') and args.oidn_library is None: p.error('--oidn-library required')
    result = run(args.manifest, args.output, args.mode,
                 json.loads(args.settings.read_text()) if args.settings else None, args.oidn_library)
    print(json.dumps({k:result[k] for k in ('mode', 'filter_seconds', 'total_seconds')}, indent=2))


if __name__ == '__main__': main()
