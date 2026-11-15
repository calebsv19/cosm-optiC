import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import spectral_batch_accumulate as batch
from spectral_components import LABELS,display


def fixture(root,seed,spp=128,value=1.):
    root.mkdir();(root/'assets').mkdir();(root/'config').mkdir()
    (root/'assets/mesh.txt').write_text('fixed geometry')
    frames=root/'render/frames';frames.mkdir(parents=True)
    a=np.zeros((2,3,11,3),dtype='<f8')
    for k in (1,2,4,5,6,7,8,9,10):a[...,k,:]=value*k
    a[...,3,:]=a[...,6:11,:].sum(-2);a[...,0,:]=batch.recombine(a)
    image=frames/'frame_0000.bmp';film=Path(str(image)+'.components.npy');np.save(film,a)
    Image.frombytes('RGBA',(3,2),b''.join(display(x,.019,True) for x in a[...,0,:].reshape(-1,3))).save(image)
    p={'samples':spp,'seed':seed,'caustics':{'seed':100+seed,'passes':16,'photons_per_pass':16384},'reconstruction':{'radius':.01}}
    scene={'extensions':{'ray_tracing':{'spectral_transport_v1':{'surface_path_v1':p}}}}
    request={'scene':{'runtime_scene_path':'scene.json'},'render':{'width':3,'height':2},'output':{'root':'render'},'run_id':root.name}
    report={'linear_components':{'written':True,'components':list(LABELS),'file':film.name,'bytes':film.stat().st_size,'sha256':batch.digest(film)},'film':{'width':3,'height':2},'quality':{'samples_per_pixel':spp,'seed':seed,'component':'total','exposure':.019,'display_transform':'spectral_soft_v1'},'smooth_glass':{'passes':16,'seed':100+seed,'launched':262144},'film_components_xyz':{},'scattering_orders':{'groups':[{'xyz':a[...,k,:].sum((0,1)).tolist()} for k in range(7,11)]},'nonscattered_indirect':{'surface_nee_pdf_mismatches':0},'termination':{'all_started_paths_classified':True}}
    for name,data in [('scene.json',scene),('request.json',request),('summary.json',{'rendered_frames':True,'frames_rendered':1,'integrator_3d':'disney_v3_spectral','spectral_transport':report})]: (root/name).write_text(json.dumps(data))
    d={'regions':False,'spp':spp,'passes':16,'camera_seed':seed,'map_seed':100+seed,'seconds':10.,'cpu_seconds':9.,'binary_sha256':'fixed','scene_sha256':batch.digest(root/'scene.json'),'image_sha256':batch.digest(image)}
    (root/'result.json').write_text(json.dumps(d));return root/'result.json',a


class BatchTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.p,self.a=fixture(self.root/'a',1,value=1.)
        self.q,self.b=fixture(self.root/'b',2,value=3.)
    def tearDown(self):self.tmp.cleanup()
    def edit(self,name,key,value):
        p=self.q.parent/name;d=json.loads(p.read_text());d[key]=value;p.write_text(json.dumps(d))
    def test_equal_weights_exact_components_and_covariance(self):
        out=self.root/'out';r=batch.accumulate([self.p,self.q],out)
        a=np.load(out/'components.npy');v=np.load(out/'variance_of_mean.npy')
        np.testing.assert_array_equal(a,(self.a+self.b)/2)
        np.testing.assert_array_equal(a[...,0,:],batch.recombine(a))
        np.testing.assert_allclose(v,(self.b-self.a)**2/4)
        self.assertTrue(r['variance_of_mean_available']);self.assertEqual(r['camera_samples'],256)
        # Total variance includes covariance: never sum marginal variances.
        self.assertGreater(v[0,0,0,1],v[0,0,1:6,1].sum())
    def test_native_legacy_clip_display(self):
        for path,a in ((self.p,self.a),(self.q,self.b)):
            image=path.parent/'render/frames/frame_0000.bmp'
            Image.frombytes('RGBA',(3,2),b''.join(display(x,.019,False) for x in a[...,0,:].reshape(-1,3))).save(image)
            d=json.loads(path.read_text());d['image_sha256']=batch.digest(image);path.write_text(json.dumps(d))
            summary=path.parent/'summary.json';d=json.loads(summary.read_text());d['spectral_transport']['quality']['display_transform']='legacy_clip';summary.write_text(json.dumps(d))
        result=batch.accumulate([self.p,self.q],self.root/'out')
        self.assertEqual(result['display_transform'],'legacy_clip')
    def test_unequal_samples_not_equal_or_photon_weighting(self):
        q,b=fixture(self.root/'c',3,spp=384,value=3.)
        r=batch.accumulate([self.p,q],self.root/'out')
        np.testing.assert_allclose(np.load(self.root/'out/components.npy'),self.a*.25+b*.75)
        self.assertFalse(r['variance_of_mean_available']);self.assertFalse((self.root/'out/variance_of_mean.npy').exists())
    def test_no_double_counting_subsets(self):
        out=batch.weighted_components([self.a,self.b],[1,1]);self.assertEqual(out[0,0,0,0],2*sum((1,2,4,5,6,7,8,9,10)))
    def test_dominant_bright_batch_is_retained(self):
        q,b=fixture(self.root/'bright',4,value=10000.)
        r=batch.accumulate([self.p,q],self.root/'out')
        self.assertGreater(r['tail_diagnostics']['total']['max_contribution_share'],.999)
        np.testing.assert_allclose(np.load(self.root/'out/components.npy'),(self.a+b)/2)
    def test_duplicate_record_refused_before_output(self):
        with self.assertRaisesRegex(ValueError,'Duplicate'):batch.accumulate([self.p,self.p],self.root/'out')
        self.assertFalse((self.root/'out').exists())
    def test_map_seed_reuse_refused(self):
        q,_=fixture(self.root/'same',1)
        with self.assertRaisesRegex(ValueError,'unique'):batch.accumulate([self.p,q],self.root/'out')
    def test_map_only_seed_reuse_refused(self):
        p=self.q.parent/'scene.json';d=json.loads(p.read_text());d['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']['caustics']['seed']=101;p.write_text(json.dumps(d))
        self.edit('result.json','map_seed',101);self.edit('result.json','scene_sha256',batch.digest(p))
        p=self.q.parent/'summary.json';d=json.loads(p.read_text());d['spectral_transport']['smooth_glass']['seed']=101;p.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'unique'):batch.accumulate([self.p,self.q],self.root/'out')
    def test_zero_variance_is_not_confidence(self):
        q,b=fixture(self.root/'constant',3,value=1.)
        r=batch.accumulate([self.p,q],self.root/'out')
        self.assertFalse(r['confidence_bounds_available'])
        self.assertEqual(float(np.load(self.root/'out/variance_of_mean.npy').max()),0.)
    def test_incompatible_map_budget(self):
        p=self.q.parent/'scene.json';d=json.loads(p.read_text());d['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']['caustics']['passes']=8;p.write_text(json.dumps(d))
        s=self.q.parent/'summary.json';r=json.loads(s.read_text());r['spectral_transport']['smooth_glass']['passes']=8;s.write_text(json.dumps(r))
        self.edit('result.json','passes',8);self.edit('result.json','scene_sha256',batch.digest(p))
        with self.assertRaisesRegex(ValueError,'drift'):batch.accumulate([self.p,self.q],self.root/'out')
    def test_camera_drift(self):
        self.edit('request.json','camera',{'position':[1,0,0]})
        with self.assertRaisesRegex(ValueError,'drift'):batch.accumulate([self.p,self.q],self.root/'out')
    def test_resource_drift(self):
        (self.q.parent/'assets/mesh.txt').write_text('changed')
        with self.assertRaisesRegex(ValueError,'drift'):batch.accumulate([self.p,self.q],self.root/'out')
    def test_external_resource_path_refused(self):
        with self.assertRaisesRegex(ValueError,'outside hashed'):
            batch.validate_resource_paths({'runtime_mesh_path':'/tmp/unbound.json'}, {})
        with self.assertRaisesRegex(ValueError,'outside hashed'):
            batch.validate_resource_paths({'runtime_mesh_path':'assets/../unbound.json'}, {})
    def test_nested_directory_symlink_refused(self):
        (self.q.parent/'assets/alias').symlink_to(self.p.parent/'assets',target_is_directory=True)
        with self.assertRaisesRegex(ValueError,'nested'):batch.accumulate([self.p,self.q],self.root/'out')
    def test_film_hash_drift(self):
        p=self.q.parent/'render/frames/frame_0000.bmp.components.npy';p.write_bytes(p.read_bytes()[:-1]+b'x')
        with self.assertRaisesRegex(ValueError,'hash drift'):batch.accumulate([self.p,self.q],self.root/'out')
    def test_incomplete_refusal(self):
        self.edit('summary.json','rendered_frames',False)
        with self.assertRaisesRegex(ValueError,'Completed'):batch.accumulate([self.p,self.q],self.root/'out')
    def test_region_refusal(self):
        self.edit('result.json','regions',True)
        with self.assertRaisesRegex(ValueError,'full-frame'):batch.accumulate([self.p,self.q],self.root/'out')
    def test_output_collision(self):
        (self.root/'out').mkdir()
        with self.assertRaisesRegex(ValueError,'already exists'):batch.accumulate([self.p,self.q],self.root/'out')
    def test_batch_bounds(self):
        for records in ([self.p],[self.p]*65):
            with self.assertRaisesRegex(ValueError,'2..64'):batch.accumulate(records,self.root/'out')
    def test_invalid_weights(self):
        for weights in ([0,1],[True,2],[float('nan'),2],[]):
            with self.assertRaises(ValueError):batch.weighted_components([self.a,self.b],weights)
    def test_nonfinite_array(self):
        self.b[0,0,0,0]=np.inf
        with self.assertRaises(ValueError):batch.weighted_components([self.a,self.b],[1,1])
    def test_seed_record_mismatch(self):
        self.edit('result.json','map_seed',77)
        with self.assertRaisesRegex(ValueError,'identity mismatch'):batch.accumulate([self.p,self.q],self.root/'out')

