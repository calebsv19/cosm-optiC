#!/usr/bin/env python3
"""Print a stable digest for the source state visible to a local package build."""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import subprocess
import sys


def git(repo: Path, *args: str) -> bytes:
    return subprocess.check_output(
        ["git", "-C", str(repo), *args], stderr=subprocess.DEVNULL
    )


def update_record(digest: "hashlib._Hash", label: bytes, payload: bytes) -> None:
    digest.update(label)
    digest.update(b"\0")
    digest.update(str(len(payload)).encode("ascii"))
    digest.update(b"\0")
    digest.update(payload)
    digest.update(b"\0")


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    digest = hashlib.sha256()

    try:
        update_record(digest, b"head", git(repo, "rev-parse", "HEAD").strip())
        update_record(
            digest,
            b"tracked_diff",
            git(repo, "diff", "--binary", "--no-ext-diff", "HEAD", "--"),
        )
        update_record(
            digest,
            b"submodules",
            git(repo, "submodule", "status", "--recursive"),
        )
        untracked = git(
            repo, "ls-files", "--others", "--exclude-standard", "-z"
        ).split(b"\0")
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"source-state-fingerprint: {error}", file=sys.stderr)
        return 1

    for encoded_path in sorted(path for path in untracked if path):
        relative = os.fsdecode(encoded_path)
        path = repo / relative
        update_record(digest, b"untracked_path", encoded_path)
        try:
            if path.is_symlink():
                update_record(
                    digest,
                    b"untracked_symlink",
                    os.fsencode(os.readlink(path)),
                )
            elif path.is_file():
                update_record(digest, b"untracked_file", path.read_bytes())
            else:
                update_record(digest, b"untracked_other", b"")
        except OSError as error:
            print(f"source-state-fingerprint: {relative}: {error}", file=sys.stderr)
            return 1

    print(digest.hexdigest())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
