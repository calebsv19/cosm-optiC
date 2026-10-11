#!/usr/bin/env python3
"""Read-only admission of isolated Ray preparation outputs and tool selection."""
from __future__ import annotations

import argparse
from pathlib import Path


def path_without_links(value: str, repository: Path) -> Path:
    raw = Path(value)
    if not value or '..' in raw.parts:
        raise ValueError('preparation path is empty or contains traversal')
    path = raw if raw.is_absolute() else repository / raw
    for component in (path, *path.parents):
        if component.is_symlink():
            raise ValueError('preparation path contains a symlink: ' + str(component))
    return path.resolve()


def overlap(first: Path, second: Path) -> bool:
    return first == second or first in second.parents or second in first.parents


def validate_paths(repository: Path, build: str, generated: str, dist: str,
                   tests: str, release: str | None = None) -> dict[str, Path]:
    repository = repository.resolve()
    roots = {name: path_without_links(value, repository) for name, value in
             [('build', build), ('generated', generated), ('dist', dist), ('tests', tests)]}
    selected = roots['build']
    if selected == repository / 'build' or selected in repository.parents or selected == repository:
        raise ValueError('preparation requires an isolated build root')
    protected = [repository / name for name in
                 ('src', 'include', 'tests', 'config', 'docs', 'tools', 'make', '.git',
                  'dist', 'build/toolchains', 'build/generated', 'build/agent_runs')]
    for name, path in roots.items():
        if path.exists() and not path.is_dir():
            raise ValueError('preparation root is not a directory: ' + name)
        if any(overlap(path, retained) for retained in protected):
            raise ValueError('preparation root overlaps source or retained output: ' + name)
        if name != 'build' and selected not in path.parents:
            raise ValueError('preparation output must be inside its selected build root: ' + name)
    children = [roots[name] for name in ('generated', 'dist', 'tests')]
    if any(overlap(a, b) for i, a in enumerate(children) for b in children[i + 1:]):
        raise ValueError('preparation output roots overlap each other')
    if any(overlap(path, selected / 'toolchains') for path in children):
        raise ValueError('preparation output overlaps compiled tools')
    if release is not None:
        output = path_without_links(release, repository)
        if any(overlap(output, path) for path in roots.values()):
            raise ValueError('release archive root overlaps preparation roots')
        if output == repository / 'build/release' or any(overlap(output, path) for path in protected):
            raise ValueError('release archive root overlaps source or retained output')
    return roots


def selected_tool(repository: Path, build: str, tool: str, architecture: str) -> Path:
    if not tool or Path(tool).name != tool or not architecture or Path(architecture).name != architecture:
        raise ValueError('tool identity must be a single path component')
    root = path_without_links(build, repository.resolve())
    validate_paths(repository, str(root), str(root / 'generated'),
                   str(root / 'dist'), str(root / 'agent_runs/ray_tracing'))
    path = path_without_links(str(root / 'toolchains/clang' / architecture / 'tools/cli' / tool), repository)
    if not path.is_file() or not path.stat().st_mode & 0o111:
        raise ValueError('selected preparation tool is absent or not executable: ' + str(path))
    return path


def fixture_root(repository: Path, build: str, tests: str, label: str) -> Path:
    root = path_without_links(build, repository.resolve())
    roots = validate_paths(repository, str(root), str(root / 'generated'),
                           str(root / 'dist'), tests)
    if not label or Path(label).is_absolute() or '..' in Path(label).parts:
        raise ValueError('fixture label escapes preparation root')
    return path_without_links(str(roots['tests'] / label), repository)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, required=True)
    parser.add_argument('--build-root', required=True)
    parser.add_argument('--generated-root')
    parser.add_argument('--dist-root')
    parser.add_argument('--test-root')
    parser.add_argument('--release-root')
    parser.add_argument('--tool')
    parser.add_argument('--architecture')
    parser.add_argument('--fixture-label')
    args = parser.parse_args()
    try:
        if args.fixture_label:
            print(fixture_root(args.repository, args.build_root, args.test_root, args.fixture_label))
        elif args.tool:
            print(selected_tool(args.repository, args.build_root, args.tool, args.architecture))
        else:
            validate_paths(args.repository, args.build_root, args.generated_root,
                           args.dist_root, args.test_root, args.release_root)
            print('ok')
    except (ValueError, TypeError) as exc:
        parser.exit(1, str(exc) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
