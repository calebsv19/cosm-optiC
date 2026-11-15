"""Execute frozen V3 budget plans with independent seeds and exact linear exports.

One invocation renders one immutable ordinary job. Region mode is explicitly a
qualification harness; default is the normal full-frame headless renderer.
"""
import argparse
import copy
import gzip
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time

import numpy as np
from spectral_budget_planner import digest, validate_final, region_spec
from spectral_components import verify, read
from spectral_error_budget import ROIS
from spectral_region_validation import verify_regions


def write(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False))


def render(cli, plan_path, base, output, camera_seed, map_seed, budget='planned', regions=False, timeout=1800):
    if not 0 < timeout <= 86400: raise ValueError('Timeout must be in (0,86400] seconds')
    cli, plan_path, base, output = map(lambda p: Path(p).resolve(), (cli,plan_path,base,output))
    plan = json.loads(plan_path.read_text())
    if plan.get('selected') is None:
        raise ValueError('Cannot execute an insufficient plan; retain its target and report the budget cap')
    roi_regions = region_spec(plan.get('regions', ROIS))
    if regions and roi_regions != region_spec(ROIS):
        raise ValueError('This region harness only supports its fixed audit patches; use full-frame mode')
    # Start with the frozen pilot's complete policy; original base supplies only
    # external resources and ordinary request/camera, never replacement settings.
    scene = copy.deepcopy(plan['controlled_scene'])
    p = scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
    p.update(samples=plan['selected']['camera_samples'], seed=camera_seed)
    p['caustics'].update(passes=plan['selected']['photon_passes'], seed=map_seed)
    validate_final(plan,scene)
    if budget=='baseline':
        p['samples']=plan['baseline_camera_samples'];p['caustics']['passes']=plan['baseline_photon_passes']
    elif budget!='planned':raise ValueError('Unknown budget')
    request=json.loads((base/'request.json').read_text())
    # The plan captures the scene but not the external request. Bind the latter
    # to the pilot request from the exact first recorded input identity.
    pilot_request=copy.deepcopy(plan['controlled_request'])
    for q in (request,pilot_request):
        q.pop('run_id',None);q.pop('output',None)
    if request!=pilot_request:raise ValueError('Request camera/render control drift')
    settings=request.get('render',{})
    if 'width' in settings and 'height' in settings and any(x+w>settings['width'] or y+h>settings['height'] for x,y,w,h in roi_regions.values()):
        raise ValueError('Final region exceeds requested film')
    request.update(run_id=output.name,output={'root':'render','overwrite':False})
    request['scene']['runtime_scene_path']='scene.json'
    identity=dict(plan_sha256=digest(plan_path),binary_sha256=digest(cli),camera_seed=camera_seed,map_seed=map_seed,budget=budget,regions=regions)
    if (output/'result.json').exists():
        d=json.loads((output/'result.json').read_text())
        if any(d[k]!=v for k,v in identity.items()) or json.loads((output/'scene.json').read_text())!=scene or json.loads((output/'request.json').read_text())!=request:
            raise ValueError('Existing result identity drift')
        if digest(output/'roi.f64.gz')!=d['roi_sha256'] or digest(output/'scene.json')!=d['scene_sha256']:raise ValueError('Existing output hash drift')
        image=output/'render/frames/frame_0000.bmp'
        summary=json.loads((output/'summary.json').read_text())['spectral_transport']
        if digest(image)!=d['image_sha256'] or digest(Path(str(image)+'.components.npy'))!=summary['linear_components']['sha256']:
            raise ValueError('Existing full film/image hash drift')
        return d
    if output.exists():raise ValueError('Unfinished identity: inspect before retry '+str(output))
    output.mkdir(parents=True)
    for n in ('assets','config'):(output/n).symlink_to(base/n,target_is_directory=True)
    write(output/'scene.json',scene);write(output/'request.json',request);start=time.monotonic()
    with (output/'render.log').open('w') as log,(output/'cpu.log').open('w') as cpu:
        child=subprocess.Popen(['/usr/bin/time','-p',str(cli),'--request','request.json','--render','--summary','summary.json','--summary-file-only'],cwd=output,stdout=log,stderr=cpu,start_new_session=True)
        write(output/'process.json',dict(pid=child.pid,started_unix=time.time(),timeout_seconds=timeout))
        try:code=child.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid,signal.SIGTERM)
            try:child.wait(timeout=5)
            except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait()
            write(output/'failure.json',dict(reason='bounded timeout',terminated_group=child.pid));raise
    if code:raise RuntimeError(f'Render failed {code}; inspect {output}')
    report=json.loads((output/'summary.json').read_text())['spectral_transport'];image=output/'render/frames/frame_0000.bmp'
    verification=verify_regions(report,image,ROIS) if regions else verify(report,image)
    w,h,values=read(str(image)+'.components.npy');a=np.array(values).reshape(h,w,11,3)
    if not regions and any(x+dx>w or y+dy>h for x,y,dx,dy in roi_regions.values()):
        raise ValueError('Final region exceeds film')
    selected=a.reshape(6,64,11,3) if regions else np.array([a[y:y+dy,x:x+dx].reshape(-1,11,3) for x,y,dx,dy in roi_regions.values()])
    with gzip.open(output/'roi.f64.gz','wb') as stream:stream.write(selected.astype('<f8').tobytes())
    times={k:float(v) for k,v in re.findall(r'^(real|user|sys)\s+([0-9.]+)',(output/'cpu.log').read_text(),re.M)}
    if report['nonscattered_indirect']['surface_nee_pdf_mismatches'] or not report['termination']['all_started_paths_classified']:raise ValueError('Transport diagnostics failed')
    result={**identity,'spp':p['samples'],'passes':p['caustics']['passes'],'seconds':time.monotonic()-start,'cpu_seconds':times['user']+times['sys'],'scene_sha256':digest(output/'scene.json'),'roi_sha256':digest(output/'roi.f64.gz'),'image_sha256':digest(image),'roi_regions':roi_regions,'verification':verification,'film':report['film'],'map':report['smooth_glass']}
    write(output/'result.json',result);print(output.name,round(result['seconds'],2),'seconds',flush=True);return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for n in ('cli','plan','base','output'):parser.add_argument('--'+n,type=Path,required=True)
    parser.add_argument('--camera-seed',type=int,required=True);parser.add_argument('--map-seed',type=int,required=True)
    parser.add_argument('--timeout-seconds',type=int,default=1800)
    parser.add_argument('--budget',choices=('baseline','planned'),default='planned');parser.add_argument('--regions',action='store_true')
    args=parser.parse_args();render(args.cli,args.plan,args.base,args.output,args.camera_seed,args.map_seed,args.budget,args.regions,args.timeout_seconds)

if __name__=='__main__':main()
