#!/usr/bin/env python3
"""Focused deterministic contract for the AI-first surface matrix planner."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command: list[str], expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True)
    if expect_success and result.returncode != 0:
        raise RuntimeError(f"command failed: {' '.join(command)}\n{result.stdout}{result.stderr}")
    if not expect_success and result.returncode == 0:
        raise RuntimeError(f"command unexpectedly succeeded: {' '.join(command)}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--document-tool", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    planner = root / "tools/procedural_surface_contract_matrix.py"
    fixture = root / "tests/fixtures/procedural_surface_contract_matrix_v1/cube_lane_matrix.json"
    with tempfile.TemporaryDirectory(prefix="surface_authoring_matrix_") as temporary:
        temporary_root = Path(temporary)
        first = temporary_root / "first"
        second = temporary_root / "second"
        command = [sys.executable, str(planner), "--matrix", str(fixture),
                   "--document-tool", str(args.document_tool), "--output-root"]
        run(command + [str(first)])
        run(command + [str(second)])
        first_receipt = json.loads((first / "matrix_receipt.json").read_text())
        second_receipt = json.loads((second / "matrix_receipt.json").read_text())
        assert first_receipt == second_receipt
        assert first_receipt["execution_mode"] == "document_readback"
        assert first_receipt["cell_count"] == 9
        cells = {cell["id"]: cell for cell in first_receipt["cells"]}
        assert "document" not in cells["control"]
        assert cells["material_only"]["proof_profiles"] == ["material"]
        assert set(cells["full_lanes"]["proof_profiles"]) == {
            "material", "microdetail", "signed_relief", "deep_inset", "selector", "attachment"}
        full_request = json.loads((first / cells["full_lanes"]["proof_request"]).read_text())
        assert "source_mesh_immutable" in full_request["required_invariants"]
        assert "boolean_union_forbidden" in full_request["required_invariants"]
        assert (first / cells["full_lanes"]["canvas_visual"]).is_file()

        invalid = json.loads(fixture.read_text())
        invalid["references"]["brown_material"]["output_domains"] = ["attached_asset"]
        invalid_path = temporary_root / "invalid.json"
        invalid_path.write_text(json.dumps(invalid), encoding="utf-8")
        rejected = run([sys.executable, str(planner), "--matrix", str(invalid_path),
                        "--output-root", str(temporary_root / "invalid")], expect_success=False)
        assert json.loads(rejected.stdout)["status"] == "error"

        invalid_adapter = json.loads(fixture.read_text())
        invalid_adapter["references"]["brown_material"]["execution"] = {"adapter": "shell"}
        invalid_adapter_path = temporary_root / "invalid_adapter.json"
        invalid_adapter_path.write_text(json.dumps(invalid_adapter), encoding="utf-8")
        rejected_adapter = run([sys.executable, str(planner), "--matrix", str(invalid_adapter_path),
                                "--output-root", str(temporary_root / "invalid_adapter")], expect_success=False)
        assert json.loads(rejected_adapter.stdout)["status"] == "error"
    print("surface_authoring_contract_matrix planner=ok repeat=ok negative=ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
