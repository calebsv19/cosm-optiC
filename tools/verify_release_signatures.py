#!/usr/bin/env python3
"""Verify nested release signatures before an Apple notarization submission."""
import argparse
from pathlib import Path
import subprocess


def check_signature(details: str, *, identity: str, executable: bool) -> None:
    fields = dict(line.split('=', 1) for line in details.splitlines() if '=' in line)
    if f'Authority={identity}' not in details.splitlines():
        raise ValueError('required Developer ID authority is missing')
    if not identity.startswith('Developer ID Application: '):
        raise ValueError('release verification requires a Developer ID identity')
    if not fields.get('Timestamp') or fields['Timestamp'] == 'none':
        raise ValueError('secure timestamp is missing')
    if executable:
        directory = next((s for s in details.splitlines() if s.startswith('CodeDirectory ')), '')
        if '(runtime)' not in directory:
            raise ValueError('executable hardened runtime is missing')


def verify_bundle(app: Path, identity: str) -> int:
    if not app.is_dir() or app.is_symlink():
        raise ValueError('release app is unavailable or symlinked')
    checked = 0
    for path in sorted(app.rglob('*')):
        if path.is_symlink() or not path.is_file():
            continue
        kind = subprocess.run(['/usr/bin/file', '-b', str(path)],
                              check=True, capture_output=True, text=True).stdout
        if 'Mach-O' not in kind:
            continue
        subprocess.run(['/usr/bin/codesign', '--verify', '--strict', str(path)], check=True)
        result = subprocess.run(['/usr/bin/codesign', '--display', '--verbose=4', str(path)],
                                check=True, capture_output=True, text=True)
        try:
            check_signature(result.stdout + result.stderr, identity=identity,
                            executable='executable' in kind)
        except ValueError as exc:
            raise ValueError(f'{path.relative_to(app)}: {exc}') from exc
        checked += 1
    if not checked:
        raise ValueError('release bundle has no Mach-O files')
    return checked


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', type=Path)
    parser.add_argument('--identity', required=True)
    args = parser.parse_args()
    try:
        print(f'Nested release signature verification passed: {verify_bundle(args.app, args.identity)} Mach-O files')
    except (ValueError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f'Nested release signature verification failed: {exc}\n')
