"""Contained dependency snapshots for optiC's scene-only workflow.

This is app policy around existing scene_runtime_v1/request formats, not a
replacement for the Physics Trio scene_project contract or worker queue.
"""
import copy
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

REPO = Path(__file__).resolve().parents[1]
PATH_KEYS = {'runtime_mesh_path', 'runtime_curve_path', 'source_uri', 'source_path',
             'file_path', 'image_path', 'texture_path', 'path', 'uri'}


def read(path):
    value = json.loads(Path(path).read_text())
    if not isinstance(value, dict):
        raise ValueError(f'expected JSON object: {path}')
    return value


def write(path, value):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + '\n')


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def contained(root, value):
    root = Path(root).resolve()
    p = Path(value)
    if not value or p.is_absolute() or '..' in p.parts or '\\' in str(value):
        raise ValueError(f'nonportable path: {value}')
    result = root / p
    # Even contained symlinks are excluded: snapshot ownership must be explicit.
    if any(part.is_symlink() for part in [result, *result.parents] if part != root.parent):
        raise ValueError(f'symlink dependency: {value}')
    if not result.resolve().is_relative_to(root):
        raise ValueError(f'escaping dependency: {value}')
    return result


def safe_id(value):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,79}', value):
        raise ValueError('IDs require 1-80 letters, digits, underscores or hyphens')
    return value


def references(doc):
    """Discover explicit file dependencies, excluding semantic target/path IDs."""
    if isinstance(doc, dict):
        for key, value in doc.items():
            if isinstance(value, str):
                file_key = (key in PATH_KEYS or key.endswith('_path') or key.endswith('_uri')
                            or key in {'runtime', 'authoring', 'projection', 'projected_source'})
                if file_key and value:
                    yield value
                elif key == 'source' and (value.startswith('/') or '/' in value or Path(value).suffix):
                    yield value
            else:
                yield from references(value)
    elif isinstance(doc, list):
        for value in doc:
            yield from references(value)


def snapshot_inputs(scene_path, request_path, dest):
    scene_path, request_path, dest = Path(scene_path), Path(request_path), Path(dest)
    source = scene_path.parent.resolve()
    source_scene_digest, source_request_digest = sha(scene_path), sha(request_path)
    scene = read(scene_path)
    if scene.get('schema_family') != 'codework_scene' or scene.get('schema_variant') != 'scene_runtime_v1':
        raise ValueError('expected codework_scene / scene_runtime_v1')
    for obj in scene.get('objects', []):
        for key in ('curve_asset_ref', 'procedural_solid_material_ref'):
            if obj.get(key):
                raise ValueError(f'{key} requires its dedicated export lane')
    objects = scene.get('objects', [])
    ids = [o['object_id'] for o in objects]
    if len(set(ids)) != len(ids):
        raise ValueError('duplicate object ID')
    # Resolve implicit mesh IDs explicitly, preserving authored paths when present.
    for obj in objects:
        if obj.get('object_type') == 'mesh_asset_instance':
            ext = obj.setdefault('extensions', {}).setdefault('line_drawing', {})
            if not ext.get('runtime_mesh_path'):
                asset_id = safe_id(obj['geometry_ref']['id'])
                ext['runtime_mesh_path'] = f'assets/mesh_assets/{asset_id}.runtime.json'
    request = read(request_path)
    if request.get('schema_version') != 'ray_tracing_agent_render_request_v1':
        raise ValueError('unsupported render request')
    if request.get('volume', {}).get('enabled') or request.get('simulation_frames') or request.get('scene_project'):
        raise ValueError('scene-only workflow: use Physics Trio exporter for simulation/volume projects')
    render = request.get('render', {})
    for key in ('width', 'height', 'frame_count', 'temporal_frames'):
        value = render.get(key, 1)
        if type(value) is not int or value < 1:
            raise ValueError(f'render.{key} must be a positive integer')
    if type(render.get('start_frame', 0)) is not int or render.get('start_frame', 0) < 0:
        raise ValueError('invalid start_frame')
    dependencies = {}
    pending = list(references(scene))
    while pending:
        rel = pending.pop()
        path = contained(source, rel)
        if rel in dependencies:
            continue
        if not path.is_file():
            raise ValueError(f'missing dependency: {rel}')
        if Path(rel).parts[0] in {'results', 'config', 'data'} or rel in {'scene_runtime.json', 'render_request.json', 'bundle_manifest.json'}:
            raise ValueError(f'dependency collides with reserved package input/output: {rel}')
        dependencies[rel] = path
        if path.suffix.lower() == '.json':
            pending.extend(references(read(path)))
        # Native dense-mesh loader uses a same-stem preview sidecar when present.
        if rel.endswith('.runtime.json'):
            preview = rel[:-len('.runtime.json')] + '.preview.json'
            if contained(source, preview).is_file():
                pending.append(preview)
    dest.mkdir(parents=True, exist_ok=True)
    for rel, path in sorted(dependencies.items()):
        target = contained(dest, rel)
        target.parent.mkdir(parents=True, exist_ok=True)
        digest = sha(path)
        shutil.copy2(path, target)
        if sha(target) != digest or sha(path) != digest:
            raise ValueError(f'dependency changed during snapshot: {rel}')
    write(dest / 'scene_runtime.json', scene)
    request = copy.deepcopy(request)
    request['scene'] = {'runtime_scene_path': 'scene_runtime.json'}
    video = copy.deepcopy(request.get('output', {}).get('video', {'enabled': False}))
    if video.get('enabled'):
        video['path'] = 'results/render.mp4'
    request['output'] = {'root': 'results', 'overwrite': False, 'video': video}
    request['progress'] = {'summary_path': 'results/render_summary.json',
                           'progress_path': 'results/render_progress.json'}
    if request.get('checkpoint', {}).get('path'):
        request['checkpoint']['path'] = 'results/checkpoint'
    write(dest / 'render_request.json', request)
    # Runtime reads defaults from its program root; ship those rather than using checkout state.
    (dest / 'config').mkdir(exist_ok=True)
    shutil.copy2(REPO / 'config/scene_config.json', dest / 'config/scene_config.json')
    write(dest / 'data/runtime/animation_config.json', {
        'inputRoot': 'config', 'outputRoot': 'results',
        'meshAssetRoot': 'assets/mesh_assets'})
    if sha(scene_path) != source_scene_digest or sha(request_path) != source_request_digest:
        raise ValueError('source scene/request changed during snapshot')
    return scene


