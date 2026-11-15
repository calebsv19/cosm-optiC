import copy
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from spectral_batch_accumulate import accumulate, recombine, digest
from spectral_denoise import component_filter, load_accumulation, run, validate_components
from spectral_denoise_backend import (
    adjacent_library_identities, digest_loaded_libraries,
    split_rgb, restore_xyz, XYZ_TO_RGB, RGB_TO_XYZ,
)
from test_spectral_batch_accumulate import fixture

SETTINGS = dict(radius=2, sigma=1.25, range_sigma=1., volume_blend=.65, caustic_blend=.25)


class LibraryIdentityTests(unittest.TestCase):
    def test_versioned_elf_aliases_bind_the_cpu_plugin_and_runtime_once(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            entry = root/'libOpenImageDenoise.so.2.5.1'
            cpu = root/'libOpenImageDenoise_device_cpu.so.2.5.1'
            thread = root/'libtbb.so.12.19'
            files = {entry: b'entry', cpu: b'cpu', thread: b'tbb'}
            for path, content in files.items():
                path.write_bytes(content)
            (root/'libOpenImageDenoise.so').symlink_to(entry.name)
            (root/'libOpenImageDenoise.so.2').symlink_to(entry.name)
            self.assertEqual(adjacent_library_identities(entry),
                             {str(path): digest(path) for path in files})
            cpu.write_bytes(b'changed')
            self.assertEqual(adjacent_library_identities(entry)[str(cpu)], digest(cpu))

    def test_mapped_identity_rejects_replaced_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)/'libtest.so.2'
            path.write_bytes(b'library')
            st = path.stat()
            device = (os.major(st.st_dev), os.minor(st.st_dev))
            self.assertEqual(digest_loaded_libraries({path: (*device, st.st_ino)}),
                             {str(path): digest(path)})
            with self.assertRaisesRegex(ValueError, 'differs from the loaded file'):
                digest_loaded_libraries({path: (*device, st.st_ino+1)})


class FilterTests(unittest.TestCase):
    def setUp(self):
        rng = np.random.default_rng(4321)
        self.a = rng.random((8, 9, 11, 3))
        self.a[..., 3, :] = self.a[..., 6:11, :].sum(-2)
        self.a[..., 0, :] = recombine(self.a)
        self.v = np.ones_like(self.a)
    def test_component_ownership_and_raw_immutable(self):
        before = self.a.copy()
        out = component_filter(self.a, self.v, SETTINGS, .019)
        np.testing.assert_array_equal(before, self.a)
        np.testing.assert_array_equal(out[..., 1:3, :], self.a[..., 1:3, :])
        np.testing.assert_array_equal(out[..., 3, :], out[..., 6:11, :].sum(-2))
        np.testing.assert_array_equal(out[..., 0, :], recombine(out))
        self.assertGreater(float(abs(out-self.a).max()), 0)
    def test_common_channel_and_child_weights(self):
        a = self.a.copy()
        for k in range(6, 11): a[..., k, :] = a[..., 7, 1, None]*np.array([2., 1., .5])*(k+1)
        a[..., 3, :] = a[..., 6:11, :].sum(-2); a[..., 0, :] = recombine(a)
        out = component_filter(a, self.v, SETTINGS, .019)
        np.testing.assert_allclose(out[..., 6:11, 0], 2*out[..., 6:11, 1], rtol=1e-14)
        np.testing.assert_allclose(out[..., 6:11, 2], .5*out[..., 6:11, 1], rtol=1e-14)
    def test_zero_variance_preserves_signal(self):
        out = component_filter(self.a, np.zeros_like(self.a), SETTINGS, .019)
        np.testing.assert_array_equal(out, self.a)
    def test_black_constant_and_tiny_dimensions(self):
        for shape in ((1, 1, 11, 3), (2, 3, 11, 3)):
            a = np.zeros(shape)
            np.testing.assert_array_equal(component_filter(a, np.ones(shape), SETTINGS, .019), a)
    def test_invalid_variance_refused(self):
        for bad in (-1., np.inf, np.nan):
            v = self.v.copy(); v[0, 0, 0, 0] = bad
            with self.assertRaises(ValueError): component_filter(self.a, v, SETTINGS, .019)
    def test_invalid_accounting_refused(self):
        a = self.a.copy(); a[0, 0, 0, 0] += 1
        with self.assertRaises(ValueError): validate_components(a)
        a = self.a.copy(); a[0, 0, 6, 0] += 1
        with self.assertRaises(ValueError): validate_components(a)
    def test_strength_radius_and_scale_guards(self):
        for key, value in (('radius', 5), ('radius', 1.5), ('sigma', 0), ('volume_blend', 1.1), ('range_sigma', np.nan)):
            s = dict(SETTINGS); s[key] = value
            with self.assertRaises(ValueError): component_filter(self.a, self.v, s, .019)
    def test_xyz_rgb_signed_identity(self):
        # Include saturated narrowband-like XYZ, black and HDR values.
        xyz = np.array([[[0., 0., 0.], [100., 1., .01], [.01, 1., 100.], [1., 100., .01]]])
        positive, residual = split_rgb(xyz)
        self.assertTrue(np.all(positive >= 0)); self.assertTrue(np.any(residual < 0))
        np.testing.assert_allclose(restore_xyz(positive, residual), xyz, rtol=1e-11, atol=1e-12)
        np.testing.assert_allclose(RGB_TO_XYZ @ XYZ_TO_RGB, np.eye(3), atol=1e-15)
    def test_float32_roundtrip_bounded(self):
        xyz = self.a[..., 0, :]*100
        p, r = split_rgb(xyz)
        self.assertLess(float(abs(restore_xyz(p.astype(np.float32), r)-xyz).max()), .0001)


class AdapterTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.root = Path(self.tmp.name)
        self.records = [fixture(self.root/str(i), i+1, value=i+1)[0] for i in range(4)]
        accumulate(self.records, self.root/'raw')
        self.manifest = self.root/'raw/manifest.json'
    def tearDown(self): self.tmp.cleanup()
    def test_two_batch_beauty_hdr_preserves_raw_and_has_no_variance_claim(self):
        accumulate(self.records[:2], self.root/'two')
        manifest = self.root/'two/manifest.json'
        before = digest(self.root/'two/components.npy')
        for mode, function in (('oidn', 'oidn_hdr'), ('oidn_signed', 'oidn_signed_hdr')):
            with patch('spectral_denoise_backend.'+function,
                       side_effect=lambda a,*args: (a.copy(), {'test_identity':True})):
                result = run(manifest, self.root/mode, mode, oidn_library='test-library')
            self.assertTrue(result['complete'])
            self.assertFalse(result['filtered_variance_claim'])
            self.assertEqual(before, digest(self.root/'two/components.npy'))
            np.testing.assert_array_equal(np.load(self.root/mode/'beauty_xyz.npy'),
                                          np.load(self.root/'two/components.npy')[..., 0, :])
    def test_two_and_three_batch_variance_guided_refused(self):
        for count in (2, 3):
            raw = self.root/f'raw-{count}'
            accumulate(self.records[:count], raw)
            with self.assertRaisesRegex(ValueError, 'At least four'):
                run(raw/'manifest.json', self.root/f'out-{count}', 'component', SETTINGS)
            self.assertFalse((self.root/f'out-{count}').exists())
            with self.assertRaisesRegex(ValueError, 'At least four'):
                load_accumulation(raw/'manifest.json')
    def test_invalid_minimum_batch_policy_refused(self):
        for minimum in (1, 3, True, 2.0):
            with self.assertRaisesRegex(ValueError, 'Supported minimum'):
                load_accumulation(self.manifest, minimum_batches=minimum)
    def test_two_batch_hdr_still_rejects_rehashed_false_mean(self):
        accumulate(self.records[:2], self.root/'two')
        path = self.root/'two/components.npy'; np.save(path, np.load(path)*2)
        manifest = self.root/'two/manifest.json'; m = json.loads(manifest.read_text())
        m['outputs'][path.name]['sha256'] = digest(path); manifest.write_text(json.dumps(m))
        with self.assertRaisesRegex(ValueError, 'mean differs'):
            run(manifest, self.root/'out', 'oidn_signed', oidn_library='unused')
        self.assertFalse((self.root/'out').exists())
    def test_success_receipt_hashes_and_no_overwrite(self):
        before = digest(self.root/'raw/components.npy')
        result = run(self.manifest, self.root/'out', 'component', SETTINGS)
        self.assertTrue(result['complete']); self.assertEqual(before, digest(self.root/'raw/components.npy'))
        for name, identity in result['outputs'].items(): self.assertEqual(identity['sha256'], digest(self.root/'out'/name))
        with self.assertRaisesRegex(ValueError, 'exists'): run(self.manifest, self.root/'out', 'component', SETTINGS)
    def test_hash_drift_refuses_before_output(self):
        p = self.root/'raw/components.npy'; data = bytearray(p.read_bytes()); data[-1] ^= 1; p.write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'hash'): run(self.manifest, self.root/'out', 'component', SETTINGS)
        self.assertFalse((self.root/'out').exists())
    def test_false_completed_marker_refused(self):
        m = json.loads(self.manifest.read_text()); m['complete'] = False; self.manifest.write_text(json.dumps(m))
        with self.assertRaises(ValueError): load_accumulation(self.manifest)
    def test_rehashed_wrong_mean_refused(self):
        p = self.root/'raw/components.npy'; a = np.load(p)*2; np.save(p, a)
        m = json.loads(self.manifest.read_text()); m['outputs'][p.name]['sha256'] = digest(p); self.manifest.write_text(json.dumps(m))
        with self.assertRaisesRegex(ValueError, 'mean differs'): load_accumulation(self.manifest)
    def test_rehashed_wrong_variance_refused(self):
        p = self.root/'raw/variance_of_mean.npy'; np.save(p, np.load(p)*2)
        m = json.loads(self.manifest.read_text()); m['outputs'][p.name]['sha256'] = digest(p); self.manifest.write_text(json.dumps(m))
        with self.assertRaisesRegex(ValueError, 'variance differs'): load_accumulation(self.manifest)
    def test_source_resource_drift_refused(self):
        (self.records[0].parent/'assets/mesh.txt').write_text('changed')
        with self.assertRaises(ValueError): load_accumulation(self.manifest)
    def test_insufficient_batches_refused(self):
        accumulate(self.records[:2], self.root/'two')
        with self.assertRaisesRegex(ValueError, 'four'): load_accumulation(self.root/'two/manifest.json')


