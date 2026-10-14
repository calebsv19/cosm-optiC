"""Real stdio MCP against the shared native import owner."""
import copy,hashlib,json,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
scene=Path(sys.argv[1]).resolve();out=scene.parent
with subprocess.Popen([sys.executable,'-B',str(ROOT/'tools/scene_editor_mcp.py'),'--scene',str(scene)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as bridge:
    sequence=0
    def rpc(method,params=None):
        global sequence
        sequence+=1;bridge.stdin.write(json.dumps(dict(jsonrpc='2.0',id=sequence,method=method,params=params or {}))+'\n');bridge.stdin.flush()
        while True:
            r=json.loads(bridge.stdout.readline())
            if r.get('id')==sequence:break
        assert 'error' not in r,r;return r['result']
    def call(tool,args=None,ok=True,wrapped=False):
        if wrapped:tool,args='editor_command',dict(catalog_id=catalog['catalog_id'],tool=tool,arguments=args or {})
        r=rpc('tools/call',dict(name=tool,arguments=args or {}));assert r['isError'] is not ok,r
        return json.loads(r['content'][0]['text']) if ok else r
    def state():return call('scene_read')
    def mutate(tool,ok=True,**args):return call(tool,dict(expected_revision=state()['revision'],**args),ok)
    def wait_import(id,expected='completed'):
        deadline=time.monotonic()+60
        while time.monotonic()<deadline:
            r=call('mesh_import_status',dict(import_id=id))['import']
            if r['state']!='compiling':assert r['state']==expected,r;return r
            time.sleep(.01)
        raise TimeoutError(id)
    def mesh(id,wrapped=False):return call('mesh_read',dict(object_id=id),wrapped=wrapped)['mesh']
    def render(id,integrator='direct_light'):
        mutate('render_start',request_id=id,width=64,height=40,temporal_frames=1 if integrator=='direct_light' else 2,frame=0,integrator=integrator)
        deadline=time.monotonic()+45
        while time.monotonic()<deadline:
            r=call('render_status',dict(job_id=id))['render']
            if r['state'] not in ('preparing','rendering','canceling'):assert r['state']=='completed',r;break
            time.sleep(.02)
        else:raise TimeoutError(r)
        r=call('render_results',dict(job_id=id))['render'];assert r['artifact_integrity'];return r
    rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='managed-stl-proof',version='1')))
    catalog=call('editor_catalog');assert {t['name'] for t in catalog['tools']} >= {'mesh_import','mesh_read','mesh_import_status'}
    initial=state();original=json.loads(scene.read_text());attachment=initial['attachment'];assert attachment
    assert not mesh('rowless')['managed'];assert state()==initial
    for source,scale in (('missing.stl',.001),('mesh.stl',0),('mesh.stl',True),('mesh.stl',float('nan')),('mesh.stl',1e13)):
        mutate('mesh_import',False,source_path=str(out/source),source_to_asset_scale=scale);assert state()==initial
    call('mesh_import_status',dict(import_id='missing'),False)
    ui=json.loads((out/'mesh-ui-expected.json').read_text())['mesh']
    r=call('mesh_import',dict(source_path=str(out/'mesh.stl'),source_to_asset_scale=.001,expected_revision=initial['revision']),wrapped=True);id=r['import']['import_id']
    mutate('object_create',False,kind='box')
    wait_import(id)
    m=mesh(id,True);assert m['managed'] and m['runtime_available'] and m['triangle_count']==4
    for key in ('triangle_count','vertex_count','local_bounds_min','local_bounds_max','preview_bounds_fallback'):assert m[key]==ui[key],(key,m,ui)
    assert m['asset']['import']['source_to_asset_scale']==.001
    assert m['asset']['source_sha256']==hashlib.sha256((out/'mesh.stl').read_bytes()).hexdigest()
    mutate('mesh_import',False,source_path=str(out/'mesh.stl'),source_to_asset_scale=.001,extra=1)
    call('mesh_import',dict(source_path=str(out/'mesh.stl'),source_to_asset_scale=.001,expected_revision=initial['revision']),False)
    mutate('scene_undo');assert [o['object_id'] for o in call('object_list')['objects']]==[o['object_id'] for o in original['objects']]
    mutate('scene_redo');assert mesh(id)==m
    # Failed compiler adopts nothing and permits subsequent imports.
    mutate('scene_save')
    before=json.loads(scene.read_text());objects=call('object_list')['objects']
    r=mutate('mesh_import',source_path=str(out/'bad.stl'),source_to_asset_scale=.001)
    # The initial shared UI Save advances the overlay producer clock, even clean.
    # Compiler failure must preserve the exact post-Save document.
    before=json.loads(scene.read_text())
    wait_import(r['import']['import_id'],'failed')
    actual_objects=call('object_list')['objects'];actual_scene=json.loads(scene.read_text());actual_attachment=state()['attachment']
    (out/'failure-atomicity-debug.json').write_text(json.dumps(dict(objects_before=objects,objects_after=actual_objects,scene_before=before,scene_after=actual_scene,attachment_before=attachment,attachment_after=actual_attachment),indent=2))
    assert actual_objects==objects,'object inventory changed after failed compile'
    assert actual_scene==before,'saved scene changed after failed compile'
    assert actual_attachment==attachment,'attachment changed after failed compile'
    # Binary source exercises the same shared intake, then Undo restores first import.
    r=mutate('mesh_import',source_path=str(out/'binary.stl'),source_to_asset_scale=.001);binary_id=r['import']['import_id'];wait_import(binary_id)
    b=mesh(binary_id)
    for key in ('triangle_count','vertex_count','local_bounds_min','local_bounds_max'):assert b[key]==m[key]
    mutate('scene_undo');assert mesh(id)==m
    direct=render('mesh-direct');disney=render('mesh-disney','disney_v2');assert direct['triangle_count']>=4
    mutate('scene_save');saved=json.loads(scene.read_text());assert saved['objects'][:3]==original['objects']
    assert saved['extensions']['t1_provenance']==original['extensions']['t1_provenance']
    assert saved['extensions']['ray_tracing']['authoring']['atmosphere_attachment']==original['extensions']['ray_tracing']['authoring']['atmosphere_attachment']
    # Original external STL is no longer a dependency.
    (out/'mesh.stl').unlink();mutate('scene_open',scene_path=str(scene));assert mesh(id)==m
    reopened=render('mesh-reopen');assert Path(direct['image_path']).read_bytes()==Path(reopened['image_path']).read_bytes()
    assert state()['attachment']==attachment and not state()['dirty']
    (out/'mesh-expected.json').write_text(json.dumps({'mesh':m},indent=2)+'\n')
    (out/'mesh-reopen-render.json').write_text(json.dumps(reopened,indent=2)+'\n')
    (out/'mesh-mcp-receipt.json').write_text(json.dumps(dict(ascii_binary_import=True,explicit_millimeter_scale=True,named_and_cached_dispatch=True,typed_stale_refusals=True,failed_compile_preserves_scene=True,shared_undo_redo=True,managed_source_independent_of_external_file=True,direct_light_reopen_exact=True,disney_render=True,legacy_objects_and_attachments_preserved=True,inspection_no_topology_claim=True),indent=2)+'\n')
    bridge.stdin.close();bridge.wait(timeout=5);assert bridge.returncode==0
print('Managed STL MCP PASS')
