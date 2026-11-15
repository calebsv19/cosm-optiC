"""Bounded local source comparisons using the native V3 request/renderer.

Preserves camera, geometry, medium, exposure and reconstruction. Writes a fresh
directory; never edits the base request, resumes uncertain work or submits jobs.
"""
import argparse
import copy
import hashlib
import html
import json
import math
from pathlib import Path
import subprocess
import time

from optical_comparison_artifacts import bmp_rgba, png_bytes

REPO = Path(__file__).resolve().parents[1]
def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')

def resolved(base, value):
    p = Path(value)
    return (p if p.is_absolute() else base.parent / p).resolve(strict=True)

def load_base(path):
    q = json.loads(path.read_text())
    if q.get('schema_version') != 'ray_tracing_agent_render_request_v2':
        raise ValueError('A native V3 v2 request is required')
    if q['render'].get('integrator_3d') != 'disney_v3_spectral':
        raise ValueError('The base must select disney_v3_spectral')
    q['scene']['runtime_scene_path'] = str(resolved(path, q['scene']['runtime_scene_path']))
    v = q['spectral'].get('participating_volume_v1', {})
    for field in ('source_path', 'mie_phase_path'):
        if field in v:
            v[field] = str(resolved(path, v[field]))
    if not q['spectral']['surface_map']['enabled']:
        raise ValueError('Enable the photon map in the base request')
    if len(q['spectral']['surface_map']['sources']) != 1:
        raise ValueError('Use a one-source base; its geometry is the testbench emitter')
    return q

def request_for(base, nm, fwhm, watts, seed, passes, coverage, size, output, name, camera_sequence="balanced", camera_seed=None):
    q = copy.deepcopy(base)
    source = q['spectral']['surface_map']['sources'][0]
    for key in ('lines', 'continuum', 'spectrum'):
        source.pop(key, None)
    if fwhm == 0:
        source.update(lines=[[nm, watts]], continuum=[])
    else:
        source['spectrum'] = {'type': 'gaussian_mixture', 'visible_watts': watts,
                             'peaks': [[nm, fwhm / math.sqrt(8 * math.log(2)), 1]]}
    q['spectral']['surface_map']['seed'] = seed
    q['spectral']['sampling'] = {'mode': 'converge', 'min_passes': passes,
                                'max_passes': passes, 'camera_coverage': coverage,
                                'camera_sequence': camera_sequence}
    if camera_seed is not None:q['spectral']['sampling']['camera_seed']=camera_seed
    q['render'].update(width=size[0], height=size[1])
    q['run_id'] = name
    q.pop('progress', None)
    q['output'] = {'root': str(output), 'overwrite': False}
    return q

def spectrum_svg(nm, fwhm, watts):
    # Lines have integrated watts; curves show the authored relative profile.
    x = lambda n: 20 + (n - 380) / 400 * 400
    if fwhm == 0:
        shape = f'<path d="M{x(nm):.2f},90 V15" stroke="white"/>'
        label = f'{nm:g} nm exact line / {watts:g} W'
    else:
        sigma = fwhm / math.sqrt(8 * math.log(2))
        pts = ' '.join(f'{x(n):.2f},{90-75*math.exp(-.5*((n-nm)/sigma)**2):.2f}' for n in range(380,781))
        shape = f'<polyline points="{pts}" fill="none" stroke="white"/>'
        label = f'{nm:g} nm / FWHM {fwhm:g} nm / {watts:g} W integrated visible power'
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 440 120"><rect width="440" height="120" fill="#252833"/>{shape}<g fill="white" font-family="sans-serif" font-size="11"><text x="20" y="108">380</text><text x="390" y="108">780 nm</text><text x="20" y="12">{label}</text></g></svg>'

