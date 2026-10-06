#!/usr/bin/env python3
"""Create a fresh editable atmosphere evaluation scene and matched native renders.

This is a local diagnostic scene, with no simulation run or acceptance promotion.
"""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
from scene_atmosphere_assets import generate

ROOT = Path(__file__).resolve().parents[1]


def point(x, y, z):
    return dict(x=x, y=y, z=z)


def primitive(name, kind, position, size, wall=False):
    frame = dict(origin=point(*position), axis_u=point(1, 0, 0),
                 axis_v=point(0, 0, 1) if wall else point(0, 1, 0),
                 normal=point(0, -1, 0) if wall else point(0, 0, 1))
    shape = dict(kind=kind, width=size[0], height=size[1], frame=frame,
                 lock_to_construction_plane=False, lock_to_bounds=False)
    if len(size) == 3:
        shape['depth'] = size[2]
    return dict(object_id=name, object_type=kind, space_mode_intent='3d',
                dimensional_mode='full_3d', transform=dict(position=point(*position),
                rotation=point(0, 0, 0), scale=point(1, 1, 1)),
                flags=dict(visible=True, locked=False, selectable=True), primitive=shape)


def write(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def create(out, codec):
    out.mkdir(parents=True, exist_ok=False)
    subprocess.run([str(codec.resolve()), str(out / 'looks.json')], check=True)
    looks = json.loads((out / 'looks.json').read_text())
    scene = json.loads((ROOT / 'tests/fixtures/ps4d_runtime_scene_visual_test.json').read_text())
    scene.update(scene_id='atmosphere_evaluation_v1', hierarchy=[], constraints=[])
    scene['objects'] = [primitive('floor', 'plane_primitive', (0, 0, 0), (10, 10)),
        primitive('backdrop', 'plane_primitive', (0, 3.3, 2.5), (8, 5), True),
        primitive('foreground_red', 'rect_prism_primitive', (-1.4, -2.4, 1.35), (.25, .4, 2.7)),
        primitive('background_blue', 'rect_prism_primitive', (1.4, 1.8, 1.35), (.25, .4, 2.7))]
    for i in range(-3, 4):
        scene['objects'].append(primitive('backdrop_stripe_' + str(i + 3),
            'plane_primitive', (i, 3.27, 2.5), (.38, 5), True))
    colors = dict(floor=0xB8BCC4, backdrop=0xECE8DF,
                  foreground_red=0xCB493A, background_blue=0x2857B5)
    materials = [dict(object_id=o['object_id'], material_id=0,
        object_color=colors.get(o['object_id'], 0x424A58), alpha=1,
        roughness=.85, reflectivity=0) for o in scene['objects']]
    scene['lights'] = [dict(light_id='evaluation_key', kind='point',
        position=point(-3, -4, 5), intensity=2, falloff_distance=12,
        color=dict(r=1, g=1, b=1), enabled=True, moving=False)]
    scene['cameras'] = [dict(camera_id='evaluation_camera', kind='perspective',
        position=point(0, -9, 3), yaw=math.pi, look_pitch=-.15)]
    scene['extensions'] = dict(ray_tracing=dict(authoring=dict(
        camera_focus_target=point(0, 0, 1.65), object_materials=materials,
        environment=dict(light_mode=2, ambient_strength=.12, top_fill_strength=.5,
            environment_preset='Sky', background_brightness_auto=False,
            background_brightness=.5, background_color=dict(r=.68, g=.8, b=1)))))
    source = out / 'scene_runtime.json'; write(source, scene)
    cache = generate(source, 'cloud_bank_v1', 12)
    volume = copy.deepcopy(looks['Light cloud'])
    volume.update({k: cache[k] for k in ('source_path', 'source_kind')})
    volume['placement'].update(translation=[0, 0, 1.4], scale=[1.1, 1.1, 1.1])
    author = scene['extensions']['ray_tracing']['authoring']
    author['atmosphere_attachment'] = volume
    write(source, scene)
    request = dict(schema_version='ray_tracing_agent_render_request_v1', run_id='atmosphere-evaluation',
        scene=dict(runtime_scene_path='scene_runtime.json'), volume=dict(enabled=False),
        render=dict(start_frame=0, frame_count=1, width=480, height=320,
                    temporal_frames=4, denoise_enabled=False, integrator_3d='disney_v2'),
        output=dict(root='evaluation-output', overwrite=False))
    write(out / 'render_request.json', request)
    (out / 'README.md').write_text('''# Atmosphere Evaluation

The pale striped backdrop and blue rear marker sit behind the volume. The red
marker sits in front. Compare the same camera and exposure with atmosphere off,
on, tinted, and more transparent. The sky-colored background remains visible
outside the backdrop. Front, side and back key-light layouts use matched off/on
comparisons. These are diagnostic images, not an artistic acceptance record.

Open scene_runtime.json, choose Environment, and edit the retained atmosphere.
File Save retains placement and appearance. Select Light cloud for the baseline.
The geometric density guide is not the shaded final appearance. Render with
Disney V2 to judge visibility. No solver or multiple-scattering model was added.
''')
    return scene, request, looks


def render(out, cli, scene, request, looks, width, height):
    variants = {}
    for name, look in [('cloud', 'Light cloud'), ('mist', 'Soft mist'), ('smoke', 'Smoke')]:
        v = copy.deepcopy(scene)
        v['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['appearance'] = looks[look]['appearance']
        variants[name] = v
    for name in ('off', 'transparent', 'blue', 'more_transparent'):
        v = copy.deepcopy(scene); a = v['extensions']['ray_tracing']['authoring']['atmosphere_attachment']
        if name == 'off': a['enabled'] = False
        if name == 'transparent': a['appearance']['opacity'] = 0
        if name == 'blue': a['appearance']['tint'] = [.2, .45, 1]
        if name == 'more_transparent': a['appearance']['opacity'] = .15
        variants[name] = v
    for layout, position in [('side', (4, 0, 4)), ('back', (0, 2.6, 4))]:
        for enabled in (False, True):
            v = copy.deepcopy(scene); v['lights'][0]['position'] = point(*position)
            v['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['enabled'] = enabled
            variants[layout + ('_cloud' if enabled else '_off')] = v
    outputs = {}
    cache = out / scene['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['source_path']
    digest = hashlib.sha256(cache.read_bytes()).hexdigest()
    for name, document in variants.items():
        source = out / (name + '.scene.json'); write(source, document)
        r = copy.deepcopy(request); r['run_id'] = name; r['scene']['runtime_scene_path'] = str(source)
        r['render'].update(width=width, height=height); r['output']['root'] = str(out / 'renders' / name)
        path = out / (name + '.request.json'); write(path, r)
        with (out / (name + '.log')).open('w') as log:
            subprocess.run([str(cli.resolve()), '--request', str(path), '--render',
                '--summary', str(out / (name + '.summary.json')), '--summary-file-only'],
                cwd=out, env=dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(out)),
                stdout=log, stderr=subprocess.STDOUT, timeout=300, check=True)
        outputs[name] = (out / 'renders' / name / 'frames/frame_0000.bmp').read_bytes()
        print(name, 'completed', flush=True)
    assert outputs['transparent'] == outputs['off']
    for name in ('cloud', 'mist', 'smoke', 'blue', 'more_transparent'):
        assert outputs[name] != outputs['off'], name
    for layout in ('side', 'back'):
        assert outputs[layout + '_cloud'] != outputs[layout + '_off']
    assert hashlib.sha256(cache.read_bytes()).hexdigest() == digest
    write(out / 'verification.json', dict(passed=True, cache_sha256=digest,
        variants=list(outputs), transparent_matches_disabled=True,
        width=width, height=height, artistic_acceptance=False))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True); p.add_argument('--codec', type=Path, required=True)
    p.add_argument('--cli', type=Path); p.add_argument('--width', type=int, default=480)
    p.add_argument('--height', type=int, default=320)
    a = p.parse_args(); out = a.out.resolve()
    scene, request, looks = create(out, a.codec)
    if a.cli: render(out, a.cli, scene, request, looks, a.width, a.height)


if __name__ == '__main__':
    main()
