#!/usr/bin/env python3
"""Focused compile and calibration contract for formed-concrete presets."""
from __future__ import annotations
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/procedural_surface_formed_concrete_preset.py"


def wall_mesh() -> dict:
    # A closed enough two-sided wall-like fixture for field distribution; only
    # the +Y side is eligible through the face-direction envelope.
    return {"mesh": {"vertices": [
        {"x": -3, "y": 0, "z": -2}, {"x": 3, "y": 0, "z": -2},
        {"x": -3, "y": 0, "z": 2}, {"x": 3, "y": 0, "z": 2},
        {"x": -3, "y": -0.2, "z": -2}, {"x": 3, "y": -0.2, "z": -2},
        {"x": -3, "y": -0.2, "z": 2}, {"x": 3, "y": -0.2, "z": 2}],
        "triangles": [{"a":0,"b":2,"c":1},{"a":1,"b":2,"c":3},{"a":4,"b":5,"c":6},{"a":5,"b":7,"c":6}]}}


def compile_variant(name: str, mesh: Path, out: Path) -> dict:
    preset = ROOT / "tests/fixtures/procedural_surface_formed_concrete_presets" / f"{name}.json"
    command = [sys.executable, str(TOOL), "--preset", str(preset), "--mesh", str(mesh),
               "--source-mesh-digest", "a" * 64, "--output-root", str(out)]
    first = subprocess.run(command, text=True, capture_output=True, check=True)
    receipt = out / "receipts/formed_concrete_preset.receipt.json"
    first_bytes = receipt.read_bytes()
    field_bytes = (out / "assets/surface_feature_field_v1.json").read_bytes()
    subprocess.run(command, text=True, capture_output=True, check=True)
    assert receipt.read_bytes() == first_bytes
    assert (out / "assets/surface_feature_field_v1.json").read_bytes() == field_bytes
    assert json.loads(first.stdout)["preset_receipt"] == str(receipt)
    return json.loads(first_bytes)


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="formed_concrete_") as temporary:
        root = Path(temporary)
        mesh = root / "wall_mesh.json"
        mesh.write_text(json.dumps(wall_mesh()), encoding="utf-8")
        results = [compile_variant(name, mesh, root / name) for name in ("low", "medium", "high")]
    realized = [entry["coverage"]["realized_eligible_fraction"] for entry in results]
    assert realized[0] < realized[1] < realized[2], realized
    for entry in results:
        assert entry["coverage"]["not_an_exact_coverage_solver"] is True
        assert entry["field_digest_sha256"] == entry["material_claim"]["aligned_field_identity"]
        assert entry["signed_displacement_ranges"]["inward_pores"]["maximum"] < 0.0
        assert entry["signed_displacement_ranges"]["outward_aggregate"]["minimum"] > 0.0
        assert entry["candidate_bounds"]["capacity_respected"] is True
    print(json.dumps({"status":"passed", "realized_eligible_coverage":realized}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
