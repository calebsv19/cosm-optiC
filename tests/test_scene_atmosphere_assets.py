import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from scene_atmosphere_assets import publish,generate


class AssetTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.root=Path(self.tmp.name)
        self.scene=self.root/'project'/'scene.json'
        self.scene.parent.mkdir();self.scene.write_text('{}')
        self.source=self.root/'cache'
        shutil.copytree(ROOT/'tests/fixtures/scene_project_worker_snapshot/assets/vf3d/active',self.source)
    def tearDown(self):
        self.tmp.cleanup()
    def test_immutable_copy_and_relocation(self):
        result=publish(self.scene,self.source/'manifest.json')
        self.assertEqual(result,publish(self.scene,self.source/'manifest.json'))
        destination=self.scene.parent/result['source_path']
        self.assertEqual(destination.read_bytes(),(self.source/'manifest.json').read_bytes())
        shutil.rmtree(self.source)
        moved=self.root/'moved';shutil.copytree(self.scene.parent,moved)
        doc=json.loads((moved/result['source_path']).read_text())
        self.assertEqual([f['frame_index'] for f in doc['frames']],[0,2,4])
        for frame in doc['frames']:
            self.assertTrue((moved/result['source_path']).parent.joinpath(frame['path']).is_file())
    def test_bad_dependency_never_publishes(self):
        manifest=self.source/'manifest.json';doc=json.loads(manifest.read_text())
        for value in ('../escape.vf3d','/absolute.vf3d','missing.vf3d'):
            doc['frames'][0]['path']=value;manifest.write_text(json.dumps(doc))
            with self.assertRaises(ValueError):publish(self.scene,manifest)
            self.assertFalse((self.scene.parent/'assets').exists())
    def test_symlink_and_mutated_asset_refused(self):
        target=self.source/'frame_000000.vf3d'
        data=target.read_bytes();target.unlink();target.symlink_to(self.source/'frame_000002.vf3d')
        with self.assertRaises(ValueError):publish(self.scene,self.source/'manifest.json')
        target.unlink();target.write_bytes(data)
        result=publish(self.scene,self.source/'manifest.json')
        (self.scene.parent/result['source_path']).write_text('{}')
        with self.assertRaises(ValueError):publish(self.scene,self.source/'manifest.json')
    def test_solver_free_generation_repeat(self):
        first=generate(self.scene,'room_haze_v1',7)
        self.assertEqual(first,generate(self.scene,'room_haze_v1',7))
        self.assertEqual(first['source_kind'],'vf3d')
        self.assertGreater((self.scene.parent/first['source_path']).stat().st_size,1000)


if __name__=='__main__':unittest.main()
