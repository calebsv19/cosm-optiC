#!/usr/bin/env python3
"""Create/edit/bind typed graphs using the renderer as schema authority."""
import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

def default_graph(source='triplanar_checker'):
    return {'version':1,'required_capability':'optic.surface_graph_v1','color_space':'linear',
        'producer':{'name':'optic-agent','revision':1},'nodes':[
        {'id':'position','kind':'coordinate','space':'object_rest','scale_m':.2,'offset':[0,0,0]},
        {'id':'pattern','kind':source,'inputs':['position'],**({'seed':17} if source=='noise3d' else {'sharpness':2})},
        {'id':'dark','kind':'color','value':[.05,.08,.12]},
        {'id':'light','kind':'color','value':[.8,.45,.12]},
        {'id':'rough','kind':'scalar','value':.65},
        {'id':'finish','kind':'mix','inputs':['dark','light','pattern']}],
        'outputs':{'base_color':'finish','roughness':'rough'}}

def validate(graph, renderer):
    with tempfile.TemporaryDirectory(prefix='optic-graph-') as tmp:
        path=Path(tmp)/'graph.json';path.write_text(json.dumps(graph,allow_nan=False))
        subprocess.run([str(renderer),'--validate-surface-graph',str(path)],check=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--source',choices=['noise3d','triplanar_checker'],default='triplanar_checker')
    parser.add_argument('--graph',type=Path)
    parser.add_argument('--node');parser.add_argument('--property');parser.add_argument('--value',help='JSON value, including input ID arrays')
    parser.add_argument('--scene',type=Path);parser.add_argument('--object-id')
    parser.add_argument('--expected-sha256',help='Required source digest when editing an existing graph or scene')
    args=parser.parse_args()
    if args.output.exists():parser.error('output must be a new candidate path')
    base=args.scene or args.graph
    if base and hashlib.sha256(base.read_bytes()).hexdigest()!=args.expected_sha256:parser.error('source SHA-256 mismatch')
    graph=json.loads(args.graph.read_text()) if args.graph else default_graph(args.source)
    if args.node:
        nodes=[node for node in graph['nodes'] if node['id']==args.node]
        if len(nodes)!=1 or not args.property or args.value is None:parser.error('edit requires one node, property and JSON value')
        if args.property not in ('value','inputs','scale_m','offset','space','seed','sharpness'):parser.error('unsupported typed property')
        nodes[0][args.property]=json.loads(args.value)
    validate(graph,args.renderer.resolve())
    result=graph
    if args.scene:
        result=json.loads(args.scene.read_text());rows=result.setdefault('extensions',{}).setdefault('ray_tracing',{}).setdefault('authoring',{}).setdefault('object_materials',[])
        objects=[o for o in result['objects'] if o['object_id']==args.object_id]
        existing=[row for row in rows if row['object_id']==args.object_id]
        if len(objects)!=1 or len(existing)>1:parser.error('object identity must be unique')
        ray=objects[0].get('extensions',{}).get('ray_tracing',{})
        if any(k in ray for k in ('surface_mapping','surface_sampling')):parser.error('graph requires an exclusive source')
        row=existing[0] if existing else {'object_id':args.object_id}
        if any(k in row for k in ('material_graph','materialGraph','material_texture_stack','materialTextureStack','surface_material_binding','authored_texture','procedural_texture','texture_id')):parser.error('existing source must be explicitly removed before graph binding')
        row['surface_graph']=graph
        if not existing:rows.append(row)
    with args.output.open('x') as stream:json.dump(result,stream,indent=2,allow_nan=False);stream.write('\n')
    print(json.dumps({'state':'graph_validated_scene_requires_preflight','path':str(args.output.resolve()),'sha256':hashlib.sha256(args.output.read_bytes()).hexdigest()}))
if __name__=='__main__':main()
