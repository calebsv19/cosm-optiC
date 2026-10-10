import os
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from scene_asset_storage import copy_asset, generated_staging

class SceneStorageTests(unittest.TestCase):
    def test_copy_is_independent_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);a=root/'a';b=root/'b';a.write_bytes(b'scene bytes')
            copy_asset(a,b);self.assertEqual(a.read_bytes(),b.read_bytes())
            b.write_bytes(b'consumer edit');self.assertEqual(a.read_bytes(),b'scene bytes')
            with self.assertRaises(ValueError):copy_asset(a,b)
            link=root/'link';link.symlink_to(a)
            with self.assertRaises(ValueError):copy_asset(link,root/'c')
    def test_success_removes_only_its_staging(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);old=root/'old';old.write_text('retained')
            with generated_staging(root) as staging:(staging/'compiled.json').write_bytes(b'scratch')
            self.assertFalse(staging.exists());self.assertEqual(old.read_text(),'retained')
    def test_failure_preserves_diagnostics(self):
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaises(RuntimeError):
                with generated_staging(t) as staging:
                    (staging/'compile.log').write_text('compiler failed');raise RuntimeError('failure')
            self.assertEqual((staging/'compile.log').read_text(),'compiler failed')

if __name__=='__main__':unittest.main()
