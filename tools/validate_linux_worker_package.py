#!/usr/bin/env python3
"""Validate ray_tracing Linux worker package hygiene."""

import argparse
import json
import stat
import struct
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

NATIVE_EXECUTABLE_FILES = {
    "bin/ray_tracing_render_headless",
    "bin/ray_tracing_job_runner",
}

ELF_MACHINE_BY_PLATFORM = {
    "linux-x86_64": 62,
    "linux-aarch64": 183,
}

PLATFORM_CAPABILITY_BY_PLATFORM = {
    "linux-x86_64": "platform-linux-x86_64-v1",
    "linux-aarch64": "platform-linux-aarch64-v1",
}


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def elf_machine(data: bytes) -> int | None:
    if len(data) < 20 or data[:4] != b"\x7fELF":
        return None
    if data[5] == 1:
        byte_order = "<"
    elif data[5] == 2:
        byte_order = ">"
    else:
        return None
    return int(struct.unpack(f"{byte_order}H", data[18:20])[0])


def validate_archive(path: str, package_root: str, platform: str) -> int:
    expected_machine = ELF_MACHINE_BY_PLATFORM.get(platform)
    if expected_machine is None:
        return fail(f"unsupported worker package platform: {platform}")
    with tarfile.open(path, "r:gz") as archive:
        members = archive.getmembers()
        if not members:
            return fail("worker archive is empty")

        by_name = {member.name: member for member in members}
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
            member = by_name.get(full_name)
            if member is None:
                return fail(f"missing executable worker file: {full_name}")
            if not (member.mode & stat.S_IXUSR):
                return fail(f"required worker file is not executable: {full_name}")

        for relative in NATIVE_EXECUTABLE_FILES:
            full_name = f"{package_root}/{relative}"
            extracted = archive.extractfile(by_name[full_name])
            data = extracted.read(64) if extracted is not None else b""
            machine = elf_machine(data)
            if machine is None:
                return fail(f"native worker file is not ELF: {full_name}")
            if machine != expected_machine:
                return fail(
                    f"native worker architecture mismatch for {full_name}: "
                    f"platform {platform} requires ELF machine {expected_machine}, got {machine}"
                )

        manifest_name = f"{package_root}/manifest.json"
        manifest_member = by_name.get(manifest_name)
        if manifest_member is None:
            return fail(f"missing worker manifest: {manifest_name}")
        extracted_manifest = archive.extractfile(manifest_member)
        try:
            manifest = json.loads(
                extracted_manifest.read().decode("utf-8")
                if extracted_manifest is not None
                else ""
            )
        except (UnicodeDecodeError, json.JSONDecodeError):
            return fail(f"invalid worker manifest JSON: {manifest_name}")
        if manifest.get("platform") != platform:
            return fail(
                f"worker manifest platform expected {platform!r}, "
                f"got {manifest.get('platform')!r}"
            )
        expected_capabilities = {
            "trio-headless-v1",
            "scene-project-portable-v1",
            "ray-tracing-project-render-v1",
            PLATFORM_CAPABILITY_BY_PLATFORM[platform],
        }
        if set(manifest.get("capabilities") or []) != expected_capabilities:
            return fail("manifest.json capabilities do not match the RayTracing package contract")

        package_manifest_name = f"{package_root}/package_manifest.json"
        package_manifest_member = by_name.get(package_manifest_name)
        if package_manifest_member is None:
            return fail(f"missing worker package manifest: {package_manifest_name}")
        extracted_package_manifest = archive.extractfile(package_manifest_member)
        try:
            package_manifest = json.loads(
                extracted_package_manifest.read().decode("utf-8")
                if extracted_package_manifest is not None
                else ""
            )
        except (UnicodeDecodeError, json.JSONDecodeError):
            return fail(f"invalid worker package manifest JSON: {package_manifest_name}")
        if package_manifest.get("platform") != platform:
            return fail(
                f"worker package manifest platform expected {platform!r}, "
                f"got {package_manifest.get('platform')!r}"
            )
        if set(package_manifest.get("capabilities") or []) != expected_capabilities:
            return fail(
                "package_manifest.json capabilities do not match the RayTracing package contract"
            )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True)
    parser.add_argument("--package-root", required=True)
    parser.add_argument("--platform", required=True)
    args = parser.parse_args()
    return validate_archive(args.archive, args.package_root, args.platform)


if __name__ == "__main__":
    raise SystemExit(main())
