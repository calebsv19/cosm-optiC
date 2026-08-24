#!/usr/bin/env python3
"""Verify an embedded optiC local-development package identity."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--identity", required=True)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    identity = json.loads(Path(args.identity).read_text())
    binary = Path(args.binary)
    source_root = Path(args.source_root).resolve()
    fingerprint_tool = source_root / "tools/packaging/macos/source-state-fingerprint.py"
    fingerprint = subprocess.check_output(
        ["python3", str(fingerprint_tool), str(source_root)], text=True
    ).strip()
    commit = subprocess.check_output(
        ["git", "-C", str(source_root), "rev-parse", "HEAD"], text=True
    ).strip()

    checks = {
        "schema_version": identity.get("schema_version")
        == "optic_local_development_build_identity_v1",
        "package_class": identity.get("package_class") == "local_development",
        "profile": identity.get("profile") == args.profile,
        "program_version": identity.get("program_version") == args.version,
        "source_commit": identity.get("source", {}).get("commit") == commit,
        "source_fingerprint": identity.get("source", {}).get("fingerprint_sha256")
        == fingerprint,
        "binary_sha256": identity.get("packaged_binary_sha256")
        == sha256_file(binary),
    }
    failed = [name for name, passed in checks.items() if not passed]
    if failed:
        raise SystemExit("build identity verification failed: " + ", ".join(failed))
    print("build identity: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
