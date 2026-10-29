#!/usr/bin/env python3
"""Compare every immutable result tick and source vertex with native optiC playback."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from rigid_scene_qualification_math import world_vertices


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('consumer', 'pack-io', 'parent', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    ap.add_argument('--case', action='append', default=[],
                    help='Select named child directories; default checks all.')
    ap.add_argument('--batch-ticks', type=int, default=40,
                    help='Ticks per observer process, 1–240; every tick is still checked.')
    a = ap.parse_args()
    if not 1 <= a.batch_ticks <= 240:
        ap.error('--batch-ticks must be in 1–240')
    a.out.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, XDG_CONFIG_HOME=str(a.out/'config'),
               RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled',
               ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    result = {}
    for parent in sorted(a.parent.iterdir()):
        if a.case and parent.name not in a.case:
            continue
        scene_path = parent/'scene.json'
        if not scene_path.is_file():
            continue
        scene = json.loads(scene_path.read_text())
        original = hashlib.sha256((parent/'result/poses.pack').read_bytes()).hexdigest()
        raw = subprocess.check_output([str(a.pack_io), 'read', str(parent/'result/poses.pack')])
        cache = json.loads(raw)
        bindings = {b['object_id']: b for b in cache['physical_input']['bindings']}
        assets = {v['asset_id']: v['root'] for v in json.loads((parent/'result/input/assets/asset_set.json').read_text())['assets']}
        meshes = {oid: json.loads((parent/'result/input/assets'/assets[b['collision_asset_id']]/'visible.json').read_text())['mesh'] for oid, b in bindings.items()}
        visible = {o['object_id'] for o in scene['objects'] if o.get('flags', {}).get('visible', True)}
        triangle_count = sum(len(m['triangles']) for oid, m in meshes.items() if oid in visible)
        maximum = 0.0
        vertices_checked = 0
        for start in range(0, len(cache['frames']), a.batch_ticks):
            ticks = list(range(start, min(start+a.batch_ticks, len(cache['frames']))))
            p = subprocess.run([str(a.consumer), str(scene_path), *map(str, ticks)], env=env,
                               capture_output=True, timeout=180, check=True)
            assert not p.stderr, p.stderr[-1000:]
            rows = [json.loads(line[4:]) for line in p.stdout.splitlines() if line.startswith(b'BR4 ')]
            assert len(rows) == len(ticks)
            for tick, row in zip(ticks, rows):
                poses = {v['object_id']: v for v in cache['frames'][tick]['bodies']}
                assert row['frame'] == tick and len(row['poses']) == len(bindings)
                for v in row['poses']:
                    expected = poses[v['id']]
                    assert v['tick'] == tick and v['body_id'] == bindings[v['id']]['body_id']
                    assert v['position'] == expected['position_m'] and v['q'] == expected['orientation_wxyz']
                expected = {oid: world_vertices(mesh, bindings[oid], poses[oid]) for oid, mesh in meshes.items()}
                assert len(row['triangles']) == triangle_count
                for triangle in row['triangles']:
                    oid = triangle['id']
                    source = meshes[oid]['triangles'][triangle['local']]
                    for actual, key in zip(triangle['vertices'], ('a', 'b', 'c')):
                        maximum = max(maximum, max(abs(x-y) for x, y in zip(actual, expected[oid][source[key]])))
                        vertices_checked += 1
            assert maximum <= 2e-12, (parent.name, maximum)
        assert hashlib.sha256((parent/'result/poses.pack').read_bytes()).hexdigest() == original
        result[parent.name] = {'ticks': len(cache['frames']), 'body_count': len(bindings),
                               'vertices_checked': vertices_checked, 'max_vertex_error_m': maximum,
                               'source_pack_unchanged': original}
        print(parent.name, result[parent.name], flush=True)
    assert result
    (a.out/'verification.json').write_text(json.dumps({'ok': True, 'cases': result}, indent=2)+'\n')


if __name__ == '__main__':
    main()
