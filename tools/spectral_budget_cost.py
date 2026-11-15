"""Calibrate full-frame camera/photon CPU cost and check independent holdouts.

This keeps full-frame timing separate from regional noise calibration. A small
timing study reports observed prediction errors, not timing confidence bounds.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from spectral_budget_planner import controlled_scene, digest
from spectral_budget_precision import nnls_small


def calibrate(reference_path, training, validation):
    ref=json.loads(Path(reference_path).read_text())
    binary=None;seen=set();records=[]
    for role,paths in [('training',training),('validation',validation)]:
        for location in paths:
            f=Path(location).resolve();root=f.parent
            if f in seen:raise ValueError('Duplicate cost record')
            seen.add(f);d=json.loads(f.read_text())
            if d.get('regions') is not False:raise ValueError('Full-frame timing records required')
            if digest(root/'scene.json')!=d['scene_sha256'] or digest(root/'roi.f64.gz')!=d['roi_sha256']:
                raise ValueError('Cost record artifact drift')
            image=root/'render/frames/frame_0000.bmp'
            summary=json.loads((root/'summary.json').read_text())['spectral_transport']
            if digest(image)!=d['image_sha256'] or digest(Path(str(image)+'.components.npy'))!=summary['linear_components']['sha256']:
                raise ValueError('Full-frame film identity drift')
            scene=json.loads((root/'scene.json').read_text());request=json.loads((root/'request.json').read_text())
            for key in ('run_id','output'):request.pop(key,None)
            if controlled_scene(scene)!=ref['controlled_scene'] or request!=ref['controlled_request']:
                raise ValueError('Full-frame scene/camera/reconstruction drift')
            if binary is None:binary=d['binary_sha256']
            if d['binary_sha256']!=binary:raise ValueError('Full-frame executable drift')
            if summary['nonscattered_indirect']['surface_nee_pdf_mismatches'] or not summary['termination']['all_started_paths_classified']:
                raise ValueError('Unqualified full-frame transport')
            p=scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
            if (p['samples'],p['seed'],p['caustics']['passes'],p['caustics']['seed'])!=(d['spp'],d['camera_seed'],d['map']['passes'],d['map']['seed']):
                raise ValueError('Full-frame budget/seed identity drift')
            if not np.isfinite(d['cpu_seconds']) or d['cpu_seconds']<=0:raise ValueError('Invalid CPU cost')
            records.append(dict(role=role,path=str(f),sha256=digest(f),camera_samples=d['spp'],photon_passes=d['map']['passes'],camera_seed=d['camera_seed'],map_seed=d['map']['seed'],cpu_seconds=d['cpu_seconds']))
    train=[r for r in records if r['role']=='training'];test=[r for r in records if r['role']=='validation']
    if len(train)<4 or len(test)<2:raise ValueError('Need four training corners and two independent holdout records')
    for key in ('camera_seed','map_seed'):
        if {r[key] for r in train}&{r[key] for r in test}:raise ValueError('Cost holdout seeds overlap training')
    base_c,base_p=ref['baseline_camera_samples'],ref['baseline_photon_passes']
    def row(r):
        c,p=r['camera_samples']/base_c,r['photon_passes']/base_p
        return [1,c,p,c*p]
    a=np.array([row(r) for r in train]);b=np.array([r['cpu_seconds'] for r in train]);x=np.array([row(r) for r in test]);y=np.array([r['cpu_seconds'] for r in test])
    if np.linalg.matrix_rank(a)!=4:raise ValueError('Combined full-frame cost is not identifiable')
    corners={(c,p) for c in (a[:,1].min(),a[:,1].max()) for p in (a[:,2].min(),a[:,2].max())}
    if not corners <= set(map(tuple,a[:,1:3])):raise ValueError('Full-frame calibration must include the bounding budget corners')
    if np.any(x[:,1:3]<a[:,1:3].min(0)) or np.any(x[:,1:3]>a[:,1:3].max(0)):
        raise ValueError('Holdout budget outside measured full-frame axes')
    models={}
    for name,n in [('additive',3),('joint',4)]:
        coeff=nnls_small(a[:,:n],b);pred=x[:,:n]@coeff;error=(pred-y)/y
        models[name]=dict(coefficients=coeff.tolist(),validation_predictions=pred.tolist(),validation_relative_errors=error.tolist(),relative_rmse=float(np.sqrt(np.mean(error**2))),all_within_20_percent=bool(np.all(abs(error)<=.20)))
    return dict(schema='optic_v3_full_frame_cost_v1',reference_sha256=digest(reference_path),renderer_sha256=binary,records=records,models=models,
                baseline_camera_samples=base_c,baseline_photon_passes=base_p,
                training_seed_pairs=len({(r['camera_seed'],r['map_seed']) for r in train}),
                confidence_bounds_available=False,
                limits=['CPU seconds on this host and exact full-frame scene only; no wall-time guarantee.',
                        'Validation records are not fitted; two holdouts do not establish timing confidence.',
                        'No scene modification or automatic time-budget promotion.'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference',type=Path,required=True)
    p.add_argument('--training',type=Path,nargs='+',required=True)
    p.add_argument('--validation',type=Path,nargs='+',required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();result=calibrate(a.reference,a.training,a.validation)
    with a.output.open('x') as stream:json.dump(result,stream,indent=2,allow_nan=False)
    print(json.dumps(result['models'],indent=2))


if __name__=='__main__':main()
