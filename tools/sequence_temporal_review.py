"""Measured temporal diagnostics with explicit cuts and a human review boundary."""
from fractions import Fraction
import hashlib
import json
import math
import struct
import subprocess


def verify_video(path, rate, count, ffmpeg='ffmpeg', ffprobe='ffprobe', timeout=None):
    p = json.loads(subprocess.check_output([ffprobe, '-v', 'error', '-select_streams', 'v:0', '-show_streams', '-show_frames', '-show_entries', 'stream=avg_frame_rate,time_base,duration_ts:frame=best_effort_timestamp', '-of', 'json', str(path)], timeout=timeout))
    s = p['streams'][0]
    if len(p['frames']) != count or Fraction(s['avg_frame_rate']) != rate or int(s['duration_ts'])*Fraction(s['time_base']) != Fraction(count, 1)/rate or any(int(f['best_effort_timestamp'])*Fraction(s['time_base']) != Fraction(i, 1)/rate for i, f in enumerate(p['frames'])):
        raise ValueError('Nonuniform matrix PTS or changed duration')
    subprocess.run([ffmpeg, '-v', 'error', '-i', str(path), '-f', 'null', '-'], check=True, timeout=timeout)


def forward(row):
    if row.get('camera_has_orientation_frame'):
        return row['camera_frame'][:3]
    yaw, pitch = row.get('camera_yaw', 0), row.get('camera_pitch', 0)
    return [math.sin(yaw)*math.cos(pitch), -math.cos(yaw)*math.cos(pitch), math.sin(pitch)]


def temporal_review(rows, frames, rate, ffmpeg='ffmpeg'):
    images, changes, cuts = [], [], []
    previous = None
    previous_speed = None
    for row in rows:
        if len(row['camera']) != 3 or not all(math.isfinite(x) for x in row['camera'] + forward(row)):
            raise ValueError('Invalid camera pose in temporal review')
    first = frames / f"frame_{rows[0]['frame']:04d}.bmp"
    with first.open('rb') as f:
        header = f.read(26)
    if header[:2] != b'BM':
        raise ValueError('Expected native BMP frames')
    width, height = struct.unpack_from('<ii', header, 18)
    if width <= 0 or height == 0:
        raise ValueError('Invalid native image dimensions')
    size = width * abs(height) * 3
    decoder = subprocess.Popen([ffmpeg, '-v', 'error', '-start_number', str(rows[0]['frame']), '-i', str(frames / 'frame_%04d.bmp'), '-frames:v', str(len(rows)), '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        for i, row in enumerate(rows):
            raw = decoder.stdout.read(size)
            if len(raw) != size:
                decoder.kill()
                decoder.communicate()
                raise ValueError('Incomplete temporal review decode')
            digest = hashlib.sha256(raw).hexdigest()
            images.append(digest)
            if previous:
                old, oldraw = previous
                cut = row.get('camera_id') != old.get('camera_id')
                if cut:
                    cuts.append(row['frame'])
                delta = sum(abs(a-b) for a, b in zip(raw, oldraw))/len(raw)
                distance = math.sqrt(sum((a-b)**2 for a, b in zip(row['camera'], old['camera'])))
                # Measurements flag review needs; they are not an artistic acceptance threshold.
                dot = sum(a*b for a, b in zip(forward(old), forward(row)))
                angle = math.degrees(math.acos(max(-1, min(1, dot))))
                speed = distance*float(rate)
                acceleration = (speed-previous_speed)*float(rate) if previous_speed is not None and not cut else None
                up_angle = None
                if old.get('camera_has_orientation_frame') and row.get('camera_has_orientation_frame'):
                    up_dot = sum(a*b for a, b in zip(old['camera_frame'][3:6], row['camera_frame'][3:6]))
                    up_angle = math.degrees(math.acos(max(-1, min(1, up_dot))))
                changes.append(dict(frame=row['frame'], declared_cut=cut, source_held=row['source_tick']==old['source_tick'], rgb_mean_absolute_change=delta, camera_distance=distance, camera_speed=None if cut else speed, camera_speed_change_per_second=acceleration, view_angle_change_degrees=angle, up_axis_change_degrees=up_angle, fov_change=row.get('camera_fov_y', 0)-old.get('camera_fov_y', 0), repeated_image=digest==images[-2]))
                previous_speed = None if cut else speed
            previous = row, raw
        tail, errors = decoder.communicate()
        if decoder.returncode or tail:
            raise ValueError('Temporal review decoder failed: ' + errors.decode(errors='replace'))
    except BaseException:
        decoder.kill()
        decoder.communicate()
        raise
    return dict(schema='ray_sequence_temporal_review_v1', rate=str(rate), samples=len(rows), distinct_images=len(set(images)), cuts=cuts, transitions=changes, visual_acceptance='review_required', image_sha256=images)
