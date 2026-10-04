#!/usr/bin/env python3
"""Completion reconciliation and non-destructive scene archive/restore tooling."""
import argparse
from fractions import Fraction
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tarfile
import tempfile

from scene_workflow_bundle import contained, read, safe_id, sha, snapshot_inputs, seal, verify, write
from scene_workflow import locked, load, save, now


def atomic_copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as f:
        tmp = Path(f.name)
        with source.open('rb') as src: shutil.copyfileobj(src, f)
        f.flush(); os.fsync(f.fileno())
    os.replace(tmp, target)


def local_path(base, value):
    path = Path(value)
    path = path if path.is_absolute() else base / path
    path = path.resolve()
    if not path.is_relative_to(base.resolve()) or not path.is_file():
        raise ValueError(f'job input must be an existing file inside run root: {value}')
    return path


def frame_manifest(root, start, count, width, height):
    expected = {f'frame_{i:04d}.bmp' for i in range(start, start + count)}
    if {p.name for p in root.glob('*.bmp')} != expected:
        raise ValueError('frame sequence has missing or unexpected frames')
    entries = []
    for i in range(start, start + count):
        path = root / f'frame_{i:04d}.bmp'
        with path.open('rb') as f: header = f.read(54)
        if len(header) < 54 or header[:2] != b'BM' or struct.unpack_from('<ii', header, 18) not in ((width, height), (width, -height)):
            raise ValueError(f'invalid BMP dimensions: {path.name}')
        if struct.unpack_from('<I', header, 2)[0] != path.stat().st_size:
            raise ValueError('BMP file length mismatch')
        entries.append({'frame': i, 'path': path.name, 'bytes': path.stat().st_size, 'sha256': sha(path)})
    return entries


def verify_video(movie, width, height, count, fps):
    result = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-count_frames',
        '-show_entries', 'stream=codec_name,width,height,r_frame_rate,nb_read_frames,duration',
        '-of', 'json', str(movie)], check=True, capture_output=True, text=True)
    stream = json.loads(result.stdout)['streams'][0]
    if (stream['width'], stream['height'], int(stream['nb_read_frames'])) != (width, height, count) or Fraction(stream['r_frame_rate']) != Fraction(str(fps)):
        raise ValueError('video dimensions, frame count or rate mismatch')
    subprocess.run(['ffmpeg', '-v', 'error', '-i', str(movie), '-f', 'null', '-'], check=True, capture_output=True)
    return dict(stream, sha256=sha(movie), bytes=movie.stat().st_size)


