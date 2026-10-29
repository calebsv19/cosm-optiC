#!/usr/bin/env python3
"""Frozen rigid dependency custody and refusal tests; no solver is substituted."""
import argparse,copy,json,shutil,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from scene_workflow_bundle import contained,read,sha,snapshot_inputs
from scene_workflow_rigid_assets import rigid_dependencies


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--parent',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=False)
    original=read(a.parent/'scene.json');deps=rigid_dependencies(original,a.parent,contained,read,sha);assert len(deps)>20
    request=a.out/'request.json';request.write_text(json.dumps({'schema_version':'ray_tracing_agent_render_request_v1','render':{'width':64,'height':64,'frame_count':1,'temporal_frames':1}}))
    target=a.out/'snapshot';snapshot_inputs(a.parent/'scene.json',request,target)
    assert all(sha(target/rel)==sha(path) for rel,path in deps.items())
    checks=['declared_rigid_bytes_preserved','historical_source_paths_not_followed']
    for name,change in [('wrong_revision',{'result_revision':'0'*64}),('escape',{'bundle_path':'../outside'}),('absolute',{'bundle_path':str(a.parent/'result')}),('reserved',{'bundle_path':'config'})]:
        s=copy.deepcopy(original);s['extensions']['ray_tracing']['rigid_result'].update(change)
        try:rigid_dependencies(s,a.parent,contained,read,sha)
        except ValueError:checks.append(name+'_refused')
        else:raise AssertionError(name)
    # Mutation is confined to our copied snapshot, never the retained producer.
    s=read(target/'scene_runtime.json');base=target/s['extensions']['ray_tracing']['rigid_result']['bundle_path'];pack=base/'poses.pack'
    with pack.open('ab') as f:f.write(b'x')
    try:rigid_dependencies(s,target,contained,read,sha)
    except ValueError:checks.append('changed_artifact_refused')
    else:raise AssertionError('changed pack accepted')
    pack.unlink();pack.symlink_to(a.parent/'result/poses.pack')
    try:rigid_dependencies(s,target,contained,read,sha)
    except ValueError:checks.append('symlink_refused')
    else:raise AssertionError('symlink accepted')
    assert all(sha(path)==sha(a.parent/rel) for rel,path in deps.items())
    (a.out/'verification.json').write_text(json.dumps({'ok':True,'checks':checks,'files_preserved':len(deps)},indent=2)+'\n');print(len(checks),'rigid snapshot checks passed')


if __name__=='__main__':main()
