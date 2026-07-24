#!/usr/bin/env python3
"""Verify independent worker identity without building or publishing a package."""

from __future__ import annotations

import json
import re
import subprocess
from pathlib import Path


SEMVER = re.compile(r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    worker_version = (root / "WORKER_VERSION").read_text(encoding="utf-8").strip()
    app_version = (root / "VERSION").read_text(encoding="utf-8").strip()
    require(SEMVER.fullmatch(worker_version) is not None, "invalid WORKER_VERSION")
    require(SEMVER.fullmatch(app_version) is not None, "invalid VERSION")

    generated = root / "build/generated/app/ray_tracing_worker_version.h"
    subprocess.run(
        [
            "python3",
            str(root / "tools/generate_worker_version_header.py"),
            "--version-file",
            str(root / "WORKER_VERSION"),
            "--output",
            str(generated),
            "--check",
        ],
        check=True,
    )
    require(
        f'#define RAY_TRACING_WORKER_RUNTIME_VERSION "{worker_version}"'
        in generated.read_text(encoding="utf-8"),
        "generated runtime version differs from WORKER_VERSION",
    )

    contract = subprocess.run(
        ["make", "-s", "package-linux-worker-dry-run"],
        cwd=root,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout
    require(
        f"worker version: {worker_version}" in contract,
        "package contract lost worker version",
    )
    require(
        f"source app version: {app_version}" in contract,
        "package contract lost source app version",
    )
    require(
        "Dry-run only: no worker package was built." in contract,
        "dry-run mutated package state",
    )
    print(
        json.dumps(
            {
                "status": "passed",
                "worker_version": worker_version,
                "source_program_version": app_version,
                "package_built": False,
                "release_authority": False,
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
