import json
from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from spectral_denoise_still import run
from spectral_batch_accumulate import digest
from test_spectral_batch_accumulate import fixture


class StillTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.root = Path(self.tmp.name)
        p, self.raw = fixture(self.root/'job', 42)
        self.summary = p.parent/'summary.json'
        self.image = p.parent/'render/frames/frame_0000.bmp'
        self.output = self.root/'candidate'
    def tearDown(self): self.tmp.cleanup()
    def invoke(self, backend=None):
        return run(self.summary, self.image, self.output, 'test-backend',
                   backend=backend or (lambda a, *_: (a.copy(), {'backend':'identity test'})))
    def test_raw_identity_and_display_roundtrip(self):
        film = Path(str(self.image)+'.components.npy'); original = digest(film)
        r = self.invoke()
        np.testing.assert_array_equal(np.load(self.output/'beauty_xyz.npy'), self.raw[...,0,:])
        np.testing.assert_array_equal(np.load(self.output/'difference_xyz.npy'), 0)
        self.assertEqual(digest(film), original)
        self.assertFalse(r['variance_available']); self.assertFalse(r['component_decomposition'])
        self.assertTrue(r['complete'])
        with self.assertRaisesRegex(ValueError, 'exists'): self.invoke()
    def test_incomplete_wrong_integrator_or_multiple_frames_refused(self):
        original = json.loads(self.summary.read_text())
        for key, value in (('rendered_frames',False),('frames_rendered',2),('integrator_3d','disney_v2')):
            d = dict(original); d[key] = value; self.summary.write_text(json.dumps(d))
            with self.assertRaises(ValueError): self.invoke()
            self.assertFalse(self.output.exists())
    def test_hash_drift_refused(self):
        p = Path(str(self.image)+'.components.npy'); data = bytearray(p.read_bytes()); data[-1] ^= 1; p.write_bytes(data)
        with self.assertRaises((ValueError, AssertionError)): self.invoke()
        self.assertFalse(self.output.exists())
    def test_backend_failure_has_no_completion_receipt(self):
        def fail(*_): raise RuntimeError('test backend failure')
        with self.assertRaises(RuntimeError): self.invoke(fail)
        self.assertFalse((self.output/'manifest.json').exists())
    def test_input_mutation_has_no_completion_receipt(self):
        def mutate(a, *_):
            self.summary.write_text(self.summary.read_text()+'\n')
            return a.copy(), {}
        with self.assertRaisesRegex(ValueError, 'changed'): self.invoke(mutate)
        self.assertFalse((self.output/'manifest.json').exists())
    def test_nonfinite_backend_refused(self):
        with self.assertRaisesRegex(ValueError, 'invalid beauty'):
            self.invoke(lambda a, *_: (np.full_like(a, np.nan), {}))
        self.assertFalse((self.output/'manifest.json').exists())
    def test_signed_split_identity_without_model(self):
        from unittest.mock import patch
        from spectral_denoise_backend import oidn_signed_hdr
        a = np.array([[[.14, .04, .8], [.6, .25, .01], [.2, .8, .1]]])
        with patch('spectral_denoise_backend.oidn_hdr', side_effect=lambda x, *_: (x.copy(), {})) as model:
            result, info = oidn_signed_hdr(a, 'identity', .019)
        np.testing.assert_allclose(result, a, rtol=1e-13, atol=1e-14)
        self.assertEqual(model.call_count, 2)
        self.assertTrue(info['experimental'])

    def test_substantial_signed_input_refused_before_output(self):
        from unittest.mock import patch
        with patch('spectral_denoise_still.split_rgb', return_value=(np.ones((2,3,3)), -np.ones((2,3,3)))):
            with self.assertRaisesRegex(ValueError, 'Unsupported gamut'): self.invoke()
        self.assertFalse(self.output.exists())


if __name__ == '__main__': unittest.main()
