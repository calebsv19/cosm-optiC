#!/usr/bin/env python3
"""Managed STL intake and per-instance shading for existing runtime scenes.

All paths in the catalog are relative to the scene directory. The scene JSON
is the transaction commit point; immutable dependencies are published first.
"""
from __future__ import annotations
import argparse
import copy
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import tempfile

SCHEMA = 'optic_managed_mesh_assets_v1'
MODES = {'flat', 'smooth', 'crease_aware'}


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def encode(value):
    return (json.dumps(value, sort_keys=True, indent=2, allow_nan=False) + '\n').encode()


def policy(mode, angle=60):
    if mode not in MODES or isinstance(angle, bool) or not isinstance(angle, (int, float)) or not math.isfinite(angle) or not 0 < angle <= 180:
        raise ValueError('invalid shading mode or crease angle')
    return {'mode': mode, 'crease_angle_degrees': angle}


def path_in(root, value):
    p = Path(value)
    if p.is_absolute() or '..' in p.parts or not p.parts:
        raise ValueError('managed path must be scene-relative')
    result = (root / p).resolve()
    if not result.is_relative_to(root.resolve()):
        raise ValueError('managed path escapes scene directory')
    return result


def publish(path, data):
    """Create immutable content, refusing a conflicting existing path."""
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError(f'immutable dependency conflict: {path}')
        return
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as f:
        tmp = Path(f.name)
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    try:
        os.link(tmp, path)
    finally:
        tmp.unlink()


def catalog(scene):
    return scene.get('extensions', {}).get('ray_tracing', {}).get('managed_mesh_assets')


def check_scene(scene):
    if scene.get('schema_variant') != 'scene_runtime_v1' or scene.get('schema_family') != 'codework_scene':
        raise ValueError('expected scene_runtime_v1')
    objects = scene.get('objects')
    if not isinstance(objects, list) or len({o['object_id'] for o in objects}) != len(objects):
        raise ValueError('scene object IDs must be unique')


def effective(cat, setting):
    asset = cat['assets'][setting['asset_id']]
    chosen = setting['shading']
    if chosen == 'inherit':
        chosen = asset['default_shading']
    else:
        chosen = policy(**{'mode': chosen['mode'], 'angle': chosen['crease_angle_degrees']})
    return chosen if cat['smoothing_enabled'] else policy('flat')


def recipe(asset, shading, compiler):
    return {'source_sha256': asset['source_sha256'], 'import': asset['import'],
            'shading': shading, 'compiler_sha256': digest(compiler), 'recipe_version': 1}


def compile_variant(root, asset, shading, compiler):
    source = path_in(root, asset['source'])
    if digest(source) != asset['source_sha256']:
        raise ValueError('retained STL digest mismatch')
    spec = recipe(asset, shading, compiler)
    key = hashlib.sha256(encode(spec)).hexdigest()
    runtime_id = 'managed_' + key[:40]
    rel = f'assets/mesh_assets/{runtime_id}.runtime.json'
    authoring = {'schema_family': 'codework_geometry', 'schema_variant': 'mesh_asset_authoring_v1',
        'schema_version': 1, 'asset_id': runtime_id, 'unit_system': 'meter', 'world_scale': 1,
        'asset_type': 'solid_mesh', 'pivot': {'origin': {'x': 0, 'y': 0, 'z': 0},
        'axis_u': {'x': 1, 'y': 0, 'z': 0}, 'axis_v': {'x': 0, 'y': 1, 'z': 0}, 'normal': {'x': 0, 'y': 0, 'z': 1}},
        'authoring': {'source_mode': 'imported_mesh', 'imported_mesh': {
            'import_id': runtime_id, 'source_format': 'stl', 'source_uri': asset['source'],
            'source_unit_system': 'meter', 'orientation_policy': 'source_axes',
            'default_surface_group_id': 'imported_surface', 'preserve_source_normals': False,
            'topology_closed_volume_observed': False, 'topology_manifold_observed': False,
            **asset['import'], 'normal_mode': 'none' if shading['mode'] == 'flat' else shading['mode'],
            'crease_angle_degrees': shading['crease_angle_degrees']}},
        'surface_groups': [], 'compile_hints': {'expect_closed_volume': False, 'expect_manifold': False}, 'extensions': {}}
    with tempfile.TemporaryDirectory(prefix='.mesh-compile-', dir=root) as temp:
        a, r = Path(temp) / 'authoring.json', Path(temp) / 'runtime.json'
        a.write_bytes(encode(authoring))
        subprocess.run([str(Path(compiler).resolve()), str(a), str(root), runtime_id, str(r)], check=True, capture_output=True)
        runtime = json.loads(r.read_text())
        if runtime['asset_id'] != runtime_id or not runtime['mesh']['triangle_count']:
            raise ValueError('compiler returned invalid runtime mesh')
        data = r.read_bytes()
    publish(path_in(root, rel), data)
    author_rel = f'assets/mesh_assets/{runtime_id}.authoring.json'
    publish(path_in(root, author_rel), encode(authoring))
    return {'runtime_id': runtime_id, 'runtime': rel, 'runtime_sha256': hashlib.sha256(data).hexdigest(),
            'authoring': author_rel, 'authoring_sha256': hashlib.sha256(encode(authoring)).hexdigest(),
            'recipe': spec, 'normal_provenance': runtime['mesh'].get('normal_provenance', 'none')}


