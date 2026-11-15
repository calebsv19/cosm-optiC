"""Accumulate completed independent ordinary V3 jobs in linear XYZ.

Requires spectral_budget_qualification result.json records. Same scene, request,
executable and per-map budget; independent camera AND map seeds. Raw component
means are weighted by primary camera samples, never photon count or brightness.
This is an offline still-image tool, not a renderer checkpoint or denoiser.
"""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import time

import numpy as np
from spectral_components import LABELS, display, verify

MAX_BATCHES = 64
MAX_PIXELS = 1048576


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def recombine(a):
    return ((a[..., 1, :] + a[..., 2, :]) + a[..., 3, :]) + (a[..., 4, :] + a[..., 5, :])


def weighted_components(arrays, samples):
    """Small-array reference and tests; final total uses canonical addition order."""
    if len(arrays) != len(samples) or not samples or any(type(n) is not int or n <= 0 for n in samples):
        raise ValueError('Positive integer camera sample weights required')
    out = np.zeros_like(arrays[0], dtype=np.float64)
    for a, n in zip(arrays, samples):
        if a.shape != out.shape or not np.isfinite(a).all():
            raise ValueError('Invalid component array')
        out += a * (n / sum(samples))
    out[..., 0, :] = recombine(out)
    return out


def resource_identity(root):
    """Bind external fixture assets/config, with a bounded read and no mutation."""
    result = {}; total = 0
    for name in ('assets', 'config'):
        base = root / name
        if not base.is_dir():
            raise ValueError('Missing job resources: ' + name)
        for p in sorted(base.rglob('*')):
            if p.is_symlink() and (not p.exists() or p.is_dir()):
                raise ValueError('Unsupported nested directory or broken resource link')
            if p.is_file():
                total += p.stat().st_size
                if total > 1024**3:
                    raise ValueError('Resource identity exceeds 1 GiB diagnostic budget')
                result[str(p.relative_to(root))] = digest(p)
    return result


def validate_resource_paths(value, resources):
    """Keep this offline fixture adapter inside its explicitly hashed resources."""
    if isinstance(value, dict):
        for key, child in value.items():
            if isinstance(child, str) and (key.endswith('_path') or key in ('path', 'filename')) and child:
                p = Path(child)
                if p.is_absolute() or '..' in p.parts or str(p) not in resources:
                    raise ValueError('Scene path outside hashed asset/config resources: ' + child)
            validate_resource_paths(child, resources)
    elif isinstance(value, list):
        for child in value:
            validate_resource_paths(child, resources)


