#!/usr/bin/env python3
"""End-to-end PSG-11 local remesh and source-acceleration contract."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile


def run(
    command: list[str], *, expect_success: bool = True
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if (result.returncode == 0) != expect_success:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise AssertionError(
            f"unexpected exit {result.returncode}: {' '.join(command)}"
        )
    return result


def compile_source_twist(
    tool: pathlib.Path,
    root: pathlib.Path,
    run_root: pathlib.Path,
    suffix: str,
) -> dict[str, object]:
    output = run_root / f"source_twist_{suffix}.runtime.json"
    receipt_path = run_root / f"source_twist_{suffix}.receipt.json"
    run(
        [
            str(tool),
            "--graph",
            str(
                root
                / "tests/fixtures/procedural_solid_graphs/source_mesh_twist.json"
            ),
            "--source",
            "source_cube="
            + str(
                root
                / "tests/fixtures/procedural_solid_graphs/source_cube.runtime.json"
            ),
            "--out",
            str(output),
            "--summary-out",
            str(receipt_path),
            "--asset-id",
            "psg11_source_twist",
            "--bounds-min",
            "-2.4,-2.4,-2.4",
            "--bounds-max",
            "2.4,2.4,2.4",
            "--cells",
            "24",
            "--local-adaptive",
            "--maximum-cells",
            "48",
            "--feature-size",
            "0.18",
        ]
    )
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    runtime = json.loads(output.read_text(encoding="utf-8"))
    assert receipt["local_adaptive"] is True
    assert receipt["local_converged"] is True
    assert receipt["adaptive"] is False
    assert receipt["local_pass_count"] == 2
    selected = receipt["local_passes"][receipt["local_selected_pass"]]
    assert selected["active_cell_ratio"] < 0.5
    assert selected["transition_surface_crossing_count"] == 0
    assert receipt["evaluated_sample_count"] < receipt["sample_count"] // 2
    assert receipt["boundary_edge_count"] == 0
    assert receipt["nonmanifold_edge_count"] == 0
    assert (
        receipt["accelerated_source_query_count"]
        == receipt["source_query_count"]
    )
    assert receipt["accelerated_source_query_count"] > 0
    assert receipt["feature_improvement_ratio"] > 0.5
    assert receipt["feature_vertex_count"] > 0
    assert receipt["feature_topology_preserved"] is True
    assert receipt["region_count"] == 1
    assert receipt["retained_triangle_count"] == receipt["triangle_count"]
    assert len(runtime["surface_groups"]) == receipt["region_count"]
    assert runtime["mesh"]["normal_provenance"] == "generated_crease_aware"
    return receipt


def compile_fixed_regions(
    tool: pathlib.Path,
    root: pathlib.Path,
    run_root: pathlib.Path,
) -> dict[str, object]:
    output = run_root / "fixed_regions.runtime.json"
    receipt_path = run_root / "fixed_regions.receipt.json"
    run([
        str(tool),
        "--graph",
        str(
            root
            / "tests/fixtures/procedural_solid_graphs/transformed_box.json"
        ),
        "--out", str(output),
        "--summary-out", str(receipt_path),
        "--asset-id", "psg15p_fixed_regions",
        "--cells", "12",
        "--assign-regions",
    ])
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    runtime = json.loads(output.read_text(encoding="utf-8"))
    assert receipt["adaptive"] is False
    assert receipt["local_adaptive"] is False
    assert receipt["quality_adaptive"] is False
    assert receipt["region_count"] == 1
    assert receipt["retained_triangle_count"] == receipt["triangle_count"]
    assert len(runtime["surface_groups"]) == receipt["region_count"]
    return receipt


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} PROCEDURAL_SOLID_ASSET_TOOL", file=sys.stderr)
        return 2
    root = pathlib.Path(__file__).resolve().parents[2]
    tool = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="psg11_agent_flow_") as temp:
        run_root = pathlib.Path(temp)
        first = compile_source_twist(tool, root, run_root, "a")
        second = compile_source_twist(tool, root, run_root, "b")
        assert first["mesh_digest_sha256"] == second["mesh_digest_sha256"]
        assert first["region_digest_sha256"] == second["region_digest_sha256"]
        assert first["local_passes"] == second["local_passes"]
        fixed = compile_fixed_regions(tool, root, run_root)
        assert fixed["region_digest_sha256"]

        hostile_output = run_root / "hostile.runtime.json"
        hostile_receipt = run_root / "hostile.receipt.json"
        run(
            [
                str(tool),
                "--graph",
                str(
                    root
                    / "tests/fixtures/procedural_solid_graphs/"
                    "blended_double_sphere.json"
                ),
                "--out",
                str(hostile_output),
                "--summary-out",
                str(hostile_receipt),
                "--asset-id",
                "hostile",
                "--cells",
                "24",
                "--local-adaptive",
                "--maximum-cells",
                "48",
                "--surface-band-cells",
                "9.0",
            ],
            expect_success=False,
        )
        assert not hostile_output.exists()
        assert not hostile_receipt.exists()

    print("PSG-11 agent local-remesh flow passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
