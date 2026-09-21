#!/usr/bin/env python3
"""Validate M0 fixture geometry/oracles. Does not test future mapping behavior."""
import importlib.util
import math
from pathlib import Path
import tempfile
import unittest
import json

SOURCE=Path(__file__).resolve().parents[1]/'fixtures/surface_material_m0/generate.py'
spec=importlib.util.spec_from_file_location('m0_fixtures',SOURCE)
fixture=importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


def points(asset, tri):
    return [tuple(asset['mesh']['vertices'][tri[k]][axis] for axis in 'xyz') for k in 'abc']


def area(p):
    a,b,c=p
    u=[b[i]-a[i] for i in range(3)]
    v=[c[i]-a[i] for i in range(3)]
    cross=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
    return math.sqrt(sum(x*x for x in cross))/2


def reconstruct_xy(asset, p):
    for tri in asset['mesh']['triangles']:
        a,b,c=points(asset,tri)
        den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        u=((b[1]-c[1])*(p[0]-c[0])+(c[0]-b[0])*(p[1]-c[1]))/den
        v=((c[1]-a[1])*(p[0]-c[0])+(a[0]-c[0])*(p[1]-c[1]))/den
        w=1-u-v
        if min(u,v,w)>=-1e-12:
            return [u*a[i]+v*b[i]+w*c[i] for i in range(3)]
    raise AssertionError('point missing from plane')


class M0FixtureContract(unittest.TestCase):
    def test_topology_groups_and_finite_nondegenerate_geometry(self):
        for asset in fixture.assets():
            with self.subTest(asset=asset['asset_id']):
                mesh=asset['mesh']
                self.assertEqual(mesh['vertex_count'],len(mesh['vertices']))
                self.assertEqual(mesh['triangle_count'],len(mesh['triangles']))
                edges={}
                for tri in mesh['triangles']:
                    p=points(asset,tri)
                    self.assertTrue(all(math.isfinite(v) for point in p for v in point))
                    self.assertGreater(area(p),1e-10)
                    for a,b in [('a','b'),('b','c'),('c','a')]:
                        key=tuple(sorted((tri[a],tri[b])))
                        edges.setdefault(key,[]).append((tri[a],tri[b]))
                self.assertTrue(all(len(v)<=2 for v in edges.values()))
                if asset['topology_flags']['closed_volume']:
                    self.assertTrue(all(len(v)==2 and v[0]==v[1][::-1] for v in edges.values()))
                covered=[]
                for group in asset['surface_groups']:
                    start,count=(group['triangle_span'][k] for k in ['start','count'])
                    for i in range(start,start+count):
                        self.assertEqual(mesh['triangles'][i]['surface_group_id'],group['group_id'])
                        covered.append(i)
                self.assertEqual(sorted(covered),list(range(len(mesh['triangles']))))

    def test_exact_plane_retriangulation_and_subdivision(self):
        planes=fixture.assets()[:3]
        tol=fixture.CONTRACT['tolerances']['same_surface_position_m']
        for asset in planes:
            self.assertAlmostEqual(sum(area(points(asset,t)) for t in asset['mesh']['triangles']),8)
            # Include boundaries, triangle diagonals, and off-diagonal interior samples.
            for y in range(17):
                for x in range(33):
                    p=(-2+x/8,-1+y/8,0)
                    q=reconstruct_xy(asset,p)
                    self.assertLessEqual(max(abs(p[i]-q[i]) for i in range(3)),tol)

    def test_curved_error_is_geometric_not_material_tolerance(self):
        tolerances=fixture.CONTRACT['tolerances']
        for asset in fixture.assets()[3:]:
            name=asset['asset_id']
            sphere=name.startswith('sphere')
            for v in asset['mesh']['vertices']:
                radius=math.sqrt(v['x']**2+v['y']**2+(v['z']**2 if sphere else 0))
                if sphere or radius>0:
                    self.assertLess(abs(radius-1),tolerances['mesh_vertex_radius_m'])
            errors=[]
            for t in asset['mesh']['triangles']:
                if not sphere and t['surface_group_id']!='side':
                    continue
                p=points(asset,t)
                q=[sum(v[i] for v in p)/3 for i in range(3)]
                radius=math.sqrt(sum(v*v for v in (q if sphere else q[:2])))
                errors.append(1-radius)
            level=name.split('_')[1]
            key=f'sphere_centroid_radial_deficit_{level}_m' if sphere else f'cylinder_side_centroid_radial_deficit_{level}_m'
            self.assertGreater(max(errors),1e-3)  # These are NOT the exact analytic surface.
            self.assertLessEqual(max(errors),tolerances[key])
        self.assertFalse(tolerances['whole_image_low_high_equality'])

    def test_independent_oracle_examples_and_label_orientation(self):
        # Data consistency only. M1/M2 must compare these constants to C adapters.
        for sample in fixture.CONTRACT['plane_samples'][:3]:
            u,v=sample['uv_tiles']
            self.assertEqual(sample['label'],f'{int(u)}_{int(v)}')
        analytic=fixture.CONTRACT['analytic_samples']
        self.assertEqual([s['uv_tiles'][0] for s in analytic[:4]],[0,2,4,6])
        self.assertEqual([s['status'] for s in analytic[-2:]],['pole_singular']*2)
        svg,ppm=fixture.grid_files()
        self.assertEqual(svg.count('<text '),32)
        self.assertIn('y="229"',svg)  # bottom-row label baseline
        self.assertTrue(ppm.startswith(b'P6\n512 256\n255\n'))
        self.assertEqual(len(ppm.split(b'\n',3)[3]),512*256*3)

    def test_generation_is_deterministic_create_only_and_legacy(self):
        with tempfile.TemporaryDirectory() as tmp:
            a,b=Path(tmp)/'a',Path(tmp)/'b'
            fixture.generate(a)
            fixture.generate(b)
            for p in a.rglob('*'):
                if p.is_file():
                    self.assertEqual(p.read_bytes(),(b/p.relative_to(a)).read_bytes())
            with self.assertRaises(FileExistsError):
                fixture.generate(a)
            for scene in a.glob('*/scene_runtime.json'):
                doc=json.loads(scene.read_text())
                self.assertEqual(doc['objects'][0]['object_id'],'surface')
                material=doc['extensions']['ray_tracing']['authoring']['object_materials'][0]
                self.assertNotIn('surface_mapping',material)
                self.assertEqual(material['material_texture_stack']['layers'][0]['parameters']['seed'],1729)


if __name__=='__main__':
    unittest.main(verbosity=2)
