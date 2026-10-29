#!/usr/bin/env python3
"""Prepare a portable optiC scene from a verified local Ball rigid result.

All executable paths and producer inputs are explicit. The Ball result store
owns native admission, replay and compare-and-swap; optiC owns render admission.
This command never overwrites its source scene or an existing output directory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def decode(data):
    def pairs(rows):
        out = {}
        for key, value in rows:
            if key in out:
                raise ValueError('duplicate JSON key: ' + key)
            out[key] = value
        return out
    return json.loads(data, object_pairs_hook=pairs,
                      parse_constant=lambda x: (_ for _ in ()).throw(ValueError(x)))


def strict(path):
    return decode(path.read_bytes())


def write(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def command(argv, log, seconds):
    with log.open('wb') as stream:
        try:
            subprocess.run([str(v) for v in argv], stdout=stream, stderr=subprocess.STDOUT,
                           timeout=seconds, check=True)
        except subprocess.SubprocessError as error:
            stream.flush()
            detail=log.read_text(errors='replace')[-2000:]
            raise ValueError(str(error) + ': ' + detail) from error


def prepare(args):
    source = args.scene.resolve(strict=True)
    output = args.output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError('output must be a new directory')
    if not output.parent.is_dir():
        raise ValueError('output parent must exist')
    if source.stat().st_size > 1024 * 1024:
        raise ValueError('scene exceeds producer input byte limit')
    scene_bytes = source.read_bytes()
    scene = decode(scene_bytes)
    if scene.get('schema_variant') != 'scene_runtime_v1':
        raise ValueError('expected flat scene_runtime_v1; compile authoring first')
    if not isinstance(scene.get('extensions', {}).get('rigid_scene'), dict):
        raise ValueError('scene needs an authored rigid_scene contract')
    # Only presentation mesh references and the result binding change. Initial
    # COM, orientation, velocities, material and foreign extensions are retained.
    with tempfile.TemporaryDirectory(prefix='.rigid-prepare-', dir=output.parent) as temporary:
        stage = Path(temporary)
        ball = args.ball_tool.resolve(strict=True)
        pack = args.pack_io.resolve(strict=True)
        producer = [sys.executable, ball, '--pack-io', pack]
        if args.op == 'run':
            producer += ['--backend', args.backend.resolve(strict=True), 'run',
                         '--scene', source, '--assets', args.assets.resolve(strict=True),
                         '--store', args.store.absolute(), '--expected-head', args.expected_head,
                         '--wall-seconds', args.wall_seconds]
            if args.ticks is not None:
                producer += ['--ticks', args.ticks]
            command(producer, stage / 'producer.json', args.wall_seconds + 15)
            produced = strict(stage / 'producer.json')
            if produced.get('ok') is not True:
                raise ValueError('producer refused result')
            bundle, revision = Path(produced['bundle']), produced['result_revision']
        else:
            bundle, revision = args.bundle.resolve(strict=True), args.expected_revision
            # inspect is container+contract verification, not a fresh solver replay.
            command(producer + ['inspect', '--bundle', bundle, '--expected-revision', revision],
                    stage / 'producer.json', args.wall_seconds)
        # Reuse the producer's verified immutable snapshot implementation. Never
        # copy unchecked inventory or reimplement its publication protocol here.
        sys.path.insert(0, str(ball.parent))
        import rigid_result_store
        manifest = rigid_result_store.snapshot_bundle(bundle, revision, stage / 'result')
        asset_set = strict(stage / 'result/input/assets/asset_set.json')
        roots = {a['asset_id']: a['root'] for a in asset_set['assets']}
        bindings = scene['extensions']['rigid_scene']['bindings']
        by_object = {b['object_id']: b for b in bindings}
        for obj in scene['objects']:
            binding = by_object.get(obj['object_id'])
            if binding:
                root = roots[binding['collision_asset_id']]
                obj.setdefault('extensions', {}).setdefault('line_drawing', {})['runtime_mesh_path'] = (
                    'result/input/assets/' + root + '/visible.json')
        scene['extensions'].setdefault('ray_tracing', {})['rigid_result'] = {
            'schema': 'ray_rigid_result_binding_v1', 'bundle_path': 'result',
            'result_revision': revision}
        write(stage / 'scene.json', scene)
        request = {'schema_version': 'ray_tracing_agent_render_request_v1',
                   'run_id': 'rigid_scene_prepare',
                   'scene': {'runtime_scene_path': str(stage / 'scene.json')},
                   'volume': {'enabled': False},
                   'render': {'start_frame': 0, 'frame_count': 1, 'width': 32,
                              'height': 32, 'normalized_t': 0, 'temporal_frames': 1},
                   'output': {'root': str(stage / 'preflight'), 'overwrite': False}}
        write(stage / 'request.json', request)
        command([args.renderer.resolve(strict=True), '--request', stage / 'request.json',
                 '--preflight', '--summary', stage / 'admission.json', '--summary-file-only'],
                stage / 'renderer.log', args.wall_seconds)
        if source.read_bytes() != scene_bytes:
            raise ValueError('source changed during preparation; output not published')
        receipt = {'schema': 'optic_rigid_scene_preparation_v1', 'ok': True,
                   'source_sha256': hashlib.sha256(scene_bytes).hexdigest(),
                   'result_revision': revision, 'endpoint_tick': manifest['endpoint_tick'],
                   'physical_input_sha256': manifest['physical_input_sha256'],
                   'engine_sha256': manifest['engine_sha256'],
                   'native_replay': args.op == 'run' and not produced.get('reused', False),
                   'producer_reused': args.op == 'run' and produced.get('reused', False),
                   'scene_path': str(output / 'scene.json')}
        write(stage / 'preparation.json', receipt)
        # The transient request contains stage paths and is not part of the
        # portable deliverable. Keep admission and command logs as diagnostics.
        (stage / 'request.json').unlink()
        shutil.rmtree(stage / 'preflight', ignore_errors=True)
        # Existing native no-replace publication keeps the portable scene and
        # all dependencies visible together. A raced destination is refused.
        rigid_result_store.publish(stage, output)
        return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('op', choices=('run', 'select'))
    for name in ('scene', 'output', 'ball-tool', 'pack-io', 'renderer'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--backend', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--store', type=Path)
    parser.add_argument('--expected-head')
    parser.add_argument('--ticks', type=int)
    parser.add_argument('--bundle', type=Path)
    parser.add_argument('--expected-revision')
    parser.add_argument('--wall-seconds', type=float, default=60)
    args = parser.parse_args()
    required = ('backend', 'assets', 'store', 'expected_head') if args.op == 'run' else ('bundle', 'expected_revision')
    if any(getattr(args, k) is None for k in required):
        parser.error('missing ' + ', '.join('--' + k.replace('_', '-') for k in required))
    if not 0 < args.wall_seconds <= 1800:
        parser.error('wall seconds must be in (0, 1800]')
    try:
        result = prepare(args)
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(json.dumps({'ok': False, 'error': str(error)})); return 2
    print(json.dumps(result, sort_keys=True)); return 0


if __name__ == '__main__':
    raise SystemExit(main())
