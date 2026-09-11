#!/usr/bin/env python3
"""Managed asset portability, failure atomicity, shading, and render proof."""
import copy
import json
import platform
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from managed_mesh_assets import update, status, catalog, digest
from smooth_mesh_reflection.prepare_reflection_matrix import build_scene, build_request
COMPILER = ROOT / f'build/toolchains/clang/{platform.machine()}/tools/smooth_mesh_reflection/compile_runtime_fixture'
RENDERER = ROOT / f'build/toolchains/clang/{platform.machine()}/tools/cli/ray_tracing_render_headless'


class ManagedMeshTest(unittest.TestCase):
    def test_cli_can_spawn_first_class_managed_instance(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / 'project'
            project.mkdir()
            scene = project / 'scene_runtime.json'
            source_scene = build_scene({f: f for f in ('crease', 'analytic_sphere', 'icosphere', 'organic_blob')})
            source_scene['objects'] = []
            source_scene['extensions']['ray_tracing']['authoring']['object_materials'] = []
            scene.write_text(json.dumps(source_scene))
            original_scene_bytes = scene.read_bytes()
            candidate = project / '.managed-ui-candidate.json'
            source = ROOT / 'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/imports/tetrahedron_ascii.stl'
            subprocess.run([
                sys.executable,
                str(ROOT / 'tools/managed_mesh_assets.py'),
                'apply',
                '--scene', str(scene),
                '--compiler', str(COMPILER),
                '--source', str(source),
                '--asset-id', 'ui_import_tetrahedron',
                '--spawn-object-id', 'ui_import_tetrahedron',
                '--default-mode', 'flat',
                '--output-scene', str(candidate),
            ], check=True, capture_output=True)
            self.assertEqual(scene.read_bytes(), original_scene_bytes)
            self.assertEqual(status(candidate, COMPILER)['status'], 'ready')
            candidate.replace(scene)
            saved = json.loads(scene.read_text())
            spawned = next(obj for obj in saved['objects'] if obj['object_id'] == 'ui_import_tetrahedron')
            self.assertEqual(spawned['object_type'], 'mesh_asset_instance')
            self.assertEqual(spawned['extensions']['ray_tracing']['managed_mesh']['asset_id'],
                             'ui_import_tetrahedron')
            self.assertEqual(status(scene, COMPILER)['status'], 'ready')
            # Match the retained editor command sequence before a fresh renderer process.
            spawned['transform'] = {
                'position': {'x': 0.4, 'y': 0.3, 'z': 1.2},
                'rotation': {'x': 12.0, 'y': 28.0, 'z': 7.0},
                'scale': {'x': 0.8, 'y': 1.1, 'z': 0.9},
            }
            authoring = saved.setdefault('extensions', {}).setdefault('ray_tracing', {}).setdefault('authoring', {})
            object_materials = authoring.setdefault('object_materials', [])
            object_materials.append({'object_id': 'ui_import_tetrahedron',
                                     'material_id': 0,
                                     'object_color': 0x4C8ED9,
                                     'roughness': 0.32})
            scene.write_text(json.dumps(saved, indent=2, sort_keys=True) + '\n')
            update(scene, COMPILER, object_id='ui_import_tetrahedron',
                   shading='crease_aware', crease_angle=52.0)
            request = build_request(project, 'foundation_a', 'tlas_blas_parity')
            request['render'].update(width=96, height=64)
            request_path = project / 'request.json'
            request_path.write_text(json.dumps(request))
            subprocess.run([str(RENDERER), '--request', str(request_path), '--render'],
                           check=True, capture_output=True)
            first_frame = project / 'renders/foundation_a_tlas_blas_parity/frames/frame_0000.bmp'
            first_hash = digest(first_frame)
            relocated = project.parent / 'relocated_foundation_a'
            project.rename(relocated)
            relocated_request = build_request(relocated, 'foundation_a_reopen', 'tlas_blas_parity')
            relocated_request['render'].update(width=96, height=64)
            relocated_request_path = relocated / 'request_reopen.json'
            relocated_request_path.write_text(json.dumps(relocated_request))
            subprocess.run([str(RENDERER), '--request', str(relocated_request_path), '--render'],
                           check=True, capture_output=True)
            reopened_frame = relocated / 'renders/foundation_a_reopen_tlas_blas_parity/frames/frame_0000.bmp'
            self.assertEqual(digest(reopened_frame), first_hash)
            reopened = json.loads((relocated / 'scene_runtime.json').read_text())
            reopened_object = next(obj for obj in reopened['objects']
                                   if obj['object_id'] == 'ui_import_tetrahedron')
            self.assertEqual(reopened_object['transform']['rotation']['y'], 28.0)
            self.assertEqual(status(relocated / 'scene_runtime.json', COMPILER)['status'], 'ready')

    def test_existing_project_validation(self):
        from scene_project_contract import validate_project
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / 'project'
            shutil.copytree(ROOT / 'tests/fixtures/scene_project_worker_snapshot', project)
            scene = project / 'scene_runtime.json'
            obj = json.loads(scene.read_text())['objects'][0]
            source = ROOT / 'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/imports/tetrahedron_ascii.stl'
            update(scene, COMPILER, source=source, asset_id='managed_fixture', object_id=obj['object_id'], default_mode='crease_aware')
            report = validate_project(project)
            self.assertEqual(report['status'], 'ok')
            self.assertTrue(any('managed_mesh_source' in f['roles'] for f in report['content']['files']))

    def test_portable_rebuild_and_failure_atomicity(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            project = temp / 'project'; project.mkdir()
            scene = project / 'scene_runtime.json'
            original = build_scene({f: f for f in ('crease', 'analytic_sphere', 'icosphere', 'organic_blob')})
            original['objects'] = [original['objects'][-4]]
            object_id = original['objects'][0]['object_id']
            scene.write_text(json.dumps(original))
            self.assertEqual(status(scene)['status'], 'legacy_unmanaged')
            source = temp / 'external.stl'
            subprocess.run([sys.executable, str(ROOT / 'tools/smooth_mesh_reflection/generate_fixtures.py'),
                '--family', 'organic_blob', '--tier', 'unit', '--output', str(source)], check=True, capture_output=True)
            update(scene, COMPILER, source=source, asset_id='dragon', object_id=object_id, default_mode='smooth')
            first = json.loads(scene.read_text())
            obj = copy.deepcopy(first['objects'][0]); obj['object_id'] = 'flat_instance'
            obj['transform']['position']['x'] += 2
            update(scene, COMPILER, asset_id='dragon', shading='flat', spawn=obj)
            saved = json.loads(scene.read_text())
            self.assertNotEqual(saved['objects'][0]['geometry_ref'], saved['objects'][1]['geometry_ref'])
            self.assertEqual(len(catalog(saved)['assets']), 1)
            report = status(scene, COMPILER)
            self.assertEqual([x['effective_shading']['mode'] for x in report['objects']], ['smooth', 'flat'])
            update(scene, COMPILER, smoothing_enabled=False)
            self.assertEqual([x['effective_shading']['mode'] for x in status(scene)['objects']], ['flat', 'flat'])
            update(scene, COMPILER, smoothing_enabled=True)
            self.assertEqual([x['effective_shading']['mode'] for x in status(scene)['objects']], ['smooth', 'flat'])
            # Failed compile cannot change the active scene.
            before = scene.read_bytes()
            tampered = json.loads(before)
            catalog(tampered)['assets']['dragon']['source'] = '../external.stl'
            scene.write_text(json.dumps(tampered))
            with self.assertRaises(ValueError):
                status(scene)
            scene.write_bytes(before)
            tampered = json.loads(before)
            tampered['objects'][0]['geometry_ref']['id'] = 'wrong_asset'
            scene.write_text(json.dumps(tampered))
            with self.assertRaises(ValueError):
                status(scene)
            scene.write_bytes(before)
            bad = temp / 'invalid.stl'; bad.write_text('not a mesh')
            with self.assertRaises(subprocess.CalledProcessError):
                update(scene, COMPILER, source=bad, asset_id='bad', object_id=object_id)
            self.assertEqual(scene.read_bytes(), before)
            with self.assertRaises(ValueError):
                update(scene, COMPILER, object_id=object_id, shading='crease_aware', crease_angle=float('nan'))
            self.assertEqual(scene.read_bytes(), before)
            # Capture original-location output, then remove both source and caches.
            baseline_request = build_request(project, 'smooth', 'tlas_blas_parity')
            baseline_request['render'].update(width=96, height=64)
            baseline_request['inspection'].update(camera_position={'x': 0, 'y': -8, 'z': 2}, camera_look_at={'x': 0, 'y': 0, 'z': 1})
            baseline_path = project / 'request.json'
            baseline_path.write_text(json.dumps(baseline_request))
            subprocess.run([str(RENDERER), '--request', str(baseline_path), '--render', '--summary', str(project / 'summary.json')], check=True, capture_output=True)
            baseline_hash = digest(project / 'renders/smooth_tlas_blas_parity/frames/frame_0000.bmp')
            # Source disappears, project moves, all derived geometry/cache disappears.
            source.unlink()
            relocated = temp / 'relocated'
            project.rename(relocated)
            scene = relocated / 'scene_runtime.json'
            hashes = {p.name: digest(p) for p in (relocated / 'assets/mesh_assets').glob('*.json')}
            shutil.rmtree(relocated / 'renders')
            shutil.rmtree(relocated / 'assets/mesh_assets')
            self.assertEqual(status(scene, COMPILER)['status'], 'rebuild_required')
            update(scene, COMPILER)
            self.assertEqual(status(scene, COMPILER)['status'], 'ready')
            from scene_project_contract import validate_explicit_paths
            self.assertEqual(validate_explicit_paths(scene)['status'], 'ok')
            self.assertEqual(hashes, {p.name: digest(p) for p in (relocated / 'assets/mesh_assets').glob('*.json')})
            # Normal compiled JSON remains portable; no old project paths.
            self.assertNotIn(str(project), scene.read_text())
            request = build_request(relocated, 'smooth', 'tlas_blas_parity')
            request['render'].update(width=96, height=64)
            request['inspection'].update(camera_position={'x': 0, 'y': -8, 'z': 2}, camera_look_at={'x': 0, 'y': 0, 'z': 1})
            request_path = relocated / 'request.json'; request_path.write_text(json.dumps(request))
            summary = relocated / 'summary.json'
            subprocess.run([str(RENDERER), '--request', str(request_path), '--render', '--summary', str(summary)], check=True, capture_output=True)
            result = json.loads(summary.read_text())
            self.assertTrue(result['rendered_frames'])
            self.assertGreater(result['bvh_summary']['triangle_count'], 0)
            out = ROOT / 'build/managed_mesh_proof'; out.mkdir(exist_ok=True)
            (out / 'readback.json').write_text(json.dumps({'status': status(scene),
                'rebuilt_hashes_match': True, 'original_removed': True, 'project_relocated': True,
                'triangle_count': result['bvh_summary']['triangle_count'],
                'route_parity_mismatches': result['prepared_acceleration']['route_parity_mismatches']}, indent=2))
            shutil.copy2(summary, out / 'render_summary.json')
            frame = relocated / 'renders/smooth_tlas_blas_parity/frames/frame_0000.bmp'
            self.assertEqual(digest(frame), baseline_hash)
            self.assertEqual(result['prepared_acceleration']['route_parity_mismatches'], 0)
            self.assertGreater(len(set(frame.read_bytes()[122:])), 4)
            shutil.copy2(frame, out / 'frame_0000.bmp')
            (out / 'relocation_image_parity.json').write_text(json.dumps({'before_sha256': baseline_hash, 'after_sha256': digest(frame), 'equal': True}, indent=2))


if __name__ == '__main__':
    unittest.main()
