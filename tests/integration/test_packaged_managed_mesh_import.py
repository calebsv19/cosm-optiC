#!/usr/bin/env python3
"""Exercise the bundled STL helper and compiler with a source outside the app."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from smooth_mesh_reflection.prepare_reflection_matrix import build_scene


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--source', type=Path)
    args = parser.parse_args()
    tools = args.app / 'Contents/Resources/bin'
    helper = tools / 'managed_mesh_assets.py'
    compiler = tools / 'compile_runtime_fixture'
    assert helper.is_file() and compiler.is_file(), 'packaged managed mesh tools missing'
    source = args.source or ROOT / 'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/imports/tetrahedron_ascii.stl'
    with tempfile.TemporaryDirectory(prefix='optic-packaged-stl-') as directory:
        project = Path(directory)
        scene = project / 'scene_runtime.json'
        candidate = project / '.candidate.json'
        document = build_scene({name: name for name in ('crease', 'analytic_sphere', 'icosphere', 'organic_blob')})
        document['objects'] = []
        document['extensions']['ray_tracing']['authoring']['object_materials'] = []
        scene.write_text(json.dumps(document))
        original = scene.read_bytes()
        subprocess.run([sys.executable, str(helper), 'apply', '--scene', str(scene),
                        '--compiler', str(compiler), '--source', str(source),
                        '--asset-id', 'package_probe', '--spawn-object-id', 'package_probe',
                        '--default-mode', 'flat', '--output-scene', str(candidate)],
                       check=True, capture_output=True, text=True, timeout=60)
        assert scene.read_bytes() == original, 'packaged helper modified the active scene'
        result = json.loads(candidate.read_text())
        assert any(obj['object_id'] == 'package_probe' for obj in result['objects'])
        assert (project / 'assets/mesh_assets').is_dir()
    print('packaged managed STL import passed')


if __name__ == '__main__':
    main()
