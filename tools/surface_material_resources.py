#!/usr/bin/env python3
"""Portable retained-runtime material bundles; app dependency selection.

Reuse core_scene_compile's v2 dependency vocabulary/content paths, and the app's
immutable publication helpers. This does not claim a shared authoring-compiler
export receipt or reinterpret retained runtime scenes.
"""
from __future__ import annotations
import argparse
import copy
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from managed_mesh_assets import check_scene, digest, encode, path_in, publish, sync_directory
from smooth_mesh_reflection.prepare_reflection_matrix import build_request

HEX = re.compile(r'[0-9a-f]{64}\Z')
CHANNELS = {'base_color', 'roughness', 'normal', 'height'}


def source_path(root, value):
    if not isinstance(value, str) or not value or '\\' in value or '..' in Path(value).parts:
        raise ValueError('invalid or traversing dependency path')
    p = Path(value)
    if p.is_absolute():
        if p.is_symlink():
            raise ValueError('symlink dependency rejected')
        result = p.resolve(strict=True)
    else:
        result = path_in(root, value)
        cursor = root.resolve()
        for part in p.parts:
            cursor /= part
            if cursor.is_symlink():
                raise ValueError('symlink dependency rejected')
    if not result.is_file():
        raise ValueError('missing regular dependency: ' + value)
    return result


def bounded_bytes(path, limit):
    if path.stat().st_size > limit:
        raise ValueError('dependency exceeds supported byte budget')
    data = path.read_bytes()
    if len(data) > limit:
        raise ValueError('dependency exceeds supported byte budget')
    return data


def dependency(kind, data):
    sha = hashlib.sha256(data).hexdigest()
    return {'kind': kind, 'identity': sha, 'sha256': sha, 'bytes': len(data),
            'path': f'dependencies/{kind}/{sha}'}


def select_dependencies(scene, root):
    """Return preserved scene plus bounded complete supported execution dependencies."""
    check_scene(scene)
    candidate = copy.deepcopy(scene)
    payloads = {}
    entries = {}
    bindings = []
    rows = candidate.get('extensions', {}).get('ray_tracing', {}).get('authoring', {}).get('object_materials', [])
    for row in rows:
        if any(key in row for key in ('authored_texture', 'surface_material_binding')):
            raise ValueError('bundle selection does not support authored manifests/region resource bindings')
    for obj in candidate['objects']:
        if any(key in obj for key in ('procedural_solid_material_ref', 'curve_asset_ref')):
            raise ValueError('bundle selection does not support procedural/curve asset resources')
        ray = obj.get('extensions', {}).get('ray_tracing', {})
        if 'managed_mesh' in ray:
            raise ValueError('managed source catalogs require the managed-mesh bundle workflow')
        history = ray.get('generated_uv_history', [])
        if not isinstance(history, list):
            raise ValueError('invalid generated UV history')
        for record in history:
            if not isinstance(record, dict) or not isinstance(record.get('projection'), str):
                raise ValueError('generated UV history requires its projection sidecar')
            projection = source_path(root, record['projection'])
            projection_data = bounded_bytes(projection, 128 * 1024 * 1024)
            metadata = json.loads(projection_data)
            projected_value = record.get('projected_source', str(Path(record['projection']).parent / 'projected.obj'))
            projected = source_path(root, projected_value)
            projected_data = bounded_bytes(projected, 128 * 1024 * 1024)
            if hashlib.sha256(projected_data).hexdigest() != metadata.get('output_sha256'):
                raise ValueError('stale generated UV geometry pin')
            if record.get('recipe', {}).get('source_sha256') != metadata.get('source_sha256'):
                raise ValueError('generated UV provenance mismatch')
            for kind, data, key in [('uv_projection', projection_data, 'projection'),
                                    ('uv_projected_obj', projected_data, 'projected_source')]:
                item = dependency(kind, data)
                payloads[item['path']] = data
                entries[item['path']] = item
                record[key] = item['path']
        sampling = ray.get('surface_sampling')
        if sampling is not None:
            if not isinstance(sampling, dict) or sampling.get('required_capability') != 'optic.surface_sampling_v1':
                raise ValueError('unsupported sampling capability')
            channels = sampling.get('channels', {})
            if not isinstance(channels, dict) or set(channels) - CHANNELS:
                raise ValueError('unsupported sampling channel')
            for channel, image in channels.items():
                if not isinstance(image, dict) or not isinstance(image.get('sha256'), str) or not HEX.fullmatch(image['sha256']):
                    raise ValueError('missing/invalid image SHA-256')
                allowed = {'linear', 'srgb'} if channel == 'base_color' else {'data'}
                if image.get('color_space') not in allowed:
                    raise ValueError('invalid channel interpretation')
                source = source_path(root, image.get('path'))
                data = bounded_bytes(source, 16 * 1024 * 1024)
                if hashlib.sha256(data).hexdigest() != image['sha256']:
                    raise ValueError('stale image pin')
                item = dependency('surface_image', data)
                payloads[item['path']] = data
                entries[item['path']] = item
                image['path'] = item['path']
                bindings.append({'object_id': obj['object_id'], 'channel': channel,
                                 'path': item['path'], 'sha256': item['sha256'],
                                 'color_space': image['color_space']})
        if obj.get('object_type') == 'mesh_asset_instance':
            asset_id = obj.get('geometry_ref', {}).get('id')
            if not isinstance(asset_id, str) or not re.fullmatch(r'[A-Za-z0-9_.-]+', asset_id):
                raise ValueError('invalid mesh identity')
            ext = obj.setdefault('extensions', {}).setdefault('line_drawing', {})
            value = ext.get('runtime_mesh_path', f'assets/mesh_assets/{asset_id}.runtime.json')
            source = source_path(root, value)
            data = bounded_bytes(source, 128 * 1024 * 1024)
            mesh = json.loads(data)
            if mesh.get('asset_id') != asset_id or not isinstance(mesh.get('mesh', {}).get('vertices'), list):
                raise ValueError('requires self-contained matching runtime mesh JSON')
            item = dependency('mesh_runtime', data)
            payloads[item['path']] = data
            entries[item['path']] = item
            ext['runtime_mesh_path'] = item['path']
    return candidate, sorted(entries.values(), key=lambda x: (x['kind'], x['identity'])), payloads, bindings


