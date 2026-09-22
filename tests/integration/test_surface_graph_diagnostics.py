#!/usr/bin/env python3
"""Check actionable graph failures through the public CLI and scene preflight."""
import argparse
import copy
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from surface_material_m6 import default_graph
from smooth_mesh_reflection.prepare_reflection_matrix import build_request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root', type=Path, required=True)
    args = parser.parse_args()
    out = args.output_root.resolve()
    out.mkdir(parents=True, exist_ok=False)
    renderer = ROOT / f'build/toolchains/clang/{platform.machine()}/tools/cli/ray_tracing_render_headless'
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(ROOT))
    results = {}

    def run(name, command, expected=None, object_id='-'):
        result = subprocess.run([str(x) for x in command], capture_output=True, text=True,
                                env=env, timeout=60)
        text = result.stdout + result.stderr
        (out / f'{name}.log').write_text(text)
        if expected is None:
            assert result.returncode == 0, (name, text)
        else:
            code, node, prop = expected
            assert result.returncode != 0, (name, text)
            for field in (f'code={code}', f'object={object_id}', f'node={node}', f'property={prop}'):
                assert field in text, (name, field, text)
            assert 'invalid or unsupported capability, node, edge' not in text
        results[name] = {'returncode': result.returncode, 'expected': expected}

    def graph_case(name, modify, expected, source='noise3d'):
        graph = default_graph(source)
        modify(graph)
        path = out / f'{name}.json'
        path.write_text(json.dumps(graph))
        run(name, [renderer, '--validate-surface-graph', path], expected)

    graph_case('valid', lambda g: None, None)
    graph_case('reordered', lambda g: g['nodes'].reverse(), None)
    graph_case('capability', lambda g: g.update(required_capability='future'),
               ('unsupported_capability', '-', 'required_capability'))
    graph_case('version', lambda g: g.update(version=2), ('unsupported_version', '-', 'version'))
    graph_case('encoding', lambda g: g.update(color_space='srgb'), ('unsupported_encoding', '-', 'color_space'))
    graph_case('required_feature', lambda g: g.update(required_features=['future']),
               ('unsupported_feature', '-', 'required_features'))
    graph_case('null_required_feature', lambda g: g.update(required_features=None),
               ('unsupported_feature', '-', 'required_features'))
    graph_case('node_required_feature', lambda g: g['nodes'][1].update(required_features=None),
               ('unsupported_feature', 'pattern', 'required_features'))
    graph_case('node_id', lambda g: g['nodes'][1].pop('id'), ('invalid_node_id', '-', 'nodes[1].id'))
    graph_case('duplicate_id', lambda g: g['nodes'][1].update(id='position'),
               ('duplicate_node_id', 'position', 'id'))
    graph_case('kind', lambda g: g['nodes'][1].update(kind='future'), ('unsupported_node', 'pattern', 'kind'))
    graph_case('input_count', lambda g: g['nodes'][5].update(inputs=['dark']),
               ('input_count', 'finish', 'inputs'))
    graph_case('reference', lambda g: g['nodes'][5]['inputs'].__setitem__(0, 'absent'),
               ('missing_reference', 'finish', 'inputs[0]'))
    graph_case('input_type', lambda g: g['nodes'][5]['inputs'].__setitem__(0, 'rough'),
               ('input_type', 'finish', 'inputs[0]'))
    graph_case('cycle', lambda g: g['nodes'][5]['inputs'].__setitem__(0, 'finish'),
               ('cycle', 'finish', 'inputs[0]'))
    graph_case('scale', lambda g: g['nodes'][0].update(scale_m=0),
               ('parameter_range', 'position', 'scale_m'))
    graph_case('space', lambda g: g['nodes'][0].update(space='uv'),
               ('unsupported_space', 'position', 'space'))
    graph_case('offset', lambda g: g['nodes'][0].update(offset=[0, 1e7, 0]),
               ('parameter_range', 'position', 'offset[1]'))
    graph_case('seed', lambda g: g['nodes'][1].update(seed=.5), ('parameter_range', 'pattern', 'seed'))
    graph_case('sharpness', lambda g: g['nodes'][1].update(sharpness=0),
               ('parameter_range', 'pattern', 'sharpness'), 'triplanar_checker')
    graph_case('color', lambda g: g['nodes'][2].update(value=[0, -1, 0]),
               ('parameter_range', 'dark', 'value[1]'))
    graph_case('output_type', lambda g: g['outputs'].update(base_color='rough'),
               ('output_type', 'rough', 'outputs.base_color'))
    graph_case('output_missing', lambda g: g['outputs'].update(base_color='absent'),
               ('missing_reference', '-', 'outputs.base_color'))
    graph_case('output_unknown', lambda g: g['outputs'].update(displacement='rough'),
               ('unsupported_output', '-', 'outputs.displacement'))
    graph_case('too_many', lambda g: g['nodes'].extend(copy.deepcopy(g['nodes']) * 6),
               ('node_limit', '-', 'nodes'))

    scene = json.loads((ROOT / 'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    scene['objects'] = [scene['objects'][0]]
    obj = scene['objects'][0]
    obj['object_id'] = 'diagnostic-surface'
    obj.setdefault('extensions', {}).setdefault('ray_tracing', {}).pop('surface_mapping', None)
    rows = [{'object_id': obj['object_id'], 'material_id': 0, 'surface_graph': default_graph()}]
    scene['extensions']['ray_tracing']['authoring']['object_materials'] = rows

    def scene_case(name, modify, expected):
        candidate = copy.deepcopy(scene)
        modify(candidate)
        path = out / f'{name}-scene.json'
        path.write_text(json.dumps(candidate))
        req = build_request(out / name, 'direct', 'flattened')
        req['scene']['runtime_scene_path'] = str(path)
        request = out / f'{name}-request.json'
        request.write_text(json.dumps(req))
        run(name, [renderer, '--request', request, '--preflight'], expected, obj['object_id'])

    def row(s):
        return s['extensions']['ray_tracing']['authoring']['object_materials'][0]

    scene_case('scene_valid', lambda s: None, None)
    scene_case('scene_node_identity', lambda s: row(s)['surface_graph']['nodes'][0].update(scale_m=0),
               ('parameter_range', 'position', 'scale_m'))
    scene_case('scene_mixed_source', lambda s: row(s).update(material_texture_stack={'layers': []}),
               ('mixed_source', '-', 'material_texture_stack'))
    scene_case('scene_transform', lambda s: s['objects'][0]['transform'].update(scale={'x': 0, 'y': 1, 'z': 1}),
               ('transform_range', '-', 'transform.scale.x'))
    (out / 'acceptance.json').write_text(json.dumps(results, indent=2) + '\n')
    print(f'{len(results)} graph diagnostic cases passed')


if __name__ == '__main__':
    main()
