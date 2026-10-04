#!/usr/bin/env python3
"""Local editable projects, revision-specific previews and frozen render inputs."""
import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import fcntl
import html
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

from scene_workflow_bundle import (REPO, contained, read, safe_id, seal, sha,
                                   snapshot_inputs, verify, write)

MANIFEST = 'scene_workflow.json'


def now():
    return datetime.now(timezone.utc).isoformat()


@contextmanager
def locked(root):
    with (root / '.scene_workflow.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield


def save(root, data):
    path = root / MANIFEST
    with tempfile.NamedTemporaryFile(mode='w', dir=root, delete=False) as f:
        tmp = Path(f.name)
        json.dump(data, f, indent=2, sort_keys=True, allow_nan=False)
        f.write('\n'); f.flush(); os.fsync(f.fileno())
    os.replace(tmp, path)


def load(root):
    data = read(root / MANIFEST)
    if data.get('schema') != 'optic_scene_workflow_v1':
        raise ValueError('unsupported workflow manifest')
    return data


def init(root, scene, request, name):
    if root.exists():
        raise ValueError('project destination exists; use a new directory')
    root.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.scene-init-', dir=root.parent) as tmp:
        stage = Path(tmp) / 'project'
        snapshot_inputs(scene, request, stage)
        save(stage, {'schema': 'optic_scene_workflow_v1', 'name': name,
                     'editable_scene': 'scene_runtime.json', 'render_intent': 'render_request.json',
                     'created_at': now(), 'accepted_revision': None, 'revisions': {}, 'previews': {}})
        (stage / 'README.md').write_text(
            '# ' + name + '\n\nEdit scene_runtime.json in optiC. render_request.json owns final render intent.\n'
            'Capture a new revision after edits, render its preview, then explicitly approve it.\n'
            'Revisions and exported packages are frozen copies; previews identify their revision.\n'
            'No remote execution, publication, archive, or frame deletion is automatic.\n')
        stage.rename(root)
    return status(root)


def capture(root, revision, renderer):
    safe_id(revision)
    with locked(root):
        data = load(root)
        source_scene_digest = sha(root / data['editable_scene'])
        source_request_digest = sha(root / data['render_intent'])
        dest = root / 'revisions' / revision
        if dest.exists() or revision in data['revisions']:
            raise ValueError('revision exists; choose a new ID')
        dest.parent.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.capture-', dir=dest.parent) as tmp:
            stage = Path(tmp) / 'revision'
            snapshot_inputs(root / data['editable_scene'], root / data['render_intent'], stage)
            if sha(root / data['editable_scene']) != source_scene_digest or sha(root / data['render_intent']) != source_request_digest:
                raise ValueError('editable inputs changed during capture; retry with a new stable revision')
            proof = seal(stage, revision, renderer)
            stage.rename(dest)
        data['revisions'][revision] = {'state': 'draft', 'created_at': now(),
            'bundle': dest.relative_to(root).as_posix(), 'manifest_sha256': proof['manifest_sha256'],
            'editable_scene_sha256': source_scene_digest,
            'editable_request_sha256': source_request_digest,
            'dependencies': {e['path']: e['sha256'] for e in read(dest / 'bundle_manifest.json')['files'] if e['path'].startswith('assets/')}}
        save(root, data)
    return proof


def revision_root(root, data, revision):
    entry = data['revisions'][revision]
    path = contained(root, entry['bundle'])
    verify(path)
    if sha(path / 'bundle_manifest.json') != entry['manifest_sha256']:
        raise ValueError('revision manifest changed')
    return path


def preview(root, revision, run_id, renderer, mode, samples, width, height):
    safe_id(run_id)
    if min(samples, width, height) < 1:
        raise ValueError('preview count/resolution must be positive')
    with locked(root):
        data = load(root)
        source = revision_root(root, data, revision)
        manifest = read(source / 'bundle_manifest.json')
        if sha(renderer) != manifest['required_renderer']['local_binary_sha256']:
            raise ValueError('renderer differs from captured revision; capture a new revision')
        dest = root / 'previews' / run_id
        if dest.exists() or run_id in data['previews']:
            raise ValueError('preview run exists')
        shutil.copytree(source, dest / 'inputs')
        verify(dest / 'inputs')
        intent = read(source / 'render_request.json')
        start = intent['render'].get('start_frame', 0)
        count = intent['render']['frame_count']
        if mode == 'sampled':
            sample_count = min(samples, count)
            indices = sorted({start + round(i * (count - 1) / max(1, sample_count - 1)) for i in range(sample_count)})
        else:
            indices = list(range(start, start + min(samples, count)))
        entry = {'state': 'running', 'revision_id': revision, 'mode': mode,
                 'frames': indices, 'width': width, 'height': height,
                 'input_manifest_sha256': sha(source / 'bundle_manifest.json'),
                 'started_at': now(), 'artifacts': []}
        data['previews'][run_id] = entry
        save(root, data)
    # Do not hold project lock through native execution; editing remains usable.
    try:
        env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(dest / 'inputs'),
            RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT=str(dest / 'cache'))
        for index in indices:
            request = json.loads(json.dumps(intent))
            request['run_id'] = f'{run_id}-{index}'
            request['render'].update(start_frame=index, frame_count=1, width=width,
                height=height, temporal_frames=1, denoise_enabled=False)
            request['sampling'] = {'frame_offset': index - start, 'frame_count': count}
            request['resources'] = {'cpu_percent': 25, 'max_workers': 1, 'reserve_cpu_count': 2}
            out = f'../frames_run_{index:04d}'
            request['output'] = {'root': out, 'overwrite': False, 'video': {'enabled': False}}
            request['progress'] = {'summary_path': out + '/render_summary.json',
                                   'progress_path': out + '/render_progress.json'}
            req = dest / 'inputs' / f'preview_{index:04d}.json'
            write(req, request)
            for action in ('preflight', 'render'):
                with (dest / f'{index:04d}_{action}.log').open('w') as log:
                    subprocess.run([str(renderer), '--request', str(req), '--' + action,
                        '--summary', str(dest / f'{index:04d}_{action}_summary.json'), '--summary-file-only'],
                        cwd=dest / 'inputs', env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
            summary = read(dest / f'{index:04d}_render_summary.json')
            if summary.get('diagnostics') != 'ok' or summary.get('frames_rendered') != 1:
                raise ValueError(f'incomplete preview frame: {index}')
            frame = dest / f'frames_run_{index:04d}' / 'frames' / f'frame_{index:04d}.bmp'
            with frame.open('rb') as f:
                header = f.read(54)
            if len(header) != 54 or header[:2] != b'BM' or struct.unpack_from('<ii', header, 18) not in ((width, height), (width, -height)):
                raise ValueError('invalid preview BMP dimensions')
            entry['artifacts'].append({'frame': index, 'path': frame.relative_to(root).as_posix(), 'sha256': sha(frame)})
        for item in manifest['files']:
            if sha(contained(dest / 'inputs', item['path'])) != item['sha256']:
                raise ValueError('preview inputs changed during native rendering')
        if sha(renderer) != manifest['required_renderer']['local_binary_sha256']:
            raise ValueError('renderer changed during preview')
        entry['state'] = 'completed'
        rows = ''.join(f'<figure><img width="{width}" height="{height}" src="{html.escape(os.path.relpath(root / a["path"], dest))}"><figcaption>Frame {a["frame"]}</figcaption></figure>' for a in entry['artifacts'])
        (dest / 'index.html').write_text('<!doctype html><meta charset="utf-8"><title>Scene preview</title><style>body{background:#191b20;color:#eee;font:16px system-ui}main{display:flex;flex-wrap:wrap}figure{margin:12px}img{object-fit:contain}</style>' + f'<h1>{html.escape(revision)} — {html.escape(mode)} preview</h1><p>Revision-specific review images. Sampled frames do not demonstrate continuous motion.</p><main>{rows}</main>')
    except Exception as exc:
        entry['state'] = 'failed'; entry['error'] = str(exc)
        raise
    finally:
        entry['finished_at'] = now()
        write(dest / 'preview_receipt.json', entry)
        with locked(root):
            data = load(root); data['previews'][run_id] = entry; save(root, data)
    return entry


def accept(root, revision, run_id, note):
    if not note.strip():
        raise ValueError('visual approval requires a review note')
    with locked(root):
        data = load(root); source = revision_root(root, data, revision)
        proof = data['previews'][run_id]
        if proof['revision_id'] != revision or proof['state'] != 'completed' or proof['input_manifest_sha256'] != sha(source / 'bundle_manifest.json'):
            raise ValueError('approval requires completed preview of this exact revision')
        if not proof.get('artifacts') or len(proof['artifacts']) != len(proof['frames']):
            raise ValueError('preview has no complete frame evidence')
        for entry in proof['artifacts']:
            if sha(contained(root, entry['path'])) != entry['sha256']:
                raise ValueError('preview artifact changed')
        data['revisions'][revision].update(state='visually_approved', review_note=note, preview_id=run_id, approved_at=now())
        data['accepted_revision'] = revision
        save(root, data)
    return status(root)


def export(root, revision, output, purpose='final'):
    with locked(root):
        data = load(root); source = revision_root(root, data, revision)
        if purpose == 'final' and data['revisions'][revision]['state'] != 'visually_approved':
            raise ValueError('final export requires explicit visual approval')
        if output.exists():
            raise ValueError('export destination exists')
        if output.resolve().is_relative_to(root.resolve() / 'revisions'):
            raise ValueError('cannot export inside revisions')
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.export-', dir=output.parent) as tmp:
            stage = Path(tmp) / 'bundle'; shutil.copytree(source, stage)
            verify(stage); stage.rename(output)
    return dict(verify(output), purpose=purpose, visually_approved=data['revisions'][revision]['state'] == 'visually_approved')


def status(root):
    data = load(root)
    data['editable_changed_since_accepted'] = bool(data['accepted_revision'] and
        sha(root / data['editable_scene']) != data['revisions'][data['accepted_revision']]['editable_scene_sha256'])
    if data['accepted_revision']:
        entry = data['revisions'][data['accepted_revision']]
        data['editable_changed_since_accepted'] |= sha(root / data['render_intent']) != entry['editable_request_sha256']
        for rel, digest in entry['dependencies'].items():
            path = contained(root, rel)
            data['editable_changed_since_accepted'] |= not path.is_file() or sha(path) != digest
    for run_id, entry in list(data.get('completed_runs', {}).items()) + list(data.get('archives', {}).items()):
        receipt = contained(root, entry['receipt'])
        if sha(receipt) != entry['receipt_sha256'] or read(receipt)['run_id'] != run_id:
            raise ValueError('completed run receipt changed')
    data['project_root'] = str(root)
    return data


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='command', required=True)
    for command in ('init', 'capture', 'preview', 'accept', 'export', 'status'):
        s = sub.add_parser(command); s.add_argument('--project', type=Path, required=True)
        if command == 'init':
            s.add_argument('--scene', type=Path, required=True); s.add_argument('--request', type=Path, required=True); s.add_argument('--name', required=True)
        if command in ('capture', 'preview', 'accept', 'export'):
            s.add_argument('--revision', required=True)
        if command in ('capture', 'preview'):
            s.add_argument('--renderer', type=Path, required=True)
        if command == 'preview':
            s.add_argument('--run-id', required=True); s.add_argument('--mode', choices=('sampled', 'consecutive'), default='sampled')
            s.add_argument('--samples', type=int, default=5); s.add_argument('--width', type=int, default=320); s.add_argument('--height', type=int, default=180)
        if command == 'accept':
            s.add_argument('--preview-id', required=True); s.add_argument('--note', required=True)
        if command == 'export':
            s.add_argument('--output', type=Path, required=True)
            s.add_argument('--purpose', choices=('final', 'diagnostic'), default='final')
    s = sub.add_parser('verify'); s.add_argument('--bundle', type=Path, required=True)
    a = p.parse_args()
    try:
        root = a.project.resolve() if hasattr(a, 'project') else None
        renderer = a.renderer.resolve() if hasattr(a, 'renderer') else None
        if a.command == 'init': result = init(root, a.scene.resolve(), a.request.resolve(), a.name)
        elif a.command == 'capture': result = capture(root, a.revision, renderer)
        elif a.command == 'preview': result = preview(root, a.revision, a.run_id, renderer, a.mode, a.samples, a.width, a.height)
        elif a.command == 'accept': result = accept(root, a.revision, a.preview_id, a.note)
        elif a.command == 'export': result = export(root, a.revision, a.output.resolve(), a.purpose)
        elif a.command == 'verify': result = verify(a.bundle)
        else: result = status(root)
        print(json.dumps(result, indent=2))
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as exc:
        p.exit(1, f'error: {exc}\n')


if __name__ == '__main__': main()
