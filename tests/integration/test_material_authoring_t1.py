#!/usr/bin/env python3
"""Native T1 authoring acceptance from a fixture with no authored material source."""
import argparse
import copy
import json
import os
import platform
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-root', required=True, type=Path)
    parser.add_argument('--binary', type=Path, help='Contained native acceptance binary')
    parser.add_argument('--mcp', action='store_true', help='Run the materials MCP slice acceptance')
    args = parser.parse_args()
    out = args.output_root.resolve()
    out.mkdir(parents=True, exist_ok=False)
    runtime = out / 'data/runtime'
    runtime.mkdir(parents=True)
    (runtime / 'animation_config.json').write_text(json.dumps({
        'editorMode': 1, 'spaceMode': 1, 'windowWidth': 1280, 'windowHeight': 800,
        'inputRoot': str(ROOT / 'config'), 'outputRoot': str(runtime),
        'videoOutputRoot': str(out / 'videos')}))
    (runtime / 'scene_config.json').write_text(json.dumps({'window': {'width': 1280, 'height': 800}}))
    scene = json.loads((ROOT / 'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    scene['scene_id'] = 't1_material_authoring'
    scene['world_scale'] = 1
    source = copy.deepcopy(scene['objects'][0])
    scene['objects'] = []
    for index, name in enumerate(('surface', 'recipient', 'rowless')):
        obj = copy.deepcopy(source)
        obj['object_id'] = name
        obj['object_type'] = obj['primitive']['kind'] = 'rect_prism_primitive'
        obj['primitive'].update(width=2, height=2, depth=.5)
        obj['primitive']['frame']['origin'] = {'x': index * 2.5, 'y': 0, 'z': 0}
        obj['transform']['position'] = {'x': 0, 'y': 0, 'z': 0}
        obj['transform']['rotation'] = {'x': 0, 'y': 0, 'z': 0}
        obj['transform']['scale'] = {'x': 1, 'y': 1, 'z': 1}
        obj['flags'] = {'visible': True, 'locked': False, 'selectable': True}
        obj.pop('extensions', None)
        scene['objects'].append(obj)
    authoring = scene.setdefault('extensions', {}).setdefault('ray_tracing', {}).setdefault('authoring', {})
    authoring['object_materials'] = [
        {'object_id': name, 'material_id': 0, 'object_color': 12756864, 'roughness': .65,
         'reflectivity': .02, 'glass_ior': 1.8 if name == 'surface' else 1.1,
         't1_unknown_producer_metadata': {'preserve': name}}
        for name in ('surface', 'recipient')]
    scene['extensions']['t1_provenance'] = {'preserve': ['unrelated', 123]}
    if args.mcp:
        from test_surface_mapping_m1 import MAPPING
        mapping = copy.deepcopy(MAPPING)
        scene['objects'][1]['extensions'] = {'ray_tracing': {'surface_mapping': mapping}}
        authoring['object_materials'][1].update(
            surface_material_binding={'version': 1, 'required_capability': 'optic.surface_material_v3'},
            material_graph={'schema_version': 1, 'graph_id': 'recipient-graph',
                'nodes': [{'node_id': 'recipient-node', 'node_kind': 'layer',
                    'layer': {'id': 'base', 'kind': 'brick', 'opacity': 1,
                        'placement': {'scale': 1, 'strength': 1}}}]})
    path = out / 'scene_runtime.json'
    path.write_text(json.dumps(scene, indent=2))
    (out / 'original_scene.json').write_text(json.dumps(scene, indent=2))
    binary = args.binary.resolve() if args.binary else ROOT / f'build/toolchains/clang/{platform.machine()}/tests/scene_editor_workspace_visual_test'
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(ROOT),
               RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out / 'cache'))
    if args.mcp: env['OPTIC_MATERIAL_MCP_TEST'] = str(ROOT / 'tests/integration/check_material_mcp.py')
    if args.mcp:
        baseline = out / 'legacy_save_baseline.json'
        baseline.write_text(json.dumps(scene, indent=2))
        with (out / 'legacy-save-baseline.log').open('w') as stream:
            result = subprocess.run([str(binary), str(out), str(baseline), '--material-mcp-legacy-save'], env=env,
                stdout=stream, stderr=subprocess.STDOUT, timeout=240)
        assert result.returncode == 0, (result.returncode, out / 'legacy-save-baseline.log')
    modes = (('--material-mcp', 'native.log'), ('--material-mcp-reopen', 'reopen.log')) if args.mcp else (('--material-authoring-t1', 'native.log'), ('--material-authoring-t1-reopen', 'reopen.log'))
    for mode, log in modes:
        with (out / log).open('w') as stream:
            result = subprocess.run([str(binary), str(out), str(path), mode], env=env,
                                    stdout=stream, stderr=subprocess.STDOUT, timeout=240)
        assert result.returncode == 0, (result.returncode, out / log)
    saved = json.loads(path.read_text())
    if args.mcp:
        assert saved['objects'] == scene['objects']
        assert saved['extensions']['t1_provenance'] == scene['extensions']['t1_provenance']
        rows = saved['extensions']['ray_tracing']['authoring']['object_materials']
        baseline_rows = json.loads((out / 'legacy_save_baseline.json').read_text())['extensions']['ray_tracing']['authoring']['object_materials']
        assert rows[1:] == baseline_rows[1:]
        assert rows[0]['t1_unknown_producer_metadata'] == authoring['object_materials'][0]['t1_unknown_producer_metadata']
        receipt = json.loads((out / 'material-mcp-receipt.json').read_text())
        receipt.update(fresh_process_reopen=True, legacy_objects_and_unrelated_row_preserved=True, untouched_file_save_baseline_parity=True)
        (out / 'acceptance.json').write_text(json.dumps(receipt, indent=2)+'\n')
        print(json.dumps(receipt, indent=2)); return
    assert saved['extensions']['t1_provenance'] == scene['extensions']['t1_provenance']
    rows = saved['extensions']['ray_tracing']['authoring']['object_materials']
    assert len(rows) == 2 and all('surface_graph' in row for row in rows)
    assert {row['object_id'] for row in rows} == {'surface', 'recipient'}
    for capture in ('t1-material-normal.ppm', 't1-material-narrow.ppm'):
        assert (out / capture).stat().st_size > 100
    receipt = json.loads((out / 'material_authoring_t1.json').read_text())
    receipt['fresh_process_reopen'] = json.loads((out / 'material_authoring_t1_reopen.json').read_text())
    receipt['unknown_scene_metadata_preserved'] = True
    (out / 'acceptance.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
