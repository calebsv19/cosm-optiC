"""Real stdio MCP against native UI authoring, not a mocked command owner."""
import copy
import json
import subprocess
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
scene=Path(sys.argv[1]).resolve()
with subprocess.Popen([sys.executable,'-B',str(ROOT/'tools/scene_editor_mcp.py'),'--scene',str(scene)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as server:
    sequence=0
    def rpc(method,params=None):
        global sequence
        sequence+=1
        server.stdin.write(json.dumps(dict(jsonrpc='2.0',id=sequence,method=method,params=params or {}))+'\n');server.stdin.flush()
        result=json.loads(server.stdout.readline());assert 'error' not in result,result
        return result['result']
    def call(name,args=None,ok=True):
        result=rpc('tools/call',dict(name=name,arguments=args or {}));assert result['isError'] is not ok,result
        return json.loads(result['content'][0]['text']) if ok else result
    def read(obj='surface'):return call('material_read',dict(object_id=obj))['material']['retained']
    def mutate(name,ok=True,**args):
        return call(name,dict(expected_revision=call('scene_read')['revision'],**args),ok)
    def undo():mutate('scene_undo')
    def identity(row):return row['material_authoring']['source_id']
    rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='materials-proof',version='1')))
    tools=rpc('tools/list')['tools'];assert {'material_list','material_read','material_preset','material_assign','material_duplicate','material_reset_source','material_reset_mapping','material_set_graph','material_set_layer_value'} <= {t['name'] for t in tools}
    assert 'expected_revision' not in next(t for t in tools if t['name']=='material_read')['inputSchema']['required']
    listed=call('material_list');assert {o['object_id'] for o in listed['objects']}=={'surface','recipient','rowless'}
    original=read();expected=json.loads((scene.parent/'material-ui-expected.json').read_text())
    assert original==expected # UI New and parameter edit -> actual MCP readback
    initial_revision=call('scene_read')['revision'];assert read('rowless') is None
    assert call('scene_read')['revision']==initial_revision
    for args in ({'object_id':'missing'}, {'object_id':True}):call('material_read',args,False)
    for preset in listed['presets']:
        mutate('material_preset',object_id='rowless',preset=preset,replace=False)
        assert read('rowless')['material_authoring']['source_id'];undo();assert read('rowless') is None
    graph=copy.deepcopy(original['surface_graph'])
    next(n for n in graph['nodes'] if n['id']=='rough')['value']=.23
    revision=call('scene_read')['revision']
    changed=mutate('material_set_graph',object_id='surface',graph=graph)
    assert read()['surface_graph']==graph
    call('material_set_graph',dict(object_id='surface',graph=original['surface_graph'],expected_revision=revision),False)
    for bad in (True,'yes'):
        mutate('material_set_layer_value',False,object_id='surface',layer_id='x',group='',property='opacity',value=bad)
    invalid=copy.deepcopy(graph);invalid['outputs']['base_color']='missing'
    mutate('material_set_graph',False,object_id='surface',graph=invalid)
    assert read()['surface_graph']==graph and call('scene_read')['revision']==changed['revision']
    undo();assert read()==original;mutate('scene_redo');assert read()['surface_graph']==graph;undo()
    mutate('material_preset',False,object_id='surface',preset='noise3d',replace=False)
    mutate('material_preset',False,object_id='surface',preset='unknown',replace=True)
    mutate('material_preset',False,object_id='surface',preset='solid',replace=1)
    mutate('material_preset',object_id='surface',preset='solid',replace=True)
    assert identity(read())!=identity(original) and read()['material_authoring']['replaced_sources'];undo();assert read()==original
    mutate('material_duplicate',object_id='surface');assert identity(read())!=identity(original);undo();assert read()==original
    recipient=read('recipient')
    mutate('material_set_layer_value',object_id='recipient',layer_id='base',group='',property='opacity',value=.43)
    assert read('recipient')['material_graph']['nodes'][0]['layer']['opacity']==.43
    undo();assert read('recipient')==recipient
    mutate('material_set_layer_value',False,object_id='recipient',layer_id='base',group='',property='opacity',value=2)
    assert read('recipient')==recipient
    mutate('material_assign',object_id='recipient',source_object_id='surface')
    assigned=read('recipient');assert assigned['surface_graph']==original['surface_graph'] and identity(assigned)!=identity(original)
    assert assigned['t1_unknown_producer_metadata']==recipient['t1_unknown_producer_metadata'];undo();assert read('recipient')==recipient
    for action in ('material_reset_source','material_reset_mapping'):
        mutate(action,object_id='surface');undo();assert read()==original
    # A numeric legacy layer edit is refused on this typed graph family.
    mutate('material_set_layer_value',False,object_id='surface',layer_id='rough',group='',property='opacity',value=.5)
    mutate('material_set_graph',object_id='surface',graph=graph)
    state=mutate('scene_save');assert not state['dirty']
    rows=json.loads(scene.read_text())['extensions']['ray_tracing']['authoring']['object_materials']
    assert rows[0]==read() and rows[1]==recipient
    (scene.parent/'material-mcp-expected.json').write_text(json.dumps(rows))
    (scene.parent/'material-mcp-receipt.json').write_text(json.dumps(dict(ui_to_mcp=True,all_presets=True,shared_history=True,typed_stale_refusals=True,source_identity_and_archives=True,save_disk_parity=True)))
    server.stdin.close();server.wait(timeout=5);assert server.returncode==0
print('Materials MCP PASS: UI source parity, stable IDs, presets, shared commands/history, typed refusals and disk save')
