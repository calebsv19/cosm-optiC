"""Experimental opt-in local V3 still: native raw render, then separately verified HDR OIDN.

Uses the request's fresh output.root; never rewrites the request or raw output.
This local command does not change renderer defaults or launch remote work.
"""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import time

import numpy as np
from spectral_batch_accumulate import digest
from spectral_denoise_backend import oidn_hdr
from spectral_denoise_still import run as denoise_still


def prepare(request):
    request = Path(request).resolve(strict=True)
    data = json.loads(request.read_text())
    render = data.get('render', {})
    if (data.get('schema_version') != 'ray_tracing_agent_render_request_v2'
            or render.get('integrator_3d') != 'disney_v3_spectral'
            or render.get('frame_count', 1) != 1 or render.get('temporal_frames', 1) != 1
            or render.get('denoise_enabled', False) is not False
            or render.get('use_tiled_renderer', False) is not False):
        raise ValueError('One serial raw V3 still required')
    width, height = render.get('width'), render.get('height')
    if (type(width) is not int or type(height) is not int
            or min(width, height) < 1 or width*height > 1048576):
        raise ValueError('Explicit dimensions within the still pixel budget required')
    output = data.get('output', {})
    if (not isinstance(output.get('root'), str) or not output['root']
            or output.get('overwrite', False) is not False
            or set(output) - {'root', 'overwrite'} or data.get('progress')):
        raise ValueError('Fresh output.root without overwrite or external progress outputs required')
    root = (request.parent/output['root']).resolve()
    if root.exists():
        raise ValueError('Output already exists; inspect or postprocess completed raw output separately')
    scene = (request.parent/data['scene']['runtime_scene_path']).resolve(strict=True)
    bound = {str(p): digest(p) for p in (request, scene)}
    return request, root, bound


def execute(command, cwd, log, timeout):
    with Path(log).open('w') as stream:
        child = subprocess.Popen(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                                 start_new_session=True)
        Path(str(log)+'.process.json').write_text(json.dumps(
            dict(pid=child.pid, started_unix=time.time(), timeout_seconds=timeout), indent=2))
        try:
            code = child.wait(timeout=timeout)
        except BaseException:
            try: os.killpg(child.pid, signal.SIGTERM)
            except ProcessLookupError: pass
            try: child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL); child.wait()
            raise
    if code:
        raise RuntimeError(f'Native command exited {code}; inspect {log}')


def run(request, cli, library, signed_policy='preserve', timeout=1800):
    if signed_policy not in ('preserve', 'filter') or not 0 < timeout <= 86400:
        raise ValueError('Supported signed policy and bounded positive timeout required')
    request, root, bound = prepare(request)
    cli, library = Path(cli).resolve(strict=True), Path(library).resolve(strict=True)
    bound[str(cli)] = digest(cli)
    # Fail unavailable/incompatible OIDN before spending the native render budget.
    _, backend = oidn_hdr(np.ones((16, 16, 3), dtype=np.float64), library, 1.)
    root.mkdir(parents=True, exist_ok=False)
    started = time.monotonic()
    working_directory = Path.cwd()
    state = dict(schema='optic_v3_render_then_denoise_v1', complete=False,
                 input_sha256=bound, signed_policy=signed_policy, output_root=str(root),
                 native_working_directory=str(working_directory),
                 raw_preserved=True, renderer_defaults_changed=False,
                 experimental=True, feature_preservation_certified=False)
    def write_state(phase):
        state.update(phase=phase, elapsed_seconds=time.monotonic()-started)
        (root/'denoise_job_state.json').write_text(json.dumps(state, indent=2))
    def check_inputs():
        if any(digest(p) != h for p, h in bound.items()):
            raise ValueError('Request, scene or renderer changed during job')
    try:
        write_state('preflight')
        command = [str(cli), '--request', str(request), '--summary-file-only']
        execute(command+['--preflight', '--summary', str(root/'preflight.json')],
                working_directory, root/'preflight.log', min(timeout, 60))
        preflight = json.loads((root/'preflight.json').read_text()).get('spectral_transport', {})
        quality = preflight.get('quality', {})
        if (preflight.get('route') != 'surface_path_v1' or preflight.get('status') != 'prepared'
                or preflight.get('linear_components', {}).get('enabled') is not True
                or quality.get('component') != 'total'
                or quality.get('display_transform') not in ('spectral_soft_v1', 'legacy_clip')
                or not np.isfinite(quality.get('exposure', 0)) or quality.get('exposure', 0) <= 0):
            raise ValueError('Prepared ordinary total V3 film with linear component export required')
        check_inputs(); write_state('rendering')
        execute(command+['--render', '--summary', str(root/'summary.json')],
                working_directory, root/'render.log', timeout)
        check_inputs(); write_state('denoising')
        result = denoise_still(root/'summary.json', root/'frames/frame_0000.bmp',
                               root/'denoised', library, signed_policy=signed_policy)
        if result.get('complete') is not True:
            raise ValueError('Denoise stage did not complete')
        check_inputs()
        state.update(complete=True, denoise_manifest_sha256=digest(root/'denoised/manifest.json'),
                     backend_preflight=backend, source_sha256=digest(__file__))
        write_state('complete')
        return state
    except BaseException as error:
        state['error'] = dict(type=type(error).__name__, message=str(error))
        write_state('failed'); raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('request', 'cli', 'oidn-library'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--signed-policy', choices=('preserve', 'filter'), default='preserve')
    parser.add_argument('--timeout-seconds', type=int, default=1800)
    args = parser.parse_args()
    result = run(args.request, args.cli, args.oidn_library, args.signed_policy, args.timeout_seconds)
    print(json.dumps({k: result[k] for k in ('complete', 'phase', 'output_root', 'elapsed_seconds')}, indent=2))


if __name__ == '__main__': main()
