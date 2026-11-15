"""Physical constraints and matched-scene invariants for the visible bench."""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import spectral_response_fixture as fixture


class SpectralResponseFixtureTests(unittest.TestCase):
    def test_visible_support_passivity_and_normalization(self):
        sources = fixture.sources()
        target = fixture.xyz(sources['white_broad'])[1]
        for curve in sources.values():
            self.assertLessEqual(len(curve), 32)
            self.assertTrue(all(a[0] < b[0] for a, b in zip(curve, curve[1:])))
            self.assertTrue(all(power >= 0 for _, power in curve))
            self.assertTrue(all(power == 0 for nm, power in curve if nm <= 400))
            self.assertAlmostEqual(fixture.xyz(curve)[1]/target, 1, places=10)
        for rho in fixture.reflectances().values():
            self.assertTrue(all(0 <= value <= 1 for _, value in rho))

    def test_metamer_matches_white_but_changes_selective_response(self):
        sources = fixture.sources()
        broad, peaks = sources['white_broad'], sources['white_three_peak']
        for a, b in zip(fixture.xyz(broad), fixture.xyz(peaks)):
            self.assertAlmostEqual(a, b, places=10)
        rho = fixture.reflectances()
        neutral = fixture.xyz(peaks, rho['neutral'])[1]/fixture.xyz(broad, rho['neutral'])[1]
        amber = fixture.xyz(peaks, rho['amber'])[1]/fixture.xyz(broad, rho['amber'])[1]
        self.assertAlmostEqual(neutral, 1, places=10)
        self.assertLess(amber, .8)  # The metamer experiment has a resolvable signal.

    def test_only_the_source_spectrum_changes_in_matched_rooms(self):
        sources = fixture.sources()
        baseline_request, baseline_scene = fixture.fixture(sources['white_broad'])
        for curve in sources.values():
            request, scene = fixture.fixture(curve)
            self.assertEqual(request, baseline_request)
            restored = copy.deepcopy(scene)
            restored['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']['light']['spectrum'] = {
                'continuum': sources['white_broad']}
            self.assertEqual(restored, baseline_scene)
            self.assertNotIn('spectral', request)
            self.assertEqual(request['render']['integrator_3d'], 'disney_v3_spectral')

    def test_wall_control_keeps_camera_source_geometry_and_budget(self):
        source = fixture.sources()['white_broad']
        a, sc_a = fixture.fixture(source, view='shadow')
        b, sc_b = fixture.fixture(source, wall='black', view='shadow')
        self.assertEqual(a, b)
        self.assertEqual(sc_a['objects'], sc_b['objects'])
        p_a = sc_a['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
        p_b = sc_b['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
        p_b['materials'][2] = p_a['materials'][2]
        self.assertEqual(p_a, p_b)


if __name__ == '__main__':
    unittest.main()