def write_gallery(root, records):
    cells = []
    for r in records:
        label = html.escape(r['name'])
        image = f'<img src="{label}/image.png" alt="Native spectral render">' if r.get('image_sha256') else ''
        s = r.get('sampling', {})
        status = html.escape(s.get('status', r['status']))
        cells.append(f'<article><h2>{label}</h2>{image}<img src="{label}/spectrum.svg" alt="Source spectrum"><p>{status}; full passes {s.get("full_passes", "—")}; samples/pixel {s.get("min_pixel_samples", "—")}–{s.get("max_pixel_samples", "—")}; local error {s.get("local_error", "—")}</p><a href="{label}/request.json">Request</a> · <a href="{label}/receipt.json">Receipt</a></article>')
    (root/'index.html').write_text('<!doctype html><meta charset="utf-8"><title>V3 source testbench</title><style>body{background:#151820;color:#edf1f8;font:16px system-ui;margin:24px}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(360px,1fr));gap:20px}article{background:#222733;padding:16px}img{display:block;width:100%;image-rendering:auto}a{color:#9fcaff}h2{font-size:18px}</style><h1>Spectral source testbench</h1><p>Fixed camera, medium, exposure and reconstruction. Equal radiant power does not imply equal visible brightness. Pass limit is not convergence.</p><main>'+''.join(cells)+'</main>')

