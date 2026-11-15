"""Explicit XYZ/linear-sRGB adapter for an externally supplied OIDN 2 CPU library.

No installation, download, renderer integration, guides or display transform.
Signed RGB residuals bypass the nonnegative HDR model and are restored exactly
before conversion back to XYZ. This policy requires scene-specific qualification.
"""
import ctypes as C
import hashlib
import os
from pathlib import Path
import re
import sys
import time

import numpy as np
from spectral_batch_accumulate import digest

XYZ_TO_RGB = np.array([[3.2404542, -1.5371385, -.4985314],
                       [-.9692660, 1.8760108, .0415560],
                       [.0556434, -.2040259, 1.0572252]], dtype=np.float64)
RGB_TO_XYZ = np.linalg.inv(XYZ_TO_RGB)


def split_rgb(xyz):
    rgb = np.asarray(xyz, dtype=np.float64) @ XYZ_TO_RGB.T
    if not np.isfinite(rgb).all():
        raise ValueError('Nonfinite RGB adapter input')
    positive = np.maximum(rgb, 0)
    return positive, rgb - positive


def restore_xyz(positive, residual):
    return (np.asarray(positive, dtype=np.float64) + residual) @ RGB_TO_XYZ.T


def adjacent_library_identities(path):
    """Snapshot the supplied runtime, including versioned ELF and Mach-O libraries."""
    paths = {path}
    for pattern in ('*.dylib', '*.so', '*.so.*'):
        paths.update(p.resolve(strict=True) for p in path.parent.glob(pattern))
    return {str(p): digest(p) for p in sorted(paths)}


def linux_loaded_libraries():
    """Read file-backed shared objects while the OIDN CPU device is alive."""
    mapped = {}
    for line in Path('/proc/self/maps').read_text().splitlines():
        fields = line.split(maxsplit=5)
        if len(fields) != 6 or not fields[5].startswith('/'):
            continue
        name = fields[5].removesuffix(' (deleted)')
        if not re.search(r'\.so(?:\.[^/]+)?$', Path(name).name):
            continue
        if name != fields[5]:
            raise ValueError('Mapped shared library was deleted: ' + name)
        path = Path(name).resolve(strict=True)
        major, minor = (int(part, 16) for part in fields[3].split(':'))
        identity = (major, minor, int(fields[4]))
        if path in mapped and mapped[path] != identity:
            raise ValueError('Mapped shared library identity disagrees: ' + name)
        mapped[path] = identity
    return mapped


def digest_loaded_libraries(mapped):
    identities = {}
    for path, (major, minor, inode) in sorted(mapped.items()):
        with path.open('rb') as stream:
            st = os.fstat(stream.fileno())
            if (os.major(st.st_dev), os.minor(st.st_dev), st.st_ino) != (major, minor, inode):
                raise ValueError('On-disk library differs from the loaded file: ' + str(path))
            identities[str(path)] = hashlib.file_digest(stream, 'sha256').hexdigest()
    return identities


