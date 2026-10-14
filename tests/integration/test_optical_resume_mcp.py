"""Supervise a killed isolated native editor and restart the same executable."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output-root',type=Path,required=True);p.add_argument('--binary',type=Path,required=True);args=p.parse_args();out=args.output_root.resolve();binary=args.binary.resolve();env=dict(os.environ,OPTIC_TEST_NATIVE_BINARY=str(binary),OPTIC_RESUME_PHASE='kill')
with out.parent.joinpath(out.name+'-death.log').open('w') as log:
 first=subprocess.run(['python3','-B',str(ROOT/'tests/integration/test_optical_matrices_mcp.py'),'--output-root',str(out),'--binary',str(binary),'--check-script',str(ROOT/'tests/integration/check_optical_resume_mcp.py')],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert first.returncode!=0 and (out/'editor-kill.json').exists(),first.returncode
# Allow the killed editor's owned MCP client to finish its error journal/EOF.
import time
for i in range(100):
 receipt=json.loads((out/'editor-death/matrix.json').read_text())
 if receipt['state']=='failed':break
 time.sleep(.1)
else:raise AssertionError('killed editor journal not published')
env.update(OPTIC_RESUME_PHASE='recover',RAY_TRACING_PROGRAM_ROOT=str(ROOT),RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'),OPTIC_MATRIX_MCP_TEST=str(ROOT/'tests/integration/check_optical_resume_mcp.py'))
with (out/'recovery-native.log').open('w') as log:
 second=subprocess.run([str(binary),str(out),str(out/'scene_runtime.json'),'--optical-matrices'],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=480)
assert second.returncode==0,(second.returncode,out/'recovery-native.log')
env['OPTIC_RESUME_PHASE']='aux'
with (out/'auxiliary-native.log').open('w') as log:
 third=subprocess.run([str(binary),str(out),str(out/'scene_runtime.json'),'--optical-matrices'],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=480)
assert third.returncode==0,(third.returncode,out/'auxiliary-native.log')
result=json.loads((out/'resume-acceptance.json').read_text());(out/'acceptance.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
