#!/usr/bin/env python3
"""Native close/reopen with two owned TERM-resistant local helper jobs."""
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
stl=out/'input.stl';stl.write_text('solid test\nendsolid test\n')
helper=out/'helper.py';helper.write_text('import os,signal,sys,time\nfrom pathlib import Path\nsignal.signal(signal.SIGTERM,signal.SIG_IGN)\nPath(sys.argv[1]+".pid").write_text(str(os.getpid()))\nwhile not Path("import-release" if sys.argv[1]=="apply" else "release").exists(): time.sleep(.01)\nif sys.argv[1]=="apply": Path(sys.argv[sys.argv.index("--output-scene")+1]).write_bytes(Path(sys.argv[sys.argv.index("--scene")+1]).read_bytes())\n')
profile=out/'profile.json';data={key:str(out/key) for key in ('ball_tool','backend','pack_io','assets','store','renderer','output')};data.update(schema='optic_rigid_run_profile_v1',workflow=str(helper),ticks='1',expected_head='none');profile.write_text(json.dumps(data))
env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(r),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'),RAY_TRACING_MANAGED_MESH_HELPER=str(helper),RAY_TRACING_MANAGED_MESH_COMPILER='/usr/bin/true',OPTIC_L3_PROFILE=str(profile),OPTIC_L3_STL=str(stl))
with (out/'native.log').open('w') as log:
 result=subprocess.run([str(a.binary.resolve()),str(out),str(scene),'--l3-process'],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=60)
assert result.returncode==0,(result.returncode,out/'native.log')
print('PASS native pending-close/reopen:',out/'native.log')
