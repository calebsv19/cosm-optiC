"""Cross-directory working-scene copy, compatibility and independent render proof."""
import hashlib,json,subprocess,sys,time,socket,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];original=Path(sys.argv[1]).resolve();parent=original.parent
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

b=Bridge();other=Bridge();pid=b.p.pid;initial=b.state();source_bytes=original.read_bytes();source=json.loads(source_bytes)
asset_paths=['assets/materials/original.png','assets/mesh_assets/asset_imported_tetrahedron_01.runtime.json','assets/mesh_assets/asset_imported_tetrahedron_01.preview.json','assets/atmosphere/cache/manifest.json','assets/atmosphere/cache/frame_000000.vf3d']
hashes={rel:hashlib.sha256((parent/rel).read_bytes()).hexdigest() for rel in asset_paths}
# Partial publication collision preserves destination-owned bytes and current attachment.
collision=parent/'collision';(collision/'assets/materials').mkdir(parents=True);(collision/'assets/materials/original.png').write_bytes(b'other-owner')
before={str(p.relative_to(collision)):p.read_bytes() for p in collision.rglob('*') if p.is_file()}
b.call('scene_save_as',{'scene_path':str(collision/'scene_runtime.json'),'expected_revision':initial['revision']},False)
assert b.state()==initial and {str(p.relative_to(collision)):p.read_bytes() for p in collision.rglob('*') if p.is_file()}==before
assert not list(collision.glob('.optic-save-as-*')) and not Path(str(collision/'scene_runtime.json')+'.optic-agent.json').exists()
# Capture an unsaved base edit and keep shared history after moving project roots.
b.mutate('object_rename',object_id='surface',name='Working copied surface');dirty=b.state();assert dirty['dirty']
copy=parent/'cross-mcp/scene_runtime.json';saved=b.mutate('scene_save_as',True,scene_path=str(copy));assert saved['scene_path']==str(copy) and not saved['dirty'] and saved['attachment']==initial['attachment'];assert b.call('editor_catalog')['scene_path']==str(copy)
assert original.read_bytes()==source_bytes and copy.read_bytes()!=source_bytes
for rel,h in hashes.items():
 assert hashlib.sha256((copy.parent/rel).read_bytes()).hexdigest()==h and hashlib.sha256((parent/rel).read_bytes()).hexdigest()==h
 assert (parent/rel).stat().st_ino!=(copy.parent/rel).stat().st_ino
other.call('scene_undo',{'expected_revision':initial['revision']},False);assert other.state()['scene_path']==str(copy)
b.mutate('scene_undo');assert b.call('object_read',{'object_id':'surface'})['object']==source['objects'][0]
b.mutate('scene_redo');assert b.call('object_read',{'object_id':'surface'})['object']['display_name']=='Working copied surface';b.mutate('scene_save')
# Render original and relocated scenes with identical working geometry/materials.
# Rename does not change raster content; exact Direct Light output must match.
images={}
for scene_path,label in [(original,'source'),(copy,'copy')]:
 b.mutate('scene_open',scene_path=str(scene_path))
 job='cross-'+label;b.mutate('render_start',request_id=job,width=32,height=32,temporal_frames=1,frame=0,integrator='direct_light')
 for _ in range(600):
  state=b.call('render_status',{'job_id':job})['render']
  if state['state']=='completed':break
  assert state['state'] not in ('failed','canceled'),state;time.sleep(.05)
 result=b.call('render_results',{'job_id':job})['render'];assert result['artifact_integrity'];images[label]=Path(result['image_path']).read_bytes()
assert images['source']==images['copy']
# Opening both UI and MCP destinations confirms saved legacy fields/volume metadata.
for destination in [parent/'cross-ui/scene_runtime.json',copy]:
 b.mutate('scene_open',True,scene_path=str(destination));assert b.state()['attachment']==initial['attachment']
 doc=json.loads(destination.read_text());assert doc['objects'][1:]==source['objects'][1:] and doc['extensions']['t1_provenance']==source['extensions']['t1_provenance']
assert b.p.pid==pid and other.p.poll() is None
# Independence: a source asset change cannot change the already copied destination.
b.mutate('scene_open',scene_path=str(original));assert original.read_bytes()==source_bytes
assert not list(copy.parent.glob('.optic-save-as-*'))
receipt={'ok':True,'named_and_fixed_bridge':True,'ui_mcp_save_as':True,'same_stdio_owner_and_attachment':True,'legacy_source_bytes_preserved':True,'texture_mesh_preview_manifest_vf3d_hashes':True,'independent_dependency_inodes':True,'partial_collision_rollback':True,'unsaved_edit_undo_redo_save_reopen':True,'direct_light_exact_bmp_parity':True,'relative_attachment_preserved':True,'tool_count':len(b.rpc('tools/list')['tools']),'source_scene':str(original),'mcp_scene':str(copy),'dependency_hashes':hashes}
(parent/'cross-save-as-receipt.json').write_text(json.dumps(receipt,indent=2));b.close();other.close();print(json.dumps(receipt))
