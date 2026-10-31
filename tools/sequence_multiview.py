"""Deliver synchronized named angles, an edited view and a comparison matrix.
Each angle is rendered through the native CLI and checked independently. Output
must be new; interrupted jobs retain their own render folders for inspection.
"""
import argparse
import copy
from fractions import Fraction
import hashlib
import json
import os
from pathlib import Path
import subprocess
from sequence_video import assemble, read, sha, _require
from sequence_temporal_review import temporal_review, verify_video


def state(row):
    return {k: row.get(k) for k in ('frame', 'rate', 'presentation_time', 'simulation_time', 'source_tick', 'source_revision', 'bodies', 'cache')}


def deliver(request_path, camera_ids, output, cli, ffmpeg='ffmpeg', ffprobe='ffprobe', include_edit=True, existing_frames=None):
    output = Path(output).resolve()
    _require(not output.exists(), 'Delivery output must be new')
    request = read(request_path)
    scene_path = Path(request['scene']['runtime_scene_path']).resolve()
    scene = read(scene_path)
    author = scene['extensions']['ray_tracing']['authoring']
    seq, rig = author['sequence'], author['camera_rig']
    names = [c['id'] for c in rig['cameras']]
    _require(1 <= len(camera_ids) <= 8 and len(set(camera_ids)) == len(camera_ids) and all(c in names for c in camera_ids), 'Select 1–8 unique declared cameras')
    _require(not any(k.startswith('camera_') for k in request.get('inspection', {})), 'Author the cameras instead of overriding evaluated views')
    _require(not request.get('output', {}).get('video', {}).get('enabled'), 'Use native frame output')
    if existing_frames is not None:
        expected = set(camera_ids) | ({'edit'} if include_edit else set())
        _require(set(existing_frames) == expected, 'Frame manifest must name exactly the requested views')
    output.mkdir(parents=True)
    rate = Fraction(seq['rate']['numerator'], seq['rate']['denominator'])
    start, count = seq['range']['start_frame'], seq['range']['frame_count']
    scene_hash = sha(scene_path)
    variants = [('edit', None)] if include_edit else []
    variants += [(f'angle_{i+1}', c) for i, c in enumerate(camera_ids)]
    proofs, baseline, allpins = [], None, {}
    try:
        for label, camera in variants:
            folder = output / label
            folder.mkdir()
            req = copy.deepcopy(request)
            req['scene'].pop('camera_id', None)
            if camera:
                req['scene']['camera_id'] = camera
            req['render'].update(start_frame=start, frame_count=count, temporal_frames=1)
            req['output'] = dict(root=str(folder / 'render'), overwrite=False)
            req.pop('video', None)
            req.pop('progress', None)
            if existing_frames is None:
                native_request = folder / 'request.json'
                native_request.write_text(json.dumps(req, indent=2) + '\n')
                env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(folder / 'program'), XDG_CONFIG_HOME=str(folder / 'config'))
                (folder / 'program').mkdir()
                with (folder / 'native.log').open('w') as log:
                    subprocess.run([str(cli), '--request', str(native_request), '--render', '--summary', str(folder / 'native.json'), '--summary-file-only'], cwd=folder, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
                frames = folder / 'render' / 'frames'
            else:
                frames = Path(existing_frames[camera or "edit"]).resolve()
            video = folder / 'video.mp4'
            proof = assemble(scene_path, frames, video, ffmpeg, ffprobe, camera)
            _require(proof['stream']['width'] == request['render']['width'] and proof['stream']['height'] == request['render']['height'], 'View dimensions do not match the delivery request')
            rows = [read(Path(str(frames / f'frame_{i:04d}.bmp') + '.sequence.json')) for i in range(start, start + count)]
            physical = [state(row) for row in rows]
            if baseline is None:
                baseline = physical
            _require(physical == baseline, 'Views disagree about physical state at the same presentation frame')
            allpins.update(proof['input_hashes'])
            quality = temporal_review(rows, frames, rate, ffmpeg)
            (folder / 'temporal-review.json').write_text(json.dumps(quality, indent=2) + '\n')
            proofs.append(dict(label=label, camera_id=camera, video=str(video), sha256=sha(video), frame_count=count, duration=str(Fraction(count, 1) / rate), temporal_review=quality))
        angles = [p for p in proofs if p['camera_id']]
        columns = min(2, len(angles))
        width, height = int(request['render']['width']), int(request['render']['height'])
        _require(width > 0 and height > 0 and width % 2 == 0 and height % 2 == 0)
        matrix = output / 'matrix.mp4'
        command = [ffmpeg, '-v', 'error', '-n']
        for p in angles:
            command += ['-i', p['video']]
        if len(angles) == 1:
            command += ['-c', 'copy', str(matrix)]
        else:
            layout = '|'.join(f'{(i % columns)*width}_{(i//columns)*height}' for i in range(len(angles)))
            command += ['-filter_complex', f'xstack=inputs={len(angles)}:layout={layout}:fill=black[v]', '-map', '[v]', '-frames:v', str(count), '-c:v', 'libx264', '-crf', '18', '-pix_fmt', 'yuv420p', '-video_track_timescale', str(rate.numerator), '-movie_timescale', str(rate.numerator), str(matrix)]
        subprocess.run(command, check=True)
        verify_video(matrix, rate, count, ffmpeg, ffprobe)
        _require(sha(scene_path) == scene_hash and all(sha(p) == h for p, h in allpins.items()), 'Inputs changed during delivery')
        result = dict(schema='ray_sequence_multiview_v1', ok=True, rate=str(rate), frame_count=count, duration=str(Fraction(count, 1)/rate), scene_sha256=scene_hash, synchronized_states_sha256=hashlib.sha256(json.dumps(baseline, sort_keys=True, separators=(',', ':')).encode()).hexdigest(), views=proofs, matrix=dict(path=str(matrix), sha256=sha(matrix), columns=columns, camera_order=camera_ids), visual_acceptance='review_required', review_checklist=['target/contact readability', 'occlusion and framing', 'cut purpose and direction', 'roll and speed changes', 'shimmer, noise and exposure continuity'])
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        return result
    except BaseException as exc:
        (output / 'failure.json').write_text(json.dumps(dict(ok=False, error=str(exc)), indent=2) + '\n')
        raise


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--request', type=Path, required=True)
    p.add_argument('--camera', action='append', required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--cli', type=Path, required=True)
    p.add_argument('--no-edit', action='store_true')
    p.add_argument('--frames-manifest', type=Path, help='JSON mapping edit and requested camera IDs to existing native frame directories; skips rendering')
    a = p.parse_args()
    r = deliver(a.request, a.camera, a.output, a.cli.resolve(), include_edit=not a.no_edit, existing_frames=read(a.frames_manifest) if a.frames_manifest else None)
    print(json.dumps({k: r[k] for k in ('ok', 'frame_count', 'duration')}))
