#!/usr/bin/env python3
"""Validate ray_tracing Linux worker package hygiene."""

import argparse
import stat
import sys
import tarfile


FORBIDDEN_SUBSTRINGS = (
    "_private_workspace_artifacts",
    "build/agent_runs",
    "docs/render_review_sets",
    "docs/material_preview_sets",
    ".env",
    "notary_submit",
)

ALLOWED_EXECUTABLE_FILES = {
    "bin/ray_tracing_render_headless",
    "bin/ray_tracing_job_runner",
    "bin/run_worker.sh",
}


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def validate_archive(path: str, package_root: str) -> int:
    with tarfile.open(path, "r:gz") as archive:
        members = archive.getmembers()

    if not members:
        return fail("worker archive is empty")

    for member in members:
        name = member.name
        if name.startswith("/") or "/../" in name or name.endswith("/..") or name == "..":
            return fail(f"unsafe archive member path: {name}")
        if name != package_root and not name.startswith(package_root + "/"):
            return fail(f"archive member outside package root: {name}")
        for forbidden in FORBIDDEN_SUBSTRINGS:
            if forbidden in name:
                return fail(f"forbidden private/generated artifact in worker archive: {name}")
        if member.isfile() and (member.mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)):
            rel = name[len(package_root) + 1 :]
            if rel not in ALLOWED_EXECUTABLE_FILES:
                return fail(f"unexpected executable file in worker archive: {name}")

    for required in ALLOWED_EXECUTABLE_FILES:
        full_name = f"{package_root}/{required}"
        matches = [member for member in members if member.name == full_name]
        if not matches:
            return fail(f"missing executable worker file: {full_name}")
        if not (matches[0].mode & stat.S_IXUSR):
            return fail(f"required worker file is not executable: {full_name}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True)
    parser.add_argument("--package-root", required=True)
    args = parser.parse_args()
    return validate_archive(args.archive, args.package_root)


if __name__ == "__main__":
    raise SystemExit(main())
