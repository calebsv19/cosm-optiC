#!/usr/bin/env python3
"""A valid asset library may carry unused solids; every byte and alias stays guarded."""
import argparse
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('scene','probe','pack-io','ball-tools','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
    source=json.loads(a.scene.read_bytes());ref=source['extensions']['ray_tracing']['rigid_result']
    bundle=(a.scene.parent/ref['bundle_path']).resolve()
    aset=json.loads((bundle/'input/assets/asset_set.json').read_bytes())
    used={b['collision_asset_id'] for b in source['extensions']['rigid_scene']['bindings']}
    unused=next(x for x in aset['assets'] if x['asset_id'] not in used)
    sys.path.insert(0,str(a.ball_tools));import rigid_result_store as store
    cache=json.loads(subprocess.check_output([str(a.pack_io),'read',str(bundle/'poses.pack')]))
    env=dict(os.environ,XDG_CONFIG_HOME=str(a.out/'config'),RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled',PYTHONDONTWRITEBYTECODE='1')
    def probe(*argv):
        r=subprocess.run([str(a.probe),*map(str,argv)],env=env,capture_output=True,text=True,timeout=60)
        assert r.returncode==0,(argv,r.stdout,r.stderr)
        return r.stdout
    probe(a.scene,0);checks=['unused verified asset accepted']
    for name in ('corrupt-unused','duplicate-id','duplicate-root'):
        target=a.out/name;shutil.copytree(bundle,target)
        scene=copy.deepcopy(source);scene['extensions']['ray_tracing']['rigid_result']['bundle_path']=str(target)
        if name=='corrupt-unused':
            path=target/'input/assets'/unused['root']/'visible.json';path.write_bytes(path.read_bytes()+b'\n')
        else:
            setpath=target/'input/assets/asset_set.json';value=json.loads(setpath.read_bytes())
            row=next(x for x in value['assets'] if x['asset_id']==unused['asset_id'])
            other=next(x for x in value['assets'] if x['asset_id'] in used)
            row['asset_id' if name=='duplicate-id' else 'root']=other['asset_id' if name=='duplicate-id' else 'root']
            setpath.write_text(json.dumps(value)+'\n')
            # Reauthenticate inventory so alias checks, rather than just a stale
            # digest, are the tested refusal boundary.
            (target/'manifest.json').unlink() # test-owned copy, not the accepted bundle
            scene['extensions']['ray_tracing']['rigid_result']['result_revision']=store.seal(target,cache)
        path=a.out/(name+'.json');path.write_text(json.dumps(scene)+'\n')
        assert '"rollback":true' in probe(a.scene,'--replacement',path)
        checks.append(name+' rejected; previous playback preserved')
    report={'ok':True,'checks':checks};(a.out/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))


if __name__=='__main__':main()
