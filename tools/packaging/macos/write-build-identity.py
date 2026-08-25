#!/usr/bin/env python3
"""Write the local-development identity embedded in a macOS app bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from datetime import datetime, timezone


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--program", required=True)
    parser.add_argument("--product", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--architecture", required=True)
    parser.add_argument("--toolchain", required=True)
    parser.add_argument("--branch", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--dirty", required=True, choices=("true", "false"))
    parser.add_argument("--source-fingerprint", required=True)
    parser.add_argument("--build-label", required=True)
    args = parser.parse_args()

    binary = Path(args.binary)
    output = Path(args.output)
    payload = {
        "schema_version": "optic_local_development_build_identity_v1",
        "package_class": "local_development",
        "profile": args.profile,
        "program": args.program,
        "product": args.product,
        "program_version": args.version,
        "architecture": args.architecture,
        "toolchain": args.toolchain,
        "source": {
            "branch": args.branch,
            "commit": args.commit,
            "dirty": args.dirty == "true",
            "fingerprint_sha256": args.source_fingerprint,
        },
        "build_label": args.build_label,
        "packaged_binary_sha256": sha256_file(binary),
        "built_at_utc": datetime.now(timezone.utc).isoformat(),
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
