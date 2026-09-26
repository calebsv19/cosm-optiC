#!/usr/bin/env python3
"""Scene-relative placement contract for offset STLs and outlier scenes."""
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from managed_mesh_placement import place_new_instance


class PlacementTest(unittest.TestCase):
    def test_offset_mesh_fits_floor_despite_existing_outlier(self):
        plane = {
            'object_type': 'plane_primitive',
            'flags': {'visible': True},
            'primitive': {
                'width': 160, 'height': 160,
                'frame': {'origin': {'x': 5, 'y': 7, 'z': 0},
                          'axis_u': {'x': 0, 'y': -1, 'z': 0},
                          'axis_v': {'x': 1, 'y': 0, 'z': 0},
                          'normal': {'x': 0, 'y': 0, 'z': 1}},
            },
        }
        asset = {'original_name': 'BodyParts3D_Skull.stl'}
        scene = {'objects': [plane, {'object_type': 'mesh_asset_instance',
                                    'transform': {'position': {'x': 0, 'y': 0, 'z': 1500}}}],
                 'extensions': {'ray_tracing': {'managed_mesh_assets': {'assets': {'skull': asset}}}}}
        obj = {'extensions': {'ray_tracing': {'managed_mesh': {'asset_id': 'skull'}}}}
        with tempfile.TemporaryDirectory() as directory:
            runtime = Path(directory) / 'skull.runtime.json'
            runtime.write_text(json.dumps({'local_bounds': {
                'min': {'x': -76, 'y': -191, 'z': 1422},
                'max': {'x': 75, 'y': 18, 'z': 1636}}}))
            result = place_new_instance(scene, obj, runtime)
        self.assertEqual(obj['display_name'], 'BodyParts3D_Skull')
        self.assertEqual(obj['transform']['pivot_policy'], 'bounds_center')
        self.assertAlmostEqual(max(result['size'].values()), 24)
        position = obj['transform']['position']
        factor = obj['transform']['scale']['x']
        center_x = position['x'] + (-76 + 75) * factor / 2
        center_y = position['y'] + (-191 + 18) * factor / 2
        self.assertLess(abs(center_x - 5), 50)
        self.assertLess(abs(center_y - 7), 50)
        self.assertGreater(math.hypot(center_x, center_y), 20)
        self.assertAlmostEqual(position['z'] + 1422 * factor, 0)
        self.assertLess(position['z'] + 1636 * factor, 80)

    def test_empty_scene_uses_one_meter_visible_object(self):
        scene = {'objects': [], 'extensions': {'ray_tracing': {'managed_mesh_assets': {
            'assets': {'mesh': {'original_name': 'off-origin.stl'}}}}}}
        obj = {'extensions': {'ray_tracing': {'managed_mesh': {'asset_id': 'mesh'}}}}
        with tempfile.TemporaryDirectory() as directory:
            runtime = Path(directory) / 'mesh.runtime.json'
            runtime.write_text(json.dumps({'local_bounds': {
                'min': {'x': 100, 'y': 100, 'z': 100},
                'max': {'x': 110, 'y': 120, 'z': 130}}}))
            result = place_new_instance(scene, obj, runtime)
        self.assertAlmostEqual(max(result['size'].values()), 0.15)
        self.assertAlmostEqual(obj['transform']['position']['z'] + 100 * obj['transform']['scale']['z'], 0)


if __name__ == '__main__':
    unittest.main()