def load_records(paths):
    if not 2 <= len(paths) <= MAX_BATCHES:
        raise ValueError('Require 2..64 independent completed batches')
    records = []; canonical = None; seeds = [set(), set()]; seen = set()
    for path in paths:
        path = Path(path).resolve(); root = path.parent
        if path in seen:
            raise ValueError('Duplicate batch record')
        seen.add(path)
        d = json.loads(path.read_text())
        if d.get('regions') is not False:
            raise ValueError('Ordinary full-frame batch required')
        scene = json.loads((root / 'scene.json').read_text())
        request = json.loads((root / 'request.json').read_text())
        summary = json.loads((root / 'summary.json').read_text())
        if summary.get('rendered_frames') is not True or summary.get('frames_rendered') != 1 or summary.get('integrator_3d') != 'disney_v3_spectral':
            raise ValueError('Completed single-frame V3 job required')
        report = summary['spectral_transport']
        image = root / 'render/frames/frame_0000.bmp'
        film = Path(str(image) + '.components.npy')
        if digest(root / 'scene.json') != d['scene_sha256'] or digest(image) != d['image_sha256']:
            raise ValueError('Batch scene/image hash drift')
        if report['linear_components']['file'] != film.name or digest(film) != report['linear_components']['sha256']:
            raise ValueError('Batch film hash drift')
        if report['quality']['component'] != 'total' or report['quality']['display_transform'] not in ('spectral_soft_v1','legacy_clip'):
            raise ValueError('Unsupported batch display policy')
        if report['nonscattered_indirect']['surface_nee_pdf_mismatches'] or not report['termination']['all_started_paths_classified']:
            raise ValueError('Unqualified batch transport')
        p = scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
        if (p['samples'],p['seed'],p['caustics']['passes'],p['caustics']['seed']) != (d['spp'],d['camera_seed'],d['passes'],d['map_seed']):
            raise ValueError('Batch sample/seed identity mismatch')
        if (report['quality']['samples_per_pixel'],report['quality']['seed'],report['smooth_glass']['passes'],report['smooth_glass']['seed']) != (d['spp'],d['camera_seed'],d['passes'],d['map_seed']):
            raise ValueError('Rendered sample/seed identity mismatch')
        if not math.isfinite(report['quality']['exposure']) or report['quality']['exposure'] <= 0:
            raise ValueError('Invalid exposure')
        if type(d['spp']) is not int or not 1 <= d['spp'] <= 65536:
            raise ValueError('Invalid camera sample count')
        for k, key in enumerate(('camera_seed', 'map_seed')):
            if type(d[key]) is not int or not 0 <= d[key] <= 0xffffffff or d[key] in seeds[k]:
                raise ValueError('Independent unique camera and map seeds required')
            seeds[k].add(d[key])
        shape = (report['film']['height'], report['film']['width'], 11, 3)
        if min(shape[:2]) < 1 or math.prod(shape[:2]) > MAX_PIXELS:
            raise ValueError('Film exceeds bounded pixel budget')
        if film.stat().st_size != 128 + math.prod(shape) * 8:
            raise ValueError('Unexpected full film size')
        for key in ('seconds','cpu_seconds'):
            if not math.isfinite(d[key]) or d[key] <= 0:
                raise ValueError('Invalid batch timing')
        # Only camera count and independent random identities may differ.
        frozen = copy.deepcopy(scene)
        q = frozen['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
        q.pop('samples'); q.pop('seed'); q['caustics'].pop('seed')
        for key in ('run_id','output'):
            request.pop(key,None)
        resources = resource_identity(root)
        validate_resource_paths(scene, resources)
        identity = (frozen, request, d['binary_sha256'], shape,
                    report['quality']['exposure'],report['quality']['display_transform'],resources)
        if canonical is None:
            canonical = identity
        elif canonical != identity:
            raise ValueError('Scene/camera/reconstruction/map budget/executable/resources drift')
        verify(report, image)
        a = np.load(film, mmap_mode='r', allow_pickle=False)
        if a.shape != shape or a.dtype != np.dtype('<f8'):
            raise ValueError('Unexpected component layout')
        bound = {str(p):digest(p) for p in (path,root/'scene.json',root/'request.json',root/'summary.json',image,film)}
        records.append(dict(path=path,root=root,data=d,report=report,film=a,bound=bound,resources=resources))
    return records


def accumulate(paths, output):
    start = time.monotonic(); output = Path(output).resolve()
    if output.exists():
        raise ValueError('Output already exists; inspect its completion manifest')
    records = load_records(paths)  # validate all inputs before creating output
    samples = [r['data']['spp'] for r in records]; total = sum(samples)
    weights = np.array(samples,dtype=float)/total; n = len(records)
    equal = len(set(samples)) == 1
    shape = records[0]['film'].shape; h,w = shape[:2]
    output.mkdir(parents=True,exist_ok=False)
    mean = np.lib.format.open_memmap(output/'components.npy',mode='w+',dtype='<f8',shape=shape)
    variance = np.lib.format.open_memmap(output/'variance_of_mean.npy',mode='w+',dtype='<f8',shape=shape) if equal else None
    # Largest nonnegative XYZ contribution fraction per pixel/component.
    share = np.zeros((h,w,11),dtype=float)
    residual = 0.; total_roundoff = 0.; peak = np.zeros(11)
    for y in range(0,h,8):
        sl = slice(y,min(h,y+8)); target = mean[sl]; target[:] = 0
        avg = np.zeros_like(target); m2 = np.zeros_like(target)
        maximum = np.zeros(target.shape[:-1]); energy = np.zeros_like(maximum)
        for i,(record,weight) in enumerate(zip(records,weights)):
            a = np.asarray(record['film'][sl])
            target += weight*a
            delta = a-avg; avg += delta/(i+1); m2 += delta*(a-avg)
            positive = np.maximum(a,0).sum(-1)*weight
            maximum = np.maximum(maximum,positive); energy += positive
            peak = np.maximum(peak,a.max(axis=(0,1,3)))
        canonical = recombine(target)
        total_roundoff = max(total_roundoff,float(np.max(abs(canonical-target[...,0,:]))))
        target[...,0,:] = canonical
        if variance is not None:
            # Includes measured covariance through the original TOTAL film.
            estimate = m2/(n*(n-1))
            if not np.isfinite(estimate).all():
                raise ValueError('Nonfinite batch variance; retain inputs for diagnosis')
            variance[sl] = estimate
        share[sl] = np.divide(maximum,energy,out=np.zeros_like(energy),where=energy>0)
        err = abs(target[...,6,:]+target[...,7:11,:].sum(-2)-target[...,3,:])
        residual = max(residual,float(err.max()))
        if not np.isfinite(target).all() or np.any(err > 3e-12*np.maximum(1,abs(target[...,3,:]))):
            raise ValueError('Accumulated component accounting failed')
    mean.flush()
    if variance is not None: variance.flush()
    from PIL import Image
    report = records[0]['report']; exposure = report['quality']['exposure']
    rgba = b''.join(display(v,exposure,report['quality']['display_transform']=='spectral_soft_v1') for v in mean[...,0,:].reshape(-1,3))
    Image.frombytes('RGBA',(w,h),rgba).save(output/'preview.png')
    # Recheck input custody after consumption. A failure leaves no completion receipt.
    for r in records:
        if any(digest(p)!=v for p,v in r['bound'].items()) or resource_identity(r['root'])!=r['resources']:
            raise ValueError('Batch changed during accumulation')
    result = dict(schema='optic_v3_independent_batch_accumulation_v1',complete=True,
        batches=n,camera_samples=total,weights=weights.tolist(),equal_camera_budgets=equal,
        map_budget_per_batch=records[0]['data']['passes'],independent_maps=n,
        photon_launches_per_batch=[r['report']['smooth_glass']['launched'] for r in records],
        total_photon_launches=sum(r['report']['smooth_glass']['launched'] for r in records),
        photon_weighting=False,shape=list(shape),components=list(LABELS),exposure=exposure,
        display_transform=report['quality']['display_transform'],
        input_records=[dict(path=str(r['path']),sha256=digest(r['path']),camera_seed=r['data']['camera_seed'],map_seed=r['data']['map_seed'],samples=r['data']['spp'],files=r['bound'],resources=r['resources']) for r in records],
        variance_of_mean_available=equal,confidence_bounds_available=False,
        max_total_weighting_roundoff=total_roundoff,max_volume_subset_residual=residual,
        tail_diagnostics={label:dict(max_batch_xyz=float(peak[k]),max_contribution_share=float(share[...,k].max()),p99_contribution_share=float(np.quantile(share[...,k],.99)),fraction_pixels_one_batch_over_half=float(np.mean(share[...,k]>.5))) for k,label in enumerate(LABELS)},
        summed_render_cpu_seconds=sum(r['data']['cpu_seconds'] for r in records),
        summed_job_elapsed_seconds=sum(r['data']['seconds'] for r in records),
        accumulation_elapsed_seconds=time.monotonic()-start,
        limits=['Independent-map ensemble at fixed reconstruction; not a single larger photon map.',
                'Sample weighting is fixed before observing radiance; unequal budgets omit variance estimates.',
                'Empirical batch variance and dominance do not bound unseen rare bright paths or reconstruction bias.',
                'XYZ component exports are not wavelength-resolved spectra. No filtering, clamping or denoising of linear films.'])
    result['outputs']={p.name:dict(bytes=p.stat().st_size,sha256=digest(p)) for p in output.iterdir() if p.is_file()}
    (output/'manifest.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--records',type=Path,nargs='+',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();r=accumulate(args.records,args.output)
    print(json.dumps({k:r[k] for k in ('batches','camera_samples','independent_maps','variance_of_mean_available','accumulation_elapsed_seconds')},indent=2))


if __name__=='__main__':main()
