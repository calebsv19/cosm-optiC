"""Real stdio render workflow over the same native document and UI job owner."""
import hashlib,json,subprocess,sys,time
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
        r=rpc('tools/call',dict(name=tool,arguments=args or {}));assert r['isError'] is not ok,r
        return json.loads(r['content'][0]['text']) if ok else r
    def state():return call('scene_read')
    def mutate(tool,**args):return call(tool,dict(expected_revision=state()['revision'],**args))
    def status(id):return call('render_status',{'job_id':id})['render']
    def results(id):return call('render_results',{'job_id':id})['render']
    def wait(id):
        deadline=time.monotonic()+40
        while time.monotonic()<deadline:
            s=status(id)
            if s['state'] not in ('preparing','rendering','canceling'):return s
            time.sleep(.02)
        raise TimeoutError(s)
    rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='render-workflow-proof',version='1')))
    tools=rpc('tools/list')['tools'];assert len(tools)==len(catalog_module.tool_list())+2
    initial=state();disk=scene.read_bytes();assert initial['attachment'] is not None
    ui=json.loads((scene.parent/'render-ui-expected.json').read_text());assert results(ui['job_id'])['artifact_integrity']
    catalog=call('editor_catalog');assert len(catalog['tools'])==len(tools)-2
    read=call('render_read');assert call('editor_command',dict(catalog_id=catalog['catalog_id'],tool='render_read',arguments={}))==read
    settings=dict(width=64,height=40,integrator='direct_light',temporal_frames=1,frame=0)
    args=dict(request_id='mcp-direct',expected_revision=initial['revision'],**settings)
    call('editor_command',dict(catalog_id=catalog['catalog_id'],tool='render_start',arguments=args));assert wait('mcp-direct')['state']=='completed'
    direct=results('mcp-direct');assert direct['artifact_integrity'] and direct['settings']==settings and direct['completed_subpasses']==1
    assert Path(direct['image_path']).read_bytes()==Path(ui['image_path']).read_bytes(), 'UI/MCP Direct Light output parity'
    assert direct['primitive_count']>0 and direct['triangle_count']>0 and direct['volume_enabled'] and direct['volume_attached']
    assert direct['source_sha256']==ui['source_sha256'] and state()==initial and scene.read_bytes()==disk
    before=Path(direct['image_path']).read_bytes();call('render_start',args,False);assert Path(direct['image_path']).read_bytes()==before
    for changes in ({'request_id':'../escape'},{'request_id':'bad','width':True},{'request_id':'bad','height':0},{'request_id':'bad','temporal_frames':2},{'request_id':'bad','extra':1},{'request_id':'bad','expected_revision':initial['revision']+1}):
        call('render_start',dict(args,**changes),False);assert scene.read_bytes()==disk and state()==initial
    call('render_status',{'job_id':'../escape'},False);call('render_results',{'job_id':'missing'},False)
    mutate('render_start',request_id='mcp-disney',**dict(settings,integrator='disney_v2',temporal_frames=2));assert wait('mcp-disney')['state']=='completed';disney=results('mcp-disney');assert disney['artifact_integrity'] and disney['completed_subpasses']==2
    mutate('render_start',request_id='mcp-failed',width=512,height=512,integrator='disney_v2',temporal_frames=2,frame=0)
    (scene.parent/'.optic-renders/mcp-failed/frame.bmp').mkdir()
    assert wait('mcp-failed')['state']=='failed';failed=results('mcp-failed');assert not failed['artifact_integrity'] and failed['diagnostic']=='save_failed'
    assert state()==initial and scene.read_bytes()==disk
    mutate('render_start',request_id='mcp-cancel',width=1024,height=768,integrator='disney_v2',temporal_frames=32,frame=0)
    assert call('render_read')['render']['active'];call('scene_undo',{'expected_revision':initial['revision']},False);call('scene_save',{'expected_revision':initial['revision']},False)
    call('render_cancel',{'job_id':'wrong','expected_revision':initial['revision']},False);mutate('render_cancel',job_id='mcp-cancel');assert wait('mcp-cancel')['state']=='canceled';canceled=results('mcp-cancel');assert not canceled['artifact_integrity'] and not Path(canceled['image_path']).exists()
    assert not call('render_read')['render']['active'] and state()==initial and scene.read_bytes()==disk
    # Fresh results are truthful even if the published artifact changes externally.
    image=Path(direct['image_path']);data=image.read_bytes();image.write_bytes(data[:-1]);assert not results('mcp-direct')['artifact_integrity'];image.write_bytes(data);assert results('mcp-direct')['artifact_integrity']
    # Unsaved camera edit -> snapshot/render -> shared Undo, without touching disk.
    mutate('camera_set_pose',position_m=[2.5,-2.5,3.75],yaw_degrees=180.,pitch_degrees=-45.)
    dirty_state=state();assert dirty_state['dirty']
    mutate('render_start',request_id='mcp-dirty',**settings);assert wait('mcp-dirty')['state']=='completed';dirty=results('mcp-dirty');assert dirty['artifact_integrity'] and dirty['source_sha256']!=direct['source_sha256']
    assert Path(dirty['image_path']).read_bytes()!=Path(direct['image_path']).read_bytes()
    assert scene.read_bytes()==disk and state()==dirty_state;mutate('scene_undo');assert state()['attachment']==initial['attachment']
    expected={'failed':failed,'dirty':dirty,'direct':direct,'disney':disney,'canceled':canceled,'ui':ui};(scene.parent/'render-mcp-expected.json').write_text(json.dumps(expected,indent=2)+'\n')
    (scene.parent/'render-mcp-receipt.json').write_text(json.dumps(dict(ui_mcp_exact_direct_light_bmp_parity=True,named_and_compatibility_workflow=True,unique_id_collision_refusal=True,typed_stale_refusals=True,disney_v2_committed_passes=True,cancel_no_final_image=True,exclusive_context_mutation_guards=True,artifact_hash_integrity_readback=True,publication_failure_terminal_state=True,scene_bytes_revision_and_attachment_preserved=True),indent=2)+'\n')
    bridge.stdin.close();bridge.wait(timeout=5);assert bridge.returncode==0
print('Render MCP workflow PASS')
