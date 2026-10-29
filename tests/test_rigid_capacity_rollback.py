#!/usr/bin/env python3
"""Refusing a 33-body replacement must preserve a loaded 32-body result."""
import argparse,copy,hashlib,json,os,subprocess
from pathlib import Path


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ['consumer','consumer-sanitizer','scene','out']:ap.add_argument('--'+name,type=Path,required=True)
    a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=False)
    original=a.scene.read_bytes();scene=json.loads(original)
    assert len(scene['extensions']['rigid_scene']['bindings'])==32
    obj=copy.deepcopy(scene['objects'][-1]);binding=copy.deepcopy(scene['extensions']['rigid_scene']['bindings'][-1])
    obj['object_id']=binding['object_id']='capacity_over_33';binding['body_id']=9999
    obj['transform']['position']['x']+=20
    scene['objects'].append(obj);scene['extensions']['rigid_scene']['bindings'].append(binding)
    # Keep references in their ordinary contained root. Never replace the good scene.
    rejected=a.scene.parent/'.capacity-33-rejected.json'
    with rejected.open('x') as f:json.dump(scene,f)
    for exe in [a.consumer,a.consumer_sanitizer]:
        p=subprocess.run([str(exe),str(a.scene),'--replacement',str(rejected)],capture_output=True,
                         env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',
                                  XDG_CONFIG_HOME=str(a.out/'config'),RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled'),timeout=180)
        assert p.returncode==0 and b'"rollback":true' in p.stdout and not p.stderr,(exe,p.returncode,p.stdout[-500:],p.stderr[-1000:])
    assert a.scene.read_bytes()==original
    rejected.unlink()
    (a.out/'verification.json').write_text(json.dumps({'ok':True,'normal_and_sanitizer':True,
        'loaded32_preserved_after33_refusal':True,'scene_sha256':hashlib.sha256(original).hexdigest()},indent=2)+'\n')


if __name__=='__main__':main()
