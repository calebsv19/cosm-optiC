"""Real stdio object commands against a native editor pumping its owner thread."""
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
        sequence+=1;server.stdin.write(json.dumps(dict(jsonrpc='2.0',id=sequence,method=method,params=params or {}))+'\n');server.stdin.flush()
        while True:
            result=json.loads(server.stdout.readline())
            if 'id' in result:break
        assert result['id']==sequence and 'error' not in result,result
        return result['result']
    def call(name,args=None,ok=True):
        result=rpc('tools/call',dict(name=name,arguments=args or {}));assert result['isError'] is not ok,result
        if not ok:return result
        value=json.loads(result['content'][0]['text']);assert value['ok'],value;return value
    def state():return call('scene_read')
    def read(id='surface'):return call('object_read',dict(object_id=id))['object']
    def mutate(tool,ok=True,**args):return call(tool,dict(expected_revision=state()['revision'],**args),ok)
    def undo():mutate('scene_undo')
    def all_objects():return [read(o['object_id']) for o in call('object_list')['objects']]
    rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='objects-proof',version='1')))
    tools=rpc('tools/list')['tools'];assert len([t for t in tools if t['name'].startswith('object_')])==8
    assert 'expected_revision' not in next(t for t in tools if t['name']=='object_read')['inputSchema']['required']
    original=read();assert original==json.loads((scene.parent/'object-ui-expected.json').read_text())
    assert 'flags' not in read('rowless')
    objects=all_objects();attachment=state()['attachment'];assert attachment is not None
    disk_before=scene.read_bytes();revision=state()['revision'];assert call('material_read',dict(object_id='rowless'))['material']['retained'] is None
    assert scene.read_bytes()==disk_before and state()['revision']==revision
    catalog=call('editor_catalog');assert len(catalog['tools'])==len(tools)-2 and catalog['scene_path']==str(scene)
    wrapped=call('editor_command',dict(catalog_id=catalog['catalog_id'],tool='object_read',arguments=dict(object_id='surface')));assert wrapped['object']==original
    def unchanged():assert all_objects()==objects and state()['attachment']==attachment
    for kind in ('box','plane'):
        created=mutate('object_create',kind=kind);id=created['object_id'];assert id not in {o['object_id'] for o in objects}
        assert read(id)['transform']['scale']==dict(x=1.,y=1.,z=1.)
        assert read(id)['primitive']['kind']==('rect_prism_primitive' if kind=='box' else 'plane_primitive')
        undo();unchanged();mutate('scene_redo');assert read(id)['object_id']==id;undo();unchanged()
    mutate('object_create',False,kind='sphere');unchanged()
    old_revision=state()['revision'];mutate('object_rename',object_id='surface',name='MCP café')
    assert read()['display_name']=='MCP café'
    call('object_rename',dict(object_id='surface',name='stale',expected_revision=old_revision),False);assert read()['display_name']=='MCP café';undo();unchanged()
    mutate('object_set_transform',object_id='surface',position=[1.25,-.5,2.],rotation_degrees=[15.,-30.,45.],scale=[.5,1.5,2.])
    t=read()['transform'];assert t['position']==dict(x=1.25,y=-.5,z=2.) and t['rotation']==dict(x=15.,y=-30.,z=45.) and t['scale']==dict(x=.5,y=1.5,z=2.)
    undo();unchanged();mutate('scene_redo');assert read()['transform']==t;undo();unchanged()
    for invalid in ([0,1,1],[-1,1,1],[True,1,1],[1,1],[float('nan'),1,1]):
        mutate('object_set_transform',False,object_id='surface',position=[0,0,0],rotation_degrees=[0,0,0],scale=invalid);unchanged()
    for flag in ('visible','locked'):
        mutate('object_set_flag',object_id='surface',flag=flag,value=(flag=='locked'))
        assert read()['flags'][flag]==(flag=='locked');undo();unchanged()
    mutate('object_set_flag',object_id='surface',flag='locked',value=True);locked_revision=state()['revision']
    for name,args in [('object_rename',dict(name='blocked')),('object_set_transform',dict(position=[0,0,0],rotation_degrees=[0,0,0],scale=[1,1,1])),('object_duplicate',{}),('object_remove',{}),('object_set_flag',dict(flag='visible',value=False))]:
        mutate(name,False,object_id='surface',**args);assert state()['revision']==locked_revision
    mutate('object_set_flag',object_id='surface',flag='locked',value=False);undo();undo();unchanged()
    for args in (dict(object_id='missing'),dict(object_id=True),dict(object_id='surface',extra=True)):call('object_read',args,False)
    mutate('object_set_flag',False,object_id='surface',flag='visible',value=1)
    mutate('object_rename',False,object_id='surface',name='é'*80);unchanged()
    duplicate=mutate('object_duplicate',object_id='recipient')['object_id'];assert duplicate!='recipient'
    src=read('recipient');dupe=read(duplicate);assert dupe['transform']==src['transform'] and dupe.get('extensions')==src.get('extensions') and dupe['producer_metadata']==src['producer_metadata']
    undo();unchanged()
    mutate('object_remove',object_id='recipient');assert 'recipient' not in {o['object_id'] for o in call('object_list')['objects']};undo();unchanged()
    # Keep an edited name and new primitive for save/reopen proof.
    mutate('object_rename',object_id='surface',name='MCP saved café')
    final_id=call('editor_command',dict(catalog_id=catalog['catalog_id'],tool='object_create',arguments=dict(kind='box',expected_revision=state()['revision'])))['object_id']
    mutate('object_set_transform',object_id=final_id,position=[3,0,0],rotation_degrees=[0,20,0],scale=[1,2,1])
    expected=all_objects();state_saved=mutate('scene_save');assert not state_saved['dirty'];assert state_saved['attachment']==attachment
    saved=json.loads(scene.read_text());assert saved['objects']==expected
    (scene.parent/'object-mcp-expected.json').write_text(json.dumps(expected,indent=2)+'\n')
    (scene.parent/'object-mcp-receipt.json').write_text(json.dumps(dict(ui_to_mcp=True,compatibility_dispatch=True,all_eight_tools=True,shared_history=True,typed_stale_locked_refusals=True,attachment_preserved=True,legacy_read_no_mutation=True,producer_metadata_preserved=True,save_disk_parity=True),indent=2)+'\n')
    server.stdin.close();server.wait(timeout=5);assert server.returncode==0
print('Objects MCP PASS: shared commands/history, UI readback, flags, CRUD, base pose, refusals and save parity')