def file_manifest(root):
    return [{'path': p.relative_to(root).as_posix(), 'bytes': p.stat().st_size, 'sha256': sha(p)}
            for p in sorted(Path(root).rglob('*')) if p.is_file() and p.name != 'bundle_manifest.json']


def seal(root, revision, renderer):
    source_commit = subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', 'HEAD'], text=True).strip()
    write(Path(root) / 'bundle_manifest.json', {
        'schema': 'optic_scene_render_bundle_v1', 'revision_id': revision,
        'required_renderer': {'source_commit': source_commit,
                              'worker_version': (REPO / 'WORKER_VERSION').read_text().strip(),
                              'local_binary_sha256': sha(renderer)},
        'files': file_manifest(root)})
    return verify(root)


def verify(root):
    root = Path(root).resolve()
    manifest = read(root / 'bundle_manifest.json')
    if manifest.get('schema') != 'optic_scene_render_bundle_v1':
        raise ValueError('unsupported bundle manifest')
    expected = set()
    for entry in manifest['files']:
        rel = entry['path']
        if rel in expected:
            raise ValueError(f'duplicate manifest path: {rel}')
        expected.add(rel)
        path = contained(root, rel)
        if not path.is_file() or path.stat().st_size != entry['bytes'] or sha(path) != entry['sha256']:
            raise ValueError(f'bundle digest mismatch: {rel}')
    actual = {p.relative_to(root).as_posix() for p in root.rglob('*')
              if p.is_file() and p != root / 'bundle_manifest.json' and not p.relative_to(root).parts[0] == 'results'}
    if actual != expected:
        raise ValueError('bundle file set mismatch')
    for rel in ('scene_runtime.json', 'render_request.json', 'config/scene_config.json', 'data/runtime/animation_config.json'):
        if rel not in expected:
            raise ValueError(f'missing bundle input: {rel}')
    for document in sorted(expected):
        if document == 'scene_runtime.json' or document.startswith('assets/') and document.endswith('.json'):
            for rel in references(read(root / document)):
                if rel not in expected:
                    raise ValueError(f'unmanifested scene dependency: {rel}')
                contained(root, rel)
    request = read(root / 'render_request.json')
    if request['scene']['runtime_scene_path'] != 'scene_runtime.json' or request['output']['root'] != 'results':
        raise ValueError('nonportable bundle request')
    return {'verified': True, 'revision_id': manifest['revision_id'], 'file_count': len(expected),
            'manifest_sha256': sha(root / 'bundle_manifest.json')}
