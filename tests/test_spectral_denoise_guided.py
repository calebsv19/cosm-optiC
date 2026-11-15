import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
from scipy.ndimage import gaussian_filter
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from spectral_denoise_guided import restore_detail, run, POLICY
from spectral_batch_accumulate import accumulate, digest
from test_spectral_batch_accumulate import fixture


class GuideTests(unittest.TestCase):
    def test_constant_and_black_identity(self):
        for v in (0., 3., -2.):
            b = np.full((4, 2, 3, 3), v)
            out, weights = restore_detail(b, b.mean(0))
            np.testing.assert_array_equal(out, b[0])
            np.testing.assert_array_equal(weights, 0)

    def test_coherent_edge_restored(self):
        truth = np.ones((32, 32, 3)); truth[:, 16:] = 5
        base = gaussian_filter(truth, (2, 2, 0))
        out, weight = restore_detail(np.repeat(truth[None], 4, axis=0), base)
        self.assertLess(np.mean((out-truth)**2), .1*np.mean((base-truth)**2))
        self.assertLessEqual(weight.max(), POLICY['maximum_raw_restore'])

    def test_uncertain_detail_is_restored_less(self):
        b = np.ones((4, 16, 16, 3)); b[:, 5:10, 5:10] += 2
        baseline = np.ones((16, 16, 3))
        _, stable = restore_detail(b, baseline)
        # Same mean, different independent-batch uncertainty.
        b += np.array([-4., -2., 2., 4.])[:, None, None, None]
        _, uncertain = restore_detail(b, baseline)
        self.assertLess(uncertain[7, 7], stable[7, 7]/2)

    def test_signed_color_shared_weight_and_convex_bounds(self):
        rng = np.random.default_rng(281)
        b = rng.normal(size=(4, 10, 11, 3)); base = rng.normal(size=(10, 11, 3))
        original = b.copy(); old_base = base.copy()
        out, weight = restore_detail(b, base)
        np.testing.assert_array_equal(b, original)
        np.testing.assert_array_equal(base, old_base)
        np.testing.assert_allclose(out, base+weight[..., None]*(b.mean(0)-base))
        self.assertTrue(np.all(out >= np.minimum(base, b.mean(0))-1e-14))
        self.assertTrue(np.all(out <= np.maximum(base, b.mean(0))+1e-14))

    def test_scale_equivariance(self):
        rng = np.random.default_rng(92); b = rng.normal(size=(4, 10, 9, 3))
        base = b.mean(0)*.8
        out, w = restore_detail(b, base)
        scaled, sw = restore_detail(37*b, 37*base)
        np.testing.assert_allclose(scaled, 37*out, atol=1e-13)
        np.testing.assert_allclose(sw, w, atol=1e-13)

    def test_invalid_inputs(self):
        good = np.ones((4, 2, 3, 3))
        for bad in (good[:3], good[..., :2], good*np.nan, good*np.inf):
            with self.assertRaises(ValueError): restore_detail(bad, good.mean(0))
        with self.assertRaises(ValueError): restore_detail(good, np.ones((3, 3, 3)))


class WorkflowTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.root = Path(self.tmp.name)
        records = [fixture(self.root/str(i), i+1, value=i+1)[0] for i in range(4)]
        accumulate(records, self.root/'raw')
        self.manifest = self.root/'raw/manifest.json'
    def tearDown(self): self.tmp.cleanup()

    def test_receipt_raw_preservation_and_overwrite_refusal(self):
        before = digest(self.root/'raw/components.npy')
        with patch('spectral_denoise_guided.oidn_signed_hdr', side_effect=lambda a,*_: (a*.8, {'test':True})):
            result = run(self.manifest, self.root/'out', 'unused-test-library')
        self.assertTrue(result['complete'])
        self.assertFalse(result['reference_used'])
        self.assertEqual(before, digest(self.root/'raw/components.npy'))
        for name, d in result['outputs'].items(): self.assertEqual(d['sha256'], digest(self.root/'out'/name))
        with self.assertRaisesRegex(ValueError, 'exists'): run(self.manifest, self.root/'out', 'unused')

    def test_invalid_raw_refused_before_backend(self):
        p = self.root/'raw/components.npy'; p.write_bytes(p.read_bytes()[:-1])
        with patch('spectral_denoise_guided.oidn_signed_hdr') as backend:
            with self.assertRaises(ValueError): run(self.manifest, self.root/'out', 'unused')
            backend.assert_not_called()
        self.assertFalse((self.root/'out').exists())

    def test_backend_failure_has_no_completed_output(self):
        with patch('spectral_denoise_guided.oidn_signed_hdr', side_effect=RuntimeError('backend failed')):
            with self.assertRaisesRegex(RuntimeError, 'backend failed'): run(self.manifest, self.root/'out', 'unused')
        self.assertFalse((self.root/'out').exists())


if __name__ == '__main__': unittest.main()
