#!/usr/bin/env python3
"""Prepare, validate and preview a generated-UV scene candidate; never adopt it."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

from managed_mesh_assets import check_scene, digest, encode, publish
from surface_material_resources import source_path, validate_runtime, publish_new_file

ROOT = Path(__file__).resolve().parents[1]


def same_geometry(before, after):
    a, b = before['mesh'], after['mesh']
    if len(a['vertices']) != len(b['vertices']) or len(a['triangles']) != len(b['triangles']):
        return False
    for x, y in zip(a['vertices'], b['vertices']):
        if any(abs(x[k] - y[k]) > 1e-12 * max(1, abs(x[k]), abs(y[k])) for k in 'xyz'):
            return False
    return all(all(x[k] == y[k] for k in 'abc') for x, y in zip(a['triangles'], b['triangles']))


def candidate(scene_path, scene_pin, source, source_pin, object_id, output, compiler, renderer,
              method='box', scale_m=1, uv_set_id='generated_uv', render=True):
    scene_path, source = Path(scene_path).resolve(strict=True), Path(source).resolve(strict=True)
    output = Path(output).absolute()
    compiler, renderer = Path(compiler).resolve(strict=True), Path(renderer).resolve(strict=True)
    if output.parent.resolve() != scene_path.parent:
        raise ValueError('candidate must share the retained scene directory for managed adoption')
    receipt_path = output.with_suffix(output.suffix + '.receipt.json')
    if output.exists() or output.is_symlink() or receipt_path.exists():
        raise ValueError('candidate/receipt outputs already exist')
    if digest(scene_path) != scene_pin or digest(source) != source_pin:
        raise ValueError('stale scene/source digest')
    if method not in {'box', 'planar_xy'} or not math.isfinite(scale_m) or not 1e-6 <= scale_m <= 1e6:
        raise ValueError('unsupported projection or scale')
    if not uv_set_id or len(uv_set_id.encode()) >= 64:
        raise ValueError('UV set identity must fit 63 bytes')
    scene = json.loads(scene_path.read_text())
    check_scene(scene)
    objects = [obj for obj in scene['objects'] if obj['object_id'] == object_id]
    if len(objects) != 1 or objects[0].get('object_type') != 'mesh_asset_instance':
        raise ValueError('select one existing mesh instance')
    obj = objects[0]
    ray = obj.setdefault('extensions', {}).setdefault('ray_tracing', {})
    if 'generated_uv_history' in ray and not isinstance(ray['generated_uv_history'], list):
        raise ValueError('generated UV history must be an array')
    if 'managed_mesh' in ray or 'procedural_solid_material_ref' in obj:
        raise ValueError('managed/procedural geometry requires its owning workflow')
    rows = scene.get('extensions', {}).get('ray_tracing', {}).get('authoring', {}).get('object_materials', [])
    row = next((r for r in rows if r['object_id'] == object_id), {})
    if any(k in row for k in ('surface_graph', 'surface_material_binding')):
        raise ValueError('selected source does not support authored-UV mapping; replace explicitly first')
    old_id = obj['geometry_ref']['id']
    old_mesh_path = source_path(scene_path.parent, obj['extensions'].get('line_drawing', {}).get(
        'runtime_mesh_path', f'assets/mesh_assets/{old_id}.runtime.json'))
    old_mesh = json.loads(old_mesh_path.read_text())
    recipe = {'version': 1, 'source_sha256': source_pin, 'method': method, 'scale_m': scale_m,
              'uv_set_id': uv_set_id, 'compiler_sha256': digest(compiler)}
    recipe_hash = hashlib.sha256(encode(recipe)).hexdigest()
    asset_id = 'uv_' + recipe_hash[:40]
    support = scene_path.parent / 'dependencies' / 'surface_uv' / recipe_hash
    with tempfile.TemporaryDirectory(prefix='.surface-uv-', dir=scene_path.parent) as directory:
        work = Path(directory)
        projected = work / 'projected.obj'
        subprocess.run([sys.executable, str(ROOT / 'tools/surface_uv_unwrap.py'), '--source', str(source),
                        '--expected-sha256', source_pin, '--output', str(projected), '--method', method,
                        '--scale-m', str(scale_m), '--uv-set-id', uv_set_id], check=True, capture_output=True)
        authoring = json.loads((ROOT / 'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json').read_text())
        authoring['asset_id'] = asset_id
        authoring['authoring']['imported_mesh'].update(
            source_format='obj', source_uri=str(projected), uv_set_id=uv_set_id,
            source_to_asset_scale=1, preserve_source_normals=True, normal_mode='none',
            crease_angle_degrees=60, source_unit_system='meter', topology_closed_volume_observed=False)
        spec, compiled_path = work / 'authoring.json', work / 'runtime.json'
        spec.write_bytes(encode(authoring))
        subprocess.run([str(Path(compiler).resolve()), str(spec), str(work), asset_id, str(compiled_path)],
                       check=True, capture_output=True, timeout=120)
        mesh = json.loads(compiled_path.read_text())
        if not same_geometry(old_mesh, mesh):
            raise ValueError('projected source changes geometry; UV candidate requires identical vertices/topology')
        mesh['extensions'] = copy.deepcopy(old_mesh.get('extensions', {}))
        attributes = mesh['mesh'].get('surface_attributes', {})
        if attributes.get('uv_set_id') != uv_set_id:
            raise ValueError('compiler did not preserve requested UV identity')
        # Immutable support files precede the scene commit point; they are safe to
        # retain if later preflight fails and never overwrite a live source.
        publish(support / 'projected.obj', projected.read_bytes())
        publish(support / 'projection.uv.json', projected.with_suffix('.uv.json').read_bytes())
        authoring['authoring']['imported_mesh']['source_uri'] = str((support / 'projected.obj').relative_to(scene_path.parent))
        publish(support / 'authoring.json', encode(authoring))
        mesh_relative = f'assets/mesh_assets/{asset_id}.runtime.json'
        publish(scene_path.parent / mesh_relative, encode(mesh))
        obj['geometry_ref']['id'] = asset_id
        obj['extensions'].setdefault('line_drawing', {})['runtime_mesh_path'] = mesh_relative
        mapping = copy.deepcopy(ray.get('surface_mapping', {}))
        prior = copy.deepcopy(mapping)
        mapping.update(version=3, required_capability='optic.authored_uv_v1', method='authored_uv',
                       source_domain='brick_cells_v1', uv_set_id=uv_set_id, uv_scale=[1, 1],
                       uv_offset=[0, 0], rotation_rad=0, seed=mapping.get('seed', 1729))
        ray['surface_mapping'] = mapping
        ray.setdefault('generated_uv_history', []).append({'recipe': recipe,
            'previous_geometry_ref': old_id, 'previous_mapping': prior,
            'projection': str((support / 'projection.uv.json').relative_to(scene_path.parent)),
            'projected_source': str((support / 'projected.obj').relative_to(scene_path.parent))})
        # Validate in the same parent as eventual adoption, keeping relative refs exact.
        staged = scene_path.parent / ('.' + output.name + '.validation-' + recipe_hash[:12])
        if staged.exists():
            raise ValueError('validation candidate path already exists')
        try:
            with staged.open('xb') as stream:
                stream.write(encode(scene))
            validation = validate_runtime(staged, renderer, support / ('preview-' + output.stem), render)
        finally:
            staged.unlink(missing_ok=True)
        if digest(scene_path) != scene_pin or digest(source) != source_pin:
            raise ValueError('source changed during candidate preparation')
        receipt = {'schema': 'optic_surface_uv_candidate_v1',
                   'state': 'reviewed_candidate_not_adopted' if render else 'preflight_only_candidate_not_adopted',
                   'source_scene_sha256': scene_pin, 'source_obj_sha256': source_pin,
                   'candidate_sha256': hashlib.sha256(encode(scene)).hexdigest(),
                   'object_id': object_id, 'asset_id': asset_id, 'uv_set_id': uv_set_id,
                   'geometry_unchanged': True, 'recipe': recipe, 'validation': validation,
                   'candidate_file': output.name,
                   'review_root': str((support / ('preview-' + output.stem)).relative_to(scene_path.parent)),
                   'projection_metadata': str((support / 'projection.uv.json').relative_to(scene_path.parent)),
                   'mesh_path': mesh_relative, 'mesh_sha256': digest(scene_path.parent / mesh_relative)}
        publish_new_file(receipt_path, encode(receipt))
        # create-only scene publication is the candidate commit point.
        publish_new_file(output, encode(scene))
        return receipt


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--scene', type=Path, required=True)
    p.add_argument('--expected-scene-sha256', required=True)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--expected-source-sha256', required=True)
    p.add_argument('--object-id', required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--compiler', type=Path, required=True)
    p.add_argument('--renderer', type=Path, required=True)
    p.add_argument('--method', choices=['box', 'planar_xy'], default='box')
    p.add_argument('--scale-m', type=float, default=1)
    p.add_argument('--uv-set-id', default='generated_uv')
    p.add_argument('--preflight-only', action='store_true')
    a = p.parse_args()
    try:
        print(json.dumps(candidate(a.scene, a.expected_scene_sha256, a.source, a.expected_source_sha256,
              a.object_id, a.output, a.compiler, a.renderer, a.method, a.scale_m, a.uv_set_id,
              not a.preflight_only), indent=2))
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        p.exit(2, str(error) + '\n')


if __name__ == '__main__':
    main()
