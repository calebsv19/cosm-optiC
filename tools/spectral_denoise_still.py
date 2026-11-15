"""Explicit HDR OIDN postprocess for one completed ordinary V3 headless still.

Preserves the original native image, XYZ components and completion summary.
No variance or component-denoised output is invented for a single native film.
"""
import argparse
import json
from pathlib import Path
import resource
import time

import numpy as np
from spectral_batch_accumulate import digest
from spectral_components import verify
from spectral_denoise import save_preview, validate_components
from spectral_denoise_backend import oidn_hdr, oidn_signed_hdr, split_rgb


def run(summary, image, output, library, backend=oidn_hdr, signed_policy='preserve'):
    summary = Path(summary).resolve(strict=True)
    image = Path(image).resolve(strict=True)
    output = Path(output).resolve()
    if output.exists():
        raise ValueError('Output already exists; inspect before recovery')
    started = time.monotonic()
    completed = json.loads(summary.read_text())
    if (completed.get('rendered_frames') is not True or completed.get('frames_rendered') != 1
            or completed.get('integrator_3d') != 'disney_v3_spectral'):
        raise ValueError('Completed single-frame headless V3 summary required')
    report = completed['spectral_transport']
    if (report['nonscattered_indirect']['surface_nee_pdf_mismatches']
            or not report['termination']['all_started_paths_classified']):
        raise ValueError('Unqualified native transport diagnostics')
    width, height = report['film']['width'], report['film']['height']
    if min(width, height) < 1 or width*height > 1048576:
        raise ValueError('Still exceeds bounded pixel budget')
    quality = report['quality']
    if quality['component'] != 'total' or quality['display_transform'] not in ('spectral_soft_v1', 'legacy_clip'):
        raise ValueError('Ordinary total-beauty display contract required')
    if not np.isfinite(quality['exposure']) or quality['exposure'] <= 0:
        raise ValueError('Invalid exposure')
    name = report['linear_components']['file']
    if Path(name).name != name or name != image.name+'.components.npy':
        raise ValueError('Component basename does not match the native image')
    film = image.parent/name
    bound = {str(p): digest(p) for p in (summary, image, film)}
    verify(report, image)
    raw = np.load(film, allow_pickle=False)
    validate_components(raw)
    _, signed = split_rgb(raw[..., 0, :])
    negative_fraction = float(np.mean(signed < 0))
    # Admission screen, not a color-accuracy guarantee. The signed stress
    # experiment failed near 45%; the original scene was below 0.002%.
    # Substantial signed input needs a separately qualified color adapter.
    if signed_policy not in ('preserve', 'filter'):
        raise ValueError('Unknown signed-color policy')
    if negative_fraction > .01 and signed_policy == 'preserve':
        raise ValueError('Unsupported gamut: over 1% negative linear-RGB components; preserve raw and qualify a signed-color adapter')
    if signed_policy == 'filter': backend = oidn_signed_hdr
    output.mkdir(parents=True, exist_ok=False)
    beauty, info = backend(raw[..., 0, :], library, quality['exposure'])
    if beauty.shape != raw[..., 0, :].shape or not np.isfinite(beauty).all():
        raise ValueError('Backend returned invalid beauty')
    np.save(output/'beauty_xyz.npy', beauty)
    np.save(output/'difference_xyz.npy', beauty-raw[..., 0, :])
    save_preview(output/'preview.png', beauty, quality['exposure'], quality['display_transform'])
    if any(digest(p) != h for p, h in bound.items()):
        raise ValueError('Native input changed during postprocessing')
    receipt = dict(schema='optic_v3_explicit_still_denoise_v1', complete=True,
                   raw_files=bound, backend=info, exposure=quality['exposure'],
                   display_transform=quality['display_transform'],
                   raw_preserved=True, component_decomposition=False,
                   variance_available=False, renderer_defaults_changed=False,
                   negative_rgb_fraction=negative_fraction,
                   negative_rgb_fraction_admission_limit=.01 if signed_policy == 'preserve' else None,
                   signed_policy=signed_policy,
                   total_seconds=time.monotonic()-started,
                   peak_rss_native_units=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
                   source_sha256={str(Path(__file__).with_name(name).resolve()):
                                  digest(Path(__file__).with_name(name)) for name in
                                  ('spectral_denoise_still.py', 'spectral_denoise_backend.py',
                                   'spectral_denoise.py', 'spectral_components.py')},
                   outputs={p.name:dict(sha256=digest(p), bytes=p.stat().st_size) for p in output.iterdir()})
    (output/'manifest.json').write_text(json.dumps(receipt, indent=2, allow_nan=False)+'\n')
    return receipt


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('summary', 'image', 'output', 'oidn-library'):
        p.add_argument('--'+key, type=Path, required=True)
    p.add_argument('--signed-policy', choices=('preserve', 'filter'), default='preserve',
                   help='filter is an explicit experimental signed-color alternative')
    a = p.parse_args()
    r = run(a.summary, a.image, a.output, a.oidn_library, signed_policy=a.signed_policy)
    print(json.dumps({'complete':r['complete'], 'total_seconds':r['total_seconds']}, indent=2))


if __name__ == '__main__': main()
