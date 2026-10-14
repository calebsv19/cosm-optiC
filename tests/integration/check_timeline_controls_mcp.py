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
    def mutate_bad(tool,**args):return call(tool,dict(expected_revision=state()['revision'],**args),False)
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
        expected=json.loads((scene.parent/'timeline-controls-expected.json').read_text());assert full()==expected
        saved=json.loads(scene.read_text());original=json.loads((scene.parent/'original_scene.json').read_text())
        assert saved['extensions']['ray_tracing']['authoring']['light_timeline']==original['extensions']['ray_tracing']['authoring']['light_timeline']
        renders=json.loads((scene.parent/'timeline-controls-renders.json').read_text())
        for key,r in renders.items():
            fresh=render('reopened-'+key,r['settings']['frame'],r['settings']['integrator']);assert r['settings']['integrator']=='disney_v2' or fresh['image_sha256']==r['image_sha256'],key
        assert not state()['dirty']
    else:
        ui=json.loads((scene.parent/'timeline-controls-ui-expected.json').read_text());known=full()
        assert known['rate']==dict(numerator=30000,denominator=1001) and not track('yaw')['enabled']
        catalog=call('editor_catalog');assert len(catalog['tools'])==len(catalog_module.tool_list())
        def clock(n=24000,d=1001,start=-5,count=106,ok=True,revision=None):
            args=dict(rate_numerator=n,rate_denominator=d,start_frame=start,frame_count=count,expected_revision=state()['revision'] if revision is None else revision)
            return call('editor_command',dict(catalog_id=catalog['catalog_id'],tool='animation_set_clock',arguments=args),ok)
        def unchanged(action):
            old=full();rev=state()['revision'];action();assert full()==old and state()['revision']==rev
        unchanged(lambda:clock(n=True,ok=False));unchanged(lambda:clock(d=0,ok=False));unchanged(lambda:clock(count=0,ok=False));unchanged(lambda:clock(start=1000000000,count=106,ok=False));unchanged(lambda:clock(start=0,count=40,ok=False));unchanged(lambda:clock(revision=state()['revision']+1,ok=False))
        mutate('scene_seek',frame=24);clock();assert state()['frame']==24
        clocked=full();assert clocked['rate']==dict(numerator=24000,denominator=1001) and clocked['range']==dict(start_frame=-5,frame_count=106)
        assert clocked['tracks']==known['tracks'];mutate('scene_undo');assert full()==known;mutate('scene_redo');assert full()==clocked
        rev=state()['revision'];clock();assert state()['revision']==rev
        mutate('scene_seek',frame=100);clock(count=80);assert state()['frame']==74;clock();mutate('scene_seek',frame=24)
        unchanged(lambda:mutate_bad('animation_remove_channel',track_id='light-progress'))
        unchanged(lambda:mutate_bad('animation_set_channel_enabled',track_id='light-progress',enabled=False))
        unchanged(lambda:mutate_bad('animation_remove_channel',track_id='missing'))
        mutate('animation_set_channel_enabled',track_id='yaw',enabled=True);enabled=full();mutate('scene_undo');assert full()==clocked;mutate('scene_redo');assert full()==enabled
        lit=render('enabled24',24);mutate('animation_set_channel_enabled',track_id='intensity',enabled=False);dark=render('disabled-intensity24',24);assert lit['image_sha256']!=dark['image_sha256']
        mutate('scene_undo');assert full()==enabled
        mutate('object_set_flag',object_id='surface',flag='locked',value=True)
        unchanged(lambda:mutate_bad('animation_set_channel_enabled',track_id='position-x',enabled=False));unchanged(lambda:mutate_bad('animation_remove_channel',track_id='position-x'));mutate('scene_undo')
        mutate('animation_set_channel_enabled',track_id='position-x',enabled=False);assert all(not track('position-'+a)['enabled'] for a in 'xyz')
        unchanged(lambda:call('animation_set_key',dict(track_id='position-x',frame=24,value=2,expected_revision=state()['revision']),False))
        mutate('animation_set_channel_enabled',track_id='position-y',enabled=True);assert all(track('position-'+a)['enabled'] for a in 'xyz')
        restored=full();mutate('animation_remove_channel',track_id='position-z');assert not any(t['target_id']=='object/surface' for t in read()['tracks']);mutate('scene_undo');assert full()==restored
        mutate('animation_remove_channel',track_id='yaw');assert not any(t['track_id']=='yaw' for t in read()['tracks']);removed=full();mutate('scene_undo');assert full()==restored;mutate('scene_redo');assert full()==removed
        mutate('animation_set_channel_enabled',track_id='intensity',enabled=False)
        before=render('before-clock24',24);clock(n=30000,d=1001);after=render('after-rate24',24);assert before['image_sha256']==after['image_sha256'],'rate retains absolute key sampling'
        renders={}
        for key,frame,integrator in [('direct0',0,'direct_light'),('direct24',24,'direct_light'),('disney24',24,'disney_v2')]:renders[key]=render(key,frame,integrator)
        assert renders['direct0']['image_sha256']!=renders['direct24']['image_sha256']
        mutate('scene_save');assert state()['attachment']==attached and not state()['dirty']
        saved=json.loads(scene.read_text())['extensions']['ray_tracing']['authoring'];raw=saved['scene_timeline'];assert raw['producer_timeline']==ui['producer_timeline'] and raw['rate']['producer_rate']=='keep' and raw['range']['producer_range']=='keep'
        for t in raw['tracks']:
            old=next(t0 for t0 in ui['tracks'] if t0['id']==t['id']);assert t['producer_metadata']==old['producer_metadata'] and t['keys']==old['keys']
        for k,r in renders.items():
            fresh=render('saved-'+k,r['settings']['frame'],r['settings']['integrator']);assert r['settings']['integrator']=='disney_v2' or fresh['image_sha256']==r['image_sha256']
            before_source=json.loads((Path(r['image_path']).parent/'source_snapshot.json').read_text())['extensions']['ray_tracing']['authoring']['scene_timeline']
            after_source=json.loads((Path(fresh['image_path']).parent/'source_snapshot.json').read_text())['extensions']['ray_tracing']['authoring']['scene_timeline'];assert before_source==after_source

        (scene.parent/'timeline-controls-expected.json').write_text(json.dumps(full(),indent=2));(scene.parent/'timeline-controls-renders.json').write_text(json.dumps(renders,indent=2))
        (scene.parent/'timeline-controls-receipt.json').write_text(json.dumps(dict(ui_to_mcp=True,cached_catalog_dispatch=True,rational_clock=True,keys_and_unknown_metadata_preserved=True,playhead_preserved_and_clamped=True,disabled_key_range_refusal=True,typed_stale_overflow_refusals=True,required_path_refusal=True,locked_group_refusal=True,grouped_xyz_enable_remove=True,channel_enable_pixel_effect=True,shared_history=True,rate_absolute_frame_parity=True,attachment_preserved=True,saved_direct_render_parity=True,disney_job_and_source_parity=True,direct_and_disney=True),indent=2))
    bridge.stdin.close();bridge.wait(timeout=5);assert bridge.returncode==0
print('Timeline controls MCP '+mode+' PASS')
