#!/usr/bin/env python3
"""Native held render result lifetime across owner replacement and publication failure."""
import argparse,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--output-root',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=a.output_root.resolve();out.mkdir(parents=True,exist_ok=False)
runtime=out/'data/runtime';runtime.mkdir(parents=True)
(runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(r/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(out/'videos')}))
(runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
scene=out/'scene.json';scene.write_bytes((r/'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_bytes())
(out/'second.json').write_bytes(scene.read_bytes())
(out/'output').mkdir();(out/'output/scene.json').write_bytes(scene.read_bytes())
env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(r),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'))
with (out/'native.log').open('w') as log:
 result=subprocess.run([str(a.binary.resolve()),str(out),str(scene),'--l3-render'],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=60)
assert result.returncode==0,(result.returncode,out/'native.log')
print('PASS native render-result replacement:',out/'native.log')
