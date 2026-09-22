#!/usr/bin/env python3
"""Pure portable dependency tests; optional real compiler/preflight/render lane."""
import argparse
import copy
import hashlib
import json
import shutil
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import surface_material_resources as resources
from surface_material_candidate import candidate


def png():
    def chunk(kind, value):
        return struct.pack('>I', len(value)) + kind + value + struct.pack('>I', zlib.crc32(kind + value) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(b'\0' + b'\x80\x80\xff\xff' * 2 + b'\0' + b'\x80\x80\xff\xff' * 2)) + chunk(b'IEND', b'')


def fixture(root):
    root.mkdir(parents=True, exist_ok=True)
    image = root / 'paint.png'
    image.write_bytes(png())
    scene = json.loads((ROOT / 'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    obj = copy.deepcopy(scene['objects'][0])
    obj['object_id'] = 'surface'
    obj.pop('extensions', None)
    mapping = {'version': 1, 'required_capability': 'optic.planar_surface_v1', 'method': 'planar',
        'space': 'object_rest', 'source_domain': 'brick_cells_v1', 'scale_policy': 'stretch_with_object',
        'origin_m': [0, 0, 0], 'axis_u': [1, 0, 0], 'axis_v': [0, 1, 0], 'tile_m': [.5, .5],
        'offset_m': [0, 0], 'pivot_m': [0, 0], 'rotation_rad': 0, 'seed': 1729}
    image_ref = {'path': str(image), 'sha256': resources.digest(image), 'color_space': 'linear', 'producer': {'keep': [1, 2]}}
    sampling = {'version': 1, 'required_capability': 'optic.surface_sampling_v1', 'filter': 'trilinear',
                'address': 'repeat', 'color_space': 'linear', 'period_tiles': [1, 1],
                'channels': {'base_color': image_ref, 'roughness': dict(image_ref, color_space='data')}}
    obj['extensions'] = {'ray_tracing': {'surface_mapping': mapping, 'surface_sampling': sampling}}
    scene['objects'] = [obj]
    scene['extensions']['ray_tracing']['authoring']['object_materials'] = [{'object_id': 'surface',
        'material_id': 0, 'object_color': 12756864, 'roughness': .8, 'reflectivity': .02,
        'material_texture_stack': {'layers': [{'id': 'brick', 'kind': 'brick', 'placement': {'scale': 1, 'strength': 1}}]}}]
    scene['extensions']['t2_unknown_metadata'] = {'unchanged': 'retained producer'}
    path = root / 'scene_runtime.json'
    path.write_bytes(resources.encode(scene))
    return path


class ResourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.scene = fixture(self.root / 'project')
        self.output = self.root / 'bundle'

    def tearDown(self):
        self.temp.cleanup()

    def bundle(self):
        return resources.bundle(self.scene, self.output, resources.digest(self.scene))

    def test_deduplicated_portable_relocation_and_metadata(self):
        before = self.scene.read_bytes()
        self.bundle()
        receipt = resources.verify(self.output)
        self.assertEqual(len(list((self.output / 'dependencies/surface_image').iterdir())), 1)
        self.assertEqual({b['color_space'] for b in receipt['bindings']}, {'linear', 'data'})
        result = json.loads((self.output / 'scene_runtime.json').read_text())
        self.assertEqual(result['extensions']['t2_unknown_metadata'], json.loads(before)['extensions']['t2_unknown_metadata'])
        channels = result['objects'][0]['extensions']['ray_tracing']['surface_sampling']['channels']
        self.assertEqual(channels['base_color']['producer'], {'keep': [1, 2]})
        moved = self.root / 'relocated'
        self.output.rename(moved)
        shutil.rmtree(self.scene.parent)
        resources.verify(moved)

    def test_generated_uv_authoring_support_relocates(self):
        source = self.scene.parent / 'projected.obj'
        source.write_text('v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n')
        metadata = {'source_sha256': 'a' * 64, 'output_sha256': resources.digest(source),
                    'triangle_charts': [{'chart_id': 'z+'}]}
        (self.scene.parent / 'projection.uv.json').write_bytes(resources.encode(metadata))
        scene = json.loads(self.scene.read_text())
        scene['objects'][0]['extensions']['ray_tracing']['generated_uv_history'] = [{
            'recipe': {'source_sha256': 'a' * 64}, 'projection': 'projection.uv.json',
            'projected_source': 'projected.obj'}]
        self.scene.write_bytes(resources.encode(scene))
        self.bundle()
        shutil.rmtree(self.scene.parent)
        resources.verify(self.output)
        result = json.loads((self.output / 'scene_runtime.json').read_text())
        history = result['objects'][0]['extensions']['ray_tracing']['generated_uv_history'][0]
        self.assertTrue((self.output / history['projected_source']).is_file())
        self.assertEqual(json.loads((self.output / history['projection']).read_text()), metadata)

    def test_create_only(self):
        self.bundle()
        before = (self.output / 'scene_runtime.json').read_bytes()
        with self.assertRaises(ValueError): self.bundle()
        self.assertEqual((self.output / 'scene_runtime.json').read_bytes(), before)

    def test_stale_scene_and_image_pins(self):
        with self.assertRaises(ValueError): resources.bundle(self.scene, self.output, '0' * 64)
        self.assertFalse(self.output.exists())
        (self.scene.parent / 'paint.png').write_bytes(b'corrupt')
        with self.assertRaises(ValueError): self.bundle()
        self.assertFalse(self.output.exists())

    def test_bundle_tamper(self):
        self.bundle()
        next((self.output / 'dependencies/surface_image').iterdir()).write_bytes(b'corrupt')
        with self.assertRaises(ValueError): resources.verify(self.output)

    def test_scene_tamper(self):
        self.bundle()
        (self.output / 'scene_runtime.json').write_text('{}')
        with self.assertRaises(ValueError): resources.verify(self.output)

    def test_traversal_and_symlink(self):
        scene = json.loads(self.scene.read_text())
        channel = scene['objects'][0]['extensions']['ray_tracing']['surface_sampling']['channels']['base_color']
        for path in ('../paint.png', 'images/../../paint.png'):
            channel['path'] = path
            with self.assertRaises(ValueError): resources.select_dependencies(scene, self.scene.parent)
        (self.scene.parent / 'linked.png').symlink_to(self.scene.parent / 'paint.png')
        channel['path'] = 'linked.png'
        with self.assertRaises(ValueError): resources.select_dependencies(scene, self.scene.parent)

    def test_corrupt_interpretation_and_unsupported_dependencies(self):
        scene = json.loads(self.scene.read_text())
        scene['objects'][0]['extensions']['ray_tracing']['surface_sampling']['channels']['roughness']['color_space'] = 'srgb'
        with self.assertRaises(ValueError): resources.select_dependencies(scene, self.scene.parent)
        scene = json.loads(self.scene.read_text())
        scene['objects'][0]['procedural_solid_material_ref'] = {'graph_path': 'missing'}
        with self.assertRaises(ValueError): resources.select_dependencies(scene, self.scene.parent)

    def test_candidate_rejects_stale_or_nonlocal_output_without_publication(self):
        obj = self.scene.parent / 'source.obj'
        obj.write_text('v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n')
        local = self.scene.parent / 'candidate.json'
        with self.assertRaisesRegex(ValueError, 'stale'):
            candidate(self.scene, '0' * 64, obj, resources.digest(obj), 'surface', local,
                      Path(sys.executable), Path(sys.executable))
        with self.assertRaisesRegex(ValueError, 'share'):
            candidate(self.scene, resources.digest(self.scene), obj, resources.digest(obj), 'surface',
                      self.root / 'outside.json', Path(sys.executable), Path(sys.executable))
        self.assertFalse(local.exists())
        self.assertFalse(local.with_suffix('.json.receipt.json').exists())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root', type=Path)
    parser.add_argument('--renderer', type=Path)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--mesh-scene', type=Path)
    parser.add_argument('--mesh-source', type=Path)
    args = parser.parse_args()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(ResourceTests))
    if not result.wasSuccessful(): return 1
    if args.renderer:
        if not args.output_root: parser.error('--output-root required for executable integration')
        out = args.output_root.resolve()
        out.mkdir(parents=True, exist_ok=False)
        scene = fixture(out / 'project')
        receipt = resources.bundle(scene, out / 'bundle', resources.digest(scene), args.renderer)
        moved = out / 'relocated'
        (out / 'bundle').rename(moved)
        shutil.rmtree(scene.parent)
        resources.verify(moved)
        resources.validate_runtime(moved / 'scene_runtime.json', args.renderer, out / 'relocated-validation', True)
        results = {'pure_tests': result.testsRun, 'relocated_preflight_render': True, 'bundle': receipt}
        if args.compiler and args.mesh_scene and args.mesh_source:
            project = out / 'uv_project'
            shutil.copytree(args.mesh_scene.resolve().parent, project,
                            ignore=shutil.ignore_patterns('renders', '*.ppm', '*.bmp', '*.log', 'cache', 'data'))
            mesh_scene = project / args.mesh_scene.name
            mesh_source = project / 't2-source.obj'
            mesh_source.write_bytes(args.mesh_source.read_bytes())
            initial_hash = resources.digest(mesh_scene)
            candidate_path = project / 'uv_candidate.json'
            results['uv_candidate'] = candidate(mesh_scene, initial_hash, mesh_source,
                resources.digest(mesh_source), 'surface', candidate_path, args.compiler, args.renderer)
            assert resources.digest(mesh_scene) == initial_hash
            resources.bundle(candidate_path, out / 'uv_bundle', resources.digest(candidate_path), args.renderer)
            moved_uv = out / 'uv_relocated'
            (out / 'uv_bundle').rename(moved_uv)
            resources.verify(moved_uv)
            resources.validate_runtime(moved_uv / 'scene_runtime.json', args.renderer, out / 'uv-relocated-validation', True)
            results['candidate_path'] = str(candidate_path)
            results['original_scene_path'] = str(mesh_scene)
        (out / 'acceptance.json').write_bytes(resources.encode(results))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
