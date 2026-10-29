"""Snapshot an immutable rigid result without traversing its historical inputs.

The native rigid consumer remains the semantic/admission authority. This helper
copies only the declared bytes, checks their seals and preserves their names.
Historical producer scene paths inside sealed artifacts are not live resources.
"""
from pathlib import Path


def rigid_dependencies(scene, source, contained, read, sha):
    binding = scene.get('extensions', {}).get('ray_tracing', {}).get('rigid_result')
    if binding is None:
        return {}
    if not isinstance(binding, dict) or binding.get('schema') != 'ray_rigid_result_binding_v1':
        raise ValueError('unsupported rigid result binding')
    base = binding.get('bundle_path', '')
    if not isinstance(base, str) or not base or Path(base).parts[0] in {'results', 'config', 'data'}:
        raise ValueError('reserved rigid bundle path')
    root = contained(source, base)
    manifest = contained(source, (Path(base)/'manifest.json').as_posix())
    if not root.is_dir() or not manifest.is_file() or manifest.stat().st_size > 1024*1024:
        raise ValueError('missing rigid result bundle')
    doc = read(manifest)
    if (doc.get('schema') != 'ball_rigid_result_bundle_v1' or
            doc.get('result_revision') != binding.get('result_revision')):
        raise ValueError('rigid result revision mismatch')
    rows = doc.get('artifacts')
    if not isinstance(rows, list) or not 1 <= len(rows) <= 1024:
        raise ValueError('rigid result artifact inventory')
    result = {manifest.relative_to(source).as_posix(): manifest}
    total = manifest.stat().st_size
    names = set()
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError('rigid result artifact row')
        rel = row.get('path', '')
        artifact = contained(root, rel)
        if rel in names or rel == 'manifest.json' or not artifact.is_file():
            raise ValueError('rigid result artifact path')
        names.add(rel)
        size = artifact.stat().st_size
        total += size
        if total > 64*1024*1024 or size != row.get('size_bytes') or sha(artifact) != row.get('sha256'):
            raise ValueError('rigid result artifact digest/size mismatch')
        result[artifact.relative_to(source).as_posix()] = artifact
    if not {'poses.pack', 'input/scene.json', 'input/assets/asset_set.json'} <= names:
        raise ValueError('incomplete rigid result inventory')
    return result
