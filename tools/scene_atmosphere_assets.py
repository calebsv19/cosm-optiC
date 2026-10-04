#!/usr/bin/env python3
"""Publish contained, immutable atmosphere inputs; never run a physics solver."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import sys
# Bundled preset imports must not create bytecode inside the signed app.
sys.dont_write_bytecode = True
import tempfile


def contained(root: Path, value: str) -> Path:
    rel = PurePosixPath(value)
    if (not value or rel.is_absolute() or '\\' in value or ':' in value
            or any(p in ('', '.', '..') for p in value.split('/'))):
        raise ValueError('dependency needs a relative path without traversal')
    path = root / value
    for part in [path, *path.parents]:
        if part.is_symlink():
            raise ValueError('symlink dependencies are not supported')
        if part == root:
            break
    if not path.is_file() or not path.resolve().is_relative_to(root.resolve()):
        raise ValueError(f'missing or escaping dependency: {value}')
    return path


def collect(source: Path) -> dict[str, bytes]:
    if source.is_symlink():
        raise ValueError('source cannot be a symlink')
    root = source.parent.resolve()
    result: dict[str, bytes] = {}
    visiting: set[str] = set()

    def visit(rel: str, depth: int = 0):
        if rel in visiting or depth > 4:
            raise ValueError('cyclic or excessively nested cache manifest')
        if rel in result:
            return
        path = contained(root, rel)
        data = path.read_bytes()
        if path.stat().st_size != len(data):
            raise ValueError('source changed during import')
        result[rel] = data
        if path.suffix.lower() != '.json':
            return
        visiting.add(rel)
        doc = json.loads(data)
        if not isinstance(doc, dict):
            raise ValueError('cache manifest must be an object')
        refs = []
        for key in ('fluid_source', 'water_source'):
            if key in doc:
                refs.append(doc[key]['path'])
        if 'frames' in doc:
            frames = doc['frames']
            last = -1
            if not isinstance(frames, list) or not frames:
                raise ValueError('cache manifest has no frames')
            for frame in frames:
                index = frame['frame_index']
                if type(index) is not int or not last < index <= 2147483647:
                    raise ValueError('cache frames need sorted actual integer IDs')
                last = index
                refs.append(frame['path'])
        elif not refs and doc.get('frame_contract') != 'water_surface_heightfield_v1' and doc.get('schema') != 'physics_sim_water_surface_heightfield_v1':
            raise ValueError('unsupported cache manifest')
        for ref in refs:
            # Validate the owner-relative reference before joining it.
            contained(path.parent, ref)
            child = (PurePosixPath(rel).parent / ref).as_posix()
            visit(child, depth + 1)
        visiting.remove(rel)

    visit(source.name)
    # Detect edits during traversal before publishing any dependency.
    for rel, data in result.items():
        if contained(root, rel).read_bytes() != data:
            raise ValueError('source changed during import')
    return result


def scene_dependencies(scene: dict, project: Path) -> list[str]:
    value=scene.get('extensions',{}).get('ray_tracing',{}).get('authoring',{}).get('atmosphere_attachment')
    if value is None:return []
    if not isinstance(value,dict) or value.get('schema_version')!='ray_atmosphere_attachment_v1':
        raise ValueError('unsupported retained atmosphere attachment')
    source=contained(project,value['source_path'])
    files=collect(source)
    prefix=source.parent.relative_to(project)
    return [(prefix/rel).as_posix() for rel in files]


def publish(scene: Path, source: Path) -> dict:
    project = scene.resolve(strict=True).parent
    files = collect(source)
    digest = hashlib.sha256()
    for rel, data in sorted(files.items()):
        digest.update(rel.encode() + b'\0' + hashlib.sha256(data).digest())
    parent = project / 'assets' / 'atmosphere'
    for path in (project / 'assets', parent):
        if path.is_symlink():
            raise ValueError('project asset directory is a symlink')
    parent.mkdir(parents=True, exist_ok=True)
    destination = parent / digest.hexdigest()
    if destination.is_symlink():
        raise ValueError('published asset root is a symlink')
    if destination.exists():
        for rel, data in files.items():
            if contained(destination, rel).read_bytes() != data:
                raise ValueError('published immutable asset was modified')
    else:
        stage = Path(tempfile.mkdtemp(prefix='.import-', dir=parent))
        try:
            for rel, data in files.items():
                target = stage / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            os.rename(stage, destination)
        finally:
            if stage.exists():
                shutil.rmtree(stage)
    suffix = source.suffix.lower()
    kind = 'vf3d' if suffix == '.vf3d' else 'pack' if suffix == '.pack' else 'manifest'
    if suffix == '.json' and 'fluid_source' in json.loads(files[source.name]):
        kind = 'scene_bundle'
    return {'source_path': (destination / source.name).relative_to(project).as_posix(),
            'source_kind': kind, 'content_sha256': digest.hexdigest()}


def generate(scene: Path, preset: str, seed: int = 1) -> dict:
    from vf3d_initial_state_preset_tool import spec_for_name, build_fields, write_vf3d
    with tempfile.TemporaryDirectory(prefix='optic-atmosphere-') as scratch:
        spec = spec_for_name(preset, seed, Path(scratch))
        # Bounded initial-state generation, without the warmup/solver lane.
        spec['grid'].update(w=32, h=24, d=16, voxel_size=0.14375)
        fields = build_fields(spec)
        path = Path(scratch) / f'{preset}.vf3d'
        write_vf3d(path, spec, fields)
        return publish(scene, path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scene', type=Path, required=True)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--source', type=Path)
    action.add_argument('--preset')
    parser.add_argument('--seed', type=int, default=1)
    args = parser.parse_args()
    try:
        result = publish(args.scene, args.source) if args.source else generate(args.scene, args.preset, args.seed)
        print(json.dumps(result))
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(json.dumps({'error': str(error)}))
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
