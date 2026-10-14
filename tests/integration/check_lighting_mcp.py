"""Real stdio MCP lighting commands over the native editor document."""
import json,math,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];scene=Path(sys.argv[1]).resolve()
sys.path.insert(0,str(ROOT/'tools'));import scene_editor_mcp_catalog as catalog_module
with subprocess.Popen([sys.executable,'-B',str(ROOT/'tools/scene_editor_mcp.py'),'--scene',str(scene)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as bridge:
    sequence=0
    def rpc(method,params=None):
        global sequence
        sequence+=1;bridge.stdin.write(json.dumps(dict(jsonrpc='2.0',id=sequence,method=method,params=params or {}))+'\n');bridge.stdin.flush()
        while True:
            result=json.loads(bridge.stdout.readline())
            if result.get('id')==sequence:break
        assert 'error' not in result,result;return result['result']
    def call(tool,args=None,ok=True):
        result=rpc('tools/call',dict(name=tool,arguments=args or {}));assert result['isError'] is not ok,result
        return json.loads(result['content'][0]['text']) if ok else result
    def state():return call('scene_read')
    def lighting():return call('lighting_read')['lighting']
    def mutate(tool,**args):return call(tool,dict(expected_revision=state()['revision'],**args))
    rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='lighting-proof',version='1')))
    listed=rpc('tools/list')['tools'];assert len(listed)==len(catalog_module.tool_list())+2
    assert 'expected_revision' not in next(t for t in listed if t['name']=='lighting_read')['inputSchema']['required']
    initial=lighting();assert initial==json.loads((scene.parent/'lighting-ui-expected.json').read_text())
    attached=state()['attachment'];assert attached is not None
    disk=scene.read_bytes();revision=state()['revision'];assert lighting()==initial and scene.read_bytes()==disk and state()['revision']==revision
    catalog=call('editor_catalog');assert len(catalog['tools'])==len(catalog_module.tool_list()) and catalog['scene_path']==str(scene)
    assert call('editor_command',dict(catalog_id=catalog['catalog_id'],tool='lighting_read',arguments={}))['lighting']==initial
    primary=dict(position_m=[1.25,-2.5,3.75],intensity=7.5,radius_m=.25)
    changed=call('editor_command',dict(catalog_id=catalog['catalog_id'],tool='lighting_set_primary',arguments=dict(expected_revision=revision,**primary)))
    edited=lighting();p=edited['primary'];assert p['static_editable']
    for a,b in zip(p['position_m'],primary['position_m']):assert math.isclose(a,b,abs_tol=1e-9)
    assert p['intensity']==7.5 and p['radius_m']==.25
    assert p['retained']['position']['x']==1.25/edited['world_scale'] and p['retained']['radius']==.25/edited['world_scale']
    call('lighting_set_primary',dict(expected_revision=revision,**primary),False)
    for bad in (dict(primary,position_m=[True,0,0]),dict(primary,position_m=[0,0]),dict(primary,position_m=[float('nan'),0,0]),dict(primary,intensity=True),dict(primary,intensity=20.1),dict(primary,radius_m=-.1),dict(primary,extra=1)):
        call('lighting_set_primary',dict(expected_revision=state()['revision'],**bad),False)
        assert lighting()==edited and state()['revision']==changed['revision']
    mutate('scene_undo');assert lighting()==initial;mutate('scene_redo');assert lighting()==edited
    environment=dict(light_mode=0,ambient_strength=.3,top_fill_strength=.8,environment_preset=2,background_brightness_auto=False,background_brightness=.7,background_color=[.2,.4,.6])
    mutate('lighting_set_environment',settings=environment);edited=lighting();assert edited['environment']==environment
    for bad in ({'ambient_strength':True},{'light_mode':.5},{'background_brightness_auto':1},{'background_color':[0,1]},{'top_fill_strength':21},{'background_brightness':float('inf')},{'unknown':1},{}):
        r=state()['revision'];call('lighting_set_environment',dict(expected_revision=r,settings=bad),False);assert lighting()==edited and state()['revision']==r
    mutate('lighting_set_environment',settings={'ambient_strength':.2});assert lighting()['environment']==dict(environment,ambient_strength=.2);mutate('scene_undo');assert lighting()==edited
    assert not mutate('scene_save')['dirty'];assert state()['attachment']==attached
    after_save=lighting();assert after_save['environment']==edited['environment'] and after_save['primary']==edited['primary'];edited=after_save
    saved=json.loads(scene.read_text());assert saved['lights'][0]==edited['primary']['retained'] and saved['extensions']['ray_tracing']['authoring']['environment']==edited['retained_environment']
    (scene.parent/'lighting-mcp-expected.json').write_text(json.dumps(edited,indent=2)+'\n')
    (scene.parent/'lighting-mcp-receipt.json').write_text(json.dumps(dict(ui_to_mcp=True,compatibility_read_and_edit=True,unit_conversion=True,typed_stale_refusals=True,shared_history=True,partial_environment_preservation=True,attachment_preserved=True,save_disk_parity=True),indent=2)+'\n')
    bridge.stdin.close();bridge.wait(timeout=5);assert bridge.returncode==0
print('Lighting MCP PASS: UI parity, metre semantics, guards, shared history and save')
