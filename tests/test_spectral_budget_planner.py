"""Analytic allocation, refusal, and held-out seed contracts."""
import copy
import sys
from pathlib import Path
import unittest
import tempfile,json,gzip,hashlib
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from spectral_budget_planner import crossed_components,fit_cost,select_budget,validate_final

class BudgetTests(unittest.TestCase):
    def test_separable_camera_map(self):
        a=np.arange(4.)[:,None,None,None,None];b=np.arange(4.)[None,:,None,None,None]
        x=np.broadcast_to(2*a+3*b,(4,4,2,8,3))
        raw=crossed_components(x)
        np.testing.assert_allclose(raw[0],4*np.var(np.arange(4.),ddof=1));np.testing.assert_allclose(raw[1],9*np.var(np.arange(4.),ddof=1));np.testing.assert_allclose(raw[2],0)
    def test_camera_and_map_choose_different_budget(self):
        cost={'coefficients':[1,1,1]}
        raw=np.zeros((3,1,3));raw[0]=1
        self.assertEqual((select_budget(raw,cost,[1,2,4],[1,2,4],.3)['selected']['camera_multiplier'],select_budget(raw,cost,[1,2,4],[1,2,4],.3)['selected']['photon_multiplier']),(4,1))
        raw[[0,1]]=raw[[1,0]]
        result=select_budget(raw,cost,[1,2,4],[1,2,4],.3)['selected'];self.assertEqual((result['camera_multiplier'],result['photon_multiplier']),(1,4))
    def test_all_xyz_channels_constrain(self):
        raw=np.zeros((3,1,3));raw[0,0]=[1,1,0];raw[1,0,2]=1
        selected=select_budget(raw,{'coefficients':[1,1,1]},[1,4],[1,4],.3)['selected'];self.assertEqual((selected['camera_multiplier'],selected['photon_multiplier']),(4,4))
    def test_insufficient_zero_and_invalid(self):
        raw=np.ones((3,1,3));self.assertIsNone(select_budget(raw,{'coefficients':[1,1,1]},[1,2],[1,2],.1)['selected'])
        with self.assertRaises(ValueError):select_budget(raw*0,{'coefficients':[1,1,1]},[1],[1],.3)
        with self.assertRaises(ValueError):crossed_components(np.zeros((2,4,1,8,3)))
        with self.assertRaises(ValueError):select_budget(raw,{'coefficients':[1,1,1]},[0],[1],.3)
    def test_cost_fit(self):
        rows=[(c,p,2+3*c+5*p) for c,p in ((1,1),(2,1),(1,2),(4,1),(1,4))]
        np.testing.assert_allclose(fit_cost(rows)['coefficients'],[2,3,5],atol=1e-12)
        with self.assertRaises(ValueError):fit_cost([(1,1,1)]*3)
    def test_negative_estimates_retained(self):
        raw=np.ones((3,1,3));raw[0,0,0]=-.1
        r=select_budget(raw,{'coefficients':[1,1,1]},[1,4],[1,4],.3)
        self.assertEqual(r['raw_signed_components'][0][0][0],-.1);self.assertEqual(r['negative_estimates'],1)
    def test_final_contract(self):
        scene={'extensions':{'ray_tracing':{'spectral_transport_v1':{'surface_path_v1':{'samples':512,'seed':99,'caustics':{'passes':16,'seed':98,'radius':.01}}}}}}
        from spectral_budget_planner import controlled_scene
        p={'controlled_scene':controlled_scene(scene),'selected':{'camera_samples':512,'photon_passes':16},'forbidden_final_camera_seeds':[1,2],'forbidden_final_map_seeds':[3,4]}
        validate_final(p,scene)
        q=scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1'];q['seed']=1
        with self.assertRaises(ValueError):validate_final(p,scene)
        q['seed']=99;q['caustics']['radius']=.02
        with self.assertRaises(ValueError):validate_final(p,scene)

