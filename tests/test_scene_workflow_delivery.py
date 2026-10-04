import json
from pathlib import Path
import struct
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import scene_workflow as w
import scene_workflow_delivery as d
from scene_workflow_bundle import read,write,sha


class DeliveryTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.run=self.root/'run';self.run.mkdir()
        self.scene=self.run/'scene_runtime.render_frozen.json'
        write(self.scene,{'schema_family':'codework_scene','schema_variant':'scene_runtime_v1','objects':[]})
        self.job=self.run/'jobs/j1';self.job.mkdir(parents=True)
        self.output=self.run/'final';(self.output/'frames').mkdir(parents=True)
        self.movie=self.run/'movie.mp4';self.movie.write_bytes(b'fixture-video')
        request={'schema_version':'ray_tracing_agent_render_request_v1','run_id':'r1','scene':{'runtime_scene_path':str(self.scene)},'volume':{'enabled':False},'render':{'width':1,'height':1,'frame_count':2,'temporal_frames':1,'start_frame':0},'output':{'root':str(self.output),'video':{'enabled':True,'path':str(self.movie),'fps':24}}}
        write(self.job/'job_request.json',request)
        write(self.job/'job_status.json',{'state':'completed','exit_code':0,'frames_completed':2,'job_id':'j1'})
        write(self.job/'result_summary.json',{'run_id':'r1','diagnostics':'ok','frames_rendered':2,'rendered_frames':True})
        write(self.job/'render_progress.json',{'run_id':'r1','stage':'completed','frames_completed':2})
        (self.job/'stderr.log').write_text('')
        write(self.output/'render_summary.json',{'old':'preflight'})
        write(self.output/'render_progress.json',{'old':'preflight'})
        header=bytearray(54);header[:2]=b'BM';struct.pack_into('<I',header,2,58);struct.pack_into('<ii',header,18,1,1)
        for i in range(2):(self.output/'frames'/f'frame_{i:04d}.bmp').write_bytes(header+b'1234')
        self.renderer=self.root/'renderer';self.renderer.write_bytes(b'fixture-renderer')
        self.project=self.run/'workflow'
        w.init(self.project,self.scene,self.job/'job_request.json','Fixture')

    def tearDown(self):self.tmp.cleanup()

    def reconcile(self):
        with patch('scene_workflow_delivery.verify_video',return_value={'sha256':sha(self.movie),'bytes':self.movie.stat().st_size}):
            return d.reconcile(self.project,self.run,'jobs/j1',self.renderer)

    def test_completion_replay_and_preserved_preflight(self):
        receipt=self.reconcile();self.assertEqual(receipt,self.reconcile())
        self.assertEqual(read(self.output/'render_summary.json')['frames_rendered'],2)
        self.assertEqual(read(self.project/'completed_runs/r1/prior_render_summary.json'),{'old':'preflight'})
        self.assertIsNone(w.load(self.project)['accepted_revision'])
        self.assertEqual(len(receipt['frames']),2)

    def test_incomplete_frames_refuse_without_alias_changes(self):
        (self.output/'frames/frame_0001.bmp').unlink()
        with self.assertRaises(ValueError):self.reconcile()
        self.assertEqual(read(self.output/'render_summary.json'),{'old':'preflight'})
        self.assertFalse((self.project/'completed_runs').exists())

    def test_completed_input_identity_cannot_change_on_replay(self):
        self.reconcile();scene=read(self.scene);scene['changed']=True;write(self.scene,scene)
        with self.assertRaisesRegex(ValueError,'identity changed'):self.reconcile()

    def test_archive_restore_and_corruption_refusal(self):
        self.reconcile();archive=self.root/'archive'
        d.archive(self.project,self.run,'r1',archive,'compact')
        result=d.restore(archive,self.root/'restored');self.assertTrue(result['verified'])
        self.assertTrue((self.root/'restored/movie.mp4').exists())
        self.assertEqual(len(list((self.output/'frames').glob('*.bmp'))),2)
        with (archive/'scene.tar.gz').open('ab') as f:f.write(b'tamper')
        with self.assertRaises(ValueError):d.restore(archive,self.root/'bad')
        self.assertFalse((self.root/'bad').exists())

    def test_archive_member_traversal_refused(self):
        self.reconcile();archive=self.root/'archive';d.archive(self.project,self.run,'r1',archive,'compact')
        with tarfile.open(archive/'scene.tar.gz','w:gz') as tar:
            entry=tarfile.TarInfo('../escape');entry.size=0;tar.addfile(entry)
        manifest=read(archive/'archive_manifest.json');manifest['archive_sha256']=sha(archive/'scene.tar.gz');write(archive/'archive_manifest.json',manifest)
        (archive/'checksums.sha256').write_text(f'{sha(archive / "scene.tar.gz")}  scene.tar.gz\n{sha(archive / "archive_manifest.json")}  archive_manifest.json\n')
        with self.assertRaises(ValueError):d.restore(archive,self.root/'bad')
        self.assertFalse((self.root/'escape').exists())


if __name__=='__main__':unittest.main()
