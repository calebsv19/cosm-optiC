"""Real MCP parity on the saved native UI physical-profile checkpoint."""
import copy, json, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];scene=Path(sys.argv[1])
with subprocess.Popen([sys.executable,str(ROOT/'tools/scene_editor_mcp.py'),'--scene',str(scene)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as server:
    sequence=0
    def rpc(method,params=None):
        global sequence
        sequence+=1;server.stdin.write(json.dumps(dict(jsonrpc='2.0',id=sequence,method=method,params=params or {}))+'\n');server.stdin.flush()
        reply=json.loads(server.stdout.readline());assert 'error' not in reply,reply
        return reply['result']
    def call(name,args=None,ok=True):
        result=rpc('tools/call',dict(name=name,arguments=args or {}));assert result['isError'] is not ok,result
        return json.loads(result['content'][0]['text'])
    rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='physical-optics-proof',version='1')))
    tools=rpc('tools/list')['tools'];schema=next(t for t in tools if t['name']=='atmosphere_set')['inputSchema']['properties']['attachment']
    assert schema['properties']['optics']['additionalProperties'] is False
    call('scene_connect',dict(scene_path=str(scene.resolve())))
    original=call('scene_read');value=original['attachment'];assert value['optics']['profile']=='physical_smoke_v1'
    assert value['optics']['mass_extinction_m2_kg']==900 and value['optics']['anisotropy']==.4
    changed=copy.deepcopy(value);changed['optics'].update(reference_concentration_kg_m3=.002,mass_extinction_m2_kg=450,scattering_albedo=.7,anisotropy=-.2,sky_illumination=True,sampling_layout='legacy_node',sky_error_budget=0)
    changed['appearance']['step_scale']=.75
    state=call('atmosphere_set',dict(expected_revision=original['revision'],attachment=changed));assert state['attachment']==changed and state['dirty']
    refusal=call('atmosphere_set',dict(expected_revision=original['revision'],attachment=value),False)
    assert refusal['revision']==state['revision'] and refusal['attachment']==changed
    for field,bad in [('scattering_albedo',1.1),('anisotropy',.96),('reference_concentration_kg_m3',True),('mass_extinction_m2_kg','450'),('sky_illumination',1),('profile','unknown'),('extra',1)]:
        invalid=copy.deepcopy(changed);invalid['optics'][field]=bad
        refused=call('atmosphere_set',dict(expected_revision=state['revision'],attachment=invalid),False)
        assert refused['revision']==state['revision'] and refused['attachment']==changed
    invalid=copy.deepcopy(changed);invalid['affects_lighting']=False
    call('atmosphere_set',dict(expected_revision=state['revision'],attachment=invalid),False)
    state=call('scene_undo',dict(expected_revision=state['revision']));assert state['attachment']==value
    state=call('scene_redo',dict(expected_revision=state['revision']));assert state['attachment']==changed
    state=call('scene_undo',dict(expected_revision=state['revision']));assert state['attachment']==value
    # Replacement must preserve the entire optical policy and placement.
    state=call('atmosphere_generate',dict(expected_revision=state['revision'],preset='mist_patch_v1'))
    assert state['attachment']['optics']==value['optics'] and state['attachment']['placement']==value['placement']
    state=call('scene_undo',dict(expected_revision=state['revision']));assert state['attachment']==value
    state=call('scene_save',dict(expected_revision=state['revision']));assert not state['dirty']
    assert json.loads(scene.read_text())['extensions']['ray_tracing']['authoring']['atmosphere_attachment']==value
    server.stdin.close();server.wait(timeout=5);assert server.returncode==0
print('Physical optics MCP PASS: native UI readback, typed setters/refusals, shared Undo/Redo, source replacement and File Save parity')