def oidn_hdr(xyz, library, input_scale):
    """Synchronous OIDN RT/high, CPU four threads, no auxiliary guides."""
    if not np.isfinite(input_scale) or input_scale <= 0:
        raise ValueError('Positive finite OIDN input scale required')
    path = Path(library).resolve(strict=True)
    identities = adjacent_library_identities(path)
    linux = sys.platform == 'linux'
    before = digest_loaded_libraries(linux_loaded_libraries()) if linux else {}
    lib = C.CDLL(str(path))
    signatures = {
        'oidnNewDevice': (C.c_void_p, [C.c_int]),
        'oidnCommitDevice': (None, [C.c_void_p]),
        'oidnSetDeviceInt': (None, [C.c_void_p, C.c_char_p, C.c_int]),
        'oidnGetDeviceInt': (C.c_int, [C.c_void_p, C.c_char_p]),
        'oidnGetDeviceError': (C.c_int, [C.c_void_p, C.POINTER(C.c_char_p)]),
        'oidnNewFilter': (C.c_void_p, [C.c_void_p, C.c_char_p]),
        'oidnSetSharedFilterImage': (None, [C.c_void_p, C.c_char_p, C.c_void_p,
                                          C.c_int, C.c_size_t, C.c_size_t,
                                          C.c_size_t, C.c_size_t, C.c_size_t]),
        'oidnSetFilterBool': (None, [C.c_void_p, C.c_char_p, C.c_bool]),
        'oidnSetFilterInt': (None, [C.c_void_p, C.c_char_p, C.c_int]),
        'oidnSetFilterFloat': (None, [C.c_void_p, C.c_char_p, C.c_float]),
        'oidnCommitFilter': (None, [C.c_void_p]),
        'oidnExecuteFilter': (None, [C.c_void_p]),
        'oidnReleaseFilter': (None, [C.c_void_p]),
        'oidnReleaseDevice': (None, [C.c_void_p]),
    }
    for name, (result, args) in signatures.items():
        fn = getattr(lib, name); fn.restype = result; fn.argtypes = args
    device = lib.oidnNewDevice(1)  # OIDN_DEVICE_TYPE_CPU
    def check():
        message = C.c_char_p()
        if lib.oidnGetDeviceError(device, C.byref(message)):
            raise RuntimeError('OIDN: ' + (message.value or b'unknown error').decode())
    filter_ = None
    start = time.monotonic()
    try:
        check()
        if not device:
            raise RuntimeError('OIDN CPU unavailable')
        lib.oidnSetDeviceInt(device, b'numThreads', 4)
        lib.oidnCommitDevice(device); check()
        version = lib.oidnGetDeviceInt(device, b'version'); check()
        if not 20000 <= version < 30000:
            raise ValueError('Qualified API requires OIDN 2.x')
        device_type = lib.oidnGetDeviceInt(device, b'type'); check()
        threads = lib.oidnGetDeviceInt(device, b'numThreads'); check()
        if device_type != 1 or threads != 4:
            raise ValueError('Expected a CPU OIDN device with a four-thread limit')
        positive, residual = split_rgb(xyz)
        if positive.max() > np.finfo(np.float32).max:
            raise ValueError('RGB exceeds float32 backend range')
        source = np.ascontiguousarray(positive, dtype=np.float32)
        output = np.empty_like(source)
        height, width, channels = source.shape
        if channels != 3:
            raise ValueError('Expected HxWx3 XYZ')
        filter_ = lib.oidnNewFilter(device, b'RT'); check()
        for name, a in ((b'color', source), (b'output', output)):
            lib.oidnSetSharedFilterImage(filter_, name, C.c_void_p(a.ctypes.data),
                                         3, width, height, 0, 0, 0)  # FLOAT3
        lib.oidnSetFilterBool(filter_, b'hdr', True)
        lib.oidnSetFilterBool(filter_, b'cleanAux', False)
        lib.oidnSetFilterInt(filter_, b'quality', 6)  # OIDN_QUALITY_HIGH
        lib.oidnSetFilterFloat(filter_, b'inputScale', input_scale)
        lib.oidnCommitFilter(filter_); check()
        lib.oidnExecuteFilter(filter_); check()
        if not np.isfinite(output).all():
            raise ValueError('Nonfinite OIDN output')
        loaded = digest_loaded_libraries(linux_loaded_libraries()) if linux else {}
        if linux and loaded.get(str(path)) != identities[str(path)]:
            raise ValueError('OIDN entry library was not observed in the Linux process')
        if any(loaded.get(p) != h for p, h in before.items()):
            raise ValueError('Loaded dependency changed during OIDN filtering')
        if adjacent_library_identities(path) != identities:
            raise ValueError('OIDN library drift during filtering')
        if any(identities.get(p, h) != h for p, h in loaded.items()):
            raise ValueError('Loaded OIDN bytes differ from the supplied runtime snapshot')
        result = restore_xyz(output, residual)
        meta = dict(backend='OIDN RT', version=version, device='CPU', threads=threads,
                    quality='high', hdr=True, cleanAux=False, guides=False,
                    inputScale=input_scale, signed_policy='positive RGB filtered; signed residual restored',
                    negative_rgb_fraction=float(np.mean(residual < 0)),
                    negative_rgb_min=float(residual.min()),
                    float32_roundtrip_max_xyz=float(abs(restore_xyz(source, residual)-xyz).max()),
                    library_sha256=identities, elapsed_seconds=time.monotonic()-start)
        if linux:
            meta['linux_loaded_libraries_sha256'] = loaded
        return result, meta
    finally:
        if filter_: lib.oidnReleaseFilter(filter_)
        if device: lib.oidnReleaseDevice(device)


def oidn_signed_hdr(xyz, library, input_scale):
    """Experimental signed-color policy: denoise both RGB parts, then subtract.

    The two nonnegative model inputs are mathematical color parts, not physical
    transport components. Independent nonlinear filtering can introduce bias;
    the raw XYZ and rendered holdout evidence remain required.
    """
    started = time.monotonic()
    positive, negative = split_rgb(xyz)
    pos_xyz, pos_info = oidn_hdr(positive @ RGB_TO_XYZ.T, library, input_scale)
    neg_info = None
    if np.any(negative < 0):
        neg_xyz, neg_info = oidn_hdr((-negative) @ RGB_TO_XYZ.T, library, input_scale)
        result = pos_xyz-neg_xyz
    else:
        result = pos_xyz
    return result, dict(backend='OIDN RT signed split', signed_policy='filter both nonnegative RGB parts, subtract in XYZ',
                        positive=pos_info, negative=neg_info,
                        negative_rgb_fraction=float(np.mean(negative < 0)),
                        elapsed_seconds=time.monotonic()-started,
                        component_decomposition=False, experimental=True)
