#!/usr/bin/env python3
"""Deterministically compare two RayTracing headless render artifact sets."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
from typing import Any


REQUEST_FAMILY = "codework_ray_tracing_artifact_comparison"
REQUEST_VARIANT = "render_artifact_pair_request_v1"
RESULT_VARIANT = "deterministic_bmp_and_summary_v1"
SUMMARY_SCHEMA = "ray_tracing_headless_summary_v1"
SEMANTIC_FIELDS = (
    "route_family",
    "route_native_3d",
    "rendered_frames",
    "frames_rendered",
    "volume_source_kind",
    "volume_visible",
    "integrator_3d",
    "render.start_frame",
    "render.frame_count",
    "render.width",
    "render.height",
    "render.normalized_t",
    "render.temporal_frames",
    "render.denoise_enabled",
    "inspection.preset",
    "inspection.trace_route",
    "inspection.caustic_mode",
    "inspection.caustic_product_mode",
)
HEX = frozenset("0123456789abcdef")


class ComparisonError(ValueError):
    pass


def canonical_bytes(document: dict[str, Any]) -> bytes:
    return (
        json.dumps(
            document,
            ensure_ascii=True,
            separators=(",", ":"),
            sort_keys=True,
        )
        + "\n"
    ).encode("ascii")


def file_digest(path: Path) -> str:
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def parse_json(path: Path) -> dict[str, Any]:
    def reject_duplicate(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ComparisonError(f"{path}: duplicate JSON key: {key}")
            result[key] = value
        return result

    try:
        value = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=reject_duplicate,
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ComparisonError(f"{path}: invalid JSON") from error
    if not isinstance(value, dict):
        raise ComparisonError(f"{path}: JSON root must be an object")
    return value


def require_string(document: dict[str, Any], name: str) -> str:
    value = document.get(name)
    if not isinstance(value, str) or not value or not value.isascii():
        raise ComparisonError(f"{name} must be a non-empty ASCII string")
    return value


def require_digest(document: dict[str, Any], name: str) -> str:
    value = require_string(document, name)
    if (
        len(value) != 71
        or not value.startswith("sha256:")
        or any(character not in HEX for character in value[7:])
    ):
        raise ComparisonError(f"{name} must be a sha256 digest")
    return value


def require_nonnegative_int(document: dict[str, Any], name: str) -> int:
    value = document.get(name)
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise ComparisonError(f"{name} must be a nonnegative integer")
    return value


def resolve_artifact(
    request_path: Path,
    side: dict[str, Any],
    name: str,
) -> Path:
    root_value = require_string(side, "artifact_root")
    root = Path(root_value)
    if not root.is_absolute():
        root = request_path.parent / root
    if root.is_symlink():
        raise ComparisonError(f"{name} artifact_root is unavailable")
    root = root.resolve()
    if not root.is_dir():
        raise ComparisonError(f"{name} artifact_root is unavailable")
    relative = Path(require_string(side, name))
    if relative.is_absolute() or ".." in relative.parts:
        raise ComparisonError(f"{name} must be a contained relative path")
    path = root / relative
    if (
        not path.is_file()
        or path.is_symlink()
        or path.resolve().parent != root
    ):
        raise ComparisonError(f"{name} is unavailable or not directly contained")
    return path


def validate_side(
    request_path: Path,
    document: dict[str, Any],
    side_name: str,
) -> dict[str, Any]:
    side = document.get(side_name)
    if not isinstance(side, dict):
        raise ComparisonError(f"{side_name} must be an object")
    allowed = {
        "lifecycle_id",
        "lifecycle_result_digest",
        "program_release_version",
        "worker_package_version",
        "artifact_root",
        "summary_path",
        "summary_digest",
        "frame_path",
        "frame_digest",
    }
    if set(side) != allowed:
        raise ComparisonError(f"{side_name} fields drifted")
    summary_path = resolve_artifact(request_path, side, "summary_path")
    frame_path = resolve_artifact(request_path, side, "frame_path")
    summary_digest = require_digest(side, "summary_digest")
    frame_digest = require_digest(side, "frame_digest")
    if file_digest(summary_path) != summary_digest:
        raise ComparisonError(f"{side_name} summary digest drifted")
    if file_digest(frame_path) != frame_digest:
        raise ComparisonError(f"{side_name} frame digest drifted")
    return {
        "lifecycle_id": require_string(side, "lifecycle_id"),
        "lifecycle_result_digest": require_digest(
            side, "lifecycle_result_digest"
        ),
        "program_release_version": require_string(
            side, "program_release_version"
        ),
        "worker_package_version": require_string(
            side, "worker_package_version"
        ),
        "summary_path": summary_path,
        "summary_digest": summary_digest,
        "frame_path": frame_path,
        "frame_digest": frame_digest,
    }


def validate_request(
    request_path: Path, document: dict[str, Any]
) -> tuple[dict[str, Any], dict[str, Any], dict[str, int]]:
    if set(document) != {
        "schema_family",
        "schema_variant",
        "comparison_id",
        "pair_id",
        "pair_result_digest",
        "source",
        "target",
        "policy",
        "operations",
    }:
        raise ComparisonError("comparison request fields drifted")
    if (
        document.get("schema_family") != REQUEST_FAMILY
        or document.get("schema_variant") != REQUEST_VARIANT
    ):
        raise ComparisonError("comparison request schema drifted")
    require_string(document, "comparison_id")
    require_string(document, "pair_id")
    require_digest(document, "pair_result_digest")
    operations = document.get("operations")
    if operations != {
        "semantic_compare": True,
        "visual_acceptance": False,
        "promotion": False,
        "current_pointer_read": False,
        "current_pointer_mutation": False,
        "network_contact": False,
        "remote_contact": False,
        "vps_contact": False,
        "custom_os_boot": False,
    }:
        raise ComparisonError("comparison request operations drifted")
    policy = document.get("policy")
    if not isinstance(policy, dict) or set(policy) != {
        "max_mean_absolute_channel_delta_ppm",
        "max_changed_pixel_ratio_ppm",
        "max_absolute_channel_delta",
    }:
        raise ComparisonError("comparison policy fields drifted")
    validated_policy = {
        name: require_nonnegative_int(policy, name)
        for name in policy
    }
    if (
        validated_policy["max_mean_absolute_channel_delta_ppm"] > 1_000_000
        or validated_policy["max_changed_pixel_ratio_ppm"] > 1_000_000
        or validated_policy["max_absolute_channel_delta"] > 255
    ):
        raise ComparisonError("comparison policy limit is out of range")
    return (
        validate_side(request_path, document, "source"),
        validate_side(request_path, document, "target"),
        validated_policy,
    )


def nested_value(document: dict[str, Any], dotted_name: str) -> Any:
    value: Any = document
    for name in dotted_name.split("."):
        if not isinstance(value, dict) or name not in value:
            raise ComparisonError(
                f"render summary lacks semantic field: {dotted_name}"
            )
        value = value[name]
    if isinstance(value, (dict, list)):
        raise ComparisonError(
            f"render summary semantic field is not scalar: {dotted_name}"
        )
    return value


def compare_summaries(
    source_path: Path, target_path: Path
) -> dict[str, Any]:
    source = parse_json(source_path)
    target = parse_json(target_path)
    if (
        source.get("schema_version") != SUMMARY_SCHEMA
        or target.get("schema_version") != SUMMARY_SCHEMA
    ):
        raise ComparisonError("unsupported RayTracing summary schema")
    differences = []
    for field in SEMANTIC_FIELDS:
        source_value = nested_value(source, field)
        target_value = nested_value(target, field)
        if source_value != target_value:
            differences.append(
                {
                    "field": field,
                    "source": source_value,
                    "target": target_value,
                }
            )
    return {
        "schema_version": SUMMARY_SCHEMA,
        "compared_fields": list(SEMANTIC_FIELDS),
        "differences": differences,
        "compatible": not differences,
    }


def read_bmp(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise ComparisonError(f"{path}: not a BMP file")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    header_size = struct.unpack_from("<I", data, 14)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    signed_height = struct.unpack_from("<i", data, 22)[0]
    planes = struct.unpack_from("<H", data, 26)[0]
    bits_per_pixel = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    supported_rgb = bits_per_pixel == 24 and compression == 0
    supported_bgra = bits_per_pixel == 32 and (
        compression == 0
        or (
            compression == 3
            and header_size >= 56
            and len(data) >= 66
            and struct.unpack_from("<I", data, 54)[0] == 0x00FF0000
            and struct.unpack_from("<I", data, 58)[0] == 0x0000FF00
            and struct.unpack_from("<I", data, 62)[0] == 0x000000FF
        )
    )
    if (
        header_size < 40
        or width <= 0
        or signed_height == 0
        or planes != 1
        or not (supported_rgb or supported_bgra)
    ):
        raise ComparisonError(f"{path}: unsupported BMP layout")
    height = abs(signed_height)
    bytes_per_pixel = bits_per_pixel // 8
    row_stride = ((width * bits_per_pixel + 31) // 32) * 4
    if len(data) != pixel_offset + row_stride * height:
        raise ComparisonError(f"{path}: BMP byte length drifted")
    pixels = []
    for y in range(height):
        row = pixel_offset + y * row_stride
        for x in range(width):
            offset = row + x * bytes_per_pixel
            pixels.append((data[offset + 2], data[offset + 1], data[offset]))
    return width, height, pixels


def compare_frames(
    source_path: Path,
    target_path: Path,
    policy: dict[str, int],
) -> dict[str, Any]:
    source_width, source_height, source_pixels = read_bmp(source_path)
    target_width, target_height, target_pixels = read_bmp(target_path)
    if (source_width, source_height) != (target_width, target_height):
        return {
            "compatible": False,
            "reason": "frame_dimensions_differ",
            "source_width": source_width,
            "source_height": source_height,
            "target_width": target_width,
            "target_height": target_height,
        }
    total_delta = 0
    max_delta = 0
    changed_pixels = 0
    for source_pixel, target_pixel in zip(source_pixels, target_pixels):
        channel_deltas = [
            abs(source_pixel[index] - target_pixel[index])
            for index in range(3)
        ]
        if any(channel_deltas):
            changed_pixels += 1
        total_delta += sum(channel_deltas)
        max_delta = max(max_delta, *channel_deltas)
    pixel_count = len(source_pixels)
    channel_count = pixel_count * 3
    mean_delta_ppm = (
        total_delta * 1_000_000 // (channel_count * 255)
        if channel_count
        else 0
    )
    changed_ratio_ppm = (
        changed_pixels * 1_000_000 // pixel_count if pixel_count else 0
    )
    thresholds_passed = (
        mean_delta_ppm
        <= policy["max_mean_absolute_channel_delta_ppm"]
        and changed_ratio_ppm
        <= policy["max_changed_pixel_ratio_ppm"]
        and max_delta <= policy["max_absolute_channel_delta"]
    )
    return {
        "compatible": True,
        "width": source_width,
        "height": source_height,
        "pixel_count": pixel_count,
        "channel_sample_count": channel_count,
        "total_absolute_channel_delta": total_delta,
        "mean_absolute_channel_delta_ppm": mean_delta_ppm,
        "changed_pixel_count": changed_pixels,
        "changed_pixel_ratio_ppm": changed_ratio_ppm,
        "max_absolute_channel_delta": max_delta,
        "exact_pixel_match": total_delta == 0,
        "thresholds_passed": thresholds_passed,
    }


def compare(request_path: Path) -> dict[str, Any]:
    request_digest = file_digest(request_path)
    document = parse_json(request_path)
    source, target, policy = validate_request(request_path, document)
    if (
        source["lifecycle_id"] == target["lifecycle_id"]
        or source["lifecycle_result_digest"]
        == target["lifecycle_result_digest"]
        or source["program_release_version"]
        == target["program_release_version"]
    ):
        raise ComparisonError(
            "source and target lifecycle/release identities must be distinct"
        )
    summary = compare_summaries(
        source["summary_path"], target["summary_path"]
    )
    image = compare_frames(source["frame_path"], target["frame_path"], policy)
    for side_name, side in (("source", source), ("target", target)):
        if (
            file_digest(side["summary_path"]) != side["summary_digest"]
            or file_digest(side["frame_path"]) != side["frame_digest"]
        ):
            raise ComparisonError(
                f"{side_name} artifact drifted during comparison"
            )
    if file_digest(request_path) != request_digest:
        raise ComparisonError("comparison request drifted during comparison")
    compatible = summary["compatible"] and image["compatible"]
    policy_passed = compatible and image.get("thresholds_passed") is True
    state = (
        "semantic_match"
        if policy_passed
        else ("semantic_difference" if compatible else "incompatible_artifacts")
    )
    return {
        "schema_family": REQUEST_FAMILY,
        "schema_variant": RESULT_VARIANT,
        "comparison_id": document["comparison_id"],
        "comparison_request_digest": request_digest,
        "semantic_contract_version": 1,
        "pair_id": document["pair_id"],
        "pair_result_digest": document["pair_result_digest"],
        "state": state,
        "source": {
            name: source[name]
            for name in (
                "lifecycle_id",
                "lifecycle_result_digest",
                "program_release_version",
                "worker_package_version",
                "summary_digest",
                "frame_digest",
            )
        },
        "target": {
            name: target[name]
            for name in (
                "lifecycle_id",
                "lifecycle_result_digest",
                "program_release_version",
                "worker_package_version",
                "summary_digest",
                "frame_digest",
            )
        },
        "policy": policy,
        "summary_comparison": summary,
        "image_comparison": image,
        "semantic_comparison_performed": True,
        "visual_acceptance_performed": False,
        "promotion_recommended": False,
        "current_pointer_read": False,
        "current_pointer_mutated": False,
        "network_contacted": False,
        "remote_contacted": False,
        "vps_contacted": False,
        "custom_os_booted": False,
        "next_boundary": (
            "human_visual_acceptance"
            if policy_passed
            else "review_semantic_difference"
        ),
    }


def write_create_only(path: Path, payload: bytes) -> None:
    path = path.resolve()
    if not path.parent.is_dir() or path.is_symlink():
        raise ComparisonError("comparison output parent is unavailable")
    try:
        with path.open("xb") as output:
            output.write(payload)
            output.flush()
            os.fsync(output.fileno())
    except FileExistsError:
        if not path.is_file() or path.is_symlink() or path.read_bytes() != payload:
            raise ComparisonError("comparison output already exists with drift")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--request", required=True)
    parser.add_argument("--output", required=True)
    arguments = parser.parse_args()
    request_path = Path(arguments.request).expanduser().resolve()
    if not request_path.is_file() or request_path.is_symlink():
        raise ComparisonError("comparison request is unavailable")
    result = compare(request_path)
    write_create_only(
        Path(arguments.output).expanduser(),
        canonical_bytes(result),
    )
    print(
        "ray_tracing artifact comparison "
        f"{result['state']}: {result['comparison_id']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
