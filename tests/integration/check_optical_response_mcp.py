"""Real stdio MCP optical edits through the native retained command owner."""
import copy,json,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];scene=Path(sys.argv[1]).resolve();out=scene.parent;mode=sys.argv[2]
with subprocess.Popen([sys.executable,'-B',str(ROOT/'tools/scene_editor_mcp.py'),'--scene',str(scene)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as bridge:
    seq=0
    def rpc(method,params=None):
        global seq
        seq+=1;bridge.stdin.write(json.dumps(dict(jsonrpc='2.0',id=seq,method=method,params=params or {}))+'\n');bridge.stdin.flush()
        while True:
            r=json.loads(bridge.stdout.readline())
            if r.get('id')==seq:break
        assert 'error' not in r,r;return r['result']
    def call(tool,args=None,ok=True,wrapped=False):
        if wrapped:tool,args='editor_command',dict(catalog_id=catalog['catalog_id'],tool=tool,arguments=args or {})
        r=rpc('tools/call',dict(name=tool,arguments=args or {}));assert r['isError'] is not ok,r
        return json.loads(r['content'][0]['text']) if ok else r
    def state():return call('scene_read')
    def mutate(tool,ok=True,**args):return call(tool,dict(expected_revision=state()['revision'],**args),ok)
    def read(id='surface',wrapped=False):return call('material_response_read',dict(object_id=id),wrapped=wrapped)['response']
    def set_response(settings,id='surface',ok=True):return mutate('material_response_set',ok,object_id=id,settings=settings)
    def render(id,integrator):
        mutate('render_start',request_id=id,width=96,height=64,frame=0,temporal_frames=1 if integrator=='direct_light' else 2,integrator=integrator)
        deadline=time.monotonic()+60
        while time.monotonic()<deadline:
            r=call('render_status',dict(job_id=id))['render']
            if r['state'] not in ('preparing','rendering','canceling'):assert r['state']=='completed',r;break
            time.sleep(.02)
        else:raise TimeoutError(r)
        r=call('render_results',dict(job_id=id))['render'];assert r['artifact_integrity'];return r
    rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='optical-proof',version='1')))
    catalog=call('editor_catalog');assert {'material_response_read','material_response_set'}<={t['name'] for t in catalog['tools']}
    if mode=='reopen':
        expected=json.loads((out/'optical-expected.json').read_text());assert read()==expected;assert not state()['dirty']
    else:
        ui=json.loads((out/'optical-ui-expected.json').read_text());assert read(wrapped=True)==ui
        original=read();graph=copy.deepcopy(original['retained']['surface_graph']);attachment=state()['attachment'];assert attachment
        disk=scene.read_bytes();initial=state();assert read('rowless')['retained'] is None;assert scene.read_bytes()==disk and state()==initial
        bad=[{}, {'ior':True},{'ior':.9},{'ior':2.6},{'transmission':-1},{'transmission':1.1},{'absorption_distance':0},{'thin_walled':1},{'interface_tint':1.5},{'interface_tint':-1},{'absorption_color':16777216},{'base_roughness':float('nan')},{'unknown':1}]
        for patch in bad:set_response(patch,ok=False);assert read()==original
        set_response({'ior':1.6},'recipient',False)
        mutate('object_set_flag',object_id='surface',flag='locked',value=True);set_response({'ior':1.6},ok=False);mutate('scene_undo')
        old=state()['revision'];settings=dict(ior=2.1,transmission=.92,absorption_distance=3.5,thin_walled=False,base_roughness=.8,interface_tint=0xDDEEFF,absorption_color=0x99BBDD)
        call('material_response_set',dict(object_id='surface',settings=settings,expected_revision=old),wrapped=True)
        current=read();assert current['effective']=={k:v for k,v in settings.items()};assert 'graph' in current['roughness_source']
        assert current['retained']['surface_graph']==graph and current['retained']['t1_unknown_producer_metadata']==original['retained']['t1_unknown_producer_metadata']
        call('material_response_set',dict(object_id='surface',settings={'ior':1.7},expected_revision=old),False)
        mutate('scene_undo');assert read()==original;mutate('scene_redo');assert read()==current
        set_response({'interface_tint':None,'absorption_color':None});assert 'glass_interface_tint' not in read()['retained'];mutate('scene_undo');assert read()==current
        # Explicit edit may create a legacy row; Undo must restore original omission.
        set_response({'base_preset':'glass','ior':1.7,'thin_walled':True},'rowless');assert read('rowless')['family']=='glass';mutate('scene_undo');assert read('rowless')['retained'] is None
        direct=render('optical-direct','direct_light');disney=render('optical-disney','disney_v2')
        mutate('scene_save');expected=read();saved=json.loads(scene.read_text());baseline=json.loads((out/'original_scene.json').read_text());assert saved['objects']==baseline['objects']
        rows=saved['extensions']['ray_tracing']['authoring']['object_materials'];assert next(r for r in rows if r['object_id']=='recipient')==baseline['extensions']['ray_tracing']['authoring']['object_materials'][1];assert not any(r['object_id']=='rowless' for r in rows)
        assert saved['extensions']['t1_provenance']==baseline['extensions']['t1_provenance'];assert state()['attachment']==attachment
        mutate('scene_open',scene_path=str(scene));assert read()==expected
        reopened=render('optical-direct-reopen','direct_light');assert Path(direct['image_path']).read_bytes()==Path(reopened['image_path']).read_bytes()
        (out/'optical-expected.json').write_text(json.dumps(expected,indent=2)+'\n');(out/'optical-render-results.json').write_text(json.dumps(dict(direct=direct,disney=disney,reopened=reopened),indent=2)+'\n')
        (out/'optical-receipt.json').write_text(json.dumps(dict(named_cached_dispatch=True,graph_source_preserved=True,graph_roughness_ownership=True,tint_clear_and_history=True,rowless_legacy_restored=True,typed_stale_locked_refusals=True,ui_mcp_readback=True,legacy_objects_metadata_attachment_preserved=True,direct_disney_render=True,direct_reopen_exact=True),indent=2)+'\n')
    bridge.stdin.close();bridge.wait(timeout=5);assert bridge.returncode==0
print('Optical response MCP',mode,'PASS')
