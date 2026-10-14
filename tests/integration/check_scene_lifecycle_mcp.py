"""Real named/fixed MCP lifecycle over one native owner; no scene overwrite."""
import hashlib,json,subprocess,sys,time,socket
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];original=Path(sys.argv[1]).resolve();parent=original.parent
initial_bytes=original.read_bytes();initial=json.loads(initial_bytes);initial_attachment=initial['extensions']['ray_tracing']['authoring']['atmosphere_attachment'];asset=parent/initial_attachment['source_path'];asset_hash=hashlib.sha256(asset.read_bytes()).hexdigest()
class Bridge:
 def __init__(self):
  self.p=subprocess.Popen([sys.executable,'-B',str(ROOT/'tools/scene_editor_mcp.py'),'--scene',str(original)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True);self.seq=0
  self.rpc('initialize',{'protocolVersion':'2025-06-18','capabilities':{},'clientInfo':{'name':'lifecycle-proof','version':'1'}})
 def rpc(self,method,params={}):
  self.seq+=1;self.p.stdin.write(json.dumps({'jsonrpc':'2.0','id':self.seq,'method':method,'params':params})+'\n');self.p.stdin.flush()
  while True:
   line=self.p.stdout.readline();assert line,'Bridge disconnected';r=json.loads(line)
   if r.get('id')==self.seq:break
  assert 'error' not in r,r;return r['result']
 def call(self,name,args={},ok=True,generic=False):
  if generic:
   catalog=self.call('editor_catalog');args={'catalog_id':catalog['catalog_id'],'tool':name,'arguments':args};name='editor_command'
  r=self.rpc('tools/call',{'name':name,'arguments':args});assert bool(r.get('isError')) is not ok,r
  return json.loads(r['content'][0]['text']) if ok else r
 def state(self):return self.call('scene_read')
 def mutate(self,tool,generic=False,**args):return self.call(tool,{'expected_revision':self.state()['revision'],**args},generic=generic)
 def close(self):self.p.stdin.close();self.p.wait(timeout=5)
b=Bridge();other=Bridge();pid=b.p.pid;tools=b.rpc('tools/list')['tools']
# Optical/catalog work may add tools. Qualify the exact current imported catalog,
# including fixed controls, while retaining the lifecycle command requirements.
sys.path.insert(0,str(ROOT/'tools'))
from scene_editor_mcp_reload import Catalog
from scene_editor_mcp_compat import fixed_tools
expected=Catalog(ROOT/'tools/scene_editor_mcp_catalog.py').tools+fixed_tools()
assert tools==expected
names={t['name'] for t in tools};assert len(names)==len(tools)
assert {'scene_open','scene_create','scene_save_as','scene_save','scene_read','editor_catalog','editor_command','object_create','object_rename','render_start','render_status','render_results'}<=names
initial_state=b.state();native_pid=json.loads(Path(str(original)+'.optic-agent.json').read_text())['pid'];socket_path=json.loads(Path(str(original)+'.optic-agent.json').read_text())['socket']
# Unsupported/occupied/malformed/open-failure cases preserve state, bytes, attachment.
(parent/'invalid.json').write_text('{"invalid":true}');(parent/'bad.json').write_text('{');(parent/'subdir').mkdir()
for tool,path in [('scene_open',parent/'bad.json'),('scene_open',parent/'invalid.json'),('scene_open',parent/'absent.json'),('scene_create',original),('scene_save_as',original),('scene_save_as',parent/'missing-parent/moved.json')]:
 b.call(tool,{'scene_path':str(path),'expected_revision':initial_state['revision']},False)
 assert b.state()==initial_state and original.read_bytes()==initial_bytes
b.mutate('object_create',kind='box');dirty=b.state();assert dirty['dirty']
for tool in ('scene_open','scene_create'):
 b.call(tool,{'scene_path':str(parent/'refused.json'),'expected_revision':dirty['revision']},False)
 assert b.state()==dirty and not (parent/'refused.json').exists()
# Save working edits without modifying original or losing the physical dependency.
copy=parent/'mcp-save-as.json';saved=b.mutate('scene_save_as',True,scene_path=str(copy));assert saved['scene_path']==str(copy) and not saved['dirty'] and saved['attachment']==initial_state['attachment']
assert original.read_bytes()==initial_bytes and hashlib.sha256(asset.read_bytes()).hexdigest()==asset_hash
assert b.call('editor_catalog')['scene_path']==str(copy)
# Another attached bridge retains its process. A queued command for the departed scene refuses.
other.call('scene_undo',{'expected_revision':initial_state['revision']},False);assert other.state()['scene_path']==str(copy)
# A raw stale path with even the CURRENT revision must refuse.
with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as c:
 c.connect(socket_path);c.sendall(json.dumps({'method':'object.create','scene_path':str(original),'params':{'kind':'box','expected_revision':saved['revision']}}).encode()+b'\n');response=b''
 while b'\n' not in response:response+=c.recv(8192)
 assert not json.loads(response)['ok']
created=parent/'mcp-created.json';fresh=b.mutate('scene_create',scene_path=str(created));assert not fresh['dirty'] and not b.call('object_list')['objects']
assert b.call('editor_catalog')['scene_path']==str(created)
# Open copied legacy/physical scene, inspect exact retained metadata and render via MCP.
b.mutate('scene_open',True,scene_path=str(copy));assert b.state()['attachment']==initial_state['attachment']
assert b.call('object_list')['objects'];saved_root=json.loads(copy.read_text());assert saved_root['objects'][:len(initial['objects'])]==initial['objects'];assert saved_root['extensions']['t1_provenance']==initial['extensions']['t1_provenance']
for integrator in ('direct_light','disney_v2'):
 job='lifecycle-'+integrator;b.mutate('render_start',request_id=job,width=24,height=24,temporal_frames=1,frame=0,integrator=integrator)
 for _ in range(600):
  status=b.call('render_status',{'job_id':job})['render']
  if status['state'] not in ('running','preparing','queued'):break
  time.sleep(.05)
 assert status['state']=='completed',status
 result=b.call('render_results',{'job_id':job})['render'];assert result['artifact_integrity'] and Path(result['image_path']).is_file(),result
# Create/edit/save/reopen validates complete persistence on the empty new scene.
b.mutate('scene_open',scene_path=str(created));assert b.call('render_read')['render']['latest_job_id']==''
obj=b.mutate('object_create',kind='plane')['object_id'];b.mutate('object_rename',object_id=obj,name='Lifecycle floor')
b.mutate('material_preset',object_id=obj,preset='solid',replace=False)
b.mutate('camera_set_pose',position_m=[0,-4,2],yaw_degrees=180,pitch_degrees=-25)
b.mutate('lighting_set_primary',position_m=[1,-2,3],intensity=8,radius_m=0)
b.mutate('lighting_set_environment',settings={'ambient_strength':.1})
b.mutate('scene_save')
created_saved=created.read_bytes()
b.mutate('scene_open',scene_path=str(original));b.mutate('scene_open',True,scene_path=str(created));assert b.call('object_read',{'object_id':obj})['object']['display_name']=='Lifecycle floor'
assert created.read_bytes()==created_saved and b.call('camera_read')['camera'] and b.call('lighting_read')['lighting']
assert b.call('material_read',{'object_id':obj})['material']['retained']
b.mutate('scene_open',scene_path=str(original));assert b.state()['attachment']==initial_state['attachment'] and original.read_bytes()==initial_bytes
assert b.p.pid==pid and b.p.poll() is None and other.p.poll() is None
assert json.loads(Path(str(copy)+'.optic-agent.json').read_text())['pid']==native_pid
receipt={'ok':True,'named_and_fixed_bridge':True,'tool_count':len(tools),'same_stdio_and_native_owner':True,'shared_ui_lifecycle':True,'monotonic_revision_and_path_guard':True,'legacy_source_unchanged':True,'physical_attachment_preserved':True,'relative_dependency_hash_unchanged':True,'dirty_refusal_and_unsaved_save_as':True,'create_edit_save_reopen':True,'direct_light_and_disney_results':True,'render_latest_id_follows_active_scene':True,'invalid_open_rollback_and_no_clobber':True,'final_scene_path':str(original)}
(parent/'lifecycle-receipt.json').write_text(json.dumps(receipt,indent=2));b.close();other.close();print(json.dumps(receipt))
