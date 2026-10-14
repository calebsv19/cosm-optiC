"""Dependency transaction tests: closure/history, no-clobber and ownership recovery."""
import argparse,copy,importlib.util,json,os,sys,tempfile
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import scene_editor_save_as_assets as assets

def run(out):
 out.mkdir(parents=True,exist_ok=False);source=out/'source';destination=out/'destination';source.mkdir();destination.mkdir()
 (source/'nested').mkdir();(source/'nested/mesh.runtime.json').write_text(json.dumps({'path':'../texture.png','producer':{'preserve':'legacy'}}));(source/'nested/mesh.preview.json').write_text('{}');(source/'texture.png').write_bytes(b'png-current');(source/'previous.png').write_bytes(b'png-history');(source/'volume.vf3d').write_bytes(b'volume')
 doc={'schema_family':'codework_scene','schema_variant':'scene_runtime_v1','schema_version':1,'scene_id':'legacy-preserved','objects':[{'runtime_mesh_path':'nested/mesh.runtime.json'}],'extensions':{'ray_tracing':{'authoring':{'atmosphere_attachment':{'schema_version':'ray_atmosphere_attachment_v1','source_path':'volume.vf3d','source_kind':'vf3d'}}}}};history=copy.deepcopy(doc);history['objects'][0]['texture_path']='previous.png';scene=source/'scene.json';scene.write_text(json.dumps(doc));initial={str(p.relative_to(source)):p.read_bytes() for p in source.rglob('*') if p.is_file()}
 def stage(docs=None):
  p=Path(tempfile.mkdtemp(prefix='.optic-save-as-',dir=destination));(p/'snapshot.json').write_text(json.dumps({'schema_version':'optic_save_as_snapshot_v1','documents':docs or [doc,history]}));return p
 def unchanged():assert {str(p.relative_to(source)):p.read_bytes() for p in source.rglob('*') if p.is_file()}==initial
 first=stage();r=assets.prepare(scene,destination/'copy.json',first);assert r['dependencies']==5;assert (source/'texture.png').stat().st_ino!=(destination/'texture.png').stat().st_ino;r=assets.finish(first,True);assert not list(destination.iterdir());unchanged()
 # Identical pre-existing dependencies are reused without ownership changes.
 (destination/'texture.png').write_bytes(b'png-current');identity=assets.identity(destination/'texture.png');s=stage();assets.prepare(scene,destination/'copy.json',s);r=assets.finish(s,False);assert r['copied']==4 and assets.identity(destination/'texture.png')==identity
 assert (destination/'previous.png').read_bytes()==b'png-history';unchanged()
 # No-clobber collision cleans files/directories created before the collision.
 for p in destination.rglob('*'):
  if p.is_file():p.unlink()
 for p in sorted(destination.rglob('*'),reverse=True):
  if p.is_dir():p.rmdir()
 (destination/'texture.png').write_bytes(b'other-owner');before={str(p.relative_to(destination)):p.read_bytes() for p in destination.rglob('*') if p.is_file()};s=stage()
 try:assets.prepare(scene,destination/'copy.json',s);assert False
 except ValueError as error:assert 'collision' in str(error)
 assert not s.exists();assert {str(p.relative_to(destination)):p.read_bytes() for p in destination.rglob('*') if p.is_file()}==before
 # Containment, missing, symlink and bounded-volume refusal before publication.
 for ref in ['../escape.png','missing.png','/tmp/outside.png']:
  bad=copy.deepcopy(doc);bad['objects']=[{'texture_path':ref}];s=stage([bad])
  try:assets.prepare(scene,destination/'copy.json',s);assert False
  except ValueError:pass
  assert not (destination/'nested').exists();__import__('shutil').rmtree(s)
 (source/'link.png').symlink_to(source/'texture.png');bad=copy.deepcopy(doc);bad['objects']=[{'texture_path':'link.png'}];s=stage([bad])
 try:assets.prepare(scene,destination/'copy.json',s);assert False
 except ValueError as error:assert 'symlink' in str(error)
 __import__('shutil').rmtree(s);(source/'link.png').unlink()
 (source/'huge.vf3d').touch();os.truncate(source/'huge.vf3d',assets.MAX_BYTES+1);bad=copy.deepcopy(doc);bad['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['source_path']='huge.vf3d';s=stage([bad])
 try:assets.prepare(scene,destination/'copy.json',s);assert False
 except ValueError as error:assert 'budget' in str(error)
 __import__('shutil').rmtree(s);(source/'huge.vf3d').unlink()
 # Destination symlink components never redirect publication.
 (destination/'texture.png').unlink();external=out/'external';external.mkdir();(destination/'nested').symlink_to(external,target_is_directory=True);s=stage()
 try:assets.prepare(scene,destination/'copy.json',s);assert False
 except (ValueError,OSError):pass
 assert not list(external.iterdir());assert not s.exists();(destination/'nested').unlink()
 # A modified published file is preserved, with its receipt for bounded recovery.
 s=stage();assets.prepare(scene,destination/'copy.json',s);(destination/'texture.png').write_bytes(b'external-edit')
 try:assets.finish(s,True);assert False
 except ValueError:pass
 assert s.exists() and (destination/'texture.png').read_bytes()==b'external-edit'
 # Dense managed runtime meshes use a larger bounded JSON allowance; generic manifests do not.
 large=source/'dense.runtime.json';large.write_text(json.dumps({'schema_family':'codework_geometry','schema_variant':'mesh_asset_runtime_v1','schema_version':1,'padding':'x'*1024}))
 dense=copy.deepcopy(doc);dense['objects']=[{'runtime_mesh_path':'dense.runtime.json'}];dense['extensions']={}
 with patch.object(assets,'MAX_JSON_BYTES',512),patch.object(assets,'MAX_RUNTIME_MESH_JSON_BYTES',2048):
  dense_stage=stage([dense]);assets.prepare(scene,destination/'dense.json',dense_stage);assets.finish(dense_stage,False)
  assert (destination/'dense.runtime.json').read_bytes()==large.read_bytes()
  large.write_text(json.dumps({'producer':'legacy','padding':'x'*1024}));refusal=stage([dense])
  try:assets.prepare(scene,destination/'dense.json',refusal);assert False
  except ValueError as error:assert 'runtime mesh' in str(error)
  __import__('shutil').rmtree(refusal)
  large.write_text(' '*2049);refusal=stage([dense])
  try:assets.prepare(scene,destination/'dense.json',refusal);assert False
  except ValueError as error:assert 'size limit' in str(error)
  __import__('shutil').rmtree(refusal)
 large.unlink()
 # These are wholly owned test files, no external consumer; retain compact receipt.
 (out/'ownership-refusal.json').write_text(json.dumps({'receipt':json.loads((s/'receipt.json').read_text()),'retained_external_edit':True},indent=2))
 result={'dense_runtime_mesh_allowance_and_bounds':True,'passed':True,'nested_manifests_and_preview':True,'undo_history_dependencies':True,'physical_volume_copy':True,'independent_copy':True,'identical_reuse':True,'collision_rollback':True,'traversal_missing_symlink_refusal':True,'bounded_volume_refusal':True,'changed_published_asset_preserved':True,'legacy_source_unchanged':True}
 (out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--output-root',type=Path,required=True);run(p.parse_args().output_root)