def execute(cli, directory, timeout):
    args = [str(cli), '--request', str(directory/'request.json'), '--summary-file-only', '--summary', str(directory/'summary.json')]
    # Admission occurs before any render. A failed cell remains reviewable.
    with (directory/'preflight.log').open('w') as log:
        subprocess.run(args, cwd=directory, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
    start = time.monotonic()
    with (directory/'render.log').open('w') as log:
        subprocess.run(args+['--render'], cwd=directory, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=timeout)
    summary = json.loads((directory/'summary.json').read_text())
    image = directory/'output/frames/frame_0000.bmp'
    w,h,pixels = bmp_rgba(image)
    (directory/'image.png').write_bytes(png_bytes(w,h,pixels))
    if summary['frames_rendered'] != 1:
        raise ValueError('Expected one static frame')
    residual = 0
    for band in summary.get('spectral_volume', {}).get('bands', []):
        error = abs(band['launched_w']-sum(band[k] for k in ('absorbed_w','receiver_w','escaped_w','depth_limited_w','surface_loss_w')))
        residual = max(residual, error)
    if residual > 1e-9:
        raise ValueError('Transport energy ledger failed')
    return {'status':'rendered', 'sampling':summary['spectral_sampling'],
            'image_sha256':digest(image), 'summary_sha256':digest(directory/'summary.json'),
            'wall_seconds':time.monotonic()-start, 'max_band_energy_residual_w':residual}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--request', type=Path, default=REPO/'tests/fixtures/spectral_surface_map/request.json')
    p.add_argument('--output-root', type=Path, required=True)
    p.add_argument('--cli', type=Path, help='Omit to prepare requests without rendering')
    p.add_argument('--wavelengths', type=float, nargs='+', default=[405,450,530])
    p.add_argument('--fwhm-nm', type=float, default=0, help='Zero selects exact lines')
    p.add_argument('--watts', type=float, default=1)
    p.add_argument('--seeds', type=int, nargs='+', default=[19,104729])
    p.add_argument('--passes', type=int, nargs='+', default=[16])
    p.add_argument('--coverage', choices=['full','adaptive'], default='full')
    p.add_argument('--camera-sequence', choices=['balanced','progressive'], default='balanced')
    p.add_argument('--camera-seed', type=int, help='Independent camera seed; omitted inherits photon seed')
    p.add_argument('--size', type=int, nargs=2, help='Diagnostic size; must preserve the base aspect ratio')
    p.add_argument('--timeout', type=int, default=300)
    a=p.parse_args()
    if not (all(math.isfinite(v) and 380<=v<=780 for v in a.wavelengths) and
            math.isfinite(a.fwhm_nm) and (a.fwhm_nm == 0 or .03<=a.fwhm_nm<=100) and math.isfinite(a.watts) and 0<a.watts<=10 and
            all(0<=s<=0x7fffffff for s in a.seeds) and (a.camera_seed is None or 0<=a.camera_seed<=0xffffffff) and all(16<=n<=64 and n%8==0 for n in a.passes) and
            (a.size is None or all(16<=v<=512 for v in a.size)) and 1<=a.timeout<=600):
        p.error('Outside bounded testbench limits (visible range, ≤64 passes, ≤512 pixels per side)')
    if (len(set(a.wavelengths))!=len(a.wavelengths) or len(set(a.seeds))!=len(a.seeds) or
            len(set(a.passes))!=len(a.passes) or len(a.wavelengths)*len(a.seeds)*len(a.passes)>18):
        p.error('Use distinct values and at most 18 cells')
    basepath=a.request.resolve(strict=True);base=load_base(basepath)
    w,h=base['render']['width'],base['render']['height']
    if a.size is None:
        if max(w,h)<=240:a.size=[w,h]
        else:
            divisor=math.gcd(w,h);uw,uh=w//divisor,h//divisor
            multiple=240//max(uw,uh)
            a.size=[uw*multiple,uh*multiple]
    if not all(16<=v<=512 for v in a.size) or a.size[0]*h!=a.size[1]*w:
        p.error('Choose a bounded --size with the exact base aspect ratio')
    cli=a.cli.resolve(strict=True) if a.cli else None
    cli_hash=digest(cli) if cli else None
    root=a.output_root.resolve();root.mkdir(parents=True, exist_ok=False)
    inputs={'base_request':{'path':str(basepath),'sha256':digest(basepath)}}
    for label,value in [('scene',base['scene']['runtime_scene_path']),('volume',base['spectral'].get('participating_volume_v1',{}).get('source_path'))]:
        if value:inputs[label]={'path':value,'sha256':digest(value)}
    save(root/'baseline.json',base)
    save(root/'inputs.json',{'inputs':inputs,'cli':str(cli) if cli else None,'cli_sha256':cli_hash,
         'reconstruction_policy':'unchanged from base request','source_geometry':'unchanged from sole base emitter'})
    records=[]
    for nm in a.wavelengths:
        for seed in a.seeds:
            for passes in a.passes:
                name=f'{nm:g}nm-seed{seed}-maps{passes}';d=root/name;d.mkdir()
                (d/'config').symlink_to(REPO/'config',target_is_directory=True)
                q=request_for(base,nm,a.fwhm_nm,a.watts,seed,passes,a.coverage,a.size,d/'output',name,a.camera_sequence,a.camera_seed)
                save(d/'request.json',q);(d/'spectrum.svg').write_text(spectrum_svg(nm,a.fwhm_nm,a.watts))
                r={'name':name,'wavelength_nm':nm,'fwhm_nm':a.fwhm_nm,'watts':a.watts,'seed':seed,'passes':passes,
                   'camera_sequence':a.camera_sequence,'camera_seed':a.camera_seed if a.camera_seed is not None else seed,
                   'status':'prepared','request_sha256':digest(d/'request.json')}
                records.append(r);save(d/'receipt.json',r);save(root/'results.json',records);write_gallery(root,records)
                try:
                    if cli:
                        if digest(cli)!=cli_hash:raise ValueError('Renderer identity changed')
                        r.update(execute(cli,d,a.timeout))
                        if digest(cli)!=cli_hash:raise ValueError('Renderer identity changed')
                except Exception as exc:
                    r.update(status='failed',error=str(exc));raise
                finally:
                    save(d/'receipt.json',r);save(root/'results.json',records);write_gallery(root,records)
                for item in inputs.values():
                    if digest(item['path'])!=item['sha256']:raise ValueError('Input drift during testbench')
                print(name,r['status'],r.get('sampling',{}).get('status',''),flush=True)
    print(root/'index.html')

if __name__ == '__main__':
    main()
