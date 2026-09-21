#!/usr/bin/env python3
"""M3 retained graph and region acceptance in an isolated native source host."""
import argparse,copy,json,os,platform,hashlib,struct,zlib,sys
from pathlib import Path
from test_surface_mapping_m1 import ROOT,MAPPING,run,build_request
sys.path.insert(0,str(ROOT/"tools"))
from surface_material_binding import bind

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output-root',type=Path,required=True)
    out=ap.parse_args().output_root.resolve();out.mkdir(parents=True,exist_ok=False)
    runtime=out/'data/runtime';runtime.mkdir(parents=True)
    (runtime/'animation_config.json').write_text(json.dumps({'editorMode':1,'spaceMode':1,'windowWidth':1280,'windowHeight':800,'inputRoot':str(ROOT/'config'),'outputRoot':str(runtime),'videoOutputRoot':str(out/'videos')}))
    (runtime/'scene_config.json').write_text(json.dumps({'window':{'width':1280,'height':800}}))
    scene=json.loads((ROOT/'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
    scene['world_scale']=1;obj=copy.deepcopy(scene['objects'][0]);scene['objects']=[obj]
    obj['object_id']='surface';obj['object_type']=obj['primitive']['kind']='rect_prism_primitive'
    obj['primitive'].update(width=4,height=2,depth=1)
    obj['primitive']['frame']['origin']={'x':0,'y':0,'z':0}
    obj['transform'].update(position={'x':0,'y':0,'z':0},rotation={'x':0,'y':0,'z':0},scale={'x':1,'y':1,'z':1})
    obj.setdefault('extensions',{}).setdefault('ray_tracing',{})['surface_mapping']=copy.deepcopy(MAPPING)
    row={'object_id':'surface','material_id':0,'object_color':12756864,'roughness':.8,'reflectivity':.02,'producer_note':'retain this',
         'surface_material_binding':{'version':1,'required_capability':'optic.surface_material_v3'},
         'material_graph':{'schema_version':1,'graph_id':'agent-graph','provenance':{'source':'agent','digest':'a'*64},'nodes':[{'node_id':'agent-node','node_kind':'layer','producer':'kept','layer':{'id':'base','kind':'brick','opacity':1,'placement':{'scale':1,'strength':1}}}]}}
    scene['extensions']['ray_tracing']['authoring']['object_materials']=[row]
    mapping_path=out/'agent-mapping.json';mapping_path.write_text(json.dumps(MAPPING))
    mapping_digest=hashlib.sha256(mapping_path.read_bytes()).hexdigest()
    graph_path=out/'agent-graph.json';row['material_graph']['surface_mapping_ref']='agent-chart'
    graph_path.write_text(json.dumps(row['material_graph']))
    graph_digest=hashlib.sha256(graph_path.read_bytes()).hexdigest()
    scene=bind(scene,'surface',mapping_path,graph_path,'layer_graph',graph_digest,mapping_digest)
    surface_doc={'schema':'ray_tracing.surface_authoring_document','schema_version':1,'document_id':'surface-doc','source_object_id':'surface',
        'source_mesh_digest_sha256':'a'*64,'material_graph':{'id':'agent-graph','digest_sha256':graph_digest,'output_domains':1},
        'surface_mapping':{'id':'agent-chart','digest_sha256':mapping_digest,'output_domains':1}}
    document_path=out/'surface-document.json';document_path.write_text(json.dumps(surface_doc))
    scene=bind(scene,'surface',mapping_path,document_path,'surface_authoring_document',hashlib.sha256(document_path.read_bytes()).hexdigest(),mapping_digest)
    path=out/'scene_runtime.json';path.write_text(json.dumps(scene,indent=2))
    binary=ROOT/f'build/toolchains/clang/{platform.machine()}/tests/scene_editor_workspace_visual_test'
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(ROOT))
    run([binary,out,path,'--mapping-m3'],out/'native.log',env)
    saved=json.loads(path.read_text())['extensions']['ray_tracing']['authoring']['object_materials'][0]
    expected=copy.deepcopy(row['material_graph']);expected['nodes'][0]['layer']['opacity']=.43
    assert saved['material_graph']==expected and saved['producer_note']=='retain this'
    assert 'material_texture_stack' not in saved
    run([binary,out,path,'--mapping-m3-reopen'],out/'reopen.log',env)
    run([binary,out,path,'--material-parity'],out/'ray-parity.log',env)
    parity=json.loads((out/'parity.json').read_text())
    assert all(face['hits']==65536 and face['max_channel_error']<=1e-6 for face in parity['faces'])
    # Authored manifest adapter executes a named chart with repeat addressing.
    image_dir=out/'image';image_dir.mkdir();(image_dir/'data/runtime').mkdir(parents=True)
    for name in ('animation_config.json','scene_config.json'):
        (image_dir/'data/runtime'/name).write_bytes((runtime/name).read_bytes())
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    pixels=b''.join(b'\0'+bytes(v for x in range(8) for v in (x*31,y*31,127,255)) for y in range(8))
    (image_dir/'grid.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',8,8,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(pixels))+chunk(b'IEND',b''))
    manifest={'schema_version':1,'export_binding_kind':'SEPARATE_FACES','primitive_kind':'PLANE','source_object_id':'surface','surface_mapping_ref':'image-chart',
              'surfaces':[{'face_role':'FRONT','file_name':'grid.png','net_layout_kind':'PLANE','net_slot':'FRONT','orientation':'R0','corner_ids':[255]*4,'edge_ids':[255]*4,'adjacent_face_roles':['NONE']*4}]}
    manifest_path=image_dir/'manifest.json';manifest_path.write_text(json.dumps(manifest))
    image_scene=copy.deepcopy(scene);image_row=image_scene['extensions']['ray_tracing']['authoring']['object_materials'][0]
    image_row.pop('material_graph');image_row.pop('surface_material_binding')
    image_scene['objects'][0]['object_type']=image_scene['objects'][0]['primitive']['kind']='plane_primitive'
    image_row['material_texture_stack']={'layers':[{'id':'substrate','kind':'solid'}]}
    image_scene=bind(image_scene,'surface',mapping_path,manifest_path,'authored_manifest',hashlib.sha256(manifest_path.read_bytes()).hexdigest(),mapping_digest)
    image_path=image_dir/'scene.json';image_path.write_text(json.dumps(image_scene))
    run([binary,image_dir,image_path,'--mapping-m3-image'],image_dir/'native.log',env)
    headless=ROOT/f'build/toolchains/clang/{platform.machine()}/tools/cli/ray_tracing_render_headless'
    def preflight(name,doc,ok):
        source=out/(name+'.json');source.write_text(json.dumps(doc))
        request=build_request(out/name,'direct','flattened');request['scene']['runtime_scene_path']=str(source)
        req=out/(name+'-request.json');req.write_text(json.dumps(request))
        run([headless,'--request',req,'--preflight'],out/(name+'.log'),env,ok)
    invalid=[]
    for name in ('missing_mapping','stale_document','duplicate_node','unknown_node','graph_and_stack','invalid_opacity','invalid_camel_influence'):
        doc=copy.deepcopy(scene);r=doc['extensions']['ray_tracing']['authoring']['object_materials'][0]
        if name=='missing_mapping':r['material_graph']['nodes'][0]['layer']['mapping_ref']='absent'
        elif name=='stale_document':r['surface_material_binding']['source_documents'][0]['sha256']='0'*64
        elif name=='duplicate_node':r['material_graph']['nodes'].append(copy.deepcopy(r['material_graph']['nodes'][0]))
        elif name=='unknown_node':r['material_graph']['nodes'][0]['node_kind']='unknown'
        elif name=='graph_and_stack':r['material_texture_stack']={'layers':[{'id':'extra','kind':'solid'}]}
        elif name=='invalid_opacity':r['material_graph']['nodes'][0]['layer']['opacity']=2
        elif name=='invalid_camel_influence':r['material_graph']['nodes'][0]['layer']['roughnessInfluence']=2
        preflight(name,doc,False);invalid.append(name)
    invalid_manifest=copy.deepcopy(manifest);invalid_manifest['surface_mapping_ref']='absent'
    bad_manifest_path=image_dir/'missing-ref-manifest.json';bad_manifest_path.write_text(json.dumps(invalid_manifest))
    missing_image_ref=copy.deepcopy(image_scene)
    image_row=missing_image_ref['extensions']['ray_tracing']['authoring']['object_materials'][0]
    image_row['authored_texture']['manifest_path']=str(bad_manifest_path)
    image_row['surface_material_binding'].pop('source_documents')
    preflight('missing_manifest_mapping',missing_image_ref,False);invalid.append('missing_manifest_mapping')
    before=mapping_path.read_bytes();mapping_path.write_bytes(before+b' ')
    preflight('stale_mapping_bytes',scene,False);mapping_path.write_bytes(before)
    preflight('valid_restored_mapping',scene,True)
    (out/'acceptance.json').write_text(json.dumps({'native_graph_and_regions':True,'fresh_reopen':True,'manifest_named_mapping':True,'normal_workspace_save':True,'ray_parity':parity,'negative_cases':invalid+['stale_mapping_bytes']},indent=2))
    print('M3 graph source, retained edits, six-face regions and fresh reopen passed',flush=True)
if __name__=='__main__':main()
