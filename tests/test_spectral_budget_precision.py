"""Absolute-noise, independent-seed and joint-cost regression contracts."""
import copy
import gzip
import json
from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from spectral_budget_precision import joint_cost, noise_bounds, choose_absolute, absolute_plan
from spectral_budget_planner import digest


class PrecisionTests(unittest.TestCase):
    def setUp(self):
        self.budgets=[(c,p) for c in (1,2,4) for p in (1,2,4)]

    def test_joint_term_identified_and_validated(self):
        t=np.array([2+3*c+5*p+7*c*p for c,p in self.budgets])
        m=joint_cost(self.budgets,np.tile(t,(16,1)))
        np.testing.assert_allclose(m['joint']['coefficients'],[2,3,5,7],atol=1e-10)
        self.assertLess(m['joint']['heldout_relative_rmse'],1e-12)
        self.assertTrue(m['interaction_improves_holdout'])

    def test_additive_remains_available(self):
        t=np.array([2+3*c+5*p for c,p in self.budgets])
        m=joint_cost(self.budgets,np.tile(t,(16,1)))
        self.assertLess(m['additive']['heldout_relative_rmse'],1e-12)
        self.assertLess(m['joint']['coefficients'][3],1e-10)

    def test_invalid_cost_domain(self):
        for b,t in [(self.budgets,np.ones((7,9))), ([(1,p) for p in range(1,10)],np.ones((16,9))),
                    (self.budgets,np.zeros((16,9))), (self.budgets[:-1]+[self.budgets[0]],np.ones((16,9)))]:
            with self.assertRaises(ValueError):joint_cost(b,t)

    def test_noise_scale_and_no_pixel_pseudoreplication(self):
        rng=np.random.default_rng(71);x=rng.normal(size=(32,2,3,1,3))
        a=noise_bounds(x,.02,resamples=1024);b=noise_bounds(np.repeat(x,64,axis=3),.02,resamples=1024)
        np.testing.assert_allclose(a['log_variance_standard_error'],b['log_variance_standard_error'],rtol=1e-12)
        self.assertTrue(np.all(np.array(b['rms_upper'])>=a['rms_upper']))
        c=noise_bounds(x,.04,resamples=1024)
        np.testing.assert_allclose(c['rms_upper'],np.array(a['rms_upper'])*2,rtol=1e-12)
        np.testing.assert_allclose(a['variance'],x.var(0,ddof=1).mean(-2)*.02**2,rtol=1e-12)

    def test_absolute_target_uses_upper_bound_and_all_channels(self):
        b=[(1,1),(4,4)];cost={'joint':{'predictions':[1,4]},'measured_cpu_mean':[1,4]}
        bounds={'rms':[[[.01,.01,.01]],[[.005,.005,.005]]], 'rms_upper':[[[.02,.02,.04]],[[.01,.01,.01]]]}
        r=choose_absolute(b,cost,bounds,.025);self.assertEqual(r['selected']['camera_multiplier'],4)
        self.assertIsNone(choose_absolute(b,cost,bounds,.001)['selected'])
        with self.assertRaises(ValueError):choose_absolute(b,cost,bounds,0)

    def test_noise_degenerate_and_nonfinite_refuse(self):
        for x in [np.ones((16,1,1,2,3)),np.ones((15,1,1,2,3)),np.full((16,1,1,2,3),np.nan)]:
            with self.assertRaises(ValueError):noise_bounds(x,1,resamples=1024)

    def test_more_independent_seeds_shrink_uncertainty(self):
        rng=np.random.default_rng(19)
        ratios=[]
        for n in (16,128):
            x=rng.normal(size=(n,1,1,64,3));b=noise_bounds(x,1,resamples=1024)
            ratios.append(np.max(np.array(b['rms_upper'])/b['rms']))
        self.assertLess(ratios[1],ratios[0])


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name)
        self.budgets=[(c,p) for c in (128,256,512) for p in (4,8,16)]
        self.pairs=[(100+i,200+i) for i in range(16)];records=[]
        scene={'extensions':{'ray_tracing':{'spectral_transport_v1':{'exposure':.019,'surface_path_v1':{'caustics':{'photons_per_pass':16384},'fixed':'reconstruction'}}}}}
        ref=dict(controlled_scene=scene,controlled_request={'camera':'fixed'},regions={'a':[0,0,2,2]},baseline_camera_samples=128,baseline_photon_passes=4,forbidden_final_camera_seeds=[1],forbidden_final_map_seeds=[2])
        self.reference=self.root/'reference.json';self.reference.write_text(json.dumps(ref));rng=np.random.default_rng(3)
        for c,p in self.budgets:
            for cs,ms in self.pairs:
                folder=self.root/f'{c}-{p}-{cs}';folder.mkdir();s=copy.deepcopy(scene);q=s['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1'];q.update(samples=c,seed=cs);q['caustics'].update(passes=p,seed=ms)
                (folder/'scene.json').write_text(json.dumps(s));(folder/'request.json').write_text(json.dumps(ref['controlled_request']))
                a=rng.normal(10,1,(1,4,11,3)).astype('<f8');(folder/'roi.f64.gz').write_bytes(gzip.compress(a.tobytes()))
                d=dict(scene_sha256=digest(folder/'scene.json'),roi_sha256=digest(folder/'roi.f64.gz'),binary_sha256='new-renderer',spp=c,camera_seed=cs,map={'passes':p,'seed':ms},cpu_seconds=1+c/128+p/4+c*p/512,roi_regions=ref['regions'])
                f=folder/'result.json';f.write_text(json.dumps(d));records.append(str(f))
        self.manifest=dict(budgets=self.budgets,seed_pairs=self.pairs,records=records,exposure=.019,resamples=1024)
        self.calibration=self.root/'calibration.json'
    def plan(self,target=.1):
        self.calibration.write_text(json.dumps(self.manifest));return absolute_plan(self.reference,self.calibration,target)
    def test_roundtrip_and_final_seed_exclusion(self):
        p=self.plan();self.assertEqual(p['status'],'measured_bound_target_met');self.assertEqual(p['selected']['photons'],65536)
        self.assertEqual(len(p['forbidden_final_camera_seeds']),17)
        self.assertIsNone(self.plan(.00001)['selected'])
    def test_insufficient_plan_cannot_start_job(self):
        from spectral_budget_qualification import render
        p=self.root/'insufficient.json';p.write_text(json.dumps(self.plan(.00001)))
        output=self.root/'never-created'
        with self.assertRaisesRegex(ValueError,'insufficient plan'):
            render(self.root/'unused-cli',p,self.root,output,900,901)
        self.assertFalse(output.exists())
    def test_incomplete_duplicate_and_seed_reuse_refusal(self):
        old=copy.deepcopy(self.manifest);self.manifest['records'].pop()
        with self.assertRaises(ValueError):self.plan()
        self.manifest=copy.deepcopy(old);self.manifest['records'].append(old['records'][0])
        with self.assertRaises(ValueError):self.plan()
        self.manifest=copy.deepcopy(old);self.manifest['seed_pairs'][1]=(100,201)
        with self.assertRaises(ValueError):self.plan()
    def test_exposure_hash_and_scene_drift_refusal(self):
        self.manifest['exposure']=1
        with self.assertRaises(ValueError):self.plan()
        self.manifest['exposure']=.019;f=Path(self.manifest['records'][0]);q=f.parent/'request.json';q.write_text('{}')
        with self.assertRaises(ValueError):self.plan()
        q.write_text(json.dumps({'camera':'fixed'}));(f.parent/'roi.f64.gz').write_bytes(b'wrong')
        with self.assertRaises(ValueError):self.plan()


class FullCostTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);root=Path(self.tmp.name)
        scene={'extensions':{'ray_tracing':{'spectral_transport_v1':{'surface_path_v1':{'caustics':{'radius':.01}}}}}}
        self.reference=root/'reference.json';self.reference.write_text(json.dumps(dict(controlled_scene=scene,controlled_request={'camera':'fixed'},baseline_camera_samples=128,baseline_photon_passes=4)))
        self.paths=[]
        for i,(c,p) in enumerate([(1,1),(4,1),(1,4),(4,4),(2,2),(4,4)]):
            folder=root/str(i);frames=folder/'render/frames';frames.mkdir(parents=True)
            s=copy.deepcopy(scene);q=s['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1'];cs,ms=(100,200) if i<4 else (300+i,400+i);q.update(samples=c*128,seed=cs);q['caustics'].update(passes=p*4,seed=ms)
            (folder/'scene.json').write_text(json.dumps(s));(folder/'request.json').write_text(json.dumps({'camera':'fixed'}));(folder/'roi.f64.gz').write_bytes(b'synthetic verified ROI');image=frames/'frame_0000.bmp';image.write_bytes(b'synthetic verified BMP');film=Path(str(image)+'.components.npy');film.write_bytes(b'synthetic verified film')
            (folder/'summary.json').write_text(json.dumps({'spectral_transport':{'linear_components':{'sha256':digest(film)},'nonscattered_indirect':{'surface_nee_pdf_mismatches':0},'termination':{'all_started_paths_classified':True}}}))
            f=folder/'result.json';f.write_text(json.dumps(dict(regions=False,scene_sha256=digest(folder/'scene.json'),roi_sha256=digest(folder/'roi.f64.gz'),image_sha256=digest(image),binary_sha256='full-cli',spp=c*128,camera_seed=cs,map={'passes':p*4,'seed':ms},cpu_seconds=2+3*c+5*p+7*c*p)));self.paths.append(f)
    def check(self):
        from spectral_budget_cost import calibrate
        return calibrate(self.reference,self.paths[:4],self.paths[4:])
    def test_independent_full_cost(self):
        r=self.check();np.testing.assert_allclose(r['models']['joint']['coefficients'],[2,3,5,7],atol=1e-10)
        self.assertTrue(r['models']['joint']['all_within_20_percent']);self.assertFalse(r['confidence_bounds_available'])
    def test_full_cost_hash_and_domain_guards(self):
        f=self.paths[0];d=json.loads(f.read_text());d['regions']=True;f.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'Full-frame'):self.check()
        d['regions']=False;f.write_text(json.dumps(d));(f.parent/'roi.f64.gz').write_bytes(b'drift')
        with self.assertRaisesRegex(ValueError,'drift'):self.check()
    def test_full_cost_holdout_seed_reuse_refuses(self):
        f=self.paths[-1];scene=f.parent/'scene.json';s=json.loads(scene.read_text());s['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']['seed']=100;scene.write_text(json.dumps(s));d=json.loads(f.read_text());d.update(camera_seed=100,scene_sha256=digest(scene));f.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'overlap'):self.check()


if __name__=='__main__':unittest.main()
