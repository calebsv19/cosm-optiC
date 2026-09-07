#!/usr/bin/env python3
"""Strict normal-connectivity acceptance with a separate expected-defect capture."""
import argparse
import json
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from managed_mesh_assets import update
from smooth_mesh_reflection.prepare_reflection_matrix import build_scene


def run(out, compiler):
    out.mkdir(parents=True, exist_ok=False)
    face = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]
    cases = {
        'point_touch_smooth': ([face, [(0, 0, 0), (0, 0, 1), (-1, 0, 0)]], 'smooth'),
        'point_touch_crease': ([face, [(0, 0, 0), (0, 0, 1), (-1, 0, 0)]], 'crease_aware'),
        'opposed_ab': ([face, list(reversed(face))], 'smooth'),
        'opposed_ba': ([list(reversed(face)), face], 'smooth'),
        'connected_coplanar': ([face, [(1, 0, 0), (1, 1, 0), (0, 1, 0)]], 'smooth'),
        'flat_control': ([face, [(0, 0, 0), (0, 0, 1), (-1, 0, 0)]], 'flat'),
    }
    meshes = {}
    for name, (faces, mode) in cases.items():
        directory = out / name; directory.mkdir()
        stl = directory / 'input.stl'
        stl.write_text('solid probe\n' + ''.join('facet normal 0 0 0\nouter loop\n' + ''.join('vertex %g %g %g\n' % p for p in f) + 'endloop\nendfacet\n' for f in faces) + 'endsolid probe\n')
        scene = build_scene({f: f for f in ('crease', 'analytic_sphere', 'icosphere', 'organic_blob')})
        scene['objects'] = scene['objects'][-1:]
        path = directory / 'scene_runtime.json'; path.write_text(json.dumps(scene))
        update(path, compiler, source=stl, asset_id='probe', object_id=scene['objects'][0]['object_id'], default_mode=mode)
        obj = json.loads(path.read_text())['objects'][0]
        meshes[name] = json.loads((directory / obj['extensions']['line_drawing']['runtime_mesh_path']).read_text())['mesh']
    def origin_normals(mesh):
        return sorted(tuple(n[k] for k in 'xyz') for v,n in zip(mesh['vertices'], mesh.get('normals', [])) if all(abs(v[k]) < 1e-12 for k in 'xyz'))
    checks = {
        'smooth_disconnected_fans_split': meshes['point_touch_smooth']['vertex_count'] == 6 and origin_normals(meshes['point_touch_smooth']) == [(0, -1, 0), (0, 0, 1)],
        'crease_disconnected_fans_split': meshes['point_touch_crease']['vertex_count'] == 6 and origin_normals(meshes['point_touch_crease']) == [(0, -1, 0), (0, 0, 1)],
        'opposed_faces_order_independent': origin_normals(meshes['opposed_ab']) == origin_normals(meshes['opposed_ba']),
        'connected_surface_stays_smooth': meshes['connected_coplanar']['vertex_count'] == 4 and all(n == {'x': 0, 'y': 0, 'z': 1} for n in meshes['connected_coplanar']['normals']),
        'flat_has_no_generated_normals': meshes['flat_control'].get('normal_count', 0) == 0,
    }
    report = {'checks': checks, 'failures': [k for k,v in checks.items() if not v],
              'origin_normals': {k: origin_normals(v) for k,v in meshes.items()},
              'status': 'pass' if all(checks.values()) else 'fail'}
    (out / 'report.json').write_text(json.dumps(report, indent=2))
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--compiler', type=Path, default=ROOT / f'build/toolchains/clang/{platform.machine()}/tools/smooth_mesh_reflection/compile_runtime_fixture')
    p.add_argument('--capture-known-defects', action='store_true')
    a=p.parse_args(); report=run(a.out.resolve(), a.compiler.resolve()); print(json.dumps(report, indent=2))
    if a.capture_known_defects:
        return 0 if set(report['failures']) == {'smooth_disconnected_fans_split', 'opposed_faces_order_independent'} else 1
    return 0 if report['status'] == 'pass' else 1

if __name__ == '__main__':
    raise SystemExit(main())
