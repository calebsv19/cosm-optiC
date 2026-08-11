#!/usr/bin/env python3
"""PSG-20 imported-STL conforming physical-inset contract proof."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def run(
    command: list[str],
    *,
    success: bool = True,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if (result.returncode == 0) != success:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise AssertionError(
            f"unexpected exit {result.returncode}: {' '.join(command)}")
    return result


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_inset(
    inset_tool: pathlib.Path,
    mesh: pathlib.Path,
    region: pathlib.Path,
    root: pathlib.Path,
    suffix: str,
    *,
    depth: str = "0.075",
    success: bool = True,
) -> dict | None:
    derived = root / f"derived_{suffix}.runtime.json"
    receipt = root / f"inset_{suffix}.receipt.json"
    provenance = root / f"provenance_{suffix}.json"
    solid = root / f"solid_{suffix}.json"
    run([
        str(inset_tool),
        "--mesh", str(mesh),
        "--region", str(region),
        "--out", str(derived),
        "--derived-asset-id", "psg20_weathered_urn_inset",
        "--summary-out", str(receipt),
        "--provenance-out", str(provenance),
        "--solid-receipt-out", str(solid),
        "--threshold", "0.56",
        "--depth", depth,
        "--depth-variation", "0.18",
    ], success=success)
    if not success:
        return None
    return {
        "derived_path": derived,
        "receipt_path": receipt,
        "provenance_path": provenance,
        "solid_path": solid,
        "receipt": json.loads(receipt.read_text(encoding="utf-8")),
        "derived": json.loads(derived.read_text(encoding="utf-8")),
        "provenance": json.loads(provenance.read_text(encoding="utf-8")),
    }


def main() -> int:
    if len(sys.argv) != 5:
        print(
            f"usage: {sys.argv[0]} INSET_TOOL REGION_TOOL STL_TOOL "
            "IMPORT_HARNESS",
            file=sys.stderr,
        )
        return 2
    root = pathlib.Path(__file__).resolve().parents[2]
    inset_tool = pathlib.Path(sys.argv[1]).resolve()
    region_tool = pathlib.Path(sys.argv[2]).resolve()
    stl_tool = pathlib.Path(sys.argv[3]).resolve()
    harness = pathlib.Path(sys.argv[4]).resolve()
    fixture = (
        root / "tests/fixtures/procedural_imported_surface_inset_psg20"
    )
    contract = json.loads(
        (fixture / "visual_contract.json").read_text(encoding="utf-8"))
    with tempfile.TemporaryDirectory(prefix="psg20_imported_inset_") as temp:
        out = pathlib.Path(temp)
        authored = out / "authored"
        run([
            sys.executable, str(stl_tool), "create",
            "--recipe", str(fixture / "weathered_urn.recipe.json"),
            "--out-root", str(authored),
        ])
        stl = (
            authored
            / "curated/psg20_weathered_urn/source/psg20_weathered_urn.stl"
        )
        assert stl.is_file()
        imported = out / "imported"
        run([
            str(harness),
            "--stl", str(stl),
            "--out", str(imported),
            "--asset-id", "psg20_weathered_urn",
            "--scene-id", "psg20_imported_surface_inset",
            "--object-id", "psg20_urn",
        ])
        mesh = (
            imported
            / "assets/mesh_assets/psg20_weathered_urn.runtime.json"
        )
        assert mesh.is_file()
        source_sha = sha(mesh)
        source = json.loads(mesh.read_text(encoding="utf-8"))
        region = out / "chipped_plaster.region.json"
        region_receipt = out / "chipped_plaster.region.receipt.json"
        run([
            str(region_tool),
            "--mesh", str(mesh),
            "--recipe", str(fixture / "chipped_plaster.region_recipe.json"),
            "--out", str(region),
            "--summary-out", str(region_receipt),
        ])
        first = compile_inset(inset_tool, mesh, region, out, "a")
        second = compile_inset(inset_tool, mesh, region, out, "b")
        assert first is not None and second is not None
        receipt = first["receipt"]
        derived = first["derived"]
        provenance = first["provenance"]
        assertions = contract["assertions"]
        adaptive_baseline = contract["adaptive_baseline"]

        assert first["receipt"] == second["receipt"]
        for key in (
            "derived_path", "provenance_path", "solid_path",
        ):
            assert sha(first[key]) == sha(second[key])
        assert sha(mesh) == source_sha
        for key, expected in adaptive_baseline.items():
            assert receipt[key] == expected, (
                f"PSG-21 adaptive baseline changed {key}: "
                f"{receipt[key]!r} != {expected!r}"
            )
        assert receipt["source_file_digest_sha256"] == source_sha
        assert receipt["source_vertex_count"] == source["mesh"]["vertex_count"]
        assert (
            receipt["source_triangle_count"]
            == source["mesh"]["triangle_count"]
        )
        assert receipt["source_mesh_immutable"] is True
        assert receipt["exact_source_and_carrier_binding"] is True
        assert receipt["source_triangle_mapping_retained"] is True
        assert receipt["replaceable_derived_geometry"] is True
        assert receipt["transition_refinement_active"] is True
        assert receipt["adaptive_refinement_active"] is True
        assert receipt["adaptive_refinement_converged"] is True
        assert receipt["adaptive_refinement_pass_count"] >= 2
        assert (
            receipt["final_max_boundary_edge_length_units"]
            < receipt["initial_max_boundary_edge_length_units"]
        )
        assert (
            receipt["final_max_boundary_edge_length_units"]
            <= receipt["target_boundary_edge_length_units"]
        )
        assert receipt["explicit_region_transition_topology"] is True
        assert receipt["closed_valid_shell"] is True
        assert receipt["boundary_loop_count"] == 1
        assert receipt["boundary_edge_count"] == 0
        assert receipt["nonmanifold_edge_count"] == 0
        assert receipt["connected_component_count"] == 1
        assert receipt["euler_characteristic"] == 2
        assert receipt["signed_volume_units3"] > 0.0
        assert (
            receipt["transition_source_triangle_count"]
            >= assertions["minimum_transition_source_triangles"]
        )
        assert (
            receipt["transition_wall_triangle_count"]
            >= assertions["minimum_wall_triangles"]
        )
        assert (
            receipt["inset_floor_triangle_count"]
            >= assertions["minimum_floor_triangles"]
        )
        assert receipt["discarded_candidate_triangle_count"] <= 16
        assert receipt["minimum_inset_depth_units"] > 0.0
        assert (
            receipt["maximum_inset_depth_units"]
            >= receipt["minimum_inset_depth_units"]
        )
        assert (
            receipt["derived_mesh_digest_sha256"]
            != receipt["source_mesh_digest_sha256"]
        )
        assert (
            receipt["derived_vertex_count"]
            == derived["mesh"]["vertex_count"]
        )
        assert (
            receipt["derived_triangle_count"]
            == derived["mesh"]["triangle_count"]
        )
        groups = {
            group["group_id"]: group["triangle_span"]["count"]
            for group in derived["surface_groups"]
        }
        assert groups == {
            "retained_surface": receipt["retained_triangle_count"],
            "transition_wall": receipt["transition_wall_triangle_count"],
            "inset_floor": receipt["inset_floor_triangle_count"],
        }
        assert provenance["triangle_count"] == receipt["derived_triangle_count"]
        assert (
            provenance["provenance_digest_sha256"]
            == receipt["provenance_digest_sha256"]
        )
        role_counts: dict[str, int] = {}
        for entry in provenance["triangles"]:
            assert 0 <= entry["source_triangle_index"] < receipt[
                "source_triangle_count"
            ]
            role_counts[entry["role"]] = role_counts.get(entry["role"], 0) + 1
        assert role_counts == groups

        multi_region = out / "multiple_chips.region.json"
        run([
            str(region_tool),
            "--mesh", str(mesh),
            "--recipe", str(fixture / "multiple_chips.region_recipe.json"),
            "--out", str(multi_region),
        ])
        multi_first = compile_inset(
            inset_tool, mesh, multi_region, out, "multi_a")
        multi_second = compile_inset(
            inset_tool, mesh, multi_region, out, "multi_b")
        assert multi_first is not None and multi_second is not None
        assert multi_first["receipt"] == multi_second["receipt"]
        for key in ("derived_path", "provenance_path", "solid_path"):
            assert sha(multi_first[key]) == sha(multi_second[key])
        multi_receipt = multi_first["receipt"]
        assert multi_receipt["selected_component_count"] == 2
        assert multi_receipt["boundary_loop_count"] == 2
        assert multi_receipt["discarded_candidate_triangle_count"] <= 16
        assert multi_receipt["adaptive_refinement_pass_count"] >= 2
        assert multi_receipt["adaptive_refinement_active"] is True
        assert multi_receipt["adaptive_refinement_converged"] is True
        assert (
            multi_receipt["final_max_boundary_edge_length_units"]
            < multi_receipt["initial_max_boundary_edge_length_units"]
        )
        assert multi_receipt["closed_valid_shell"] is True
        assert multi_receipt["boundary_edge_count"] == 0
        assert multi_receipt["nonmanifold_edge_count"] == 0
        assert multi_receipt["connected_component_count"] == 1
        assert multi_receipt["euler_characteristic"] == 2
        assert multi_receipt["source_mesh_digest_sha256"] == (
            receipt["source_mesh_digest_sha256"]
        )
        assert sha(mesh) == source_sha

        stale_mesh = out / "stale.runtime.json"
        stale = json.loads(mesh.read_text(encoding="utf-8"))
        stale["mesh"]["vertices"][0]["x"] += 0.001
        stale_mesh.write_text(json.dumps(stale), encoding="utf-8")
        compile_inset(
            inset_tool, stale_mesh, region, out, "stale", success=False)
        compile_inset(
            inset_tool, mesh, region, out, "excess_depth",
            depth="0.50", success=False)

        print(json.dumps({
            "status": "ok",
            "fixture_source": "fresh_generated_stl",
            "source_triangle_count": receipt["source_triangle_count"],
            "derived_triangle_count": receipt["derived_triangle_count"],
            "transition_wall_triangle_count":
                receipt["transition_wall_triangle_count"],
            "inset_floor_triangle_count":
                receipt["inset_floor_triangle_count"],
            "boundary_ring_edge_count": receipt["boundary_ring_edge_count"],
            "source_mesh_digest_sha256":
                receipt["source_mesh_digest_sha256"],
            "derived_mesh_digest_sha256":
                receipt["derived_mesh_digest_sha256"],
        }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
