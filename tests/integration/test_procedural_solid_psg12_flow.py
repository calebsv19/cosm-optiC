#!/usr/bin/env python3
"""End-to-end PSG-12 quality refinement and split-normal contract."""

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


def compile_quality(
    tool: pathlib.Path,
    root: pathlib.Path,
    run_root: pathlib.Path,
    fixture: str,
    asset_id: str,
    suffix: str,
    *,
    source: bool = False,
) -> tuple[dict[str, object], dict[str, object]]:
    output = run_root / f"{asset_id}_{suffix}.runtime.json"
    receipt_path = run_root / f"{asset_id}_{suffix}.receipt.json"
    command = [
        str(tool),
        "--graph",
        str(root / "tests/fixtures/procedural_solid_graphs" / fixture),
        "--out",
        str(output),
        "--summary-out",
        str(receipt_path),
        "--asset-id",
        asset_id,
        "--cells",
        "24",
        "--quality-adaptive",
        "--maximum-cells",
        "48",
        "--quality-maximum-cells",
        "96",
        "--feature-size",
        "0.18",
    ]
    if source:
        command.extend(
            [
                "--source",
                "source_cube="
                + str(
                    root
                    / "tests/fixtures/procedural_solid_graphs"
                    / "source_cube.runtime.json"
                ),
            ]
        )
    run(command)
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    runtime = json.loads(output.read_text(encoding="utf-8"))
    assert receipt["quality_adaptive"] is True
    assert receipt["local_adaptive"] is False
    assert receipt["adaptive"] is False
    assert receipt["quality_refinement_triggered"] is True
    assert receipt["quality_refinement_selected"] is True
    assert receipt["quality_baseline_cells"] == 48
    assert receipt["quality_selected_cells"] == 96
    assert (
        receipt["quality_selected_signed_distance_rms_units"]
        < receipt["quality_baseline_signed_distance_rms_units"]
    )
    assert (
        receipt["quality_selected_face_gradient_rms_degrees"]
        < receipt["quality_baseline_face_gradient_rms_degrees"]
    )
    assert receipt["quality_refinement_improvement_ratio"] > 0.08
    assert receipt["crease_qef_rms_after"] < receipt["crease_qef_rms_before"]
    assert receipt["crease_qef_improvement_ratio"] > 0.10
    assert receipt["crease_topology_preserved"] is True
    assert receipt["shading_split_vertex_count"] > 0
    assert (
        receipt["shading_output_vertex_count"]
        > receipt["shading_source_vertex_count"]
    )
    assert (
        receipt["shading_hard_corner_rms_degrees_after"]
        < receipt["shading_hard_corner_rms_degrees_before"]
    )
    assert receipt["shading_geometric_topology_preserved"] is True
    assert receipt["boundary_edge_count"] == 0
    assert receipt["nonmanifold_edge_count"] == 0
    assert receipt["region_count"] > 0
    assert runtime["mesh"]["vertex_count"] == receipt["vertex_count"]
    assert runtime["mesh"]["triangle_count"] == receipt["triangle_count"]
    assert runtime["mesh"]["normal_provenance"] == "generated_crease_aware"
    return receipt, runtime


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} PROCEDURAL_SOLID_ASSET_TOOL", file=sys.stderr)
        return 2
    root = pathlib.Path(__file__).resolve().parents[2]
    tool = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="psg12_quality_flow_") as temp:
        run_root = pathlib.Path(temp)
        first, _ = compile_quality(
            tool, root, run_root, "transformed_box.json",
            "psg12_transformed_box", "a"
        )
        second, _ = compile_quality(
            tool, root, run_root, "transformed_box.json",
            "psg12_transformed_box", "b"
        )
        assert first["mesh_digest_sha256"] == second["mesh_digest_sha256"]
        assert first["local_passes"] == second["local_passes"]
        assert (
            first["shading_split_vertex_count"]
            == second["shading_split_vertex_count"]
        )

        source, _ = compile_quality(
            tool, root, run_root, "source_mesh_twist.json",
            "psg12_source_twist", "a", source=True
        )
        assert source["source_query_count"] > 0
        assert (
            source["accelerated_source_query_count"]
            == source["source_query_count"]
        )

        hostile_output = run_root / "hostile.runtime.json"
        hostile_receipt = run_root / "hostile.receipt.json"
        run(
            [
                str(tool),
                "--graph",
                str(
                    root
                    / "tests/fixtures/procedural_solid_graphs"
                    / "transformed_box.json"
                ),
                "--out",
                str(hostile_output),
                "--summary-out",
                str(hostile_receipt),
                "--asset-id",
                "psg12_hostile",
                "--cells",
                "24",
                "--quality-adaptive",
                "--maximum-cells",
                "48",
                "--quality-maximum-cells",
                "96",
                "--maximum-output-vertices",
                "4",
            ],
            expect_success=False,
        )
        assert not hostile_output.exists()
        assert not hostile_receipt.exists()

    print("PSG-12 quality refinement and split-normal flow passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
