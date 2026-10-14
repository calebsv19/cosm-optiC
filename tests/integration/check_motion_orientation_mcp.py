"""Real stdio bridge qualification of native spatial path commands."""
import json,subprocess,sys,time,copy
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
 def bad(tool,**args):return call(tool,dict(expected_revision=state()['revision'],**args),False)
 def plan(target):return call('motion_plan_read',dict(target_id=target))['plan']
 def orient(target):return call('follower_orientation_read',dict(target_id=target))['orientation']
 targets=['object/surface','camera/main','light/mirror_key_light']
 def full():return {target:dict(plan=plan(target),orientation=orient(target)) for target in targets}
 def request(arrival=2.):return dict(start_time=.1,max_speed=5.,acceleration=50.,braking=50.,waypoints=[dict(progress=0.,speed=0.,hold=0.,fixed_arrival=False,arrival=0.),dict(progress=1.,speed=0.,hold=0.,fixed_arrival=True,arrival=arrival)])
 def render(id,frame,integrator='direct_light'):
  before=state();disk=scene.read_bytes();mutate('render_start',request_id=id,width=64,height=40,frame=frame,integrator=integrator,temporal_frames=2 if integrator=='disney_v2' else 1)
  for _ in range(1500):
   r=call('render_status',dict(job_id=id))['render']
   if r['state'] not in ('preparing','rendering','canceling'):break
   time.sleep(.02)
  assert r['state']=='completed',r;r=call('render_results',dict(job_id=id))['render'];assert r['artifact_integrity'] and r['enabled_lights']>0;assert state()==before and scene.read_bytes()==disk;return r
 rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='spatial-paths-proof',version='1')))
 assert len(rpc('tools/list')['tools'])==len(catalog_module.tool_list())+2
 attachment=state()['attachment']
 if mode=='reopen':
  assert full()==json.loads((scene.parent/'motion-orientation-expected.json').read_text())
  for key,r in json.loads((scene.parent/'motion-orientation-renders.json').read_text()).items():
   fresh=render('reopened-'+key,r['settings']['frame'],r['settings']['integrator']);assert r['settings']['integrator']=='disney_v2' or fresh['image_sha256']==r['image_sha256']
  assert not state()['dirty']
 else:
  known=full();assert known[targets[0]]['orientation']['follow_direction'] and known[targets[0]]['orientation']['start_roll_degrees']==25 and known[targets[0]]['plan']['max_speed']==5
  assert known[targets[0]]['plan']['producer_plan']=='keep';catalog=call('editor_catalog');assert len(catalog['tools'])==len(catalog_module.tool_list())
  def unchanged(action):
   old=full();rev=state()['revision'];frame=state()['frame'];action();assert full()==old and state()['revision']==rev and state()['frame']==frame
  unchanged(lambda:call('motion_plan_apply',dict(target_id=targets[0],request=request(),expected_revision=state()['revision']+1),False))
  invalid=request();invalid['max_speed']=True;unchanged(lambda:bad('motion_plan_apply',target_id=targets[0],request=invalid))
  unchanged(lambda:bad('motion_plan_apply',target_id=targets[0],request=request(.11)));unchanged(lambda:bad('motion_plan_apply',target_id='object/missing',request=request()))
  unchanged(lambda:bad('follower_orientation_set',target_id=targets[0],settings={}));unchanged(lambda:bad('follower_orientation_set',target_id=targets[0],settings={'camera_mode':'route'}));unchanged(lambda:bad('follower_orientation_set',target_id=targets[0],settings={'forward_axis':True}));unchanged(lambda:bad('follower_orientation_set',target_id=targets[0],settings={'start_roll_degrees':36001}));unchanged(lambda:bad('follower_orientation_set',target_id=targets[1],settings={'forward_axis':0}))
  mutate('object_set_flag',object_id='surface',flag='locked',value=True);unchanged(lambda:bad('motion_plan_restore',target_id=targets[0]));unchanged(lambda:bad('follower_orientation_set',target_id=targets[0],settings={'start_roll_degrees':30}));mutate('scene_undo')
  before=full();mutate('motion_plan_restore',target_id=targets[0]);assert plan(targets[0]) is None;mutate('scene_undo');assert full()==before
  effects={}
  for label,target in zip(['object','camera','light'],targets):
   before=full();rev=state()['revision'];preview=call('editor_command',dict(catalog_id=catalog['catalog_id'],tool='motion_plan_preview',arguments=dict(target_id=target,request=request(),expected_revision=rev)))['preview'];assert full()==before and state()['revision']==rev and abs(preview['end_time']-2)<1e-9 and preview['waypoints'][0]['distance_m']==0
   old=render('plan-'+label+'-before24',24);mutate('motion_plan_apply',target_id=target,request=request());planned=full();new=render('plan-'+label+'-after24',24);assert old['image_sha256']!=new['image_sha256'],label;effects[label]=True
   mutate('scene_undo');assert full()==before;mutate('scene_redo');assert full()==planned
   route='path-'+str(targets.index(target)+1);row=call('path_read',dict(path_id=route))['path']['points'][0]
   unchanged(lambda:bad('path_set_point',path_id=route,point_id=row['id'],position=[x+.1 for x in row['position']],incoming=row['incoming'],outgoing=row['outgoing'],segment=row['segment']))
   unchanged(lambda:bad('path_bind',target_id=target,path_id=route,attach=False))
  unchanged(lambda:bad('animation_set_clock',rate_numerator=24,rate_denominator=1,start_frame=0,frame_count=80))
  progress=next(t['track_id'] for t in call('animation_read')['animation']['tracks'] if t['property_id']=='object/path_progress')
  unchanged(lambda:bad('animation_set_key',track_id=progress,frame=24,value=.5))
  # Restore one plan, preview holds, then reapply; original progress keys stay exact.
  mutate('motion_plan_restore',target_id=targets[0]);held=request();held['waypoints'].insert(1,dict(progress=.5,speed=0.,hold=.1,fixed_arrival=True,arrival=1.));rev=state()['revision'];preview=mutate('motion_plan_preview',target_id=targets[0],request=held)['preview'];assert state()['revision']==rev and preview['waypoints'][1]['departure']>preview['waypoints'][1]['arrival'];mutate('scene_undo');assert plan(targets[0]) is not None
  # Reapply preserves imported top-level and same-index waypoint metadata.
  mutate('motion_plan_restore',target_id=targets[0]);mutate('scene_undo');mutate('motion_plan_apply',target_id=targets[0],request=request());assert plan(targets[0])['producer_plan']=='keep' and plan(targets[0])['waypoints'][0]['producer_waypoint']=='keep'
  before=orient(targets[0]);mutate('follower_orientation_set',target_id=targets[0],settings={'follow_direction':True,'forward_axis':0,'rotation_offset_degrees':[0,0,15],'start_up':[0,0,1],'start_roll_degrees':0,'end_roll_degrees':0,'end_roll_enabled':True});baseline=render('object-roll-before24',24);mutate('follower_orientation_set',target_id=targets[0],settings={'start_roll_degrees':40,'end_roll_degrees':80});changed=render('object-roll-after24',24);assert baseline['image_sha256']!=changed['image_sha256'];edited=orient(targets[0]);mutate('scene_undo');restored=render('object-roll-restored24',24);assert restored['image_sha256']==baseline['image_sha256'];mutate('scene_redo');assert orient(targets[0])==edited
  mutate('follower_orientation_set',target_id=targets[1],settings={'camera_mode':'target_aim','use_focus_target':True,'start_up':[0,0,1],'start_roll_degrees':0,'end_roll_degrees':0,'end_roll_enabled':True});baseline=render('camera-roll-before24',24);mutate('follower_orientation_set',target_id=targets[1],settings={'start_roll_degrees':25,'end_roll_degrees':70});changed=render('camera-roll-after24',24);assert baseline['image_sha256']!=changed['image_sha256'];edited=orient(targets[1]);mutate('scene_undo');restored=render('camera-roll-restored24',24);assert restored['image_sha256']==baseline['image_sha256'];mutate('scene_redo');assert orient(targets[1])==edited
  mutate('follower_orientation_set',target_id=targets[2],settings={'start_up':[0,1,0],'start_roll_degrees':20,'end_roll_degrees':45,'end_roll_enabled':True})
  expected=full();renders={key:render(key,frame,integrator) for key,frame,integrator in [('direct0',0,'direct_light'),('direct24',24,'direct_light'),('disney24',24,'disney_v2')]};assert renders['direct0']['image_sha256']!=renders['direct24']['image_sha256'];mutate('scene_save');assert state()['attachment']==attachment and not state()['dirty']
  saved=json.loads(scene.read_text());original=json.loads((scene.parent/'original_scene.json').read_text());a=saved['extensions']['ray_tracing']['authoring'];o=original['extensions']['ray_tracing']['authoring'];assert a['scene_timeline']==o['scene_timeline'] and a['light_timeline']==o['light_timeline'] and a['object_materials']==o['object_materials'] and saved['objects']==original['objects'] and a['motion_paths']['paths']==o['motion_paths']['paths']
  for b,old in zip(a['motion_paths']['bindings'],o['motion_paths']['bindings']):assert b['producer_binding']==old['producer_binding'] and b['orientation_frame']['producer_frame']=='keep'
  for key,r in renders.items():
   fresh=render('saved-'+key,r['settings']['frame'],r['settings']['integrator']);assert r['settings']['integrator']=='disney_v2' or fresh['image_sha256']==r['image_sha256']
  (scene.parent/'motion-orientation-expected.json').write_text(json.dumps(expected,indent=2));(scene.parent/'motion-orientation-renders.json').write_text(json.dumps(renders,indent=2));(scene.parent/'motion-orientation-receipt.json').write_text(json.dumps(dict(ui_to_mcp=True,cached_catalog_dispatch=True,three_target_plan_pixel_effects=effects,object_camera_orientation_pixel_effects=True,orientation_undo_exact_pixels=True,plan_preview_pure=True,holds_fixed_arrivals=True,restore_original_progress=True,plan_dependency_lock_refusals=True,typed_stale_locked_refusals=True,shared_history=True,producer_metadata_preserved=True,legacy_sources_and_attachment_preserved=True,saved_direct_hash_parity=True,disney_render_complete=True),indent=2))
 bridge.stdin.close();bridge.wait(timeout=5);assert bridge.returncode==0
print('Motion plan/orientation MCP '+mode+' PASS')
