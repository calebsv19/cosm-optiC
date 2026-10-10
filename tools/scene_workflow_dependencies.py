"""Resolve nested resource references without flattening manifest ownership."""
import os
from pathlib import Path


def resolve_references(project, owner, values, contained):
    project=Path(project).resolve()
    owner=Path(owner).resolve()
    for value in values:
        path=Path(value)
        if not value or path.is_absolute() or '\\' in value:
            raise ValueError(f'nonportable dependency: {value}')
        # Parent-relative paths are valid in nested native manifests. Resolve
        # beside the owner, then validate the normalized project-relative path.
        # Direct scene references still go through contained() before discovery.
        normalized=Path(os.path.normpath(owner.parent/path))
        if not normalized.is_relative_to(project):
            raise ValueError(f'escaping manifest dependency: {value}')
        root_candidate=contained(project,value) if '..' not in path.parts else None
        owned=normalized.relative_to(project).as_posix()
        owner_candidate=contained(project,owned)
        # Explicit conventional project paths retain their established meaning.
        # Bare/nested manifest references resolve beside the owning manifest.
        if path.parts[0] in ('assets','config','data') and root_candidate and root_candidate.is_file():
            chosen=root_candidate
        elif owner_candidate.is_file():
            chosen=owner_candidate
        elif root_candidate and root_candidate.is_file():
            chosen=root_candidate
        else:
            raise ValueError(f'missing dependency {value} referenced by {owner.relative_to(project)}')
        yield chosen.relative_to(project).as_posix()