class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        records=[]
        for c,m,n,p in [(c,m,128,4) for c in (1,2,3) for m in (4,5,6)]+[(1,4,512,4),(1,4,128,16)]:
            folder=self.root/f'{c}-{m}-{n}-{p}';folder.mkdir()
            scene={'extensions':{'ray_tracing':{'spectral_transport_v1':{'surface_path_v1':{'samples':n,'seed':c,'caustics':{'passes':p,'seed':m,'photons_per_pass':16384},'policy':'fixed'}}}}}
            (folder/'scene.json').write_text(json.dumps(scene));(folder/'request.json').write_text(json.dumps({'camera':[0,1,2]}))
            a=np.ones((6,64,11,3),dtype='<f8')*(c+2*m)
            (folder/'roi.f64.gz').write_bytes(gzip.compress(a.tobytes()))
            from spectral_budget_planner import digest
            result=dict(scene_sha256=digest(folder/'scene.json'),roi_sha256=digest(folder/'roi.f64.gz'),binary_sha256='test',spp=n,camera_seed=c,map={'passes':p,'seed':m},cpu_seconds=1+n/128+p/4)
            f=folder/'result.json';f.write_text(json.dumps(result));records.append(str(f))
        self.manifest=dict(camera_seeds=[1,2,3],map_seeds=[4,5,6],baseline_camera_samples=128,baseline_photon_passes=4,records=records,camera_multipliers=[1,2,4],photon_multipliers=[1,2,4],target_variance_ratio=.3)
    def run_plan(self):
        from spectral_budget_planner import plan
        f=self.root/'pilot.json';f.write_text(json.dumps(self.manifest));return plan(f)
    def test_roundtrip(self):
        p=self.run_plan();self.assertEqual(p['selected']['camera_samples'],256);self.assertEqual(p['selected']['photon_passes'],16)
        self.assertEqual(p['controlled_request'],{'camera':[0,1,2]})
    def test_missing_duplicate_hash_and_camera(self):
        original=list(self.manifest['records'])
        self.manifest['records']=original[1:]
        with self.assertRaises(ValueError):self.run_plan()
        self.manifest['records']=original+[original[0]]
        with self.assertRaises(ValueError):self.run_plan()
        self.manifest['records']=original
        folder=Path(original[0]).parent;(folder/'request.json').write_text('{}')
        with self.assertRaises(ValueError):self.run_plan()
        (folder/'request.json').write_text(json.dumps({'camera':[0,1,2]}));(folder/'roi.f64.gz').write_bytes(b'wrong')
        with self.assertRaises(ValueError):self.run_plan()
    def test_custom_full_frame_plan(self):
        from spectral_budget_planner import digest
        self.manifest['regions']={'shadow':[0,0,2,2],'caustic':[2,1,2,2]}
        for location in self.manifest['records']:
            f=Path(location);d=json.loads(f.read_text());folder=f.parent;frames=folder/'render/frames';frames.mkdir(parents=True)
            path=frames/'frame_0000.bmp.components.npy';np.save(path,np.full((3,4,11,3),d['camera_seed']+2*d['map']['seed'],dtype='<f8'))
            (folder/'summary.json').write_text(json.dumps({'spectral_transport':{'linear_components':{'sha256':digest(path)}}}))
            d['regions']=False;f.write_text(json.dumps(d))
        p=self.run_plan();self.assertEqual(p['region_order'],['shadow','caustic']);self.assertEqual(p['selected']['camera_samples'],256)
    def test_extrapolation_refuses(self):
        self.manifest['camera_multipliers']=[1,8]
        with self.assertRaises(ValueError):self.run_plan()

class RegionTests(unittest.TestCase):
    def test_region_contract(self):
        from spectral_budget_planner import region_spec
        self.assertEqual(region_spec({'shadow':[1,2,2,3]}),{'shadow':[1,2,2,3]})
        for value in ({},{'a':[-1,0,2,2]},{'a':[0,0,2,2],'b':[2,2,1,1]},{'a':[0,0,0,2]}):
            with self.assertRaises(ValueError):region_spec(value)
    def test_full_film_crop_and_bounds(self):
        from spectral_budget_planner import pilot_pixels,digest
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);frames=root/'render/frames';frames.mkdir(parents=True)
            a=np.arange(4*6*11*3,dtype='<f8').reshape(4,6,11,3);path=frames/'frame_0000.bmp.components.npy';np.save(path,a)
            (root/'summary.json').write_text(json.dumps({'spectral_transport':{'linear_components':{'sha256':digest(path)}}}))
            regions={'shadow':[0,0,2,2],'caustic':[4,2,2,2]}
            b=pilot_pixels(root,{'regions':False},regions)
            np.testing.assert_array_equal(b[0],a[:2,:2].reshape(4,11,3));np.testing.assert_array_equal(b[1],a[2:,4:].reshape(4,11,3))
            with self.assertRaises(ValueError):pilot_pixels(root,{'regions':False},{'bad':[5,3,2,2]})
            with self.assertRaises(ValueError):pilot_pixels(root,{},regions)

if __name__=='__main__':unittest.main()