def resolve_all(root, scene, compiler):
    cat = catalog(scene)
    reusable = {}
    for obj in scene['objects']:
        setting = obj.get('extensions', {}).get('ray_tracing', {}).get('managed_mesh')
        if not setting:
            continue
        asset = cat['assets'][setting['asset_id']]
        source = path_in(root, asset['source'])
        if digest(source) != asset['source_sha256']:
            raise ValueError('retained STL digest mismatch')
        chosen = effective(cat, setting)
        spec = recipe(asset, chosen, compiler)
        key = hashlib.sha256(encode(spec)).hexdigest()
        old = setting.get('compiled')
        if old and old.get('recipe') == spec and old.get('runtime_id') == 'managed_' + key[:40] and all(
                path_in(root, old[name]).is_file() and digest(path_in(root, old[name])) == old[name + '_sha256']
                for name in ('runtime', 'authoring')):
            reusable[key] = old
        if key not in reusable:
            reusable[key] = compile_variant(root, asset, chosen, compiler)
        variant = reusable[key]
        setting['compiled'] = variant
        obj['geometry_ref'] = {'kind': 'mesh_asset', 'id': variant['runtime_id']}
        obj.setdefault('extensions', {}).setdefault('line_drawing', {})['runtime_mesh_path'] = variant['runtime']


def status(scene_path, compiler=None):
    scene_path = Path(scene_path).resolve()
    root = scene_path.parent
    scene = json.loads(scene_path.read_text())
    check_scene(scene)
    cat = catalog(scene)
    if cat is None:
        return {'status': 'legacy_unmanaged', 'objects': []}
    if cat.get('schema') != SCHEMA or type(cat.get('smoothing_enabled')) is not bool or not isinstance(cat.get('assets'), dict):
        raise ValueError('invalid managed catalog')
    for asset_id, asset in cat['assets'].items():
        if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', asset_id):
            raise ValueError('invalid asset ID')
        policy(asset['default_shading']['mode'], asset['default_shading']['crease_angle_degrees'])
        imp = asset['import']
        if type(imp['weld_vertices']) is not bool or any(isinstance(imp[k], bool) or not isinstance(imp[k], (float, int)) or not math.isfinite(imp[k]) or imp[k] <= 0 for k in ('source_to_asset_scale', 'weld_tolerance')):
            raise ValueError('invalid import recipe')
        source = path_in(root, asset['source'])
        if not source.is_file() or digest(source) != asset['source_sha256']:
            raise ValueError('retained source missing or changed')
    rows = []
    for obj in scene['objects']:
        setting = obj.get('extensions', {}).get('ray_tracing', {}).get('managed_mesh')
        if not setting:
            continue
        chosen = effective(cat, setting)
        v = setting['compiled']
        expected = {'source_sha256': cat['assets'][setting['asset_id']]['source_sha256'],
                    'import': cat['assets'][setting['asset_id']]['import'], 'shading': chosen,
                    'compiler_sha256': digest(compiler) if compiler else v['recipe']['compiler_sha256'], 'recipe_version': 1}
        ready = v['recipe'] == expected
        key = hashlib.sha256(encode(v['recipe'])).hexdigest()
        if v['runtime_id'] != 'managed_' + key[:40]:
            raise ValueError('compiled runtime ID does not match recipe')
        for name in ('runtime', 'authoring'):
            p = path_in(root, v[name])
            ready = ready and p.is_file() and digest(p) == v[name + '_sha256']
        if obj.get('geometry_ref') != {'kind': 'mesh_asset', 'id': v['runtime_id']} or obj.get('extensions', {}).get('line_drawing', {}).get('runtime_mesh_path') != v['runtime']:
            raise ValueError('managed instance binding mismatch')
        rows.append({'object_id': obj['object_id'], 'asset_id': setting['asset_id'],
                     'effective_shading': chosen, 'status': 'ready' if ready else 'rebuild_required'})
    return {'status': 'ready' if all(r['status'] == 'ready' for r in rows) else 'rebuild_required', 'objects': rows}


