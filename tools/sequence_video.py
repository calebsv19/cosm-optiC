"""Encode one complete native sequence without renaming frames or changing time."""
import argparse
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from sequence_encoder import encoding_args, preflight_encoder

def _require(condition, message='Invalid sequence input, receipt or video schedule'):
    if not condition:
        raise ValueError(message)

def read(path):
    return json.loads(Path(path).read_text())

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def assemble(scene_path, frames, output, ffmpeg='ffmpeg', ffprobe='ffprobe', camera_id=None, encoder='libx264'):
    readiness = preflight_encoder(ffmpeg, ffprobe, encoder)
    scene_path, frames, output = map(Path, (scene_path, frames, output))
    scene_hash = sha(scene_path)
    scene = read(scene_path)
    seq = scene['extensions']['ray_tracing']['authoring']['sequence']
    _require(seq['schema'] == 'ray_sequence_clock_v1' and seq['version'] == 1)
    rig = scene['extensions']['ray_tracing']['authoring'].get('camera_rig')
    if camera_id and not rig:
        raise ValueError('camera_id requires an authored camera rig')
    rate = Fraction(seq['rate']['numerator'], seq['rate']['denominator'])
    start, count = (seq['range']['start_frame'], seq['range']['frame_count'])
    _require(rate > 0 and rate.numerator <= 2147483647 and 0 < count <= 1000000 and (start >= 0))
    _require(not output.exists() and (not output.with_suffix(output.suffix + '.json').exists()))
    _require(output.parent.is_dir())
    paths = [frames / f'frame_{i:04d}.bmp' for i in range(start, start + count)]
    _require(set(frames.glob('*.bmp')) == set(paths), 'missing or extra frames')
    pins, receipts = ({}, [])
    for local, image in enumerate(paths):
        receipt_path = Path(str(image) + '.sequence.json')
        row = read(receipt_path)
        _require(row['schema'] == 'ray_sequence_frame_v1' and row['frame'] == start + local)
        _require(Fraction(*row['rate']) == rate)
        _require(Fraction(*row['presentation_time']) == Fraction(local, 1) / rate)
        _require(row['source_revision'] == seq['source']['revision'])
        _require(row['scene_sha256'] == scene_hash and row['image_sha256'] == sha(image))
        segments = [s for s in seq['segments'] if s['start_frame'] <= local < s['start_frame'] + s['frame_count']]
        _require(len(segments) == 1)
        s = segments[0]
        sim = Fraction(s['source_start']['numerator'], s['source_start']['denominator'])
        sim += Fraction(s['speed']['numerator'], s['speed']['denominator']) * Fraction(local - s['start_frame'], 1) / rate
        _require(Fraction(*row['simulation_time']) == sim)
        tick = sim * Fraction(seq['source']['rate']['numerator'], seq['source']['rate']['denominator'])
        _require(tick.denominator == 1 and row['source_tick'] == tick)
        _require(seq['source']['first_tick'] <= tick <= seq['source']['last_tick'])
        if rig:
            from camera_sequence_schedule import sample
            expected = sample(rig, local, camera_id)
            _require(row['camera_id'] == expected['id'] and row['shot_id'] == expected['shot'] and row['camera_selected'] == expected['selected'])
            _require(row['camera_clock_policy'] == expected['policy'] and Fraction(*row['camera_motion_time']) == expected['time'])
            moves = scene['extensions']['ray_tracing']['authoring'].get('inspection_moves', [])
            moves = list(moves) + [dict(camera_id=e['camera_id'], id='event_'+e['id'], **e['intent']['hold']) for e in scene['extensions']['ray_tracing']['authoring'].get('event_cameras', [])]
            entered = [m for m in moves if m['camera_id'] == expected['id']]
            if entered:
                prior = [m for m in entered if m['start_frame'] <= local]
                entered = [max(prior, key=lambda m: m['start_frame']) if prior else min(entered, key=lambda m: m['start_frame'])]
            if entered:
                _require(len(entered) == 1)
                move = entered[0]
                _require(row.get('inspection_id') == move['id'] and Fraction(*row['inspection_time']) == Fraction(max(0, min(local-move['start_frame'], move['frame_count']-1)), 1)/rate)
                if move['start_frame'] <= local < move['start_frame']+move['frame_count']:
                    _require(row['source_tick'] == move['source_tick'])
            else:
                _require(not row.get('inspection_id'))
        if 'sequence_volume' in scene['extensions']['ray_tracing']['authoring']:
            binding = scene['extensions']['ray_tracing']['authoring']['sequence_volume']
            pin = [p for p in binding['frames'] if p['frame_index'] == tick]
            cache = row.get('cache', {})
            _require(not row['bodies'] and len(pin) == 1 and cache.get('id') == scene['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['id'] and cache.get('tick') == tick and cache.get('content_digest') == pin[0]['sha256'], 'volume receipt is not the pinned complete source state')
        else:
            _require(row['bodies'] and all((b['tick'] == tick for b in row['bodies'])))
        receipts.append(row)
        pins[str(image)] = sha(image)
        pins[str(receipt_path)] = sha(receipt_path)
    encoded = False
    try:
        subprocess.run([ffmpeg, '-v', 'error', '-n', '-framerate', str(rate), '-start_number', str(start), '-i', str(frames / 'frame_%04d.bmp'), '-frames:v', str(count), *encoding_args(encoder), '-pix_fmt', 'yuv420p', '-movflags', '+faststart', '-video_track_timescale', str(rate.numerator), '-movie_timescale', str(rate.numerator), str(output)], check=True)
        encoded = True
        probe = json.loads(subprocess.check_output([ffprobe, '-v', 'error', '-select_streams', 'v:0', '-show_streams', '-show_frames', '-show_entries', 'stream=width,height,avg_frame_rate,time_base,duration,duration_ts,nb_frames:frame=best_effort_timestamp', '-of', 'json', str(output)]))
        stream = probe['streams'][0]
        _require(Fraction(stream['avg_frame_rate']) == rate and len(probe['frames']) == count)
        base = Fraction(stream['time_base'])
        _require(all((int(f['best_effort_timestamp']) * base == Fraction(i, 1) / rate for i, f in enumerate(probe['frames']))))
        _require(int(stream['duration_ts'])*base == Fraction(count, 1)/rate)
        subprocess.run([ffmpeg, '-v', 'error', '-i', str(output), '-f', 'null', '-'], check=True)
        _require(sha(scene_path) == scene_hash)
        _require(all((sha(p) == digest for p, digest in pins.items())))
        proof = dict(ok=True, encoder=encoder, encoder_readiness=readiness, schema='ray_sequence_video_v1', rate=str(rate), frame_count=count, duration=str(Fraction(count, 1) / rate), stream=stream, all_pts_exact=True, decoded=True, input_hashes=pins, scene_sha256=scene_hash, video_sha256=sha(output))
        output.with_suffix(output.suffix + '.json').write_text(json.dumps(proof, indent=2) + '\n')
        return proof
    except BaseException:
        if encoded:
            output.with_suffix(output.suffix + '.failed.json').write_text(json.dumps({'ok': False, 'video': str(output)}))
        raise

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--camera-id', help='Fixed named view; omit to follow authored shots')
    p.add_argument('--scene', type=Path)
    p.add_argument('--frames', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--ffmpeg', default=shutil.which('ffmpeg'))
    p.add_argument('--ffprobe', default=shutil.which('ffprobe'))
    p.add_argument('--encoder', choices=('libx264', 'mpeg4'), default='libx264')
    p.add_argument('--preflight', action='store_true', help='Check actual encoder readiness without rendering')
    a = p.parse_args()
    if a.preflight:
        print(json.dumps(preflight_encoder(a.ffmpeg, a.ffprobe, a.encoder)))
        return
    if not all((a.scene, a.frames, a.output)):
        p.error('--scene, --frames and --output are required for assembly')
    result = assemble(a.scene, a.frames, a.output, a.ffmpeg, a.ffprobe, a.camera_id, a.encoder)
    print(json.dumps({k: result[k] for k in ['ok', 'rate', 'frame_count', 'duration']}))
if __name__ == '__main__':
    main()
