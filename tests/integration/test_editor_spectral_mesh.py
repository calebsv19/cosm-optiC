"""Native editor V3 deposition switching with fog overlapping a convex prism."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
from test_editor_spectral_render import REPO, prepare


def run(binary, root):
    prepare(root, True, 'adaptive')
    nx, ny, nz = 28, 20, 6
    n = nx * ny * nz
    density = [float(0 < x < nx-1 and 0 < y < ny-1 and 0 < z < nz-1)
               for z in range(nz) for y in range(ny) for x in range(nx)]
    header = struct.pack('<5I4xdQd7fI3I4x', 0x56463344, 1, nx, ny, nz,
                         0., 0, .1, -.2, -.4, -.15, .05, 0, 0, 1, 0, 0, 0, 0)
    field = root/'detector_fog.vf3d'
    field.write_bytes(header + struct.pack(f'<{n}f', *density) + bytes(n*17))
    scene = json.loads((root/'scene.json').read_text())
    volume = scene['extensions']['ray_tracing']['spectral_transport_v1']['participating_volume_v1']
    volume.update(cell_m=.002, sigma_a_per_m=[[380,.01],[780,.01]],
                  sigma_s_per_m=[[380,.58],[780,.4]])
    (root/'scene.json').write_text(json.dumps(scene, indent=2))
    with (root/'native.log').open('w') as log:
        subprocess.run([binary, root, root/'scene.json', '--spectral-ui'],
                       cwd=root, env=dict(os.environ, OPTIC_TEST_PRISM='1',
                       RAY_TRACING_PROGRAM_ROOT=str(REPO),
                       RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(root/'cache')),
                       stdout=log, stderr=log, check=True, timeout=1800)
    for tag in ('surface', 'volume'):
        receipt = json.loads((root/(tag+'.json')).read_text())
        assert receipt['output_verified']
        assert hashlib.sha256(Path(receipt['image_path']).read_bytes()).hexdigest() == receipt['image_sha256']
        assert receipt['volume_attached'] == (tag == 'volume')
        assert (receipt['volume_scatter_events'] > 0) == (tag == 'volume')
        assert receipt['surface_deposits'] > 0
        assert receipt['settings']['integrator'] == 'disney_v3_spectral'
        if tag == 'volume':
            assert receipt['spectral_volume_frame_sha256'] == hashlib.sha256(field.read_bytes()).hexdigest()
    return json.loads((root/'volume.json').read_text())['image_sha256']


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, required=True)
    args = parser.parse_args()
    root = args.output_root.resolve()
    root.mkdir(exist_ok=False)
    assert run(args.binary.resolve(), root/'first') == run(args.binary.resolve(), root/'replay')
    print('PASS ordinary editor mesh-overlap adaptive volume, deposition switching and deterministic replay')
