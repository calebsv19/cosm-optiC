"""Resolve nested resource references without flattening manifest ownership."""
from pathlib import Path


def resolve_references(project, owner, values, contained):
    project=Path(project).resolve()
    owner=Path(owner).resolve()
    for value in values:
        root_candidate=contained(project,value)
        prefix=owner.parent.relative_to(project)
        owned=(prefix/value).as_posix()
        owner_candidate=contained(project,owned)
        # Explicit conventional project paths retain their established meaning.
        # Bare/nested manifest references resolve beside the owning manifest.
        if Path(value).parts[0] in ('assets','config','data') and root_candidate.is_file():
            chosen=root_candidate
        elif owner_candidate.is_file():
            chosen=owner_candidate
        elif root_candidate.is_file():
            chosen=root_candidate
        else:
            raise ValueError(f'missing dependency {value} referenced by {owner.relative_to(project)}')
        yield chosen.relative_to(project).as_posix()