class MetricTests(unittest.TestCase):
    def test_paired_reference_error_improvement_and_frozen_failure_screens(self):
        from spectral_denoise_metrics import evaluate, feature_metrics
        from spectral_error_budget import ROIS
        rng = np.random.default_rng(992)
        yy, xx = np.indices((192, 256))
        truth = (.3+xx*.001+yy*.002)[..., None]*np.array([1.1, 1., .8])
        batches = truth + rng.normal(0, .08, (4, *truth.shape))
        raw = batches.mean(0)
        refs = np.stack([truth[y:y+h, x:x+w].reshape(-1, 3) for x, y, w, h in ROIS.values()])
        refs = refs + rng.normal(0, .005, (16, *refs.shape))
        contract = dict(feature_windows={'caustic':[100,132,52,28], 'glint':[146,26,30,42],
                        'glass':[91,30,88,104], 'beam':[112,101,38,34], 'color_shadow':[75,77,42,42]},
                        preservation={'maximum_region_mean_drift_from_raw_relative':.02,
                        'maximum_global_mean_drift_relative':.01, 'maximum_region_uv1976_drift':.005,
                        'floor_caustic_centroid_shift_pixels':.5, 'floor_caustic_rms_width_drift_relative':.05,
                        'glint_peak_and_contrast_drift_relative':.05, 'glass_edge_gradient_energy_drift_relative':.05,
                        'beam_rms_width_drift_relative':.05},
                        quality={'aggregate_regional_XYZ_RMSE_reduction_min':.1,
                                 'maximum_individual_region_XYZ_RMSE_regression':.05})
        identity = evaluate(raw, raw, truth, batches, refs, contract, 1.)
        self.assertTrue(identity['gates']['preservation'])
        self.assertFalse(identity['gates']['aggregate_improvement'])
        self.assertFalse(identity['accepted'])
        clean = evaluate(raw, truth, truth, batches, refs, contract, 1.)
        self.assertGreater(clean['aggregate_RMSE_reduction'], .9)
        self.assertLess(clean['reference_only_paired_MSE_difference_bootstrap95'][1], 0.)
        dim = evaluate(raw, raw*.5, truth, batches, refs, contract, 1.)
        self.assertFalse(dim['gates']['preservation'])
        self.assertFalse(dim['features']['gates']['glint_peak'])
        self.assertAlmostEqual(dim['features']['shifts']['glint_peak_relative'], .5)
        self.assertFalse(dim['gates']['no_region_regression'])


if __name__ == '__main__': unittest.main()
