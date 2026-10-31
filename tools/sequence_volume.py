"""Bind a retained contiguous VF3D manifest to an existing exact sequence.
Writes a new scene beside the input so its project-relative assets keep meaning.
Native file admission verifies the full VF3D payload and exact source mapping.
"""
import argparse
import hashlib
import json
from pathlib import Path


def bind(scene_path, output):
    scene_path, output = Path(scene_path).resolve(), Path(output).absolute()
    if output.exists() or output.parent != scene_path.parent:
        raise ValueError('Use a fresh output beside the source scene')
    scene = json.loads(scene_path.read_text())
    author = scene['extensions']['ray_tracing']['authoring']
    attachment = author['atmosphere_attachment']
    if attachment['source_kind'] != 'manifest':
        raise ValueError('Only a complete VF3D manifest is supported')
    manifest = (scene_path.parent / attachment['source_path']).resolve()
    manifest.relative_to(scene_path.parent)
    entries = json.loads(manifest.read_text())['frames']
    pins = []
    for entry in entries:
        frame = (manifest.parent / entry['path']).resolve()
        frame.relative_to(scene_path.parent)
        pins.append(dict(frame_index=entry['frame_index'], sha256=hashlib.sha256(frame.read_bytes()).hexdigest()))
    if not pins or [p['frame_index'] for p in pins] != list(range(pins[0]['frame_index'], pins[0]['frame_index'] + len(pins))):
        raise ValueError('Manifest must be a sorted contiguous recorded envelope')
    playback = attachment['playback']
    playback.update(animated=True, missing='error')
    author['sequence']['source'] = dict(id=scene['scene_id'], revision=hashlib.sha256(manifest.read_bytes()).hexdigest(), rate=dict(numerator=playback['rate_numerator'], denominator=playback['rate_denominator']), first_tick=pins[0]['frame_index'], last_tick=pins[-1]['frame_index'])
    author['sequence_volume'] = dict(schema='ray_sequence_volume_v1', frames=pins)
    with output.open('x') as f:
        json.dump(scene, f, indent=2)
        f.write('\n')
    return output


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--scene', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    print(bind(a.scene, a.output))
