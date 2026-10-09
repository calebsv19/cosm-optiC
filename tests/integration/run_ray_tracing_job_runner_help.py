#!/usr/bin/env python3
"""Verify real job-runner help is successful and has no filesystem effects."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def check_help(runner, arguments):
    with tempfile.TemporaryDirectory(prefix="ray-job-runner-help-") as directory:
        root = Path(directory)
        cwd = root / "cwd"
        cwd.mkdir()
        environment = dict(os.environ, HOME=str(root / "absent-home"),
                           TMPDIR=str(cwd))
        result = subprocess.run([str(runner), *arguments], cwd=cwd,
                                env=environment, capture_output=True, text=True,
                                timeout=10)
        output = result.stdout + result.stderr
        if result.returncode != 0 or "usage:" not in output:
            raise AssertionError(f"{arguments}: exit {result.returncode}: {output}")
        if set(root.iterdir()) != {cwd} or list(cwd.iterdir()):
            raise AssertionError(f"{arguments}: help created files or directories")
        if "failed to resolve jobs root" in output:
            raise AssertionError(f"{arguments}: help reached reconciliation")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--package-manifest", type=Path)
    args = parser.parse_args()
    runner = args.runner.resolve(strict=True)
    for arguments in (["--help"], ["-h"], ["status", "--help"],
                      ["reconcile", "-h"]):
        check_help(runner, arguments)
    if args.package_manifest:
        manifest = json.loads(args.package_manifest.read_text())
        command = manifest["self_test"]
        if command != {"type": "command", "argv": ["bin/ray_tracing_job_runner", "--help"]}:
            raise AssertionError("unexpected manifest self-test contract")
        if runner != (args.package_manifest.parent / command["argv"][0]).resolve():
            raise AssertionError("manifest self-test does not select this runner")
        check_help(runner, command["argv"][1:])
    print("job-runner help and optional manifest self-test passed")


if __name__ == "__main__":
    main()
