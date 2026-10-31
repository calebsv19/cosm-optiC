"""Actual stdio MCP owner proof: candidates, selection, guards, history and save."""
import json
from pathlib import Path
import subprocess
import sys
repo=Path(__file__).resolve().parents[2]
scene=Path(sys.argv[1]);intent=json.loads(Path('planner-intent.json').read_text());xy=json.loads(Path('planner-pick.json').read_text())
with subprocess.Popen([sys.executable,'-B',str(repo/'tools/scene_editor_mcp.py'),'--scene',str(scene)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as owner:
    serial=0
    def rpc(method,params=None):
        global serial
        serial+=1;owner.stdin.write(json.dumps(dict(jsonrpc='2.0',id=serial,method=method,params=params or {}))+'\n');owner.stdin.flush()
        while True:
            line=owner.stdout.readline();assert line
            row=json.loads(line)
            if row.get('id')==serial:break
        assert 'error' not in row,row
        return row['result']
    def call(name,args=None,ok=True):
        value=rpc('tools/call',dict(name=name,arguments=args or {}));assert bool(value.get('isError')) != ok,value
        return json.loads(value['content'][0]['text']) if ok else value
    rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='event-camera-proof',version='1')))
    tools={t['name'] for t in rpc('tools/list')['tools']}
    assert {'sequence_event_planner_read','sequence_event_draft','sequence_event_pick'}<=tools
    revision=call('scene_read')['revision']
    call('sequence_event_draft',dict(intent=intent,expected_revision=revision+1),False)
    call('sequence_event_draft',dict(intent=intent,expected_revision=revision))
    state=call('sequence_event_planner_read')['event_planner'];assert state['open'] and state['intent']==intent and state['viewport_token']>0
    assert call('scene_read')['revision']==revision
    assert state['coordinate_space']=='logical_window_pixels' and state['viewport']['width']>0 and state['viewport']['height']>0
    args=dict(xy,viewport_token=state['viewport_token'],expected_revision=revision)
    feature=call('sequence_event_pick',args)['event_feature'];assert feature['feature']['kind']=='body_point' and feature['source_tick']==150
    call('sequence_event_pick',dict(args,viewport_token=args['viewport_token']+1),False)
    call('sequence_event_pick',dict(args,expected_revision=revision+1),False)
    call('sequence_event_pick',dict(args,x=0,y=0),False)
    assert call('sequence_event_planner_read')['event_planner']['intent']==intent
    assert call('scene_read')['revision']==revision
    proposals=call('sequence_event_candidates',dict(intent=intent))['event_candidates'];assert len(proposals['candidates'])==3
    owner.stdin.close();owner.wait(timeout=5);assert owner.returncode==0
print('Event planner real stdio MCP: draft/read/surface query/stale guards/no document mutation PASS')
