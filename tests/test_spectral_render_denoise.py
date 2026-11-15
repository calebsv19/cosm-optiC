import json
from pathlib import Path
import tempfile
import os
import sys
import subprocess
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import spectral_render_denoise as job


class Workflow(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name); self.request = self.base/'request.json'
        self.root = self.base/'output'; self.scene = self.base/'scene.json'
        self.scene.write_text('{}'); self.cli = self.base/'cli'; self.cli.write_text('native')
        self.library = self.base/'oidn'; self.library.write_text('runtime')
        self.data = dict(schema_version='ray_tracing_agent_render_request_v2',
                         scene=dict(runtime_scene_path='scene.json'),
                         render=dict(width=16, height=16, integrator_3d='disney_v3_spectral'),
                         output=dict(root='output', overwrite=False))
        self.save()

    def save(self): self.request.write_text(json.dumps(self.data))
    def invoke(self): return job.run(self.request, self.cli, self.library, 'filter', 10)
    def native(self, command, cwd, log, timeout):
        self.assertEqual(cwd, Path.cwd())
        cwd = self.root
        if '--preflight' in command:
            (cwd/'preflight.json').write_text(json.dumps(dict(spectral_transport=dict(
                route='surface_path_v1', status='prepared', linear_components=dict(enabled=True),
                quality=dict(component='total', display_transform='spectral_soft_v1', exposure=.019)))))
        if '--render' in command:
            (cwd/'frames').mkdir(); (cwd/'frames/frame_0000.bmp').write_bytes(b'raw')
            (cwd/'summary.json').write_text('{}')
    def denoise(self, summary, image, output, library, signed_policy):
        self.assertEqual(image.read_bytes(), b'raw'); self.assertEqual(signed_policy, 'filter')
        output.mkdir(); (output/'manifest.json').write_text('{"complete":true}')
        return dict(complete=True)

    def test_success_preserves_request_and_links_separate_result(self):
        before = self.request.read_bytes()
        with patch.object(job, 'oidn_hdr', return_value=(None, {})), patch.object(job, 'execute', side_effect=self.native) as native, patch.object(job, 'denoise_still', side_effect=self.denoise):
            result = self.invoke()
        self.assertTrue(result['complete']); self.assertEqual(native.call_count, 2)
        self.assertEqual(self.request.read_bytes(), before)
        self.assertEqual((self.root/'frames/frame_0000.bmp').read_bytes(), b'raw')
        self.assertEqual(result['denoise_manifest_sha256'], job.digest(self.root/'denoised/manifest.json'))
        with self.assertRaisesRegex(ValueError, 'already exists'): self.invoke()

    def test_unsupported_or_overwriting_request_refuses_before_backend(self):
        for key, value in [('frame_count', 2), ('denoise_enabled', True), ('width', 2000000)]:
            old = self.data['render'].copy(); self.data['render'][key] = value; self.save()
            with self.assertRaises(ValueError): self.invoke()
            self.data['render'] = old
        self.data['output']['overwrite'] = True; self.save()
        with self.assertRaises(ValueError): self.invoke()
        self.assertFalse(self.root.exists())

    def test_backend_failure_does_not_start_native_or_claim_output(self):
        with patch.object(job, 'oidn_hdr', side_effect=RuntimeError('backend unavailable')), patch.object(job, 'execute') as native:
            with self.assertRaisesRegex(RuntimeError, 'backend unavailable'): self.invoke()
        native.assert_not_called(); self.assertFalse(self.root.exists())

    def test_native_failure_never_denoises(self):
        with patch.object(job, 'oidn_hdr', return_value=(None, {})), patch.object(job, 'execute', side_effect=RuntimeError('native failed')), patch.object(job, 'denoise_still') as denoise:
            with self.assertRaises(RuntimeError): self.invoke()
        denoise.assert_not_called()
        self.assertFalse(json.loads((self.root/'denoise_job_state.json').read_text())['complete'])

    def test_filter_failure_preserves_completed_raw(self):
        with patch.object(job, 'oidn_hdr', return_value=(None, {})), patch.object(job, 'execute', side_effect=self.native), patch.object(job, 'denoise_still', side_effect=RuntimeError('filter failed')):
            with self.assertRaises(RuntimeError): self.invoke()
        self.assertEqual((self.root/'frames/frame_0000.bmp').read_bytes(), b'raw')
        self.assertEqual(json.loads((self.root/'denoise_job_state.json').read_text())['phase'], 'failed')

    def test_unsupported_native_route_stops_before_render(self):
        calls = []
        def calibration(command, cwd, log, timeout):
            calls.append(command)
            (self.root/'preflight.json').write_text('{"spectral_transport":{"route":"calibration"}}')
        with patch.object(job, 'oidn_hdr', return_value=(None, {})), patch.object(job, 'execute', side_effect=calibration):
            with self.assertRaisesRegex(ValueError, 'ordinary total V3'): self.invoke()
        self.assertEqual(len(calls), 1); self.assertIn('--preflight', calls[0])

    def test_real_native_timeout_reaps_owned_process(self):
        log = self.base/'timeout.log'
        with self.assertRaises(subprocess.TimeoutExpired):
            job.execute([sys.executable, '-c', 'import time; time.sleep(30)'], self.base, log, .05)
        pid = json.loads(Path(str(log)+'.process.json').read_text())['pid']
        with self.assertRaises(ProcessLookupError): os.kill(pid, 0)

    def test_real_native_nonzero_exit_is_reported(self):
        with self.assertRaisesRegex(RuntimeError, 'exited 7'):
            job.execute([sys.executable, '-c', 'raise SystemExit(7)'], self.base, self.base/'exit.log', 5)

    def test_input_drift_prevents_filter(self):
        def changed(*args):
            self.native(*args)
            if '--render' in args[0]: self.scene.write_text('{"changed":true}')
        with patch.object(job, 'oidn_hdr', return_value=(None, {})), patch.object(job, 'execute', side_effect=changed), patch.object(job, 'denoise_still') as denoise:
            with self.assertRaisesRegex(ValueError, 'changed during job'): self.invoke()
        denoise.assert_not_called()


if __name__ == '__main__': unittest.main()
