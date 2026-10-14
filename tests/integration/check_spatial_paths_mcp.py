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
 def paths():return call('path_list')['motion_paths']
 def path(id):return call('path_read',dict(path_id=id))['path']
 def full():
  d=paths();d['paths']=[path(p['id']) for p in d['paths']];d.pop('world_scale');return d
 def tracks():return call('animation_read')['animation']['tracks']
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
 if mode=='legacy':
  disk=scene.read_bytes();rev=state()['revision'];assert paths()['paths']==[] and not call('animation_read')['animation']['present'];assert scene.read_bytes()==disk and state()['revision']==rev
  id=mutate('path_create',name='First route',origin=[0,0,0],length=1)['path_id'];assert path(id)['name']=='First route';mutate('scene_undo');assert paths()['paths']==[] and not call('animation_read')['animation']['present'];mutate('scene_redo')
  mutate('path_bind',target_id='object/surface',path_id=id,attach=True);mutate('path_bind',target_id='camera/main',path_id=id,attach=True);mutate('path_prepare_light');target=next(t['target_id'] for t in tracks() if t['property_id']=='light/path_progress');mutate('path_bind',target_id=target,path_id=id,attach=True)
  # Maximum route and point capacity refuse without partial changes.
  for i in range(15):mutate('path_create',name='Capacity '+str(i),origin=[0,0,0],length=1)
  old=full();rev=state()['revision'];bad('path_create',name='Overflow',origin=[0,0,0],length=1);assert full()==old and state()['revision']==rev
  for i in range(30):mutate('path_add_point',path_id=id,point_id=path(id)['points'][-1]['id'])
  old=full();rev=state()['revision'];bad('path_add_point',path_id=id,point_id=path(id)['points'][-1]['id']);assert full()==old and state()['revision']==rev
  mutate('scene_save');assert state()['attachment']==attachment and not state()['dirty'];saved=json.loads(scene.read_text());original=json.loads(disk);assert saved['extensions']['ray_tracing']['authoring']['object_materials']==original['extensions']['ray_tracing']['authoring']['object_materials'] and saved['objects']==original['objects']
 elif mode=='reopen':
  assert full()==json.loads((scene.parent/'spatial-paths-expected.json').read_text())
  for key,r in json.loads((scene.parent/'spatial-paths-renders.json').read_text()).items():
   fresh=render('reopened-'+key,r['settings']['frame'],r['settings']['integrator']);assert r['settings']['integrator']=='disney_v2' or r['image_sha256']==fresh['image_sha256']
  assert not state()['dirty']
 else:
  known=full();assert known==json.loads((scene.parent/'spatial-paths-ui-expected.json').read_text());catalog=call('editor_catalog');assert len(catalog['tools'])==len(catalog_module.tool_list())
  def unchanged(action):
   old=full();rev=state()['revision'];action();assert full()==old and state()['revision']==rev
  unchanged(lambda:call('path_create',dict(name='bad',origin=[0,0,0],length=1,expected_revision=state()['revision']+1),False))
  unchanged(lambda:bad('path_create',name='bad',origin=[False,0,0],length=1));unchanged(lambda:bad('path_create',name='bad',origin=[0,0,0],length=-1));unchanged(lambda:bad('path_remove',path_id='missing'))
  id=known['paths'][-1]['id'];point=path(id)['points'][0]['id'];mutate('path_point_mode',path_id=id,point_id=point,mode='independent')
  mutate('path_set_point',path_id=id,point_id=point,position=[0,0,0],incoming=[-.2,0,0],outgoing=[.2,0,0],segment='cubic')
  added=mutate('path_add_point',path_id=id,point_id=point)['point_id'];assert len(path(id)['points'])==3
  mutate('path_remove_point',path_id=id,point_id=added);assert len(path(id)['points'])==2
  unchanged(lambda:bad('path_remove_point',path_id=id,point_id=point))
  mutate('path_point_mode',path_id=id,point_id=point,mode='corner');unchanged(lambda:bad('path_set_point',path_id=id,point_id=point,position=[0,0,0],incoming=[1,0,0],outgoing=[1,0,0],segment='cubic'));mutate('path_point_mode',path_id=id,point_id=point,mode='independent')
  mutate('path_rename',path_id=id,name='MCP object route');renamed=full();mutate('scene_undo');assert path(id)['name']=='UI object route';mutate('scene_redo');assert full()==renamed
  rev=state()['revision'];call('editor_command',dict(catalog_id=catalog['catalog_id'],tool='path_rename',arguments=dict(path_id=id,name='Cached bridge object route',expected_revision=rev)));assert path(id)['name']=='Cached bridge object route'
  mutate('object_set_flag',object_id='surface',flag='locked',value=True);unchanged(lambda:bad('path_bind',target_id='object/surface',path_id=id,attach=True));mutate('scene_undo')
  before_tracks=tracks();mutate('path_bind',target_id='object/surface',path_id=id,attach=True);assert not any(t['enabled'] for t in tracks() if t['property_id'].startswith('object/transform/position'))
  unchanged(lambda:bad('path_remove',path_id=id));mutate('path_bind',target_id='object/surface',path_id=id,attach=False);assert all(t['enabled'] for t in tracks() if t['property_id'].startswith('object/transform/position'));mutate('path_bind',target_id='object/surface',path_id=id,attach=True)
  # Camera/light use distinct routes near their original visible positions.
  camera=mutate('path_create',name='Camera route',origin=[.625,-1.25,1.875],length=.2)['path_id'];mutate('path_bind',target_id='camera/main',path_id=camera,attach=True)
  mutate('path_bind',target_id='camera/main',path_id=camera,attach=False);mutate('path_bind',target_id='camera/main',path_id=camera,attach=True)
  mutate('path_prepare_light');prior=full();mutate('path_convert_legacy',target='light');assert len(full()['paths'])==len(prior['paths'])+1;mutate('scene_undo');assert full()==prior
  light_target=next(t['target_id'] for t in tracks() if t['property_id']=='light/path_progress')
  light=mutate('path_create',name='Light route',origin=[.82,-.92,.34],length=.4)['path_id'];mutate('path_bind',target_id=light_target,path_id=light,attach=True)
  mutate('path_bind',target_id=light_target,path_id=light,attach=False);mutate('path_bind',target_id=light_target,path_id=light,attach=True)
  unchanged(lambda:bad('path_bind',target_id='light/missing',path_id=light,attach=True));unchanged(lambda:bad('animation_remove_channel',track_id=next(t['track_id'] for t in tracks() if t['property_id']=='light/route_progress')))
  unchanged(lambda:bad('path_convert_legacy',target='camera'));unchanged(lambda:bad('path_convert_legacy',target='light'))
  delete=mutate('path_create',name='Disposable route',origin=[0,0,0],length=1)['path_id'];mutate('path_remove',path_id=delete);assert not any(p['id']==delete for p in paths()['paths']);mutate('scene_undo');assert path(delete);mutate('scene_redo')
  effects={}
  for label,route in [('object',id),('camera',camera),('light',light)]:
   row=path(route)['points'][0];before=render(label+'-before24',24);position=row['position'].copy();position[0]+=.5
   mutate('path_set_point',path_id=route,point_id=row['id'],position=position,incoming=row['incoming'],outgoing=row['outgoing'],segment=row['segment'])
   after=render(label+'-after24',24);assert before['image_sha256']!=after['image_sha256'],label
   mutate('scene_undo');restored=render(label+'-restored24',24);assert before['image_sha256']==restored['image_sha256'];effects[label]=True
  renders={key:render(key,frame,integrator) for key,frame,integrator in [('direct0',0,'direct_light'),('direct24',24,'direct_light'),('disney24',24,'disney_v2')]};assert renders['direct0']['image_sha256']!=renders['direct24']['image_sha256']
  expected=full();mutate('scene_save');assert state()['attachment']==attachment and not state()['dirty']
  saved=json.loads(scene.read_text());old=json.loads((scene.parent/'original_scene.json').read_text());a=saved['extensions']['ray_tracing']['authoring'];o=old['extensions']['ray_tracing']['authoring']
  assert a['motion_paths']['producer_root']=='keep' and a['motion_paths']['paths'][0]==o['motion_paths']['paths'][0]
  assert a['scene_timeline']['producer_timeline']==o['scene_timeline']['producer_timeline'] and a['scene_timeline']['rate']==o['scene_timeline']['rate'] and a['scene_timeline']['range']==o['scene_timeline']['range']
  for t in o['scene_timeline']['tracks']:
   current=next(x for x in a['scene_timeline']['tracks'] if x['id']==t['id']);assert current['keys']==t['keys'] and current['producer_metadata']==t['producer_metadata']
  assert a['light_timeline']==o['light_timeline'] and a['camera_path']==o['camera_path'] and a['object_materials']==o['object_materials'] and saved['objects']==old['objects']
  for key,r in renders.items():
   fresh=render('saved-'+key,r['settings']['frame'],r['settings']['integrator']);assert r['settings']['integrator']=='disney_v2' or fresh['image_sha256']==r['image_sha256']
  (scene.parent/'spatial-paths-expected.json').write_text(json.dumps(expected,indent=2));(scene.parent/'spatial-paths-renders.json').write_text(json.dumps(renders,indent=2));(scene.parent/'spatial-paths-receipt.json').write_text(json.dumps(dict(ui_to_mcp=True,cached_catalog_dispatch=True,stable_path_point_ids=True,three_follower_targets=True,individual_follower_pixel_effects=effects,detach_source_restore=True,explicit_legacy_conversion=True,legacy_metadata_and_optional_omissions_preserved=True,typed_stale_capacity_dependency_refusals=True,shared_history=True,attachment_preserved=True,direct_frame_pixel_effect=True,saved_direct_render_parity=True,disney_render_complete=True),indent=2))
 bridge.stdin.close();bridge.wait(timeout=5);assert bridge.returncode==0
print('Spatial paths MCP '+mode+' PASS')