class CampaignTests(unittest.TestCase):
    def setUp(self):
        from unittest.mock import patch
        import spectral_batch_campaign as campaign
        self.campaign=campaign;self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.base=self.root/'base';fixture(self.base,900)
        self.cli=self.root/'cli';self.cli.write_text('fixed binary')
        self.plan=self.root/'plan.json';self.plan.write_text(json.dumps({'selected':{'camera_samples':128},'controlled_scene':{}}))
        self.output=self.root/'campaign';self.calls=[]
        def render(cli,plan,base,out,cs,ms,timeout):
            self.calls.append(out)
            if (out/'result.json').exists():return
            if out.exists():raise ValueError('Unfinished identity')
            fixture(out,cs)
        self.mock=patch.object(campaign,'render',side_effect=render);self.mock.start()
    def tearDown(self):self.mock.stop();self.tmp.cleanup()
    def run_campaign(self,**kwargs):
        return self.campaign.run(self.cli,self.plan,self.base,self.output,[1,2],[101,102],**kwargs)
    def test_complete_and_exact_resume(self):
        a=self.run_campaign();b=self.run_campaign(resume=True)
        self.assertEqual(a,b);self.assertFalse((self.output/'active.lock').exists())
    def test_drift_refuses_before_render(self):
        self.run_campaign();self.calls.clear();self.cli.write_text('drift')
        with self.assertRaisesRegex(ValueError,'exact identity'):self.run_campaign(resume=True)
        self.assertEqual(self.calls,[])
    def test_no_overwrite(self):
        self.run_campaign()
        with self.assertRaisesRegex(ValueError,'resume'):self.run_campaign()
    def test_unknown_job_is_not_retried(self):
        from unittest.mock import patch
        def fail(*args,**kwargs):
            args[3].mkdir();raise RuntimeError('interrupted')
        with patch.object(self.campaign,'render',side_effect=fail):
            with self.assertRaises(RuntimeError):self.run_campaign()
        with self.assertRaisesRegex(ValueError,'Unfinished'):self.run_campaign(resume=True)
    def test_active_owner_refusal(self):
        self.run_campaign();(self.output/'active.lock').write_text('owner')
        with self.assertRaises(FileExistsError):self.run_campaign(resume=True)
        self.assertEqual((self.output/'active.lock').read_text(),'owner')
    def test_invalid_bound(self):
        with self.assertRaises(ValueError):self.run_campaign(timeout=0)
        self.assertFalse(self.output.exists())
    def test_changed_output_refused_on_resume(self):
        self.run_campaign();(self.output/'accumulated/preview.png').write_bytes(b'bad')
        with self.assertRaisesRegex(ValueError,'identity drift'):self.run_campaign(resume=True)

if __name__=='__main__':unittest.main()
