"""Project ownership, dependency containment and approval regression tests."""
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import scene_workflow as workflow
from scene_workflow_bundle import REPO, read, sha, verify, write


class WorkflowTests(unittest.TestCase):
    def test_retained_atmosphere_complete_cache_dependency_snapshot(self):
        from scene_workflow_bundle import snapshot_inputs
        cache=self.source/'assets'/'atmosphere'
        shutil.copytree(REPO/'tests/fixtures/scene_project_worker_snapshot/assets/vf3d/active',cache)
        scene_path=self.source/'scene_runtime.json';scene=read(scene_path)
        scene.setdefault('extensions',{}).setdefault('ray_tracing',{}).setdefault('authoring',{})['atmosphere_attachment']={
            'schema_version':'ray_atmosphere_attachment_v1','source_path':'assets/atmosphere/manifest.json'}
        write(scene_path,scene)
        destination=self.root/'atmosphere-snapshot'
        snapshot_inputs(scene_path,self.request,destination)
        for index in (0,2,4):
            rel=f'assets/atmosphere/frame_{index:06d}.vf3d'
            self.assertEqual((destination/rel).read_bytes(),(self.source/rel).read_bytes())
        doc=read(cache/'manifest.json');doc['frames'][0]['path']='missing.vf3d';write(cache/'manifest.json',doc)
        with self.assertRaises(ValueError):snapshot_inputs(scene_path,self.request,self.root/'refused')
        self.assertFalse((self.root/'refused').exists())
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        shutil.copytree(REPO / 'tests/fixtures/mesh_asset_runtime_spheres', self.source)
        self.request = self.root / 'intent.json'
        write(self.request, {'schema_version': 'ray_tracing_agent_render_request_v1',
            'scene': {'runtime_scene_path': 'ignored'}, 'volume': {'enabled': False},
            'render': {'width': 160, 'height': 96, 'start_frame': 0, 'frame_count': 200, 'temporal_frames': 8, 'integrator_3d': 'disney_v2'},
            'output': {'root': '/elsewhere', 'video': {'enabled': True, 'fps': 24}},
            'inspection': {'camera_zoom': 1.1}})
        self.project = self.root / 'project'
        self.renderer = self.root / 'renderer'; self.renderer.write_bytes(b'fixture-renderer')

    def tearDown(self): self.temp.cleanup()

    def init(self):
        return workflow.init(self.project, self.source / 'scene_runtime.json', self.request, 'Test')

    def test_relocated_bundle_preserves_scene_and_intent(self):
        self.init(); workflow.capture(self.project, 'r1', self.renderer)
        frozen = self.project / 'revisions/r1'
        scene = read(frozen / 'scene_runtime.json')
        for obj in scene['objects']:
            if obj['object_type'] == 'mesh_asset_instance':
                self.assertTrue((frozen / obj['extensions']['line_drawing']['runtime_mesh_path']).is_file())
        self.assertEqual(read(frozen / 'render_request.json')['render'], read(self.request)['render'])
        self.assertEqual(read(frozen / 'render_request.json')['inspection'], read(self.request)['inspection'])
        relocated = self.root / 'relocated'; shutil.copytree(frozen, relocated)
        shutil.rmtree(self.source); shutil.rmtree(self.project)
        self.assertTrue(verify(relocated)['verified'])
        (relocated / 'assets/mesh_assets/asset_sphere_16x8.runtime.json').write_text('{}')
        with self.assertRaisesRegex(ValueError, 'digest mismatch'): verify(relocated)

    def test_missing_external_and_symlink_dependencies_fail_without_project(self):
        for value in ('assets/missing.runtime.json', '../external.json', '/tmp/external.json', 'assets/link.json'):
            scene = read(self.source / 'scene_runtime.json')
            scene['objects'][1].setdefault('extensions', {}).setdefault('line_drawing', {})['runtime_mesh_path'] = value
            write(self.source / 'scene_runtime.json', scene)
            (self.source / 'assets/link.json').unlink(missing_ok=True)
            (self.source / 'assets/link.json').symlink_to(self.request)
            with self.assertRaises(ValueError): self.init()
            self.assertFalse(self.project.exists())

    def test_revisions_are_immutable_and_approval_requires_exact_preview(self):
        self.init(); workflow.capture(self.project, 'r1', self.renderer)
        with self.assertRaises(ValueError): workflow.capture(self.project, 'r1', self.renderer)
        with self.assertRaisesRegex(ValueError, 'approval'): workflow.export(self.project, 'r1', self.root / 'final')
        data = workflow.load(self.project)
        data['previews']['p1'] = {'state': 'completed', 'revision_id': 'wrong', 'input_manifest_sha256': 'x', 'artifacts': [], 'frames': [0]}
        workflow.save(self.project, data)
        with self.assertRaises(ValueError): workflow.accept(self.project, 'r1', 'p1', 'reviewed')
        data['previews']['p1'].update(revision_id='r1', input_manifest_sha256=sha(self.project / 'revisions/r1/bundle_manifest.json'))
        artifact = self.project / 'previews/test.bmp'; artifact.parent.mkdir(); artifact.write_bytes(b'fixture')
        data['previews']['p1']['artifacts'] = [{'path': 'previews/test.bmp', 'sha256': sha(artifact)}]
        workflow.save(self.project, data)
        workflow.accept(self.project, 'r1', 'p1', 'unit-test approval only')
        self.assertTrue(workflow.export(self.project, 'r1', self.root / 'final')['verified'])
        scene = read(self.project / 'scene_runtime.json'); scene['test_edit'] = True
        write(self.project / 'scene_runtime.json', scene)
        self.assertTrue(workflow.status(self.project)['editable_changed_since_accepted'])
        artifact.write_bytes(b'tampered')
        with self.assertRaises(ValueError): workflow.accept(self.project, 'r1', 'p1', 'changed')

    def test_recursive_dependencies_and_source_files(self):
        scene = read(self.source / 'scene_runtime.json')
        scene['extensions']['custom'] = {'source_path': 'assets/extra.json'}
        write(self.source / 'assets/extra.json', {'image_path': 'assets/image.bin'})
        (self.source / 'assets/image.bin').write_bytes(b'image')
        write(self.source / 'scene_runtime.json', scene)
        self.init(); workflow.capture(self.project, 'r1', self.renderer)
        self.assertTrue((self.project / 'revisions/r1/assets/image.bin').is_file())

    def test_diagnostic_export_does_not_approve_and_tampered_manifest_fails(self):
        self.init(); workflow.capture(self.project, 'r1', self.renderer)
        result = workflow.export(self.project, 'r1', self.root / 'diagnostic', 'diagnostic')
        self.assertFalse(result['visually_approved'])
        self.assertIsNone(workflow.load(self.project)['accepted_revision'])
        path = self.project / 'revisions/r1/bundle_manifest.json'
        data = read(path); data['revision_id'] = 'different'; write(path, data)
        with self.assertRaisesRegex(ValueError, 'manifest changed'):
            workflow.export(self.project, 'r1', self.root / 'tampered', 'diagnostic')

    def test_oversampled_short_clip_includes_last_frame(self):
        self.init()
        req = read(self.project / 'render_request.json'); req['render']['frame_count'] = 3
        write(self.project / 'render_request.json', req)
        workflow.capture(self.project, 'r1', self.renderer)
        with patch('scene_workflow.subprocess.run', side_effect=OSError('fixture stop')):
            with self.assertRaises(OSError):
                workflow.preview(self.project, 'r1', 'short', self.renderer, 'sampled', 20, 80, 48)
        self.assertEqual(workflow.load(self.project)['previews']['short']['frames'], [0, 1, 2])

    def test_render_failure_retains_receipt_without_approval(self):
        self.init(); workflow.capture(self.project, 'r1', self.renderer)
        with patch('scene_workflow.subprocess.run', side_effect=OSError('fixture failure')):
            with self.assertRaises(OSError): workflow.preview(self.project, 'r1', 'p1', self.renderer, 'sampled', 3, 80, 48)
        data = workflow.load(self.project)
        self.assertEqual(data['previews']['p1']['state'], 'failed')
        self.assertEqual(data['previews']['p1']['frames'], [0, 100, 199])
        self.assertIsNone(data['accepted_revision'])
        self.assertTrue((self.project / 'previews/p1/preview_receipt.json').is_file())


if __name__ == '__main__': unittest.main()
