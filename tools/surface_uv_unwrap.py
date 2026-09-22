#!/usr/bin/env python3
"""Bounded triangle-OBJ planar/box chart generation; no topology reduction."""
import argparse
import hashlib
import json
import math
from pathlib import Path

def unwrap(text,method,scale):
    vertices=[];normals=[];faces=[]
    def index(token,count):
        value=int(token);value=value-1 if value>0 else count+value
        if value<0 or value>=count:raise ValueError('OBJ index out of bounds')
        return value
    for line in text.splitlines():
        fields=line.split('#',1)[0].split()
        if not fields:continue
        key,*values=fields
        if key in ('v','vn'):
            if len(values)!=3:raise ValueError('requires three-component vertices/normals')
            vector=tuple(map(float,values))
            if not all(math.isfinite(v) and abs(v)<=1e9 for v in vector):raise ValueError('invalid coordinate')
            (vertices if key=='v' else normals).append(vector)
        elif key=='f':
            if len(values)!=3:raise ValueError('triangles only; triangulate explicitly first')
            face=[]
            for value in values:
                parts=value.split('/')
                if len(parts)>3:raise ValueError('invalid face token')
                face.append((index(parts[0],len(vertices)),index(parts[2],len(normals)) if len(parts)==3 and parts[2] else None))
            faces.append(face)
        elif key not in ('vt','o','g','s','usemtl','mtllib'):raise ValueError('unsupported OBJ directive '+key)
    if not vertices or not faces:raise ValueError('empty mesh')
    lines=['# optiC deterministic generated per-corner UVs']+['v '+' '.join(format(v,'.17g') for v in p) for p in vertices]+['vn '+' '.join(format(v,'.17g') for v in p) for p in normals]
    charts=[]
    for face in faces:
        p=[vertices[c[0]] for c in face];a=[p[1][i]-p[0][i] for i in range(3)];b=[p[2][i]-p[0][i] for i in range(3)];n=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
        if math.hypot(*n)<1e-20:raise ValueError('degenerate triangle has no projection chart')
        axis=2 if method=='planar_xy' else max(range(3),key=lambda i:abs(n[i]));sign=1 if n[axis]>=0 else -1
        charts.append({'axis':axis,'sign':sign,'chart_id':'xyz'[axis]+('+' if sign>0 else '-')})
        u,v=(axis+1)%3,(axis+2)%3
        for point in p:lines.append('vt '+format(sign*point[u]/scale,'.17g')+' '+format(point[v]/scale,'.17g'))
    for i,face in enumerate(faces):lines.append('f '+' '.join(f'{v+1}/{3*i+j+1}'+(f'/{n+1}' if n is not None else '') for j,(v,n) in enumerate(face)))
    return '\n'.join(lines)+'\n',charts

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--expected-sha256',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--method',choices=['planar_xy','box'],default='box');p.add_argument('--scale-m',type=float,default=1);p.add_argument('--uv-set-id',default='generated_uv');a=p.parse_args()
    if a.output.exists() or a.output.with_suffix('.uv.json').exists():p.error('candidate outputs already exist')
    data=a.source.read_bytes();digest=hashlib.sha256(data).hexdigest()
    if digest!=a.expected_sha256:p.error('source SHA-256 mismatch')
    if not math.isfinite(a.scale_m) or not 1e-6<=a.scale_m<=1e6:p.error('scale must be finite in [1e-6,1e6] meters')
    if not a.uv_set_id or len(a.uv_set_id.encode())>=64:p.error('UV set identity must fit 63 bytes')
    try:text,charts=unwrap(data.decode(),a.method,a.scale_m)
    except (ValueError,UnicodeError) as e:p.error(str(e))
    metadata={'version':1,'source_sha256':digest,'output_sha256':hashlib.sha256(text.encode()).hexdigest(),'uv_set_id':a.uv_set_id,'method':a.method,'scale_m':a.scale_m,'triangle_charts':charts,'policy':'per_corner_projection_overlaps_allowed'}
    a.output.write_text(text);a.output.with_suffix('.uv.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(json.dumps({'triangles':len(charts),'uv_set_id':a.uv_set_id,'output':str(a.output)}))
if __name__=='__main__':main()