def reconcile(project, run_root, job_rel, renderer):
    project, run_root, renderer = project.resolve(), run_root.resolve(), renderer.resolve()
    job = contained(run_root, job_rel)
    status, request, summary, progress = [read(job / name) for name in
        ('job_status.json', 'job_request.json', 'result_summary.json', 'render_progress.json')]
    run_id = safe_id(request['run_id'])
    if status['job_id'] != job.name:
        raise ValueError('job identity does not match selected directory')
    render = request['render']; count = render['frame_count']; start = render.get('start_frame', 0)
    if status['state'] != 'completed' or status['exit_code'] != 0 or status['frames_completed'] != count:
        raise ValueError('job is not successfully complete')
    if summary.get('run_id') != run_id or summary.get('diagnostics') != 'ok' or summary.get('frames_rendered') != count or not summary.get('rendered_frames'):
        raise ValueError('terminal result does not match completed job')
    if progress.get('run_id') != run_id or progress.get('stage') != 'completed' or progress.get('frames_completed') != count:
        raise ValueError('terminal progress does not match job')
    source = local_path(run_root, request['scene']['runtime_scene_path'])
    output = Path(request['output']['root'])
    output = (output if output.is_absolute() else job / output).resolve()
    if not output.is_relative_to(run_root): raise ValueError('render output escapes run root')
    entries = frame_manifest(output / 'frames', start, count, render['width'], render['height'])
    movie = local_path(run_root, request['output']['video']['path'])
    video = verify_video(movie, render['width'], render['height'], count, request['output']['video']['fps'])
    with locked(project):
        data = load(project)
        dest = project / 'completed_runs' / run_id
        if dest.exists():
            receipt = read(dest / 'completion.json')
            if receipt['source_scene_sha256'] != sha(source) or receipt['request_sha256'] != sha(job / 'job_request.json') or receipt['frames'] != entries or receipt['video']['sha256'] != video['sha256']:
                raise ValueError('completion identity changed; refusing replacement')
        else:
            dest.parent.mkdir(exist_ok=True)
            with tempfile.TemporaryDirectory(prefix='.completion-', dir=dest.parent) as tmp:
                stage = Path(tmp) / 'completion'; stage.mkdir()
                snapshot_inputs(source, job / 'job_request.json', stage / 'inputs')
                seal(stage / 'inputs', run_id, renderer)
                for name in ('job_status.json', 'job_request.json', 'result_summary.json', 'render_progress.json', 'stderr.log'):
                    shutil.copy2(job / name, stage / name)
                for name in ('render_summary.json', 'render_progress.json'):
                    alias = output / name
                    if alias.exists(): shutil.copy2(alias, stage / ('prior_' + name))
                receipt = {'schema': 'optic_scene_completion_v1', 'run_id': run_id,
                    'job_id': status['job_id'], 'state': 'completed_verified', 'verified_at': now(),
                    'source_scene_sha256': sha(source), 'request_sha256': sha(job / 'job_request.json'),
                    'source_scene': source.relative_to(run_root).as_posix(),
                    'job': job.relative_to(run_root).as_posix(), 'output': output.relative_to(run_root).as_posix(),
                    'video_path': movie.relative_to(run_root).as_posix(), 'video': video,
                    'frames': entries, 'render': render, 'artistic_approval': False}
                write(stage / 'completion.json', receipt); stage.rename(dest)
        # Replay also repairs an interruption between receipt publication and alias replacement.
        atomic_copy(job / 'result_summary.json', output / 'render_summary.json')
        atomic_copy(job / 'render_progress.json', output / 'render_progress.json')
        data.setdefault('completed_runs', {})[run_id] = {'state': 'completed_verified',
            'receipt': (dest / 'completion.json').relative_to(project).as_posix(),
            'receipt_sha256': sha(dest / 'completion.json'), 'artistic_approval': False}
        save(project, data)
    return receipt


