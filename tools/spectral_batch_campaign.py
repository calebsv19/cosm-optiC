"""Run a bounded independent V3 still campaign and accumulate completed jobs.

Use an executable frozen budget plan (including an explicitly labeled diagnostic
cap probe). All jobs use its same reconstruction and per-map budget. Completed
jobs can be reused with --resume; unfinished/uncertain jobs are never resubmitted.
"""
import argparse
import json
import os
from pathlib import Path
import time

from spectral_batch_accumulate import accumulate, digest, resource_identity, validate_resource_paths, MAX_BATCHES
from spectral_budget_qualification import render


def run(cli,plan,base,output,camera_seeds,map_seeds,timeout=7200,job_timeout=1800,resume=False):
    cli,plan,base,output=map(lambda x:Path(x).resolve(),(cli,plan,base,output))
    n=len(camera_seeds)
    if n!=len(map_seeds) or not 2<=n<=MAX_BATCHES:
        raise ValueError('Need 2..64 matched independent camera/map seeds')
    for seeds in (camera_seeds,map_seeds):
        if len(set(seeds))!=n or any(type(s) is not int or not 0<=s<=0xffffffff for s in seeds):
            raise ValueError('Seeds must be distinct uint32 integers in each stream')
    if not 0<timeout<=86400 or not 0<job_timeout<=timeout:
        raise ValueError('Campaign/job wall bounds must be positive, ordered and <=86400 s')
    p=json.loads(plan.read_text())
    if p.get('selected') is None:raise ValueError('An executable budget plan is required')
    identity=dict(schema='optic_v3_batch_campaign_v1',binary_sha256=digest(cli),plan_sha256=digest(plan),
        base_request_sha256=digest(base/'request.json'),resources=resource_identity(base),
        camera_seeds=camera_seeds,map_seeds=map_seeds,timeout_seconds=timeout,job_timeout_seconds=job_timeout)
    validate_resource_paths(p['controlled_scene'],identity['resources'])
    if output.exists():
        if not resume or not (output/'campaign.json').is_file() or json.loads((output/'campaign.json').read_text())!=identity:
            raise ValueError('Existing campaign requires exact identity and --resume')
    else:
        output.mkdir(parents=True)
        (output/'campaign.json').write_text(json.dumps(identity,indent=2)+'\n')
    lock=output/'active.lock'
    fd=os.open(lock,os.O_CREAT|os.O_EXCL|os.O_WRONLY,0o600)
    os.write(fd,str(os.getpid()).encode());os.close(fd)
    start=time.monotonic()
    try:
        paths=[]
        for i,(cs,ms) in enumerate(zip(camera_seeds,map_seeds)):
            remaining=timeout-(time.monotonic()-start)
            if remaining<=0:raise TimeoutError('Campaign wall budget exhausted; completed jobs retained')
            # This owner refuses any existing incomplete job, including after interruption.
            job=output/f'batch-{i:02}'
            render(cli,plan,base,job,cs,ms,timeout=min(job_timeout,remaining))
            paths.append(job/'result.json')
        if digest(cli)!=identity['binary_sha256'] or digest(plan)!=identity['plan_sha256'] or digest(base/'request.json')!=identity['base_request_sha256'] or resource_identity(base)!=identity['resources']:
            raise ValueError('Campaign input drift')
        target=output/'accumulated'
        if target.exists():
            manifest=json.loads((target/'manifest.json').read_text())
            if not manifest.get('complete') or [v['path'] for v in manifest['input_records']]!=list(map(str,paths)) or any(digest(v['path'])!=v['sha256'] for v in manifest['input_records']) or any(digest(target/k)!=v['sha256'] for k,v in manifest['outputs'].items()):
                raise ValueError('Accumulated result identity drift')
            for record in manifest['input_records']:
                if any(digest(path)!=sha for path,sha in record['files'].items()) or resource_identity(Path(record['path']).parent)!=record['resources']:
                    raise ValueError('Accumulated input identity drift')
            return manifest
        return accumulate(paths,target)
    finally:
        lock.unlink()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cli','plan','base','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--camera-seeds',type=int,nargs='+',required=True)
    p.add_argument('--map-seeds',type=int,nargs='+',required=True)
    p.add_argument('--timeout-seconds',type=int,default=7200)
    p.add_argument('--job-timeout-seconds',type=int,default=1800)
    p.add_argument('--resume',action='store_true')
    a=p.parse_args();r=run(a.cli,a.plan,a.base,a.output,a.camera_seeds,a.map_seeds,a.timeout_seconds,a.job_timeout_seconds,a.resume)
    print(json.dumps({k:r[k] for k in ('batches','camera_samples','summed_render_cpu_seconds')},indent=2))

if __name__=='__main__':main()
