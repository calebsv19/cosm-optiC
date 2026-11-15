"""Plan fixed ordinary V3 camera/photon budgets from an independent crossed pilot.

Consumes verified regional result.json/roi.f64.gz captures, never renders or
mutates a scene. Predictions concern sampling variance, not bias or convergence.
"""
import argparse
import copy
import gzip
import hashlib
import itertools
import json
from pathlib import Path

import numpy as np
from spectral_error_budget import ROIS

SCHEMA = 'optic_v3_budget_plan_v1'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def controlled_scene(scene):
    value = copy.deepcopy(scene)
    p = value['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
    for key in ('samples', 'seed'):
        p.pop(key, None)
    for key in ('passes', 'seed'):
        p['caustics'].pop(key, None)
    return value


def crossed_components(values):
    """Balanced two-factor random effects; pixel axis is averaged, not replicated.

    Input [camera seed,map seed,region,pixel,XYZ]. Raw negative moment estimates
    remain visible. Nonnegative values are used only for allocation predictions.
    """
    x = np.asarray(values, dtype=float)
    if x.ndim != 5 or min(x.shape[:2]) < 3 or not np.isfinite(x).all():
        raise ValueError('Need finite 3x3 or larger crossed XYZ pilot')
    nc, nm = x.shape[:2]
    camera_mean, map_mean, mean = x.mean(1), x.mean(0), x.mean((0, 1))
    residual = x - camera_mean[:, None] - map_mean[None, :] + mean
    interaction = (residual**2).sum((0, 1)) / ((nc-1)*(nm-1))
    raw = np.stack((camera_mean.var(0, ddof=1)-interaction/nm,
                    map_mean.var(0, ddof=1)-interaction/nc, interaction)).mean(2)
    return raw


def fit_cost(rows):
    """Nonnegative setup + camera + photon fit; regional CPU, not wall time."""
    a = np.array([[1., r[0], r[1]] for r in rows])
    b = np.array([r[2] for r in rows])
    if len(rows) < 3 or np.linalg.matrix_rank(a) != 3 or not np.isfinite(b).all() or np.min(b) <= 0:
        raise ValueError('Need positive independent camera and photon cost calibration')
    candidates = []
    for count in (1, 2, 3):
        for columns in itertools.combinations(range(3), count):
            c = np.zeros(3); c[list(columns)] = np.linalg.lstsq(a[:, columns], b, rcond=None)[0]
            if (c >= 0).all():
                candidates.append((float(np.sum((a@c-b)**2)), c))
    error, c = min(candidates, key=lambda item: item[0])
    return dict(coefficients=c.tolist(), relative_rmse=float(np.sqrt(error/len(b))/b.mean()),
                measure='regional CPU seconds; setup + camera multiplier + photon multiplier')


def select_budget(raw, cost, camera_choices, photon_choices, target):
    if not 0 < target < 1:
        raise ValueError('Target must be a variance ratio strictly between zero and one')
    if any(x < 1 or not np.isfinite(x) for x in camera_choices+photon_choices):
        raise ValueError('Budget multipliers must be finite and at least one')
    raw = np.asarray(raw)
    if raw.ndim != 3 or raw.shape[0] != 3 or raw.shape[2] != 3 or not np.isfinite(raw).all():
        raise ValueError('Expected finite camera/map/interaction by region by XYZ components')
    positive = np.maximum(raw, 0)
    total = positive.sum(0)
    if np.any(total <= 0):
        raise ValueError('Unresolved zero-variance region/channel: choose a measurable pilot region')
    coefficients = np.array(cost['coefficients']); choices = []
    for c, p in itertools.product(camera_choices, photon_choices):
        ratio = (positive[0]/c + positive[1]/p + positive[2]/(c*p))/total
        choices.append(dict(camera_multiplier=c, photon_multiplier=p,
                            predicted_region_XYZ_ratio=ratio.tolist(),
                            worst_ratio=float(ratio.max()),
                            predicted_cpu_seconds=float(coefficients@[1, c, p]),
                            meets_target=bool(np.all(ratio <= target))))
    passing = [v for v in choices if v['meets_target']]
    selected = min(passing, key=lambda v: (v['predicted_cpu_seconds'], v['worst_ratio'])) if passing else None
    return dict(selected=selected, candidates=choices, status='predicted_target_met' if selected else 'budget_cap_insufficient',
                raw_signed_components=raw.tolist(), allocation_components=positive.tolist(),
                negative_estimates=int(np.sum(raw < 0)))


def region_spec(value):
    if not isinstance(value, dict) or not value:
        raise ValueError('Regions must be a nonempty name to [x,y,width,height] mapping')
    result = {}
    for name, box in value.items():
        if not isinstance(name, str) or not name or len(box) != 4 or any(type(v) is not int for v in box):
            raise ValueError('Invalid named integer region')
        x,y,w,h = box
        if min(x,y) < 0 or min(w,h) <= 0: raise ValueError('Invalid region extent')
        result[name] = [x,y,w,h]
    if len({v[2]*v[3] for v in result.values()}) != 1:
        raise ValueError('Crossed region captures require equal pixel counts per patch')
    return result


def pilot_pixels(root, record, regions):
    """Arbitrary named equal-area patches from ordinary full films, or packed audits."""
    pixels = next(iter(regions.values()))[2]*next(iter(regions.values()))[3]
    if record.get('regions') is False:
        from spectral_components import read
        image = root/'render/frames/frame_0000.bmp'
        path = Path(str(image)+'.components.npy')
        summary = json.loads((root/'summary.json').read_text())['spectral_transport']
        if digest(path) != summary['linear_components']['sha256']:
            raise ValueError('Full pilot film hash mismatch')
        width,height,values = read(path)
        film = np.array(values).reshape(height,width,11,3)
        if any(x+w>width or y+h>height for x,y,w,h in regions.values()):
            raise ValueError('Pilot region exceeds the full film')
        return np.array([film[y:y+h,x:x+w].reshape(pixels,11,3) for x,y,w,h in regions.values()])
    captured = region_spec(record.get('roi_regions', ROIS))
    if captured != regions:
        raise ValueError('Packed audit regions differ; use a matching capture or an ordinary full film')
    return np.frombuffer(gzip.decompress((root/'roi.f64.gz').read_bytes()),dtype='<f8').reshape(len(regions),pixels,11,3)


def plan(manifest_path):
    manifest_path = Path(manifest_path).resolve()
    m = json.loads(manifest_path.read_text())
    cameras, maps = m['camera_seeds'], m['map_seeds']
    if len(set(cameras)) != len(cameras) or len(set(maps)) != len(maps) or min(len(cameras), len(maps)) < 3:
        raise ValueError('Independent unique seeds required on each axis, at least three')
    regions = region_spec(m.get('regions', ROIS))
    base_c, base_p = m['baseline_camera_samples'], m['baseline_photon_passes']
    if min(base_c, base_p) < 1:
        raise ValueError('Invalid baseline budget')
    cells = {}; costs = []; identities = []; reference = None; binary = None; request_reference = None
    for location in m['records']:
        f = (manifest_path.parent/location).resolve(); d = json.loads(f.read_text()); root = f.parent
        if digest(root/'scene.json') != d['scene_sha256'] or digest(root/'roi.f64.gz') != d['roi_sha256']:
            raise ValueError('Pilot artifact hash mismatch')
        scene = json.loads((root/'scene.json').read_text()); normalized = controlled_scene(scene)
        request = json.loads((root/'request.json').read_text())
        for key in ('run_id', 'output'): request.pop(key, None)
        if request_reference is None: request_reference = request
        if request != request_reference: raise ValueError('Pilot request/camera drift')
        if reference is None: reference, binary = normalized, d['binary_sha256']
        if normalized != reference or binary != d['binary_sha256']:
            raise ValueError('Pilot scene/reconstruction/policy or binary drift')
        p = scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
        if (p['samples'],p['seed'],p['caustics']['passes'],p['caustics']['seed']) != (d['spp'],d['camera_seed'],d['map']['passes'],d['map']['seed']):
            raise ValueError('Pilot record/scene budget mismatch')
        if d.get('nonscattered_indirect', {}).get('surface_nee_pdf_mismatches', 0):
            raise ValueError('Unqualified light PDF route')
        a = pilot_pixels(root, d, regions)
        if not np.isfinite(a).all(): raise ValueError('Nonfinite pilot film')
        c, n = d['spp'], d['map']['passes']; key = (d['camera_seed'],d['map']['seed'])
        costs.append((c/base_c,n/base_p,d['cpu_seconds']))
        identities.append(dict(result=str(f), result_sha256=digest(f), roi_sha256=d['roi_sha256'], camera_seed=key[0], map_seed=key[1]))
        if c == base_c and n == base_p:
            if key in cells: raise ValueError('Duplicate crossed cell')
            cells[key] = a[:,:,0,:]
    expected = set(itertools.product(cameras,maps))
    if set(cells) != expected: raise ValueError('Missing or unexpected crossed pilot cell')
    if max(m['camera_multipliers']) > max(r[0] for r in costs) or max(m['photon_multipliers']) > max(r[1] for r in costs):
        raise ValueError('Requested grid exceeds measured axis calibration bounds')
    raw = crossed_components([[cells[c,p] for p in maps] for c in cameras])
    cost = fit_cost(costs)
    result = select_budget(raw,cost,m['camera_multipliers'],m['photon_multipliers'],m['target_variance_ratio'])
    if result['selected']:
        s=result['selected'];s['camera_samples']=int(base_c*s['camera_multiplier']);s['photon_passes']=int(base_p*s['photon_multiplier'])
        if s['camera_samples']!=base_c*s['camera_multiplier'] or s['photon_passes']!=base_p*s['photon_multiplier']:
            raise ValueError('Selected counts must be exact integers')
        s['photons']=s['photon_passes']*reference['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']['caustics']['photons_per_pass']
    result.update(schema=SCHEMA, regions=regions, region_order=list(regions), channels=['X','Y','Z'], component_order=['camera','photon_map','interaction'],
                  target_variance_ratio=m['target_variance_ratio'], baseline_camera_samples=base_c,baseline_photon_passes=base_p,
                  cost_model=cost, pilot_records=identities, controlled_scene=reference, controlled_request=request_reference, renderer_sha256=binary,
                  forbidden_final_camera_seeds=sorted({r['camera_seed'] for r in identities}),
                  forbidden_final_map_seeds=sorted({r['map_seed'] for r in identities}),
                  limits=['Fixed global budget; no adaptive stopping or pilot accumulation.',
                          'Inverse-count camera/map and product-interaction model is a prediction, not convergence proof.',
                          'Negative raw estimates preserved; allocation clips only the model terms to zero.',
                          'Regional XYZ constraints do not certify unmeasured pixels, bias, or perceptual color error.',
                          'Regional CPU fit is not a whole-frame timing estimate.'])
    return result


def validate_final(plan_value, scene):
    p=scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1'];selected=plan_value['selected']
    if selected is None: raise ValueError('Cannot execute an insufficient plan')
    if controlled_scene(scene)!=plan_value['controlled_scene']: raise ValueError('Final scene/reconstruction drift')
    if p['seed'] in plan_value['forbidden_final_camera_seeds'] or p['caustics']['seed'] in plan_value['forbidden_final_map_seeds']:
        raise ValueError('Final seeds must be independent of all pilot/calibration seeds')
    if (p['samples'],p['caustics']['passes'])!=(selected['camera_samples'],selected['photon_passes']):
        raise ValueError('Final counts differ from frozen plan')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pilot',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();result=plan(args.pilot)
    with args.output.open('x') as stream:json.dump(result,stream,indent=2,allow_nan=False)
    print(json.dumps({'status':result['status'],'selected':result['selected']},indent=2))

if __name__=='__main__':main()