def archive(project, run_root, run_id, output, retention):
    project, run_root, output = project.resolve(), run_root.resolve(), output.resolve()
    safe_id(run_id)
    if output.exists(): raise ValueError('archive destination exists')
    if output.resolve().is_relative_to(run_root): raise ValueError('archive must be outside source project')
    with locked(project):
        data = load(project); entry = data['completed_runs'][run_id]
        receipt_path = contained(project, entry['receipt']); receipt = read(receipt_path)
        if sha(receipt_path) != entry['receipt_sha256']: raise ValueError('completion receipt changed')
        selected = {}
        # Complete portable rendered input, editable inputs, frozen revisions and compact workflow records.
        for p in project.rglob('*'):
            if p.is_file() and not p.is_symlink() and p.relative_to(project).parts[0] not in {'previews', 'archives'} and p.name != '.scene_workflow.lock':
                selected['workflow/' + p.relative_to(project).as_posix()] = p
        for name in ('scene_runtime.accepted.json', 'scene_runtime.render_frozen.json', 'README.md'):
            p = run_root / name
            if p.is_file(): selected[name] = p
        movie = local_path(run_root, receipt['video_path']); selected[receipt['video_path']] = movie
        if sha(movie) != receipt['video']['sha256']: raise ValueError('completed movie changed')
        indices = {receipt['frames'][0]['frame'], receipt['frames'][len(receipt['frames']) // 2]['frame'], receipt['frames'][-1]['frame']}
        for frame in receipt['frames']:
            if retention == 'full' or frame['frame'] in indices:
                rel = receipt['output'] + '/frames/' + frame['path']; p = local_path(run_root, rel)
                if sha(p) != frame['sha256']: raise ValueError('completed frame changed')
                selected[rel] = p
        # Top-level scene uses the same managed asset paths as the workflow input.
        for p in (run_root / 'assets').rglob('*'):
            if p.is_file(): selected[p.relative_to(run_root).as_posix()] = p
        for p in run_root.glob('*recipe*.json'): selected[p.name] = p
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.archive-', dir=output.parent) as tmp:
            stage = Path(tmp); archive_path = stage / 'scene.tar.gz'
            records = [{'path': rel, 'bytes': p.stat().st_size, 'sha256': sha(p)} for rel, p in sorted(selected.items())]
            with tarfile.open(archive_path, 'w:gz', compresslevel=3) as tar:
                for record in records:
                    rel = record['path']; p = selected[rel]
                    if p.is_symlink(): raise ValueError('archive symlink rejected')
                    tar.add(p, arcname=rel, recursive=False)
                    if sha(p) != record['sha256']: raise ValueError('archive source changed')
            manifest = {'schema': 'optic_scene_archive_v1', 'run_id': run_id, 'retention': retention,
                'created_at': now(), 'source_deleted': False, 'files': records,
                'archive_sha256': sha(archive_path), 'archive_bytes': archive_path.stat().st_size}
            write(stage / 'archive_manifest.json', manifest)
            (stage / 'checksums.sha256').write_text(f'{manifest["archive_sha256"]}  payload/scene.tar.gz\n{sha(stage / "archive_manifest.json")}  payload/archive_manifest.json\n')
            (stage / 'RESTORE.md').write_text('Restore with tools/scene_workflow_delivery.py restore --archive <archive-directory> --output <new-directory>.\nVerify the restored workflow/completed_runs/<run-id>/inputs with scene_workflow.py verify.\nCompact archives contain representative frames, not the full sequence. Source files are never deleted.\n')
            stage.rename(output)
    return manifest


def restore(archive_root, output):
    archive_root, output = archive_root.resolve(), output.resolve()
    checked = {}
    for line in (archive_root / 'checksums.sha256').read_text().splitlines():
        digest, name = line.split()
        name = name.removeprefix('payload/')
        if name not in {'scene.tar.gz', 'archive_manifest.json'} or name in checked or sha(archive_root / name) != digest:
            raise ValueError('archive checksum inventory mismatch')
        checked[name] = digest
    if set(checked) != {'scene.tar.gz', 'archive_manifest.json'}:
        raise ValueError('archive checksum inventory incomplete')
    manifest = read(archive_root / 'archive_manifest.json')
    if manifest.get('schema') != 'optic_scene_archive_v1' or sha(archive_root / 'scene.tar.gz') != manifest['archive_sha256']:
        raise ValueError('archive digest/schema mismatch')
    if output.exists(): raise ValueError('restore destination exists')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.restore-', dir=output.parent) as tmp:
        stage = Path(tmp) / 'project'; stage.mkdir()
        expected = {e['path']: e for e in manifest['files']}
        if len(expected) != len(manifest['files']): raise ValueError('duplicate archive manifest path')
        with tarfile.open(archive_root / 'scene.tar.gz', 'r:gz') as tar:
            seen = set()
            for member in tar:
                target = contained(stage, member.name)
                if not member.isfile() or member.name not in expected or member.name in seen or member.size != expected[member.name]['bytes']:
                    raise ValueError('unexpected archive member/type/size')
                target.parent.mkdir(parents=True, exist_ok=True)
                with tar.extractfile(member) as src, target.open('wb') as dst: shutil.copyfileobj(src, dst)
                if sha(target) != expected[member.name]['sha256']: raise ValueError('restored file digest mismatch')
                seen.add(member.name)
            if seen != set(expected): raise ValueError('archive files missing')
        inputs = stage / 'workflow/completed_runs' / manifest['run_id'] / 'inputs'
        verify(inputs)
        stage.rename(output)
    return {'verified': True, 'file_count': len(expected), 'archive_sha256': manifest['archive_sha256'], 'output': str(output)}


def record_archive(project, run_id, archive_manifest, remote_receipt, restore_receipt):
    project = project.resolve(); safe_id(run_id)
    manifest, remote, restored = read(archive_manifest), read(remote_receipt), read(restore_receipt)
    imported = remote.get('export_item_import', {})
    reconciliation = imported.get('existing_destination_reconciliation', {})
    if (manifest.get('schema') != 'optic_scene_archive_v1' or manifest['run_id'] != run_id
            or remote.get('status') != 'completed'
            or imported.get('status') != 'dry_run_destination_matches_source'
            or not reconciliation.get('matches') or imported.get('delete_performed')
            or not restored.get('verified') or restored.get('diagnostics') != 'ok'):
        raise ValueError('archive requires fresh exact-copy readback and restored render proof')
    hashes = {e['relpath']: e['sha256'] for e in imported.get('verified_checksums', [])}
    if (hashes.get('payload/scene.tar.gz') != manifest['archive_sha256']
            or hashes.get('payload/archive_manifest.json') != sha(archive_manifest)):
        raise ValueError('remote archive digest differs from local archive')
    with locked(project):
        data = load(project); completed = data['completed_runs'][run_id]
        completion = read(contained(project, completed['receipt']))
        inputs = contained(project, str(Path(completed['receipt']).parent / 'inputs'))
        if (sha(contained(project, completed['receipt'])) != completed['receipt_sha256']
                or sha(inputs / 'scene_runtime.json') != restored['source_scene_sha256']):
            raise ValueError('restored proof is not the completed scene')
        record = {'schema': 'optic_verified_scene_archive_receipt_v1', 'run_id': run_id,
            'state': 'archived_verified_sources_retained', 'retention': manifest['retention'],
            'archive_sha256': manifest['archive_sha256'], 'archive_manifest_sha256': sha(archive_manifest),
            'remote_path': imported['destination_path'], 'remote_host': 'linux_pc',
            'remote_readback_sha256': sha(remote_receipt), 'restored_render': restored,
            'source_deleted': False, 'recorded_at': now()}
        dest = project / 'archives' / run_id / 'archive_receipt.json'
        if dest.exists():
            prior = read(dest)
            if prior['archive_sha256'] != record['archive_sha256'] or prior['remote_path'] != record['remote_path']:
                raise ValueError('archive receipt identity conflict')
            record = prior
        else: write(dest, record)
        data.setdefault('archives', {})[run_id] = {'state': record['state'],
            'receipt': dest.relative_to(project).as_posix(), 'receipt_sha256': sha(dest)}
        save(project, data)
    return record


def main():
    p = argparse.ArgumentParser(description=__doc__); subs = p.add_subparsers(dest='command', required=True)
    s = subs.add_parser('reconcile'); s.add_argument('--project', type=Path, required=True); s.add_argument('--run-root', type=Path, required=True); s.add_argument('--job', required=True); s.add_argument('--renderer', type=Path, required=True)
    s = subs.add_parser('archive'); s.add_argument('--project', type=Path, required=True); s.add_argument('--run-root', type=Path, required=True); s.add_argument('--run-id', required=True); s.add_argument('--output', type=Path, required=True); s.add_argument('--retention', choices=('compact','full'), default='compact')
    s = subs.add_parser('restore'); s.add_argument('--archive', type=Path, required=True); s.add_argument('--output', type=Path, required=True)
    s = subs.add_parser('record-archive'); s.add_argument('--project', type=Path, required=True); s.add_argument('--run-id', required=True); s.add_argument('--archive-manifest', type=Path, required=True); s.add_argument('--remote-receipt', type=Path, required=True); s.add_argument('--restore-receipt', type=Path, required=True)
    a = p.parse_args()
    try:
        if a.command == 'reconcile': result = reconcile(a.project.resolve(),a.run_root.resolve(),a.job,a.renderer.resolve())
        elif a.command == 'archive': result = archive(a.project.resolve(),a.run_root.resolve(),a.run_id,a.output.resolve(),a.retention)
        elif a.command == 'record-archive': result = record_archive(a.project.resolve(),a.run_id,a.archive_manifest.resolve(),a.remote_receipt.resolve(),a.restore_receipt.resolve())
        else: result = restore(a.archive.resolve(),a.output.resolve())
        print(json.dumps({k:v for k,v in result.items() if k not in {'files','frames'}}, indent=2))
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as exc: p.exit(1, f'error: {exc}\n')


if __name__ == '__main__': main()