def update(scene_path, compiler, *, source=None, asset_id=None, object_id=None,
           shading='inherit', default_mode='flat', crease_angle=60, scale=1.0,
           weld_tolerance=1e-6, smoothing_enabled=None, spawn=None):
    """Intake/bind, change policy, or rebuild. Existing objects are preserved.

    spawn is an optional complete scene object; its ID must not already exist.
    Compiler failure leaves the scene unchanged. Unreferenced immutable files
    from interrupted work can be retained safely; no automatic deletion occurs.
    """
    scene_path = Path(scene_path).resolve()
    root = scene_path.parent
    with (root / ('.' + scene_path.name + '.managed.lock')).open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        original = scene_path.read_bytes()
        scene = json.loads(original)
        check_scene(scene)
        cat = scene.setdefault('extensions', {}).setdefault('ray_tracing', {}).setdefault(
            'managed_mesh_assets', {'schema': SCHEMA, 'smoothing_enabled': True, 'assets': {}})
        if cat['schema'] != SCHEMA:
            raise ValueError('unsupported managed catalog')
        if smoothing_enabled is not None:
            if type(smoothing_enabled) is not bool:
                raise ValueError('smoothing_enabled must be boolean')
            cat['smoothing_enabled'] = smoothing_enabled
        if source is not None:
            if object_id is None and spawn is None:
                raise ValueError('intake requires an object binding or spawn template')
            if not asset_id or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', asset_id) or asset_id in cat['assets']:
                raise ValueError('new unique asset ID required')
            if any(isinstance(x, bool) or not math.isfinite(x) or x <= 0 for x in (scale, weld_tolerance)):
                raise ValueError('scale and weld tolerance must be finite and positive')
            data = Path(source).read_bytes()
            sha = hashlib.sha256(data).hexdigest()
            rel = f'assets/sources/{sha}.stl'
            publish(path_in(root, rel), data)
            cat['assets'][asset_id] = {'source': rel, 'source_sha256': sha,
                'original_name': Path(source).name, 'default_shading': policy(default_mode, crease_angle),
                'import': {'source_to_asset_scale': scale, 'weld_vertices': True, 'weld_tolerance': weld_tolerance}}
        if spawn is not None:
            scene['objects'].append(copy.deepcopy(spawn))
            check_scene(scene)
            object_id = spawn['object_id']
        if object_id is not None:
            obj = next((o for o in scene['objects'] if o['object_id'] == object_id), None)
            if obj is None or obj.get('object_type') != 'mesh_asset_instance':
                raise ValueError('mesh object not found')
            ext = obj.setdefault('extensions', {}).setdefault('ray_tracing', {})
            previous = ext.get('managed_mesh', {})
            selected = asset_id or previous.get('asset_id')
            if selected not in cat['assets']:
                raise ValueError('unknown managed asset')
            ext['managed_mesh'] = {'asset_id': selected, 'shading': 'inherit' if shading == 'inherit' else policy(shading, crease_angle)}
        resolve_all(root, scene, compiler)
        with tempfile.NamedTemporaryFile(dir=root, suffix='.json', delete=False) as f:
            pending = Path(f.name)
            f.write(encode(scene)); f.flush(); os.fsync(f.fileno())
        try:
            if status(pending, compiler)['status'] != 'ready':
                raise ValueError('candidate not ready')
            if scene_path.read_bytes() != original:
                raise ValueError('scene changed concurrently; retry from fresh scene')
            os.replace(pending, scene_path)
        finally:
            pending.unlink(missing_ok=True)
    return status(scene_path, compiler)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=['status', 'apply'])
    p.add_argument('--scene', type=Path, required=True)
    p.add_argument('--compiler', type=Path)
    p.add_argument('--source', type=Path)
    p.add_argument('--asset-id'); p.add_argument('--object-id')
    p.add_argument('--shading', choices=['inherit', *sorted(MODES)], default='inherit')
    p.add_argument('--default-mode', choices=sorted(MODES), default='flat')
    p.add_argument('--crease-angle', type=float, default=60)
    p.add_argument('--scale', type=float, default=1)
    p.add_argument('--weld-tolerance', type=float, default=1e-6)
    p.add_argument('--smoothing', choices=['on', 'off'])
    a = p.parse_args()
    try:
        if a.action == 'status':
            result = status(a.scene, a.compiler)
        else:
            if not a.compiler:
                p.error('apply requires --compiler')
            result = update(a.scene, a.compiler, source=a.source, asset_id=a.asset_id,
                object_id=a.object_id, shading=a.shading, default_mode=a.default_mode,
                crease_angle=a.crease_angle, scale=a.scale, weld_tolerance=a.weld_tolerance,
                smoothing_enabled=None if a.smoothing is None else a.smoothing == 'on')
        print(json.dumps(result, indent=2))
        return 0 if result['status'] != 'rebuild_required' else 1
    except (ValueError, KeyError, TypeError, OSError, subprocess.CalledProcessError) as e:
        p.exit(2, f'managed mesh: {e}\n')


if __name__ == '__main__':
    raise SystemExit(main())
