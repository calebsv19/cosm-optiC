"""Actual stdio MCP owner proof: candidates, selection, guards, history and save."""
import json
from pathlib import Path
import subprocess
import sys
repo=Path(__file__).resolve().parents[2]
scene=Path(sys.argv[1]);intent=json.loads((scene.parent/'intent.json').read_text())
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
    revision=call('scene_read')['revision']
    proposals=call('sequence_event_candidates',dict(intent=intent))['event_candidates'];assert len(proposals['candidates'])==3 and call('scene_read')['revision']==revision
    candidate=min((c for c in proposals['candidates'] if c['usable']),key=lambda c:c['possible_occlusion_samples'])
    args=dict(intent=intent,candidate=candidate['index'],proposal_sha256=candidate['proposal_sha256'],expected_revision=revision)
    call('sequence_event_select',dict(args,proposal_sha256='0'*64),False)
    call('sequence_event_select',dict(args,expected_revision=revision+1),False)
    assert call('scene_read')['revision']==revision
    call('sequence_event_select',args)
    read=call('sequence_read')['sequence'];assert len(read['event_cameras'])==1
    assert call('sequence_read_section',dict(section='event_cameras'))['sequence']['event_cameras']==read['event_cameras']
    call('scene_undo',dict(expected_revision=call('scene_read')['revision']))
    assert not call('sequence_read')['sequence']['event_cameras']
    call('scene_redo',dict(expected_revision=call('scene_read')['revision']))
    assert call('sequence_read')['sequence']['event_cameras']==read['event_cameras']
    # A bound seed cannot be independently moved; the failed edit preserves revision.
    camera=json.loads(json.dumps(read['camera_rig']['cameras'][-1]));camera['position'][0]+=1
    revision=call('scene_read')['revision'];call('sequence_camera_upsert',dict(expected_revision=revision,camera=camera),False);assert call('scene_read')['revision']==revision
    unrelated=json.loads(json.dumps(read['camera_rig']['cameras'][0]));unrelated['id']='camera/unrelated'
    call('sequence_camera_upsert',dict(expected_revision=revision,camera=unrelated))
    call('scene_undo',dict(expected_revision=call('scene_read')['revision']))
    # Replanning the same owned ID is one atomic replacement, without duplicate cameras.
    refined=json.loads(json.dumps(intent));refined['framing']['fov_y']+=5
    proposal=call('sequence_event_candidates',dict(intent=refined))['event_candidates']
    chosen=min((c for c in proposal['candidates'] if c['usable']),key=lambda c:c['possible_occlusion_samples'])
    call('sequence_event_select',dict(expected_revision=proposal['revision'],intent=refined,candidate=chosen['index'],proposal_sha256=chosen['proposal_sha256']))
    updated=call('sequence_read')['sequence'];assert len(updated['event_cameras'])==1 and len(updated['camera_rig']['cameras'])==len(read['camera_rig']['cameras'])
    assert updated['event_cameras'][0]['intent']['framing']['fov_y']==refined['framing']['fov_y']
    call('scene_undo',dict(expected_revision=call('scene_read')['revision']))
    assert call('sequence_read')['sequence']['event_cameras']==read['event_cameras']
    call('sequence_event_remove',dict(expected_revision=call('scene_read')['revision'],id=intent['id']))
    assert not call('sequence_read')['sequence']['event_cameras']
    call('scene_undo',dict(expected_revision=call('scene_read')['revision']))
    assert call('sequence_read')['sequence']['event_cameras']==read['event_cameras']
    call('scene_save',dict(expected_revision=call('scene_read')['revision']))
    owner.stdin.close();owner.wait(timeout=5);assert owner.returncode==0
print('Real event-camera stdio MCP/read/proposal guards/Undo/Redo/save PASS')
