"""Real MCP stdio protocol against a live isolated native editor."""
import copy
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
scene=Path(sys.argv[1])
with subprocess.Popen([sys.executable,str(ROOT/'tools/scene_editor_mcp.py'),'--scene',str(scene)],
                      stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as server:
    request_id=0
    def rpc(method,params={}):
        global request_id
        request_id+=1
        server.stdin.write(json.dumps({'jsonrpc':'2.0','id':request_id,'method':method,'params':params})+'\n');server.stdin.flush()
        response=json.loads(server.stdout.readline())
        assert response['id']==request_id and 'error' not in response,response
        return response['result']
    def call(name,args={},success=True):
        result=rpc('tools/call',{'name':name,'arguments':args})
        assert result['isError'] is not success,result
        return json.loads(result['content'][0]['text'])
    initialized=rpc('initialize',{'protocolVersion':'2025-06-18','capabilities':{},'clientInfo':{'name':'native-proof','version':'1'}})
    assert initialized['capabilities']['tools']=={'listChanged':False}
    server.stdin.write(json.dumps({'jsonrpc':'2.0','method':'notifications/initialized'})+'\n');server.stdin.flush()
    assert len(rpc('tools/list')['tools'])==10
    connected=call('scene_connect',{'scene_path':str(scene.resolve())});assert connected['ok']
    state=call('scene_read');assert state['attachment']['appearance']['density_scale']==2
    original_revision=state['revision']
    state=call('atmosphere_import',{'expected_revision':state['revision'],'source':str(scene.parent/'assets/cache/manifest.json')})
    assert state['dirty'] and state['attachment']['source_path'].startswith('assets/atmosphere/')
    imported=copy.deepcopy(state['attachment'])
    value=copy.deepcopy(state['attachment']);value['appearance']['density_scale']=0.8
    value['placement']['translation']=[1,2,3]
    value['playback'].update(animated=True,rate_numerator=30000,rate_denominator=1001)
    previous=state['revision']
    state=call('atmosphere_set',{'expected_revision':previous,'attachment':value})
    refusal=call('atmosphere_remove',{'expected_revision':previous},False)
    assert refusal['revision']==state['revision'] and refusal['attachment']==state['attachment']
    missing=copy.deepcopy(value);missing['source_path']='assets/nope.vf3d'
    refusal=call('atmosphere_set',{'expected_revision':state['revision'],'attachment':missing},False)
    assert refusal['revision']==state['revision']
    state=call('scene_seek',{'expected_revision':state['revision'],'frame':4});assert state['frame']==4
    state=call('scene_save',{'expected_revision':state['revision']});assert not state['dirty']
    saved=json.loads(scene.read_text())['extensions']['ray_tracing']['authoring']['atmosphere_attachment']
    assert saved==value
    state=call('scene_undo',{'expected_revision':state['revision']});assert state['attachment']==imported
    state=call('scene_redo',{'expected_revision':state['revision']});assert state['attachment']==value
    state=call('atmosphere_generate',{'expected_revision':state['revision'],'preset':'room_haze_v1'})
    assert state['attachment']['source_kind']=='vf3d' and not state['attachment']['playback']['animated']
    assert state['attachment']['placement']==value['placement'] and state['attachment']['appearance']==value['appearance']
    state=call('atmosphere_remove',{'expected_revision':state['revision']});assert state['attachment'] is None
    state=call('scene_undo',{'expected_revision':state['revision']});assert state['attachment']['source_kind']=='vf3d'
    state=call('scene_save',{'expected_revision':state['revision']});assert not state['dirty']
    server.stdin.close();server.wait(timeout=5);assert server.returncode==0
print('MCP live PASS: initialize/list/call, import/generate, shared commands, revision/missing refusal, seek, undo/redo, File Save parity')
