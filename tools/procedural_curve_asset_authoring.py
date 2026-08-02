#!/usr/bin/env python3
"""Author and deterministically expand editable PSG-23D curve fields."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
from typing import Any


SCHEMA = "ray_tracing.procedural_curve_field_authoring_v1"


def canonical(document: dict) -> bytes:
    return (json.dumps(
        document, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ) + "\n").encode("utf-8")


def digest_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    temporary.write_bytes(data)
    os.replace(temporary, path)


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("authoring document must be an object")
    return value


def finite_number(value: Any, name: str) -> float:
    if not isinstance(value, (int, float)) or not math.isfinite(float(value)):
        raise ValueError(f"{name} must be finite")
    return float(value)


def validate(document: dict) -> None:
    if document.get("schema") != SCHEMA or document.get("schema_version") != 1:
        raise ValueError("unsupported curve authoring schema")
    asset_id = document.get("asset_id")
    if not isinstance(asset_id, str) or not asset_id or len(asset_id) >= 64:
        raise ValueError("asset_id must be a non-empty short string")
    layout = document.get("layout")
    strand = document.get("strand")
    if not isinstance(layout, dict) or not isinstance(strand, dict):
        raise ValueError("layout and strand objects are required")
    rows = layout.get("rows")
    columns = layout.get("columns")
    points = strand.get("points")
    if not isinstance(rows, int) or not 1 <= rows <= 128:
        raise ValueError("layout.rows must be in [1, 128]")
    if not isinstance(columns, int) or not 1 <= columns <= 128:
        raise ValueError("layout.columns must be in [1, 128]")
    if not isinstance(points, int) or not 2 <= points <= 128:
        raise ValueError("strand.points must be in [2, 128]")
    for key in ("spacing_x", "spacing_y"):
        if finite_number(layout.get(key), f"layout.{key}") <= 0.0:
            raise ValueError(f"layout.{key} must be positive")
    if not isinstance(layout.get("seed"), int):
        raise ValueError("layout.seed must be an integer")
    for key in ("length", "root_radius", "tip_radius"):
        if finite_number(strand.get(key), f"strand.{key}") <= 0.0:
            raise ValueError(f"strand.{key} must be positive")
    if strand["tip_radius"] > strand["root_radius"]:
        raise ValueError("tip radius cannot exceed root radius")
    for key in ("direction_x", "direction_y", "bend", "curl", "length_variation"):
        finite_number(strand.get(key), f"strand.{key}")
    if not 0.0 <= strand["length_variation"] <= 0.9:
        raise ValueError("strand.length_variation must be in [0, 0.9]")


def noise(seed: int, index: int, channel: int) -> float:
    payload = f"{seed}:{index}:{channel}".encode("ascii")
    integer = int.from_bytes(hashlib.sha256(payload).digest()[:8], "big")
    return (integer / float(2**64 - 1)) * 2.0 - 1.0


def expand(document: dict) -> dict:
    validate(document)
    layout = document["layout"]
    strand = document["strand"]
    rows = layout["rows"]
    columns = layout["columns"]
    count = rows * columns
    points_per = strand["points"]
    strands: list[dict] = []
    for index in range(count):
        row, column = divmod(index, columns)
        root_x = (column - (columns - 1) * 0.5) * layout["spacing_x"]
        root_y = (row - (rows - 1) * 0.5) * layout["spacing_y"]
        phase = noise(layout["seed"], index, 0) * math.pi
        length = strand["length"] * (
            1.0 + strand["length_variation"] *
            noise(layout["seed"], index, 1)
        )
        point_docs = []
        for point_index in range(points_per):
            t = point_index / (points_per - 1)
            smooth = t * t * (3.0 - 2.0 * t)
            # Keep the authored root attached to its exact grid carrier while
            # allowing deterministic mid-strand curl.
            curl = (
                strand["curl"] * math.sin(t * math.pi) *
                math.sin(t * math.pi * 2.0 + phase)
            )
            position = {
                "x": root_x + strand["direction_x"] * length * t
                     + strand["bend"] * length * smooth * smooth,
                "y": root_y + strand["direction_y"] * length * t + curl,
                "z": length * t,
            }
            radius = (
                strand["root_radius"] * (1.0 - t) +
                strand["tip_radius"] * t
            )
            point_docs.append({
                "position": {axis: round(value, 12)
                             for axis, value in position.items()},
                "radius": round(radius, 12),
            })
        strands.append({"strand_index": index, "points": point_docs})
    authoring_sha = digest_bytes(canonical(document))
    return {
        "schema_family": "codework_curve_asset",
        "schema_variant": "curve_asset_runtime_v1",
        "schema_version": 1,
        "asset_id": document["asset_id"],
        "source_authoring_sha256": authoring_sha,
        "compile_meta": {
            "compiler": "procedural_curve_asset_authoring.py",
            "deterministic": True,
            "strand_count": count,
            "points_per_strand": points_per,
        },
        "strands": strands,
    }


def parse_value(text: str) -> Any:
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return text


def set_path(document: dict, dotted: str, value: Any) -> None:
    parts = dotted.split(".")
    if not parts or any(not part for part in parts):
        raise ValueError(f"invalid edit path: {dotted}")
    target: Any = document
    for part in parts[:-1]:
        if not isinstance(target, dict) or part not in target:
            raise ValueError(f"unknown edit path: {dotted}")
        target = target[part]
    if not isinstance(target, dict) or parts[-1] not in target:
        raise ValueError(f"unknown edit path: {dotted}")
    target[parts[-1]] = value


def command_generate(args: argparse.Namespace) -> int:
    authoring = load(args.authoring)
    runtime = expand(authoring)
    runtime_bytes = canonical(runtime)
    atomic_write(args.output, runtime_bytes)
    print(json.dumps({
        "asset_id": runtime["asset_id"],
        "authoring_sha256": runtime["source_authoring_sha256"],
        "runtime_sha256": digest_bytes(runtime_bytes),
        "strand_count": runtime["compile_meta"]["strand_count"],
        "points_per_strand": runtime["compile_meta"]["points_per_strand"],
        "output": str(args.output.resolve()),
    }, sort_keys=True))
    return 0


def command_edit(args: argparse.Namespace) -> int:
    document = load(args.input)
    before = digest_bytes(canonical(document))
    if args.expect_sha256 and before != args.expect_sha256:
        raise ValueError("authoring compare-and-swap digest mismatch")
    for edit in args.set:
        if "=" not in edit:
            raise ValueError("--set requires path=JSON_VALUE")
        dotted, value = edit.split("=", 1)
        set_path(document, dotted, parse_value(value))
    validate(document)
    data = canonical(document)
    atomic_write(args.output, data)
    print(json.dumps({
        "before_sha256": before,
        "after_sha256": digest_bytes(data),
        "output": str(args.output.resolve()),
    }, sort_keys=True))
    return 0


def command_inspect(args: argparse.Namespace) -> int:
    document = load(args.input)
    runtime = expand(document)
    print(json.dumps({
        "asset_id": document["asset_id"],
        "authoring_sha256": digest_bytes(canonical(document)),
        "strand_count": runtime["compile_meta"]["strand_count"],
        "points_per_strand": runtime["compile_meta"]["points_per_strand"],
        "editable_handles": {
            "density": ["layout.rows", "layout.columns"],
            "spacing": ["layout.spacing_x", "layout.spacing_y"],
            "length": ["strand.length", "strand.length_variation"],
            "direction": ["strand.direction_x", "strand.direction_y"],
            "shape": ["strand.bend", "strand.curl", "strand.points"],
            "profile": ["strand.root_radius", "strand.tip_radius"],
        },
    }, sort_keys=True))
    return 0


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__)
    commands = root.add_subparsers(dest="command", required=True)
    generate = commands.add_parser("generate")
    generate.add_argument("--authoring", type=Path, required=True)
    generate.add_argument("--output", type=Path, required=True)
    generate.set_defaults(function=command_generate)
    edit = commands.add_parser("edit")
    edit.add_argument("--input", type=Path, required=True)
    edit.add_argument("--output", type=Path, required=True)
    edit.add_argument("--expect-sha256")
    edit.add_argument("--set", action="append", default=[], required=True)
    edit.set_defaults(function=command_edit)
    inspect = commands.add_parser("inspect")
    inspect.add_argument("--input", type=Path, required=True)
    inspect.set_defaults(function=command_inspect)
    return root


def main() -> int:
    args = parser().parse_args()
    try:
        return args.function(args)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        raise SystemExit(f"error: {error}") from error


if __name__ == "__main__":
    raise SystemExit(main())
