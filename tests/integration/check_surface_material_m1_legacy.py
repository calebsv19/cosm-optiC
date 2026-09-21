#!/usr/bin/env python3
"""Assert M1 sampler parity and frozen M0 legacy render bytes after the diagnostic."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root', type=Path, required=True)
    args = parser.parse_args()
    old = json.loads((ROOT / 'tests/fixtures/surface_material_m0/legacy_baseline.json').read_text())
    current = json.loads((args.output_root / 'diagnostic.json').read_text())
    assert set(current['cases']) == set(old['diagnostic']['cases'])
    frames = {}
    for name, expected in old['files'].items():
        if name.endswith('.bmp'):
            actual = hashlib.sha256((args.output_root / name).read_bytes()).hexdigest()
            assert actual == expected, name
            frames[name] = actual
    assert len(frames) == 8
    max_channel_error = max_cache_error = 0
    for case, metrics in current['cases'].items():
        for face, baseline in zip(metrics['faces'], old['diagnostic']['cases'][case]['faces'], strict=True):
            assert face['hits'] == baseline['hits'], (case, face)
            assert face['max_channel_error'] <= 1e-6, (case, face)
            assert face['cache_rgb_mae'] <= .05, (case, face)
            max_channel_error = max(max_channel_error, face['max_channel_error'])
            max_cache_error = max(max_cache_error, face['cache_rgb_mae'])
    for change in current['changes'].values():
        assert change['viewport_albedo_changed_samples'] == change['render_albedo_changed_samples']
    print(json.dumps({'legacy_runtime_bmp_hashes_match': frames,
                      'max_uncached_channel_error': max_channel_error,
                      'max_cache_rgb_mae': max_cache_error,
                      'cache_rgb_mae_limit': .05,
                      'face_override_effects_match': True}, indent=2))


if __name__ == '__main__':
    main()
