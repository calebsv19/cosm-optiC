#!/usr/bin/env python3
"""Real consumer refusal before BR2/BR4; no simulation or render is accepted."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-ball',required=True,type=Path)
    parser.add_argument('--renderer',required=True,type=Path)
    parser.add_argument('--ball-root',required=True,type=Path)
    parser.add_argument('--out',required=True,type=Path)
    args=parser.parse_args();args.out.mkdir(exist_ok=False)
    ray=Path(__file__).resolve().parents[1]
    sys.path.insert(0,str(args.ball_root/'tools'))
    import ball_scene_bundle as bundle
    checks=[]
    source=args.out/'finite_source'
    shutil.copytree(args.ball_root/'tests/fixtures/central_scene_c1_c2',source)
    author=source/'scene_authoring.json';document=json.loads(author.read_text())
    document['extensions']['rigid_scene']={'schema':'core_rigid_scene_v1'}
    author.write_text(json.dumps(document));before=hashlib.sha256(author.read_bytes()).hexdigest()
    bundle.compile_scene(args.native_ball,author,args.out/'compiled')
    owner_out=args.out/'owner';owner_out.mkdir(mode=0o700)
    result=subprocess.run([str(args.native_ball),'--collision-scene-owner',str(args.ball_root/'assets'),str(args.out/'compiled/scene_runtime.json'),str(owner_out),'inspect'],input='{"op":"session","args":{}}\n',capture_output=True,text=True,timeout=10)
    reply=json.loads(result.stdout)
    assert result.returncode==1 and not reply['ok'] and reply['error']=='rigid_scene_execution_unsupported_use_contract_inspection',result
    assert not list(owner_out.iterdir())
    assert hashlib.sha256(author.read_bytes()).hexdigest()==before
    checks.append('Ball native refuses generalized declaration without advancing or publishing')
    request=json.loads((ray/'tests/fixtures/agent_render_preflight_request.json').read_text())
    runtime=ray/'config/samples/ps4d_runtime_scene_visual_test.json'
    scene=json.loads(runtime.read_text());scene.setdefault('extensions',{})['rigid_scene']={'schema':'core_rigid_scene_v1'}
    derived=args.out/'guard_scene_runtime.json';derived.write_text(json.dumps(scene))
    for preflight in [True,False]:
        case=args.out/('preflight' if preflight else 'render');case.mkdir()
        request['run_id']='br1_guard_'+case.name;request['scene']={'runtime_scene_path':str(derived)}
        request['output']={'root':str(case/'output'),'overwrite':False}
        request['progress']={'summary_path':str(case/'summary.json'),'progress_path':str(case/'progress.json')}
        path=case/'request.json';path.write_text(json.dumps(request))
        command=[str(args.renderer),'--request',str(path)]+(['--preflight'] if preflight else [])
        result=subprocess.run(command,capture_output=True,text=True,timeout=30)
        (case/'run.log').write_text(result.stdout+result.stderr)
        diagnostic=result.stdout+result.stderr+(case/'summary.json').read_text() if (case/'summary.json').exists() else result.stdout+result.stderr
        assert result.returncode!=0 and ('rigid_scene execution' in diagnostic or 'rigid result' in diagnostic),(case,result.returncode,diagnostic[-2000:])
        assert not list(case.rglob('*.bmp'))
        checks.append('Ray '+case.name+' refuses before frames are produced')
    report={'status':'PASS','checks':checks};(args.out/'proof.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))


if __name__=='__main__':
    main()
