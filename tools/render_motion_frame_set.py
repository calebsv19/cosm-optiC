#!/usr/bin/env python3
"""Create a portable fixed-camera motion scene and optionally render its frame set.

Every invocation owns a new output directory. Existing outputs are never replaced.
The native request/scene JSON are retained for later edits and CLI rerenders.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

REPO = Path(__file__).resolve().parents[1]


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def point(x, y, z):
    return dict(x=x, y=y, z=z)


def build_scene(root, fps):
    fixture = REPO / 'tests/fixtures/mesh_asset_runtime_spheres'
    scene = json.loads((fixture / 'scene_runtime.json').read_text())
    scene['scene_id'] = scene['source_scene_id'] = 'agent_object_motion_v1'
    scene['objects'] = [o for o in scene['objects'] if o['object_id'] in ('obj_floor', 'obj_sphere_medium')]
    sphere = next(o for o in scene['objects'] if o['object_id'] == 'obj_sphere_medium')
    sphere['transform']['position'] = point(-1.4, .3, .7)
    sphere['transform']['scale'] = point(.5, .5, .5)
    asset = 'asset_sphere_16x8.runtime.json'
    (root / 'assets/mesh_assets').mkdir(parents=True)
    shutil.copy2(fixture / 'assets/mesh_assets' / asset, root / 'assets/mesh_assets' / asset)
    # Runtime mesh lookup is relative to the saved scene, making this portable.
    author = scene['extensions']['ray_tracing']['authoring']
    author['object_materials'] = [m for m in author['object_materials'] if m['object_id'] in ('obj_floor', 'obj_sphere_medium')]
    for material in author['object_materials']:
        if material['object_id'] == 'obj_sphere_medium':
            material['object_color'] = 0xED752E
    light = point(-2.5, -2.8, 4.2)
    author['light_path'] = {'mode': 'BEZIER_CUBIC', 'points': [{'x': light['x'], 'y': light['y'], 'rotation': 0, 'handleLink': False}]}
    author['light_path_depth'] = {'points': [{'z': light['z'], 'lookPitch': 0}]}
    author['light_settings'] = {'intensity': 4.0, 'radius': .12}
    poses = [(0, (-1.4, .3, .7)), (8, (0, .3, 1.4)), (11, (0, .3, 1.4)), (19, (1.4, .3, .7))]
    tracks = []
    for axis, name in enumerate('xyz'):
        tracks.append({'id': f'object-position-{name}', 'target_id': 'object/obj_sphere_medium',
                       'property_id': f'object/transform/position_{name}', 'value_type': 'scalar',
                       'unit': 'world_distance', 'source': 'authored', 'enabled': True,
                       'keys': [{'frame': frame, 'value': pose[axis], 'interpolation': 'linear', 'incoming_handle': {'frame_offset': 0, 'value_offset': 0},
                                 'outgoing_handle': {'frame_offset': 0, 'value_offset': 0}} for frame, pose in poses]})
    author['scene_timeline'] = {'version': 1, 'rate': {'numerator': fps, 'denominator': 1},
                              'range': {'start_frame': 0, 'frame_count': 20}, 'tracks': tracks}
    return scene


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='New frame-set directory; must not exist')
    parser.add_argument('--cli', type=Path, required=True, help='Built ray_tracing_render_headless executable')
    parser.add_argument('--width', type=int, default=320)
    parser.add_argument('--height', type=int, default=180)
    parser.add_argument('--temporal-frames', type=int, default=1)
    parser.add_argument('--fps', type=int, default=24)
    parser.add_argument('--start-frame', type=int, default=0)
    parser.add_argument('--frame-count', type=int, default=20)
    parser.add_argument('--integrator', choices=['direct_light', 'disney', 'disney_v2'], default='direct_light')
    parser.add_argument('--render', action='store_true', help='Preflight, render, then verify the frame set')
    args = parser.parse_args()
    if min(args.width, args.height, args.temporal_frames, args.fps, args.frame_count) < 1:
        parser.error('Resolution, sampling, FPS and count must be positive')
    if args.start_frame < 0 or args.start_frame + args.frame_count > 20:
        parser.error('This recipe has frames 0-19; choose a subrange within it')
    cli = args.cli.resolve()
    if not cli.is_file():
        parser.error('Build the supplied renderer CLI first')
    root = args.output.resolve()
    if root.exists():
        parser.error('Output exists; choose a new directory to preserve prior runs')
    root.mkdir(parents=True)
    (root / 'config').mkdir()
    (root / 'data/runtime').mkdir(parents=True)
    shutil.copy2(REPO / 'config/scene_config.json', root / 'config/scene_config.json')
    write_json(root / 'data/runtime/animation_config.json', {
        'inputRoot': str(root / 'config'), 'outputRoot': str(root / 'data/runtime'),
        'meshAssetRoot': str(root / 'assets/mesh_assets'), 'fps': args.fps})
    environment = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(root),
                       RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT=str(root / 'data/mesh_cache'))
    scene_path = root / 'scene_runtime.json'
    write_json(scene_path, build_scene(root, args.fps))
    camera = point(0, -6.2, 2.6)
    look = point(0, .3, 1.0)
    request = {'schema_version': 'ray_tracing_agent_render_request_v1', 'run_id': root.name,
               'scene': {'runtime_scene_path': 'scene_runtime.json'}, 'volume': {'enabled': False},
               'render': {'start_frame': args.start_frame, 'frame_count': args.frame_count,
                          'width': args.width, 'height': args.height, 'temporal_frames': args.temporal_frames,
                          'integrator_3d': args.integrator},
               'inspection': {'camera_position': camera, 'camera_look_at': look, 'camera_zoom': 1.0,
                              'environment_light_mode': 'ambient', 'ambient_strength': .45,
                              'top_fill_strength': 1.5, 'light_intensity': 4.0, 'light_radius': .12},
               'output': {'root': '.', 'overwrite': False},
               'progress': {'summary_path': 'render_summary.json', 'progress_path': 'render_progress.json'}}
    request_path = root / 'render_request.json'
    write_json(request_path, request)
    write_json(root / 'recipe.json', {'schema': 'optic_motion_frame_set_recipe_v1',
        'fps': args.fps, 'frame_range': [args.start_frame, args.start_frame + args.frame_count - 1],
        'object': 'obj_sphere_medium', 'motion': 'move 0-8; hold 8-11; resume 11-19',
        'camera_fixed': True, 'light_fixed': True, 'renderer': str(cli),
        'renderer_sha256': hashlib.sha256(cli.read_bytes()).hexdigest(),
        'pixel_dimensions': [args.width, args.height], 'temporal_frames': args.temporal_frames,
        'integrator': args.integrator})
    (root / 'README.md').write_text(
        '# Object motion frame set\n\n'
        'Mesh sphere; fixed camera and light. Frames 0-8 move, 8-11 hold, 11-19 resume.\n'
        f'Authored rate: {args.fps} fps. Open `frames/` in sCope to inspect numbered BMPs.\n\n'
        '`scene_runtime.json` and `assets/` are portable scene inputs. `render_request.json`\n'
        'holds the exact resolution, temporal sampling, frame range, camera and output settings.\n'
        'Use a fresh output root for another run. Pixel width/height determine render resolution;\n'
        'monitor DPI does not add pixels to these headless frames.\n')
    if not args.render:
        print(f'Prepared: {request_path}')
        return
    for mode in ('preflight', 'render'):
        with (root / f'{mode}.log').open('w') as log:
            subprocess.run([str(cli), '--request', str(request_path), '--' + mode,
                            '--summary', str(root / f'{mode}_summary.json'), '--summary-file-only'],
                           cwd=root, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)
    summary = json.loads((root / 'render_summary.json').read_text())
    if summary['frames_rendered'] != args.frame_count:
        raise RuntimeError('Renderer did not finish the requested frame count')
    frames = [root / 'frames' / f'frame_{i:04d}.bmp' for i in range(args.start_frame, args.start_frame + args.frame_count)]
    entries = []
    for index, path in enumerate(frames, args.start_frame):
        data = path.read_bytes()
        if data[:2] != b'BM' or len(data) < 54:
            raise RuntimeError(f'Invalid BMP: {path}')
        dimensions = struct.unpack_from('<ii', data, 18)
        if (dimensions[0], abs(dimensions[1])) != (args.width, args.height):
            raise RuntimeError(f'Wrong pixel dimensions: {path}')
        entries.append({'frame': index, 'file': str(path.relative_to(root)), 'bytes': len(data),
                        'sha256': hashlib.sha256(data).hexdigest()})
    by_frame = {entry['frame']: entry['sha256'] for entry in entries}
    if all(frame in by_frame for frame in (0, 8, 11, 19)) and args.integrator == 'direct_light':
        if by_frame[0] == by_frame[8] or by_frame[11] == by_frame[19]:
            raise RuntimeError('Expected visible motion did not change the rendered images')
        if by_frame[8] != by_frame[11]:
            raise RuntimeError('Hold frames changed despite the fixed camera/light')
    if summary['evaluated_camera']['position'] != list(camera.values()):
        raise RuntimeError('Rendered camera differs from fixed request')
    write_json(root / 'frame_manifest.json', {'fps': args.fps, 'count': len(entries),
               'pixel_dimensions': [args.width, args.height], 'camera_fixed': True, 'frames': entries})
    print(f'Rendered and verified {len(frames)} frames: {root / "frames"}')


if __name__ == '__main__':
    main()
