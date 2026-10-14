#!/usr/bin/env python3
"""Native animation MCP acceptance with legacy objects and atmosphere."""
import argparse
import copy
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-root', required=True, type=Path)
    parser.add_argument('--binary', required=True, type=Path, help='Contained native acceptance binary')
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
    import shutil
    scene=json.loads((ROOT/'tests/fixtures/motion_orientation_mcp/scene_runtime.json').read_text())
    shutil.copyfile(ROOT/'tests/fixtures/scene_project_worker_snapshot/assets/vf3d/active/frame_000000.vf3d',out/'attached.vf3d')
    path=out/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2));(out/'original_scene.json').write_text(json.dumps(scene,indent=2))
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'),OPTIC_MOTION_ORIENTATION_TEST=str(ROOT/'tests/integration/check_motion_orientation_mcp.py'))
    for mode,log in (('--motion-orientation-mcp','native.log'),('--motion-orientation-mcp-reopen','reopen.log')):
        with (out/log).open('w') as stream:result=subprocess.run([str(args.binary.resolve()),str(out),str(path),mode],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        assert result.returncode==0,(result.returncode,out/log)
    receipt=json.loads((out/'motion-orientation-receipt.json').read_text());receipt.update(fresh_process_reopen=True,fixed_sampling_direct_disney_fresh_reopen=True,native_ui_motion_plan_orientation=True)
    (out/'acceptance.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))

if __name__=='__main__':main()
