"""Read and verify ordinary V3 float64 XYZ components without NumPy.

Usage: python3 tools/spectral_components.py --summary report.json --image frame.bmp
The report's linear_components.file is relative to the image directory.
"""
import argparse
import ast
from array import array
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

LABELS = ('total', 'direct', 'surface_indirect', 'volume', 'caustic',
          'caustic_bounce', 'volume_caustic', 'scatter_1', 'scatter_2',
          'scatter_3_4', 'scatter_5_plus')


def read(path):
    with Path(path).open('rb') as stream:
        prefix = stream.read(10)
        if prefix[:8] != b'\x93NUMPY\x01\x00':
            raise ValueError('Expected NPY v1')
        header = ast.literal_eval(stream.read(struct.unpack('<H', prefix[8:])[0]).decode('ascii'))
        height, width, count, channels = header['shape']
        if header['descr'] != '<f8' or header['fortran_order'] or count != 11 or channels != 3:
            raise ValueError('Unexpected V3 component layout')
        values = array('d'); values.frombytes(stream.read())
        if sys.byteorder != 'little':
            values.byteswap()
        if len(values) != height*width*count*channels:
            raise ValueError('Truncated or oversized V3 component file')
    return width, height, values


def display(xyz, exposure, soft):
    x, y, z = (v*exposure for v in xyz)
    rgb = [3.2404542*x-1.5371385*y-.4985314*z,
           -.9692660*x+1.8760108*y+.0415560*z,
           .0556434*x-.2040259*y+1.0572252*z]
    if soft:
        low = min(rgb)
        if low < 0:
            lum = max(0, .2126729*rgb[0]+.7151522*rgb[1]+.0721750*rgb[2])
            chroma = lum/(lum-low)
            rgb = [chroma*(c-low) for c in rgb]
        scale = 1/(1+max(rgb)); rgb = [c*scale for c in rgb]
    result = []
    for c in rgb:
        c = max(0, min(1, c))
        c = 12.92*c if c <= .0031308 else 1.055*c**(1/2.4)-.055
        result.append(math.floor(255*c+.5))
    return bytes(result+[255])


def verify(report, image):
    from optical_comparison_artifacts import bmp_rgba
    export = report['linear_components']
    assert export['written'] and export['components'] == list(LABELS)
    path = Path(image).parent/export['file']
    assert path.name == export['file']  # manifest supplies a basename
    assert path.stat().st_size == export['bytes']
    assert hashlib.sha256(path.read_bytes()).hexdigest() == export['sha256']
    width, height, data = read(path)
    assert (width, height) == (report['film']['width'], report['film']['height'])
    w, h, rgba = bmp_rgba(Path(image)); assert (w, h) == (width, height)
    sums = [[0.]*3 for _ in LABELS]
    max_residual = 0.; max_relative = 0.; exact = 0
    selected = LABELS.index(report['quality']['component'])
    for pixel in range(width*height):
        components = [data[pixel*33+k*3:pixel*33+(k+1)*3] for k in range(11)]
        assert all(math.isfinite(v) for group in components for v in group)
        for c in range(3):
            total = ((components[1][c]+components[2][c])+components[3][c])+(components[4][c]+components[5][c])
            assert struct.pack('<d',total) == struct.pack('<d',components[0][c]), (pixel,c,total,components[0][c])
            residual = abs(sum(components[k][c] for k in range(7,11))+components[6][c]-components[3][c])
            max_residual = max(max_residual,residual)
            max_relative = max(max_relative,residual/max(1e-300,abs(components[3][c])))
            assert residual <= 3e-12*max(1,abs(components[3][c])), (pixel,c,residual)
        expected = display(components[selected],report['quality']['exposure'],report['quality']['display_transform']=='spectral_soft_v1')
        assert expected == rgba[pixel*4:(pixel+1)*4], (pixel,expected,rgba[pixel*4:(pixel+1)*4])
        exact += 1
        for k in range(11):
            for c in range(3):sums[k][c] += components[k][c]
    ledger = report['film_components_xyz']
    for k in range(1,7):
        key = 'volume_caustic_subset' if k == 6 else LABELS[k]
        if key in ledger:
            assert all(math.isclose(a,b,rel_tol=1e-12,abs_tol=1e-12) for a,b in zip(sums[k],ledger[key]))
    for k,group in enumerate(report['scattering_orders']['groups']):
        assert all(math.isclose(a,b,rel_tol=1e-12,abs_tol=1e-12) for a,b in zip(sums[7+k],group['xyz']))
    return dict(exact_recombined_pixels=exact,exact_display_pixels=exact,
                max_volume_subset_absolute_residual=max_residual,
                max_volume_subset_relative_residual=max_relative,component_xyz=sums)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--summary',type=Path,required=True)
    parser.add_argument('--image',type=Path,required=True)
    args = parser.parse_args()
    report = json.loads(args.summary.read_text()); report = report.get('spectral_transport',report)
    print(json.dumps(verify(report,args.image),indent=2))