def validate_runtime(scene_path, renderer, work, render=False):
    work.mkdir(parents=True, exist_ok=True)
    request = build_request(work, 'direct', 'flattened')
    request['scene']['runtime_scene_path'] = str(scene_path.resolve())
    request['render'].update(width=240, height=180, integrator_3d='direct_light')
    request['inspection'].update(camera_position={'x': 3, 'y': -5, 'z': 4},
                                 camera_look_at={'x': 0, 'y': 0, 'z': 0})
    path = work / 'request.json'
    path.write_bytes(encode(request))
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(Path(__file__).resolve().parents[1]),
               RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(work / 'cache'))
    for mode in (['--preflight', '--render'] if render else ['--preflight']):
        with (work / (mode[2:] + '.log')).open('w') as log:
            completed = subprocess.run([str(Path(renderer).resolve()), '--request', str(path), mode],
                                       env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
        if completed.returncode:
            raise ValueError('renderer ' + mode + ' rejected candidate: ' +
                             (work / (mode[2:] + '.log')).read_text(errors='replace')[-3000:])
    result = {'preflight': True, 'preview_render': render, 'renderer_sha256': digest(renderer)}
    if render:
        frames = list((work / 'renders/direct_flattened/frames').glob('*.bmp'))
        summary = json.loads((work / 'renders/direct_flattened/render_summary.json').read_text())
        if len(frames) != 1 or frames[0].stat().st_size <= 54 or summary.get('frames_rendered') != 1:
            raise ValueError('preview did not export one complete frame')
        result.update(preview_frame=str(frames[0].relative_to(work)), preview_sha256=digest(frames[0]))
    return result


def publish_directory(stage, output):
    """Same-volume atomic no-replace rename, as required for immutable bundles."""
    library = ctypes.CDLL(None, use_errno=True)
    if sys.platform == 'darwin':
        call = library.renamex_np
        call.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
        status = call(os.fsencode(stage), os.fsencode(output), 4)  # RENAME_EXCL
    elif sys.platform.startswith('linux'):
        call = library.renameat2
        call.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
        status = call(-100, os.fsencode(stage), -100, os.fsencode(output), 1)  # RENAME_NOREPLACE
    else:
        raise ValueError('atomic create-only directory publication unsupported on this platform')
    if status:
        raise OSError(ctypes.get_errno(), 'create-only bundle publication failed')
    sync_directory(output.parent)


def publish_new_file(path, data):
    """Durable create-only file commit; a partial final file is never exposed."""
    path = Path(path)
    with tempfile.NamedTemporaryFile(prefix='.surface-publish-', dir=path.parent, delete=False) as stream:
        stage = Path(stream.name)
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    try:
        os.link(stage, path)
        sync_directory(path.parent)
    finally:
        stage.unlink(missing_ok=True)


def bundle(scene_path, output, expected_sha256, renderer=None):
    scene_path = Path(scene_path).resolve(strict=True)
    output = Path(output).absolute()
    if output.exists() or output.is_symlink():
        raise ValueError('bundle output already exists')
    data = scene_path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected_sha256:
        raise ValueError('stale source scene pin')
    scene, entries, payloads, bindings = select_dependencies(json.loads(data), scene_path.parent)
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.surface-bundle-', dir=output.parent))
    try:
        for relative, payload in payloads.items():
            publish(stage / relative, payload)
        publish(stage / 'scene_runtime.json', encode(scene))
        manifest = {'schema_family': 'codework_scene_dependencies',
                    'schema_variant': 'scene_dependency_manifest_v2', 'schema_version': 2,
                    'dependencies': entries}
        # Match the shared v2 parser's canonical field order/compact bytes.
        publish(stage / 'scene_dependencies.json', (json.dumps(manifest, separators=(',', ':')) + '\n').encode())
        validation = validate_runtime(stage / 'scene_runtime.json', renderer, stage / 'validation') if renderer else {'preflight': False, 'preview_render': False}
        receipt = {'schema': 'optic_surface_resource_bundle_v1', 'source_scene_sha256': expected_sha256,
                   'scene_sha256': digest(stage / 'scene_runtime.json'),
                   'dependencies_sha256': digest(stage / 'scene_dependencies.json'),
                   'bindings': bindings, 'validation': validation,
                   'state': 'portable_candidate_not_adopted'}
        publish(stage / 'surface_resources_receipt.json', encode(receipt))
        verify(stage)
        publish_directory(stage, output)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    return receipt


def verify(root):
    root = Path(root).resolve(strict=True)
    receipt = json.loads(source_path(root, 'surface_resources_receipt.json').read_text())
    if receipt.get('schema') != 'optic_surface_resource_bundle_v1':
        raise ValueError('unsupported resource receipt')
    for name, key in [('scene_runtime.json', 'scene_sha256'), ('scene_dependencies.json', 'dependencies_sha256')]:
        if digest(source_path(root, name)) != receipt.get(key):
            raise ValueError('bundle artifact digest mismatch')
    scene = json.loads((root / 'scene_runtime.json').read_text())
    manifest = json.loads((root / 'scene_dependencies.json').read_text())
    if (manifest.get('schema_family'), manifest.get('schema_variant'), manifest.get('schema_version')) != (
            'codework_scene_dependencies', 'scene_dependency_manifest_v2', 2):
        raise ValueError('unsupported dependency manifest')
    expected_scene, entries, payloads, bindings = select_dependencies(scene, root)
    if expected_scene != scene or entries != manifest.get('dependencies') or bindings != receipt.get('bindings'):
        raise ValueError('bundle dependency closure or interpretation mismatch')
    for entry in entries:
        payload = payloads[entry['path']]
        if len(payload) != entry['bytes'] or hashlib.sha256(payload).hexdigest() != entry['sha256']:
            raise ValueError('bundle dependency mismatch')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    create = commands.add_parser('bundle')
    create.add_argument('--scene', type=Path, required=True)
    create.add_argument('--expected-sha256', required=True)
    create.add_argument('--output', type=Path, required=True)
    create.add_argument('--renderer', type=Path, required=True)
    check = commands.add_parser('verify')
    check.add_argument('--bundle', type=Path, required=True)
    check.add_argument('--renderer', type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'bundle':
            result = bundle(args.scene, args.output, args.expected_sha256, args.renderer)
        else:
            result = verify(args.bundle)
            if args.renderer:
                with tempfile.TemporaryDirectory(prefix='optic-relocated-preflight-') as work:
                    validate_runtime(args.bundle / 'scene_runtime.json', args.renderer, Path(work))
        print(json.dumps(result, indent=2))
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        parser.exit(2, str(error) + '\n')


if __name__ == '__main__':
    main()
