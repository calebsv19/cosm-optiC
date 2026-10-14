#!/usr/bin/env python3
"""Bounded scene dependency transaction; native File/MCP owns scene publication."""
import argparse
import errno
import json
import os
from pathlib import Path
import shutil
import stat
import sys
sys.dont_write_bytecode = True
from scene_workflow_bundle import references, contained, sha, safe_id
from scene_workflow_dependencies import resolve_references
from scene_atmosphere_assets import scene_dependencies
from scene_asset_storage import copy_asset
MAX_BYTES=512*1024*1024
MAX_FILES=2048
MAX_JSON_BYTES=16*1024*1024
MAX_RUNTIME_MESH_JSON_BYTES=64*1024*1024
SCHEMA='optic_save_as_assets_v1'

def write_receipt(stage,receipt):
    temp=stage/'receipt.next'
    with temp.open('w') as stream:
        json.dump(receipt,stream);stream.flush();os.fsync(stream.fileno())
    os.replace(temp,stage/'receipt.json')
    fd=os.open(stage,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:os.fsync(fd)
    finally:os.close(fd)

def identity(path):
    st=path.lstat()
    return [st.st_dev,st.st_ino]

def parent_fd(root,relative,receipt=None):
    """All destination traversal/publication uses directory descriptors, no links."""
    parts=Path(relative).parts
    if not parts or Path(relative).is_absolute() or any(x in ('.','..') for x in parts):
        raise ValueError('unsafe destination dependency')
    fd=os.open(root,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    prefix=[]
    try:
        for name in parts[:-1]:
            prefix.append(name)
            if receipt is not None:
                try:
                    os.mkdir(name,mode=0o755,dir_fd=fd)
                    receipt['directories'].append({'path':'/'.join(prefix),'identity':identity(root/Path(*prefix))})
                    write_receipt(Path(receipt['stage']),receipt)
                except FileExistsError:pass
            new=os.open(name,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=fd)
            os.close(fd);fd=new
        return fd,parts[-1]
    except BaseException:
        os.close(fd);raise

def digest_fd(fd):
    import hashlib
    h=hashlib.sha256();before=os.fstat(fd)
    if not stat.S_ISREG(before.st_mode):raise ValueError('destination dependency is not a regular file')
    while block:=os.read(fd,1024*1024):h.update(block)
    after=os.fstat(fd)
    if (before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns)!=(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns):
        raise ValueError('destination changed during verification')
    return h.hexdigest()

def verify(root,entry):
    fd,name=parent_fd(root,entry['path'])
    try:
        file=os.open(name,os.O_RDONLY|os.O_NOFOLLOW,dir_fd=fd)
        try:return digest_fd(file)==entry['sha256']
        finally:os.close(file)
    finally:os.close(fd)

def finish(stage,rollback):
    receipt=json.loads((stage/'receipt.json').read_text())
    if receipt.get('schema')!=SCHEMA or Path(receipt['stage'])!=stage:raise ValueError('invalid copy receipt')
    root=Path(receipt['destination'])
    if identity(root)!=receipt['destination_identity']:raise ValueError('destination directory ownership changed; retained transaction')
    if rollback:
        for entry in reversed(receipt['files']):
            if entry['reused']:continue
            try:fd,name=parent_fd(root,entry['path'])
            except OSError as error:
                if error.errno==errno.ENOENT or (not entry.get('published') and error.errno in (errno.ELOOP,errno.ENOTDIR)):continue
                raise
            try:
                try:st=os.stat(name,dir_fd=fd,follow_symlinks=False)
                except FileNotFoundError:continue
                if [st.st_dev,st.st_ino]!=entry['identity']:
                    if not entry.get('published'):continue
                    raise ValueError('published dependency ownership/content changed; retained transaction')
                if not verify(root,entry):raise ValueError('published dependency content changed; retained transaction')
                os.unlink(name,dir_fd=fd);os.fsync(fd)
            finally:os.close(fd)
        for entry in reversed(receipt['directories']):
            path=root/entry['path']
            if not path.exists():continue
            if identity(path)!=entry['identity']:raise ValueError('created directory ownership changed; retained transaction')
            fd,name=parent_fd(root,entry['path'])
            try:
                try:os.rmdir(name,dir_fd=fd);os.fsync(fd)
                except OSError as error:
                    if error.errno!=errno.ENOTEMPTY:raise
            finally:os.close(fd)
    else:
        if not all(verify(root,entry) for entry in receipt['files']):raise ValueError('destination dependency changed before commit; retained transaction')
    # Only private invocation staging is recursively removed.
    shutil.rmtree(stage)
    return {'ok':True,'dependencies':len(receipt['files']),'copied':sum(not x['reused'] for x in receipt['files']),'bytes':receipt['bytes']}

def prepare(source_scene,destination,stage):
    if source_scene.is_symlink():raise ValueError("source scene cannot be a symlink")
    source=source_scene.resolve(strict=True).parent
    root=destination.parent
    docs=json.loads((stage/'snapshot.json').read_text())
    if docs.get('schema_version')!='optic_save_as_snapshot_v1' or not isinstance(docs.get('documents'),list) or not docs['documents']:
        raise ValueError('invalid native working/history snapshot')
    dependencies={};total=0;pending=[]
    for doc in docs['documents']:
        if doc.get('schema_family')!='codework_scene' or doc.get('schema_variant')!='scene_runtime_v1':raise ValueError('unsupported retained scene')
        for obj in doc.get('objects',[]):
            if obj.get('curve_asset_ref') or obj.get('procedural_solid_material_ref'):raise ValueError('native curve/procedural solid reference requires its dedicated export lane')
            if obj.get('object_type')=='mesh_asset_instance':
                path=obj.get('extensions',{}).get('line_drawing',{}).get('runtime_mesh_path')
                if not path:pending.append('assets/mesh_assets/'+safe_id(obj['geometry_ref']['id'])+'.runtime.json')
        pending.extend(references(doc))
        pending.extend(scene_dependencies(doc,source,max_bytes=MAX_BYTES,max_files=MAX_FILES))
    while pending:
        rel=pending.pop()
        if rel in dependencies:continue
        path=contained(source,rel)
        if not path.is_file():raise ValueError('missing dependency: '+rel)
        if rel==destination.name or any(x.endswith('.optic-agent.json') or x.startswith('.optic-save-as-') for x in Path(rel).parts):raise ValueError('dependency collides with scene/control output')
        size=path.stat().st_size;total+=size
        if total>MAX_BYTES or len(dependencies)>=MAX_FILES:raise ValueError('Save As dependency copy exceeds 512 MiB/2048 files')
        dependencies[rel]={'source':path,'sha256':sha(path),'identity':identity(path)}
        if path.suffix.lower()=='.json':
            mesh_candidate=rel.endswith('.runtime.json')
            limit=MAX_RUNTIME_MESH_JSON_BYTES if mesh_candidate else MAX_JSON_BYTES
            if size>limit:raise ValueError('dependency JSON exceeds bounded size limit')
            document=json.loads(path.read_text())
            if size>MAX_JSON_BYTES and not (isinstance(document,dict) and document.get('schema_family')=='codework_geometry' and document.get('schema_variant')=='mesh_asset_runtime_v1' and document.get('schema_version')==1):
                raise ValueError('large dependency JSON must be a version 1 runtime mesh')
            pending.extend(resolve_references(source,path,references(document),contained))
        if rel.endswith('.runtime.json'):
            preview=rel[:-len('.runtime.json')]+'.preview.json'
            candidate=contained(source,preview)
            if candidate.is_file():pending.append(preview)
    if shutil.disk_usage(root).free<total+8*1024*1024:raise ValueError('insufficient destination storage')
    receipt={'schema':SCHEMA,'stage':str(stage),'destination':str(root),'destination_identity':identity(root),'files':[],'directories':[],'bytes':total}
    write_receipt(stage,receipt)
    try:
        for rel,entry in sorted(dependencies.items()):
            file=stage/'files'/rel;file.parent.mkdir(parents=True,exist_ok=True);copy_asset(entry['source'],file)
            if sha(file)!=entry['sha256'] or identity(entry['source'])!=entry['identity'] or sha(entry['source'])!=entry['sha256']:raise ValueError('source dependency changed during copy')
            with file.open('rb') as stream:os.fsync(stream.fileno())
            receipt['files'].append({'path':rel,'sha256':entry['sha256'],'identity':identity(file),'reused':False,'published':False})
        write_receipt(stage,receipt)
        for entry in receipt['files']:
            fd,name=parent_fd(root,entry['path'],receipt)
            try:
                try:
                    os.link(stage/'files'/entry['path'],name,dst_dir_fd=fd,follow_symlinks=False)
                    entry['published']=True
                except FileExistsError:
                    if not verify(root,entry):raise ValueError('destination dependency collision: '+entry['path'])
                    entry['reused']=True
                os.fsync(fd);write_receipt(stage,receipt)
            finally:os.close(fd)
        for rel,entry in dependencies.items():
            if identity(entry['source'])!=entry['identity'] or sha(entry['source'])!=entry['sha256']:raise ValueError('source changed during publication')
        if not all(verify(root,entry) for entry in receipt['files']):raise ValueError('copied dependency verification failed')
        return {'ok':True,'dependencies':len(receipt['files']),'bytes':total}
    except BaseException:
        finish(stage,True);raise

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--source-scene',type=Path);parser.add_argument('--destination',type=Path);parser.add_argument('--stage',type=Path,required=True);parser.add_argument('--finish',choices=['commit','rollback']);args=parser.parse_args()
    try:
        stage=args.stage.absolute()
        if stage.is_symlink() or stage.parent.is_symlink() or not stage.name.startswith('.optic-save-as-') or stage.stat().st_uid!=os.getuid():raise ValueError('invalid private copy stage')
        result=finish(stage,args.finish=='rollback') if args.finish else prepare(args.source_scene,args.destination,stage)
        print(json.dumps(result));return 0
    except (ValueError,OSError,KeyError,TypeError,json.JSONDecodeError) as error:
        print(json.dumps({'ok':False,'error':str(error),'retained_stage':str(args.stage) if args.stage.exists() else None}));return 1
if __name__=='__main__':raise SystemExit(main())
