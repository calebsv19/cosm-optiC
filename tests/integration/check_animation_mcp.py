"""Real bridge qualification of shared animation commands and render sampling."""
import json,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];scene=Path(sys.argv[1]).resolve();mode=sys.argv[2]
sys.path.insert(0,str(ROOT/'tools'));import scene_editor_mcp_catalog as catalog_module
with subprocess.Popen([sys.executable,'-B',str(ROOT/'tools/scene_editor_mcp.py'),'--scene',str(scene)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as bridge:
    sequence=0
    def rpc(method,params=None):
        global sequence
        sequence+=1;bridge.stdin.write(json.dumps(dict(jsonrpc='2.0',id=sequence,method=method,params=params or {}))+'\n');bridge.stdin.flush()
        while True:
            line=bridge.stdout.readline();assert line,'bridge ended';r=json.loads(line)
            if r.get('id')==sequence:break
        assert 'error' not in r,r;return r['result']
    def call(tool,args=None,ok=True):
        r=rpc('tools/call',dict(name=tool,arguments=args or {}));assert r['isError'] is not ok,r
        return json.loads(r['content'][0]['text']) if ok else r
    def state():return call('scene_read')
    def mutate(tool,**args):return call(tool,dict(expected_revision=state()['revision'],**args))
    def read():return call('animation_read')['animation']
    def track(id):return call('animation_track_read',dict(track_id=id))['track']
    def full():
        a=read();return dict(version=1,rate=a['rate'],range=a['range'],tracks=[track(t['track_id']) for t in a['tracks']])
    def render(id,frame,integrator='direct_light'):
        before=state();disk=scene.read_bytes()
        mutate('render_start',request_id=id,width=64,height=40,frame=frame,integrator=integrator,temporal_frames=2 if integrator=='disney_v2' else 1)
        for _ in range(2000):
            r=call('render_status',dict(job_id=id))['render']
            if r['state'] not in ('preparing','rendering','canceling'):break
            time.sleep(.02)
        assert r['state']=='completed',r
        r=call('render_results',dict(job_id=id))['render'];assert r['artifact_integrity'] and r['enabled_lights']>0
        assert state()==before and scene.read_bytes()==disk
        return r
    rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='animation-proof',version='1')))
    assert len(rpc('tools/list')['tools'])==len(catalog_module.tool_list())+2
    attached=state()['attachment'];assert attached is not None
    if mode=='reopen':
        assert full()==json.loads((scene.parent/'animation-mcp-expected.json').read_text())
        expected=json.loads((scene.parent/'animation-renders.json').read_text())
        for key,r in expected.items():
            fresh=render('reopened-'+key,r['settings']['frame'],r['settings']['integrator'])
            assert Path(fresh['image_path']).read_bytes()==Path(r['image_path']).read_bytes(),key
        assert not state()['dirty']
    else:
        render('baseline-disney',0,'disney_v2')
        render('baseline-disney24',24,'disney_v2')
        original=full();assert original==json.loads((scene.parent/'animation-ui-expected.json').read_text())
        revision=state()['revision'];disk=scene.read_bytes();assert read()['valid'] and full()==original and state()['revision']==revision and scene.read_bytes()==disk
        catalog=call('editor_catalog');assert call('editor_command',dict(catalog_id=catalog['catalog_id'],tool='animation_read',arguments={}))['animation']==read()
        mutate('animation_activate');assert full()==original and state()['revision']==revision
        lens=next(t['track_id'] for t in read()['tracks'] if t['property_id']=='camera/fov_y')
        call('animation_set_key',dict(track_id=lens,frame=20,value=70,expected_revision=revision+1),False)
        for bad in (dict(frame=True,value=70),dict(frame=-1,value=70),dict(frame=20,value=True),dict(frame=20,value=float('nan')),dict(frame=20,value=180)):
            call('animation_set_key',dict(track_id=lens,expected_revision=state()['revision'],**bad),False)
            assert full()==original and state()['revision']==revision
        mutate('animation_set_key',track_id=lens,frame=20,value=70)
        changed=full();mutate('scene_undo');assert full()==original;mutate('scene_redo');assert full()==changed
        mutate('animation_move_key',track_id=lens,frame=20,destination_frame=24)
        mutate('animation_interpolation',track_id=lens,frame=0,mode='cubic_bezier')
        mutate('animation_handles',track_id=lens,frame=0,incoming_frames=0,incoming_value=0,outgoing_frames=4,outgoing_value=2)
        mutate('animation_tangent',track_id=lens,frame=24,mode='auto_clamped')
        mutate('animation_set_key',track_id=lens,frame=12,value=64);mutate('animation_delete_key',track_id=lens,frame=12)
        old=full();rev=state()['revision'];call('animation_move_key',dict(track_id=lens,frame=24,destination_frame=0,expected_revision=rev),False);assert full()==old and state()['revision']==rev
        render('after-lens',24,'disney_v2')
        mutate('animation_add_channel',track_id=lens,property_id='camera/yaw')
        yaw=next(t['track_id'] for t in read()['tracks'] if t['property_id']=='camera/yaw')
        mutate('animation_set_key',track_id=yaw,frame=24,value=.15)
        render('after-yaw',24,'disney_v2')
        light=next(t['track_id'] for t in read()['tracks'] if t['property_id']=='light/path_progress')
        mutate('animation_add_channel',track_id=light,property_id='light/intensity')
        intensity=next(t['track_id'] for t in read()['tracks'] if t['property_id']=='light/intensity')
        mutate('animation_set_key',track_id=intensity,frame=24,value=1.5)
        mutate('animation_object_channels',object_id='surface')
        x=next(t['track_id'] for t in read()['tracks'] if t['target_id']=='object/surface' and t['property_id'].endswith('position_x'))
        mutate('animation_set_key',track_id=x,frame=24,value=.8)
        render('after-object',24,'disney_v2')
        mutate('object_set_flag',object_id='surface',flag='locked',value=True);rev=state()['revision'];old=full()
        call('animation_set_key',dict(track_id=x,frame=12,value=.4,expected_revision=rev),False);assert state()['revision']==rev and full()==old;mutate('scene_undo')
        call('camera_set_pose',dict(position_m=[0,0,0],yaw_degrees=0,pitch_degrees=0,expected_revision=state()['revision']),False)
        mutate('scene_seek',frame=12);head=state()['frame'];mutate('animation_set_key',track_id=x,frame=24,value=.8);assert state()['frame']==head
        renders={}
        for key,frame,integrator in [('direct0',0,'direct_light'),('direct24',24,'direct_light'),('disney24',24,'disney_v2')]:renders[key]=render(key,frame,integrator)
        assert Path(renders['direct0']['image_path']).read_bytes()!=Path(renders['direct24']['image_path']).read_bytes(),'animated frame pixel effect'
        before_save={k:Path(v['image_path']).read_bytes() for k,v in renders.items()}
        mutate('scene_save');assert not state()['dirty'] and state()['attachment']==attached
        for k,v in renders.items():
            r=render('saved-'+k,v['settings']['frame'],v['settings']['integrator']);assert Path(r['image_path']).read_bytes()==before_save[k]
        expected=full();saved=json.loads(scene.read_text())['extensions']['ray_tracing']['authoring'];assert saved['scene_timeline']==expected
        (scene.parent/'animation-mcp-expected.json').write_text(json.dumps(expected,indent=2))
        (scene.parent/'animation-renders.json').write_text(json.dumps(renders,indent=2))
        (scene.parent/'animation-mcp-receipt.json').write_text(json.dumps(dict(ui_to_mcp=True,compatibility_bridge=True,key_value_time_interpolation_handles_tangent_delete=True,camera_light_object_channels=True,stale_typed_collision_locked_refusals=True,shared_history=True,playhead_preserved=True,attachment_preserved=True,unsaved_saved_render_parity=True,animated_pixel_effect=True,direct_and_disney=True),indent=2))
    bridge.stdin.close();bridge.wait(timeout=5);assert bridge.returncode==0
print('Animation MCP '+mode+' PASS')
