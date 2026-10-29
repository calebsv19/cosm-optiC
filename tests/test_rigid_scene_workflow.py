#!/usr/bin/env python3
"""BR5 ordinary CLI success, continuation, portability and refusal boundaries."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('scene','assets','backend','pack-io','ball-tool','renderer','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
    source=a.scene.read_bytes();checks=[]
    tool=Path(__file__).resolve().parents[1]/'tools/rigid_scene_workflow.py'
    env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',XDG_CONFIG_HOME=str(a.out/'config'),
             RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled',
             RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT=str(a.out/'cache'))
    common=['--ball-tool',a.ball_tool,'--pack-io',a.pack_io,'--renderer',a.renderer]
    def call(op,name,extra,ok=True,scene=a.scene):
        cmd=[sys.executable,'-B',tool,op,'--scene',scene,'--output',a.out/name]+common+extra
        r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,env=env,timeout=100)
        assert (r.returncode==0)==ok,(name,r.stdout,r.stderr)
        result=json.loads(r.stdout);assert result['ok']==ok,(name,result)
        (a.out/(name+'-receipt.json')).write_text(json.dumps(result,indent=2)+'\n')
        if not ok:assert not (a.out/name).exists(),name
        checks.append(name);return result
    run=['--backend',a.backend,'--assets',a.assets,'--store',a.out/'store','--ticks','180']
    first=call('run','first',run+['--expected-head','none'])
    revision=first['result_revision'];bundle=a.out/'first/result'
    assert first['native_replay'] and not first['producer_reused']
    prepared=json.loads((a.out/'first/scene.json').read_text());original=json.loads(source)
    assert prepared['extensions']['rigid_scene']==original['extensions']['rigid_scene']
    for before,after in zip(original['objects'],prepared['objects']):
        assert before['transform']==after['transform']
        assert (a.out/'first'/after['extensions']['line_drawing']['runtime_mesh_path']).is_file()
    selected=call('select','selected',['--bundle',bundle,'--expected-revision',revision])
    assert not selected['native_replay']
    again=call('run','reuse',run+['--expected-head',revision])
    assert again['result_revision']==revision and again['producer_reused'] and not again['native_replay']
    head=(a.out/'store/head.json').read_bytes()
    call('run','stale-head',run+['--expected-head','none'],False)
    changed=copy.deepcopy(original);changed['extensions']['rigid_scene']['bindings'][-1]['mass_kg']=3
    changed_path=a.out/'changed.json';changed_path.write_text(json.dumps(changed))
    call('select','changed-physical',['--bundle',bundle,'--expected-revision',revision],False,changed_path)
    unsupported=copy.deepcopy(original);unsupported['extensions']['rigid_scene']['bindings'][-1]['role']='kinematic'
    unsupported_path=a.out/'unsupported.json';unsupported_path.write_text(json.dumps(unsupported))
    call('run','unsupported',run+['--expected-head',revision],False,unsupported_path)
    call('select','wrong-revision',['--bundle',bundle,'--expected-revision','0'*64],False)
    # Corruption must not be adopted or damage the previous accepted store.
    shutil.copytree(bundle,a.out/'corrupt');pack=a.out/'corrupt/poses.pack';raw=bytearray(pack.read_bytes());raw[-1]^=1;pack.write_bytes(raw)
    call('select','corrupt-bundle',['--bundle',a.out/'corrupt','--expected-revision',revision],False)
    # The output gate preserves arbitrary existing caller data.
    existing=a.out/'existing';existing.mkdir();(existing/'owned.txt').write_text('preserve')
    cmd=[sys.executable,'-B',tool,'select','--scene',a.scene,'--output',existing]+common+['--bundle',bundle,'--expected-revision',revision]
    result=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,env=env,timeout=100)
    assert result.returncode==2 and (existing/'owned.txt').read_text()=='preserve';checks.append('existing-output-preserved')
    assert a.scene.read_bytes()==source and (a.out/'store/head.json').read_bytes()==head
    assert not list(a.out.glob('.rigid-prepare-*'))
    # Move the deliverable; all scene/result/mesh references must remain usable.
    moved=a.out/'relocated';os.rename(a.out/'selected',moved)
    request={'schema_version':'ray_tracing_agent_render_request_v1','run_id':'br5_relocated',
             'scene':{'runtime_scene_path':str(moved/'scene.json')},'volume':{'enabled':False},
             'render':{'start_frame':154,'frame_count':1,'width':32,'height':32,'temporal_frames':181},
             'output':{'root':str(a.out/'relocated-preflight'),'overwrite':False}}
    request_path=a.out/'relocated-request.json';request_path.write_text(json.dumps(request))
    r=subprocess.run([str(a.renderer),'--request',str(request_path),'--preflight'],capture_output=True,text=True,env=env,timeout=60)
    assert r.returncode==0,r.stdout+r.stderr;checks.append('relocated-scene-admission')
    result={'ok':True,'checks':checks,'result_revision':revision,'source_sha256':hashlib.sha256(source).hexdigest()}
    (a.out/'verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));return 0


if __name__=='__main__':raise SystemExit(main())
