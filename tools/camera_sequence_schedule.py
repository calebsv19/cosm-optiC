"""Independent exact camera schedule readback for native video receipts."""
from fractions import Fraction

def rational(o):
    return Fraction(o['numerator'], o['denominator'])

def sample(rig, local_frame, camera_id=None):
    rate = rational(rig['rate'])
    shots = rig['shots']
    active = [s for s in shots if s['start_frame'] <= local_frame < s['start_frame'] + s['frame_count']]
    if len(active) != 1:
        raise ValueError('camera shot coverage')
    shot = active[0]
    camera_id = camera_id or shot['camera_id']
    cameras = [c for c in rig['cameras'] if c['id'] == camera_id]
    if len(cameras) != 1:
        raise ValueError('camera identity')
    camera = cameras[0]
    clock = camera['clock']
    mode = clock['mode']
    t = Fraction(local_frame, 1) / rate
    if mode != 'continuous':
        t = Fraction(0)
        for segment in shots:
            if segment['camera_id'] != camera_id or local_frame < segment['start_frame']:
                continue
            elapsed = Fraction(min(local_frame-segment['start_frame'], segment['frame_count']), 1) / rate
            if mode == 'selected':
                t += elapsed
            elif mode == 'clip':
                t = rational(segment['clip_start']) + elapsed
            else:
                raise ValueError('camera clock policy')
    t = min(rational(clock['offset']) + t*rational(clock['speed']), Fraction(clock['duration_frames']-1, 1)/rate)
    return dict(id=camera_id, shot=shot['id'], selected=camera_id == shot['camera_id'], time=t, policy={'continuous':1, 'selected':2, 'clip':3}[mode])
