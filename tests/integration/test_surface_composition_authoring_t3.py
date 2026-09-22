#!/usr/bin/env python3
"""Native composition creation, typed wiring, face inheritance and retained reopen."""
import argparse
import json
import os
import platform
import subprocess
from pathlib import Path
from test_surface_resources_t2 import ROOT, fixture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root', type=Path, required=True)
    args = parser.parse_args()
    folder = args.output_root.resolve()
    if folder.exists():
        parser.error('output root must be new')
    scene = fixture(folder, 1, False)
    binary = ROOT / f'build/toolchains/clang/{platform.machine()}/tests/scene_editor_workspace_visual_test'
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(ROOT),
               RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(folder / 'cache'),
               OPTIC_T3_BASE_COLOR=str(folder / 'images/base_color.png'),
               OPTIC_T3_ROUGHNESS=str(folder / 'images/roughness.png'))
    result = {}
    for mode, receipt in (('--composition-authoring-t3', 'composition_authoring_t3'),
                          ('--composition-authoring-t3-reopen', 'composition_authoring_t3_reopen')):
        with (folder / (receipt + '.log')).open('w') as log:
            run = subprocess.run([str(binary), str(folder), str(scene), mode], env=env,
                                 stdout=log, stderr=subprocess.STDOUT, timeout=300)
        assert run.returncode == 0, (mode, run.returncode, folder)
        result[receipt] = json.loads((folder / (receipt + '.json')).read_text())
    (folder / 'acceptance.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
