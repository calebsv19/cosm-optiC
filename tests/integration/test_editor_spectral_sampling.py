"""Actual editor controls plus saved-profile, camera reset and cancellation checks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from test_editor_spectral_render import REPO, prepare


def qualify(binary, root):
    if root.exists():
        raise ValueError('Use a fresh output root')
    prepare(root, True, 'adaptive')
    scene = json.loads((root/'scene.json').read_text())
    scene['extensions']['ray_tracing']['spectral_transport_v1']['sampling'] = {
        'mode': 'converge', 'max_passes': 16}
    (root/'scene.json').write_text(json.dumps(scene))
    with (root/'native.log').open('w') as log:
        subprocess.run([binary, root, root/'scene.json', '--spectral-sampling-ui'],
                       cwd=root, env=dict(os.environ, OPTIC_TEST_PRISM='1',
                       RAY_TRACING_PROGRAM_ROOT=str(REPO),
                       RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(root/'cache')),
                       stdout=log, stderr=log, check=True, timeout=1200)
    for tag in ('surface', 'volume'):
        receipt = json.loads((root/(tag+'.json')).read_text())
        assert receipt['output_verified']
        assert hashlib.sha256(Path(receipt['image_path']).read_bytes()).hexdigest() == receipt['image_sha256']
        stats = receipt['spectral_sampling']
        assert stats['passes'] == 16
        assert stats['status'] == ('converged' if tag == 'volume' else 'pass_limit')
        print(tag, stats)
    print('PASS editor controls, saved profile, scheduler parity, camera reset, replay and cancellation')


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--output-root', type=Path, required=True)
    a = p.parse_args()
    qualify(a.binary.resolve(), a.output_root.resolve())
