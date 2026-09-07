#!/usr/bin/env python3
"""Retain a fixed-budget/adaptive/denoise comparison; never overwrite evidence."""
import argparse
import copy
import json
import math
from pathlib import Path
import struct
import subprocess


def pixels(path):
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    if struct.unpack_from('<H', data, 28)[0] != 32:
        raise ValueError('Expected renderer 32-bit BMP')
    rows = []
    for y in range(abs(height)):
        start = offset + (abs(height)-1-y if height > 0 else y)*width*4
        row = data[start:start+width*4]
        rows.extend(row[i:i+3] for i in range(0, len(row), 4))
    return b''.join(rows)


def summarize(root):
    report = {}
    reference = pixels(next((root/'reference/frames').glob('frame_*.bmp')))
    for name in ('reference', 'fixed48', 'adaptive48', 'denoised48'):
        run = root/name
        summary = json.loads((run/'summary.json').read_text())
        stats = next(v for v in summary.values()
                     if isinstance(v, dict) and 'temporal_pixels_rendered' in v)
        image = pixels(next((run/'frames').glob('frame_*.bmp')))
        if len(image) != len(reference):
            raise ValueError('Comparison dimensions differ')
        report[name] = {
            'display_rgb_rmse_to_96_pass_reference': math.sqrt(sum((a-b)**2 for a,b in zip(image, reference))/len(image)),
            'pixel_samples_rendered': stats['temporal_pixels_rendered'],
            'pixel_samples_skipped': stats['temporal_pixels_skipped'],
            'denoise_filtered_pixels': stats['denoise_reconstructed_pixel_count'],
            'denoise_preserved_glossy_pixels': stats['denoise_preserved_mirror_glossy_pixel_count'],
            'route_mismatches': summary['prepared_acceleration']['route_parity_mismatches'],
        }
    (root/'comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    if any(v['route_mismatches'] for v in report.values()):
        raise RuntimeError('Tracing route mismatches; evidence retained')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--request', type=Path, required=True)
    parser.add_argument('--renderer', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = args.out.resolve()
    root.mkdir(parents=True, exist_ok=False)
    request = json.loads(args.request.read_text())
    scene = Path(request['scene']['runtime_scene_path'])
    if not scene.is_absolute():
        raise ValueError('Use an absolute runtime_scene_path for this standalone comparison')
    for name, passes, adaptive, denoise in (
        ('reference', 96, False, False), ('fixed48', 48, False, False),
        ('adaptive48', 48, True, False), ('denoised48', 48, True, True)):
        run = root/name
        run.mkdir()
        q = copy.deepcopy(request)
        q['run_id'] = name
        q['render'].update(frame_count=1, temporal_frames=passes,
                           adaptive_sampling_enabled=adaptive, denoise_enabled=denoise)
        q['output'] = {'root': str(run), 'overwrite': False}
        q['progress'] = {'summary_path': str(run/'summary.json'), 'progress_path': str(run/'progress.json')}
        path = run/'request.json'
        path.write_text(json.dumps(q, indent=2)+'\n')
        with (run/'render.log').open('w') as log:
            subprocess.run([str(args.renderer.resolve()), '--request', str(path), '--render',
                            '--summary', str(run/'summary.json')], stdout=log, stderr=subprocess.STDOUT, check=True)
    print(json.dumps(summarize(root), indent=2))


if __name__ == '__main__':
    main()
