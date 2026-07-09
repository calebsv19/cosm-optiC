#!/usr/bin/env python3
"""Fixture tests for the ray_tracing Linux worker package validator."""

from __future__ import annotations

import argparse
import io
import os
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path


PACKAGE_ROOT = "ray_tracing-test-worker"
REQUIRED_EXECUTABLES = (
    "bin/ray_tracing_render_headless",
    "bin/ray_tracing_job_runner",
    "bin/run_worker.sh",
)


def add_file(archive: tarfile.TarFile, name: str, mode: int = 0o644, data: bytes = b"x\n") -> None:
    info = tarfile.TarInfo(name)
    info.mode = mode
    info.size = len(data)
    archive.addfile(info, io.BytesIO(data))


def write_archive(path: Path, members: list[tuple[str, int]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(path, "w:gz") as archive:
        for name, mode in members:
            add_file(archive, name, mode)


def valid_members() -> list[tuple[str, int]]:
    members = [(f"{PACKAGE_ROOT}/{name}", 0o755) for name in REQUIRED_EXECUTABLES]
    members.extend(
        [
            (f"{PACKAGE_ROOT}/manifest.json", 0o644),
            (f"{PACKAGE_ROOT}/docs/headless_agent_render_cli.md", 0o644),
            (f"{PACKAGE_ROOT}/config/scene_config.json", 0o644),
        ]
    )
    return members


def run_validator(root_dir: Path, archive: Path, package_root: str = PACKAGE_ROOT) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(root_dir / "tools" / "validate_linux_worker_package.py"),
            "--archive",
            str(archive),
            "--package-root",
            package_root,
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def expect_pass(label: str, result: subprocess.CompletedProcess[str]) -> None:
    if result.returncode != 0:
        raise SystemExit(f"{label}: expected pass, got {result.returncode}: {result.stderr}")


def expect_fail(label: str, result: subprocess.CompletedProcess[str], needle: str) -> None:
    if result.returncode == 0:
        raise SystemExit(f"{label}: expected failure")
    if needle not in result.stderr:
        raise SystemExit(
            f"{label}: expected stderr to contain {needle!r}; got {result.stderr!r}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-root", help="optional fixture output root")
    args = parser.parse_args()

    root_dir = Path(__file__).resolve().parents[2]
    work_root = Path(args.work_root) if args.work_root else root_dir / "build" / "agent_runs" / "ray_tracing" / "linux_worker_package_validator"
    if work_root.exists():
        shutil.rmtree(work_root)
    work_root.mkdir(parents=True)

    cases: list[tuple[str, list[tuple[str, int]], int, str | None]] = [
        ("valid_minimal", valid_members(), 0, None),
        ("unsafe_member_path", [(f"{PACKAGE_ROOT}/../escape", 0o644), *valid_members()], 1, "unsafe archive member path"),
        ("outside_package_root", [("other-root/bin/ray_tracing_render_headless", 0o755), *valid_members()], 1, "archive member outside package root"),
        ("forbidden_private_lane", [(f"{PACKAGE_ROOT}/build/agent_runs/private.json", 0o644), *valid_members()], 1, "forbidden private/generated artifact"),
        ("unexpected_executable", [(f"{PACKAGE_ROOT}/docs/readme.md", 0o755), *valid_members()], 1, "unexpected executable file"),
        (
            "missing_required_executable",
            [(name, mode) for name, mode in valid_members() if name != f"{PACKAGE_ROOT}/bin/run_worker.sh"],
            1,
            "missing executable worker file",
        ),
        (
            "required_not_executable",
            [
                (f"{PACKAGE_ROOT}/bin/ray_tracing_render_headless", 0o755),
                (f"{PACKAGE_ROOT}/bin/ray_tracing_job_runner", 0o755),
                (f"{PACKAGE_ROOT}/bin/run_worker.sh", 0o644),
                (f"{PACKAGE_ROOT}/manifest.json", 0o644),
            ],
            1,
            "required worker file is not executable",
        ),
    ]

    for label, members, expected_status, stderr_needle in cases:
        archive_path = work_root / f"{label}.tar.gz"
        write_archive(archive_path, members)
        result = run_validator(root_dir, archive_path)
        if expected_status == 0:
            expect_pass(label, result)
        else:
            require(stderr_needle is not None, f"{label}: missing expected stderr needle")
            expect_fail(label, result, stderr_needle)

    print(f"ray tracing Linux worker package validator fixtures passed: {work_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
