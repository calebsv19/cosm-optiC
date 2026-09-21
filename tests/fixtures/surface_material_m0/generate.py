#!/usr/bin/env python3
"""Deterministic M0 geometry/oracle assets; no material mapping implementation."""
import argparse
import copy
import json
import math
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from generate_mesh_asset_sphere_fixtures import make_sphere, mesh_instance, vertex

CONTRACT = json.loads(Path(__file__).with_name('contract.json').read_text())


def mesh(name, vertices, triangles, groups, closed):
    return {
        'schema_family': 'codework_geometry', 'schema_variant': 'mesh_asset_runtime_v1',
        'schema_version': 1, 'asset_id': name, 'source_asset_id': name,
        'asset_type': 'solid_mesh',
        'compile_meta': {'profile': 'runtime_default', 'generator': 'surface_material_m0'},
        'local_bounds': {side: {axis: fn(v[axis] for v in vertices) for axis in 'xyz'}
                         for side, fn in [('min', min), ('max', max)]},
        'mesh': {'vertex_count': len(vertices), 'triangle_count': len(triangles),
                 'vertices': vertices, 'triangles': triangles},
        'surface_groups': groups,
        'topology_flags': {'closed_volume': closed, 'manifold_expected': True},
        'extensions': {}}


def triangle(a, b, c, group):
    return dict(a=a, b=b, c=c, surface_group_id=group)


def group(name, start, count):
    return {'group_id': name, 'semantic': name,
            'triangle_span': {'start': start, 'count': count}}


def plane(name, nx, ny, alternate=False):
    vertices = [vertex(-2 + 4*x/nx, -1 + 2*y/ny, 0)
                for y in range(ny+1) for x in range(nx+1)]
    triangles = []
    for y in range(ny):
        for x in range(nx):
            a = y*(nx+1)+x
            b, d, c = a+1, a+nx+1, a+nx+2
            faces = [(a,b,d),(b,c,d)] if alternate else [(a,b,c),(a,c,d)]
            triangles.extend(triangle(*t, 'plane') for t in faces)
    return mesh(name, vertices, triangles, [group('plane', 0, len(triangles))], False)


def cylinder(name, n):
    vertices = [vertex(math.cos(2*math.pi*i/n), math.sin(2*math.pi*i/n), z)
                for z in [-1,1] for i in range(n)]
    vertices += [vertex(0,0,-1), vertex(0,0,1)]
    triangles = []
    for i in range(n):
        j = (i+1)%n
        triangles += [triangle(i,j,n+j,'side'), triangle(i,n+j,n+i,'side')]
    triangles += [triangle(2*n,(i+1)%n,i,'bottom') for i in range(n)]
    triangles += [triangle(2*n+1,n+i,n+(i+1)%n,'top') for i in range(n)]
    return mesh(name, vertices, triangles,
                [group('side',0,2*n),group('bottom',2*n,n),group('top',3*n,n)], True)


def assets():
    return [plane('plane_diagonal_a',1,1), plane('plane_diagonal_b',1,1,True),
            plane('plane_subdivided',8,4),
            make_sphere('sphere_low',8,4,'low'), make_sphere('sphere_high',32,16,'high'),
            cylinder('cylinder_low',8), cylinder('cylinder_high',32)]


def grid_files():
    cols, rows, cell = (CONTRACT['grid'][k] for k in ['columns','rows','cell_pixels'])
    w, h = cols*cell, rows*cell
    pixels = bytearray(w*h*3)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">']
    digits = ['111101101101111','010110010010111','111001111100111','111001111001111',
              '101101111001001','111100111001111','111100111101111','111001001001001',
              '111101111101111','111101111001111']
    for row in range(rows):
        for col in range(cols):
            rgb = (48+col*24, 56+row*48, 200 if (col+row)%2 else 96)
            x0,y0 = col*cell,(rows-1-row)*cell
            svg += [f'<rect x="{x0}" y="{y0}" width="{cell}" height="{cell}" fill="rgb{rgb}" stroke="black"/>',
                    f'<text x="{x0+10}" y="{y0+37}" fill="white" font-family="monospace" font-size="18">{col}_{row}</text>']
            for y in range(cell):
                for x in range(cell):
                    i=((y0+y)*w+x0+x)*3
                    pixels[i:i+3]=bytes((0,0,0) if x<2 or y<2 else rgb)
            for number, dx in [(col,12),(row,36)]:
                for k, bit in enumerate(digits[number]):
                    if bit=='1':
                        for dy in range(4):
                            for xx in range(4):
                                i=((y0+20+(k//3)*4+dy)*w+x0+dx+(k%3)*4+xx)*3
                                pixels[i:i+3]=b'\xff\xff\xff'
            for x in range(27,33):
                i=((y0+40)*w+x0+x)*3
                pixels[i:i+3]=b'\xff\xff\xff'
    svg.append('</svg>')
    return '\n'.join(svg)+'\n', f'P6\n{w} {h}\n255\n'.encode()+pixels


def generate(out):
    out.mkdir(parents=True, exist_ok=False)
    template=json.loads((ROOT/'tests/fixtures/mesh_asset_runtime_spheres/scene_runtime.json').read_text())
    for asset in assets():
        name=asset['asset_id']
        folder=out/name
        dest=folder/'assets/mesh_assets'
        dest.mkdir(parents=True)
        (dest/f'{name}.runtime.json').write_text(json.dumps(asset,indent=2)+'\n')
        scene=copy.deepcopy(template)
        scene['scene_id']=name
        obj=mesh_instance('surface',name,'mat_sphere_high',0,0,0)
        obj['transform']['scale']={'x':1,'y':1,'z':1}
        scene['objects']=[obj]
        scene.setdefault('extensions',{}).setdefault('ray_tracing',{})['authoring']={
            'object_materials':[{'object_id':'surface','material_id':0,'object_color':12756864,
                'roughness':0.8,'reflectivity':0.02,
                'material_texture_stack':{'layers':[{'id':'base','name':'Brick','kind':'brick',
                    'enabled':True,'placement':{'scale':1,'strength':1},
                    'parameters':{'seed':CONTRACT['seed']}}]}}]}
        (folder/'scene_runtime.json').write_text(json.dumps(scene,indent=2)+'\n')
    svg,ppm=grid_files()
    (out/'labeled_grid.svg').write_text(svg)
    (out/'labeled_grid.ppm').write_bytes(ppm)
    (out/'contract.json').write_text(json.dumps(CONTRACT,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root',type=Path,required=True)
    generate(parser.parse_args().output_root)
