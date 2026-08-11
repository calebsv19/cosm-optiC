#!/usr/bin/env python3
"""Resolve a named editable wood-grain relief profile into PSG-18 inputs."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from procedural_surface_feature_spot_compiler import canonical, digest_bytes


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--grain-field", type=Path, required=True)
    parser.add_argument("--selected-face", required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    preset = load(args.preset)
    relief = preset.get("grain_relief", {})
    profile = relief.get("profiles", {}).get(args.profile)
    if preset.get("schema") != "ray_tracing.wood_surface_preset_v1" or not profile:
        raise SystemExit("preset/profile is not a supported wood relief profile")
    maximum_height = float(profile["maximum_height_units"])
    geometry = profile["geometry"]
    if geometry not in {"none", "selected_face_shell"} or maximum_height < 0.0:
        raise SystemExit("relief profile is invalid")
    grain_field_bytes = args.grain_field.read_bytes()
    grain_field = json.loads(grain_field_bytes)
    preset_digest = digest_bytes(canonical(preset))
    if grain_field.get("preset_digest_sha256") != preset_digest:
        raise SystemExit("grain field preset digest does not match the relief preset")
    result = {
        "schema": "ray_tracing.wood_grain_relief_request_v1",
        "schema_version": 1,
        "preset_id": preset["preset_id"],
        "preset_digest_sha256": preset_digest,
        "profile": args.profile,
        "selected_face": args.selected_face,
        "grain_field_path": str(args.grain_field),
        "grain_field_digest_sha256": digest_bytes(grain_field_bytes),
        "geometry": geometry,
        "maximum_height_units": maximum_height,
        "compiler_options": {
            "wood_grain_relief_scale": 1.0,
            "displacement_amplitude_units": maximum_height,
            "selected_face_required": geometry == "selected_face_shell",
        },
        "claim": (
            "material and shading normal only" if geometry == "none" else
            "one closed PSG-18 selected-face shell with physical grain height"),
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(canonical(result) + b"\n")
    print(json.dumps({"out": str(args.out), "digest_sha256": digest_bytes(canonical(result))}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
