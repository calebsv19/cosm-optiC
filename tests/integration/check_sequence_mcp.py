"""Real stdio MCP read/guard/history check against the isolated native owner."""
import json,subprocess,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[2]
with subprocess.Popen([sys.executable,'-B',str(repo/'tools/scene_editor_mcp.py'),'--scene',sys.argv[1]],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as p:
    serial=0
    def rpc(method,params=None):
        global serial
        serial+=1;p.stdin.write(json.dumps(dict(jsonrpc='2.0',id=serial,method=method,params=params or {}))+'\n');p.stdin.flush()
        while True:
            line=p.stdout.readline();assert line
            row=json.loads(line)
            if row.get('id')==serial:break
        assert 'error' not in row,row
        return row['result']
    def call(name,args=None,ok=True):
        value=rpc('tools/call',dict(name=name,arguments=args or {}));assert bool(value.get('isError')) != ok,value
        return json.loads(value['content'][0]['text']) if ok else value
    rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='sequence-proof',version='1')))
    read=call('sequence_read')['sequence'];assert len(read['inspection_moves'])==1
    assert call('sequence_read_section',dict(section='camera_rig'))['sequence']['camera_rig']==read['camera_rig']
    call('sequence_read_section',dict(section='unknown'),False)
    state=call('scene_read');revision=state['revision']
    call('sequence_clock_set',dict(expected_revision=revision-1,camera_id='camera/A',clock={}),False)
    assert call('scene_read')['revision']==revision
    rig=json.loads(json.dumps(read['camera_rig']));extra=json.loads(json.dumps(rig['cameras'][0]));extra['id']='camera/unrelated';rig['cameras'].append(extra)
    call('sequence_apply',dict(expected_revision=revision,changes=dict(camera_rig=rig)))
    assert len(call('sequence_read')['sequence']['inspection_moves'])==1
    call('scene_undo',dict(expected_revision=call('scene_read')['revision']))
    revision=call('scene_read')['revision']
    call('sequence_inspection_remove',dict(expected_revision=revision,id='contact_orbit'))
    assert not call('sequence_read')['sequence']['inspection_moves']
    call('scene_undo',dict(expected_revision=call('scene_read')['revision']))
    assert len(call('sequence_read')['sequence']['inspection_moves'])==1
    assert call('sequence_read')['sequence']['camera_rig']==read['camera_rig']
    call('scene_save',dict(expected_revision=call('scene_read')['revision']))
    p.stdin.close();p.wait(timeout=5);assert p.returncode==0
print('Real sequence MCP read/guard/history/save PASS')
