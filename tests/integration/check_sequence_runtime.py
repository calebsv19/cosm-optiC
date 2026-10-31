"""Native Stage2 qualification over a caller-selected authenticated rigid fixture.

No solver, source mutation, app installation or original-scene write. The caller
provides fresh output and compatible freshly built native binaries.
"""
import argparse
import copy
from fractions import Fraction
import hashlib
import json
import math
import os
from pathlib import Path
import random
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    for name in ['base', 'history', 'renderer', 'probe', 'out']:
        parser.add_argument('--'+name, type=Path, required=True)
    a = parser.parse_args()
    a.out.mkdir(exist_ok=False)
    repo = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(repo/'tools'))
    from scene_workflow_bundle import snapshot_inputs
    from sequence_video import assemble
    root = a.out/'fixture'
    snapshot_inputs(a.base/'scene_runtime.json', a.base/'render_request.json', root)
    scene_path = root/'scene_runtime.json'
    scene = json.loads(scene_path.read_text())
    original_physics = copy.deepcopy(scene['extensions']['rigid_scene'])
    result = scene['extensions']['ray_tracing']['rigid_result']
    record = json.loads(a.history.read_text())
    assert record['endpoint_tick'] >= 300
    manifest = json.loads((root/result['bundle_path']/'manifest.json').read_text())
    assert manifest['result_revision'] == result['result_revision']
    assert record['physical_input_sha256'] == manifest['physical_input_sha256']
    start, count = 12, 48
    rate = dict(numerator=24, denominator=1)
    rational = lambda n, d=1: dict(numerator=n, denominator=d)
    seq = dict(schema='ray_sequence_clock_v1', version=1, rate=rate,
        range=dict(start_frame=start, frame_count=count), source=dict(id=scene['scene_id'],
        revision=result['result_revision'], rate=rational(240), first_tick=0,
        last_tick=manifest['endpoint_tick']), segments=[
            dict(id='play', start_frame=0, frame_count=15, kind='play', source_start=rational(0), speed=rational(1)),
            dict(id='inspect', start_frame=15, frame_count=18, kind='hold', source_start=rational(5, 8), speed=rational(0)),
            dict(id='resume', start_frame=33, frame_count=15, kind='play', source_start=rational(5, 8), speed=rational(1))])
    author = scene['extensions']['ray_tracing']['authoring']
    author['sequence'] = seq
    tracks = []
    def track(prop, kind, unit, first, last):
        keys = [dict(frame=f, value=v, interpolation='linear',
            incoming_handle=dict(frame_offset=0, value_offset=0),
            outgoing_handle=dict(frame_offset=0, value_offset=0))
            for f, v in [(start, first), (start+count-1, last)]]
        tracks.append(dict(id=prop.replace('/', '_'), target_id='camera/main',
            property_id=prop, value_type=kind, unit=unit, source='authored', enabled=True, keys=keys))
    positions = [[1.8, -4, 5.2], [.5, -4, 5.2]]
    track('camera/position', 'vec3', 'world_distance', *positions)
    yaws = [math.atan2(-p[0], p[1]) for p in positions]
    pitches = [math.atan2(2-p[2], math.hypot(p[0], p[1])) for p in positions]
    track('camera/yaw', 'scalar', 'radians', *yaws)
    track('camera/pitch', 'scalar', 'radians', *pitches)
    author['scene_timeline'] = dict(version=1, rate=rate, range=seq['range'], tracks=tracks)
    scene_path.write_text(json.dumps(scene, indent=2)+'\n')
    pins = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(a.out/'program'),
        XDG_CONFIG_HOME=str(a.out/'config'),
        RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT=str(a.out/'cache'),
        PYTHONDONTWRITEBYTECODE='1')
    (a.out/'program').mkdir()
    logs = a.out/'logs'; logs.mkdir()
    def run(name, cmd, success=True):
        p = subprocess.run(list(map(str, cmd)), cwd=a.out/'program', env=env,
                           capture_output=True, text=True, timeout=180)
        (logs/(name+'.log')).write_text(p.stdout+p.stderr)
        assert (p.returncode == 0) == success, (name, p.returncode, p.stdout[-400:], p.stderr[-1000:])
        return [json.loads(s[4:]) for s in p.stdout.splitlines() if s.startswith('BR4 ')]
    sample_order = list(range(count)); random.Random(42).shuffle(sample_order)
    sample_order += [0, 15, 32, 33, 47, 15]
    rows = run('native', [a.probe, scene_path, *sample_order])
    assert len(rows) == len(sample_order)
    expected = {f['tick']: {b['body_id']: b for b in f['bodies']} for f in record['frames']}
    by_frame = {}
    for row in rows:
        i = row['frame']; tick = i*10 if i < 15 else 150 if i < 33 else 150+(i-33)*10
        assert row['source_tick'] == tick
        assert row['presentation_seconds'] == i/24
        for b in row['poses']:
            ref = expected[tick][b['body_id']]
            assert b['tick'] == tick and b['position'] == ref['position_m'] and b['q'] == ref['orientation_wxyz']
        if i in by_frame:
            assert row == by_frame[i]
        by_frame[i] = row
    for i in range(15, 33):
        assert by_frame[i]['poses'] == by_frame[15]['poses']
        assert by_frame[i]['triangles'] == by_frame[15]['triangles']
    assert by_frame[15]['camera'] != by_frame[32]['camera']
    assert by_frame[0]['triangles'] != by_frame[47]['triangles']
    saved = root/'saved.json'
    run('save-overlay', [a.probe, scene_path, '--sequence-save', saved])
    assert json.loads(saved.read_text())['extensions']['ray_tracing']['authoring']['sequence'] == seq
    assert run('reopened', [a.probe, saved, 15, 32, 47]) == [by_frame[i] for i in [15, 32, 47]]
    bad = []
    def mutation(name, change):
        s = copy.deepcopy(scene); change(s)
        path = root/(name+'.json');path.write_text(json.dumps(s))
        run(name, [a.probe, path, 0], False)
        run(name+'-rollback', [a.probe, scene_path, '--replacement', path])
        bad.append(name)
    def sq(s): return s['extensions']['ray_tracing']['authoring']['sequence']
    mutation('wrong-revision', lambda s: sq(s)['source'].__setitem__('revision', '0'*64))
    mutation('wrong-id', lambda s: sq(s)['source'].__setitem__('id', 'other'))
    mutation('wrong-rate', lambda s: sq(s)['source'].__setitem__('rate', rational(480)))
    mutation('beyond-result', lambda s: sq(s)['source'].__setitem__('last_tick', manifest['endpoint_tick']+1))
    mutation('clock-mismatch', lambda s: s['extensions']['ray_tracing']['authoring']['scene_timeline']['range'].__setitem__('frame_count', count+1))
    mutation('null-sequence', lambda s: s['extensions']['ray_tracing']['authoring'].__setitem__('sequence', None))
    mutation('unknown-key', lambda s: sq(s).__setitem__('unknown', 1))
    mutation('discontinuous', lambda s: sq(s)['segments'][1].__setitem__('source_start', rational(1)))
    mutation('no-result', lambda s: s['extensions']['ray_tracing'].pop('rigid_result'))
    assert scene['extensions']['rigid_scene'] == original_physics
    request = json.loads((root/'render_request.json').read_text())
    request['scene']['runtime_scene_path'] = str(scene_path)
    request['render'].update(width=192, height=128, start_frame=start, frame_count=count,
        temporal_frames=1, integrator_3d='direct_light')
    for key in ['camera_position', 'camera_look_at', 'camera_zoom']:
        request['inspection'].pop(key, None)
    request['output'] = dict(root=str(a.out/'render'), overwrite=False, video=dict(enabled=False))
    request['progress'] = dict(summary_path=str(a.out/'render/summary.json'), progress_path=str(a.out/'render/progress.json'))
    request_path = a.out/'request.json'; request_path.write_text(json.dumps(request))
    run('preflight', [a.renderer, '--request', request_path, '--preflight', '--summary', a.out/'preflight.json', '--summary-file-only'])
    legacy = copy.deepcopy(request)
    legacy['output']['video'] = dict(enabled=True, fps=24, path=str(a.out/'render/legacy.mp4'))
    legacy_path = a.out/'legacy-video-request.json';legacy_path.write_text(json.dumps(legacy))
    run('legacy-video-refused', [a.renderer, '--request', legacy_path, '--preflight', '--summary', a.out/'legacy-video.json', '--summary-file-only'], False)
    assert 'sequence video requires' in (a.out/'legacy-video.json').read_text()
    run('render', [a.renderer, '--request', request_path, '--render', '--summary', a.out/'render.json', '--summary-file-only'])
    images = a.out/'render/frames'; hashes = []
    for i in range(count):
        image = images/f'frame_{start+i:04d}.bmp'
        receipt = json.loads(Path(str(image)+'.sequence.json').read_text())
        assert receipt['source_tick'] == by_frame[i]['source_tick']
        assert receipt['camera'] == by_frame[i]['camera']
        assert Fraction(*receipt['presentation_time']) == Fraction(i, 24)
        for b in receipt['bodies']:
            ref = expected[receipt['source_tick']][b['body_id']]
            assert b['position'] == ref['position_m'] and b['q'] == ref['orientation_wxyz']
        hashes.append(hashlib.sha256(image.read_bytes()).hexdigest())
    assert len(set(hashes[15:33])) == 18, 'camera move during hold must produce distinct images'
    movie = a.out/'sequence.mp4'
    proof = assemble(scene_path, images, movie)
    assert proof['frame_count'] == count and proof['duration'] == '2' and proof['all_pts_exact']
    # Refuse image/receipt mismatch rather than silently repair numbering/timing.
    first = images/f'frame_{start:04d}.bmp.sequence.json'; original = first.read_bytes()
    row = json.loads(original);row['presentation_time'] = [1, 1];first.write_text(json.dumps(row))
    try:
        assemble(scene_path, images, a.out/'refused.mp4')
        raise AssertionError('forged timing accepted')
    except (AssertionError, ValueError) as exc:
        if str(exc) == 'forged timing accepted': raise
    finally: first.write_bytes(original)
    # A fractional-rate hold proves native/encoded clocks without rounding FPS.
    fractional_scene = copy.deepcopy(scene)
    fq = fractional_scene['extensions']['ray_tracing']['authoring']
    fq['sequence']['rate'] = dict(numerator=30000, denominator=1001)
    fq['sequence']['range'] = dict(start_frame=0, frame_count=3)
    fq['sequence']['segments'] = [dict(id='held', start_frame=0, frame_count=3,
        kind='hold', source_start=rational(5, 8), speed=rational(0))]
    fq['scene_timeline']['rate'] = fq['sequence']['rate']
    fq['scene_timeline']['range'] = fq['sequence']['range']
    for t in fq['scene_timeline']['tracks']:
        t['keys'][0]['frame'] = 0;t['keys'][-1]['frame'] = 2
    frac_scene_path = root/'fractional.json';frac_scene_path.write_text(json.dumps(fractional_scene))
    frac_rows = run('fractional-native', [a.probe, frac_scene_path, 0, 1, 2])
    assert all(row['source_tick'] == 150 for row in frac_rows)
    frac_request = copy.deepcopy(request)
    frac_request['scene']['runtime_scene_path'] = str(frac_scene_path)
    frac_request['render'].update(start_frame=0, frame_count=3)
    frac_request['output']['root'] = str(a.out/'fractional-render')
    frac_request['progress'] = dict(summary_path=str(a.out/'fractional-render/summary.json'), progress_path=str(a.out/'fractional-render/progress.json'))
    frac_path = a.out/'fractional-request.json';frac_path.write_text(json.dumps(frac_request))
    run('fractional-render', [a.renderer, '--request', frac_path, '--render', '--summary', a.out/'fractional-render.json', '--summary-file-only'])
    frac_proof = assemble(frac_scene_path, a.out/'fractional-render/frames', a.out/'fractional.mp4')
    assert frac_proof['rate'] == '30000/1001' and frac_proof['all_pts_exact']
    for path, digest in pins.items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest
    (a.out/'result.json').write_text(json.dumps(dict(ok=True, native_samples=len(rows),
        exact_body_comparisons=sum(len(r['poses']) for r in rows), hold_frames=18,
        static_hold_geometry=True, moving_hold_camera=True, save_reopen=True,
        source_input_unchanged=True, refused_mutations=bad, native_frames=count,
        uniform_pts=True, fractional_rate_pts=True, video=str(movie)), indent=2)+'\n')
    print('Stage2 native source/camera/hold/render/video qualification passed')


if __name__ == '__main__': main()
