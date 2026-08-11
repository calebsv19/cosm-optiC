#!/usr/bin/env python3
"""PSG-23A rooted strand/fiber authoring contract proof."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def run(command: list[str], *, success: bool = True) -> None:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if (result.returncode == 0) != success:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise AssertionError(
            f"unexpected exit {result.returncode}: {' '.join(command)}")


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_strands(
    tool: pathlib.Path,
    mesh: pathlib.Path,
    region: pathlib.Path,
    root: pathlib.Path,
    suffix: str,
    *,
    length: str = "0.34",
    success: bool = True,
) -> dict | None:
    tube = root / f"strands_{suffix}.runtime.json"
    strands = root / f"strands_{suffix}.asset.json"
    receipt = root / f"strands_{suffix}.receipt.json"
    provenance = root / f"strands_{suffix}.provenance.json"
    run([
        str(tool),
        "--mesh", str(mesh),
        "--region", str(region),
        "--tube-out", str(tube),
        "--strand-out", str(strands),
        "--strand-asset-id", "psg23a_scalp_rooted_strands",
        "--summary-out", str(receipt),
        "--provenance-out", str(provenance),
        "--threshold", "0.58",
        "--length", length,
        "--root-radius", "0.016",
        "--tip-radius", "0.005",
        "--root-penetration", "0.012",
        "--bend", "0.24",
        "--curl", "0.10",
        "--max-strands", "24",
    ], success=success)
    if not success:
        return None
    return {
        "tube_path": tube,
        "strand_path": strands,
        "receipt_path": receipt,
        "provenance_path": provenance,
        "tube": json.loads(tube.read_text(encoding="utf-8")),
        "strands": json.loads(strands.read_text(encoding="utf-8")),
        "receipt": json.loads(receipt.read_text(encoding="utf-8")),
        "provenance": json.loads(provenance.read_text(encoding="utf-8")),
    }


def main() -> int:
    if len(sys.argv) != 5:
        print(
            f"usage: {sys.argv[0]} STRAND_TOOL REGION_TOOL STL_TOOL "
            "IMPORT_HARNESS",
            file=sys.stderr,
        )
        return 2
    root = pathlib.Path(__file__).resolve().parents[2]
    strand_tool = pathlib.Path(sys.argv[1]).resolve()
    region_tool = pathlib.Path(sys.argv[2]).resolve()
    stl_tool = pathlib.Path(sys.argv[3]).resolve()
    harness = pathlib.Path(sys.argv[4]).resolve()
    fixture = (
        root / "tests/fixtures/procedural_imported_surface_strands_psg23a"
    )
    assertions = json.loads(
        (fixture / "visual_contract.json").read_text(encoding="utf-8")
    )["assertions"]
    with tempfile.TemporaryDirectory(prefix="psg23a_strands_") as temp:
        out = pathlib.Path(temp)
        authored = out / "authored"
        run([
            sys.executable, str(stl_tool), "create",
            "--recipe", str(fixture / "scalp_bust.recipe.json"),
            "--out-root", str(authored),
        ])
        stl = (
            authored
            / "curated/psg23a_scalp_bust/source/psg23a_scalp_bust.stl"
        )
        assert stl.is_file()
        imported = out / "imported"
        run([
            str(harness),
            "--stl", str(stl),
            "--out", str(imported),
            "--asset-id", "psg23a_scalp_bust",
            "--scene-id", "psg23a_imported_surface_strands",
            "--object-id", "psg23a_scalp",
        ])
        mesh = (
            imported
            / "assets/mesh_assets/psg23a_scalp_bust.runtime.json"
        )
        source_sha = sha(mesh)
        source = json.loads(mesh.read_text(encoding="utf-8"))
        region = out / "scalp_hair.region.json"
        region_receipt = out / "scalp_hair.region.receipt.json"
        run([
            str(region_tool),
            "--mesh", str(mesh),
            "--recipe", str(fixture / "scalp_hair.region_recipe.json"),
            "--out", str(region),
            "--summary-out", str(region_receipt),
        ])
        first = compile_strands(strand_tool, mesh, region, out, "a")
        second = compile_strands(strand_tool, mesh, region, out, "b")
        assert first is not None and second is not None
        receipt = first["receipt"]
        strands = first["strands"]
        tube = first["tube"]
        provenance = first["provenance"]

        assert first["receipt"] == second["receipt"]
        assert sha(first["tube_path"]) == sha(second["tube_path"])
        assert sha(first["strand_path"]) == sha(second["strand_path"])
        assert sha(first["provenance_path"]) == sha(
            second["provenance_path"])
        assert sha(mesh) == source_sha
        assert receipt["source_file_digest_sha256"] == source_sha
        assert receipt["source_vertex_count"] == source["mesh"]["vertex_count"]
        assert (
            receipt["source_triangle_count"]
            == source["mesh"]["triangle_count"]
        )
        for key in (
            "source_mesh_immutable",
            "exact_source_and_carrier_binding",
            "root_triangle_mapping_retained",
            "root_barycentrics_valid",
            "root_attachment_verified",
            "finite_continuous_strands",
            "overlap_gate_passed",
            "self_intersection_gate_passed",
            "closed_valid_tube_shells",
            "replaceable_strand_asset",
            "triangle_tube_proof_backend",
        ):
            assert receipt[key] is True, key
        assert receipt["strand_count"] >= assertions["minimum_strands"]
        assert strands["strand_count"] == receipt["strand_count"]
        assert (
            strands["points_per_strand"]
            >= assertions["minimum_control_points_per_strand"]
        )
        assert receipt["control_point_count"] == (
            strands["strand_count"] * strands["points_per_strand"]
        )
        assert receipt["connected_component_count"] == receipt["strand_count"]
        assert receipt["euler_characteristic"] == 2 * receipt["strand_count"]
        assert receipt["boundary_edge_count"] == 0
        assert receipt["nonmanifold_edge_count"] == 0
        assert receipt["signed_volume_units3"] > 0.0
        assert receipt["minimum_root_penetration_units"] > 0.0
        assert receipt["minimum_root_clearance_units"] > 0.0
        assert receipt["minimum_strand_length_units"] > 0.0
        assert (
            receipt["maximum_strand_length_units"]
            > receipt["minimum_strand_length_units"]
        )
        assert receipt["inter_strand_overlap_pair_count"] <= (
            assertions["maximum_overlap_pairs"]
        )
        assert receipt["strand_self_intersection_pair_count"] <= (
            assertions["maximum_self_intersection_pairs"]
        )
        assert receipt["root_cap_triangle_count"] >= (
            assertions["minimum_root_cap_triangles"]
        )
        assert receipt["shaft_triangle_count"] >= (
            assertions["minimum_shaft_triangles"]
        )
        assert receipt["tip_cap_triangle_count"] >= (
            assertions["minimum_tip_cap_triangles"]
        )
        assert receipt["tube_vertex_count"] == tube["mesh"]["vertex_count"]
        assert receipt["tube_triangle_count"] == tube["mesh"]["triangle_count"]
        groups = {
            group["group_id"]: group["triangle_span"]["count"]
            for group in tube["surface_groups"]
        }
        assert groups == {
            "root_cap": receipt["root_cap_triangle_count"],
            "strand_shaft": receipt["shaft_triangle_count"],
            "tip_cap": receipt["tip_cap_triangle_count"],
        }
        assert provenance["triangle_count"] == receipt["tube_triangle_count"]
        assert {
            item["role"] for item in provenance["triangles"]
        } == {"root_cap", "strand_shaft", "tip_cap"}
        for strand in strands["strands"]:
            bary = strand["root_barycentrics"]
            assert abs(sum(bary) - 1.0) < 1.0e-12
            assert all(value >= 0.0 for value in bary)
            assert (
                strand["source_triangle_index"]
                < receipt["source_triangle_count"]
            )
            radii = [point["radius"] for point in strand["points"]]
            assert all(
                radii[i] > radii[i + 1]
                for i in range(len(radii) - 1)
            )

        stale = out / "stale.runtime.json"
        stale.write_text(mesh.read_text(encoding="utf-8"), encoding="utf-8")
        stale_document = json.loads(stale.read_text(encoding="utf-8"))
        stale_document["mesh"]["vertices"][0]["x"] += 0.01
        stale.write_text(
            json.dumps(stale_document, indent=2) + "\n", encoding="utf-8")
        compile_strands(
            strand_tool, stale, region, out, "stale", success=False)
        compile_strands(
            strand_tool, mesh, region, out, "oversized",
            length="1.20", success=False)

        print(json.dumps({
            "status": "ok",
            "fixture_source": "fresh_generated_stl",
            "source_triangle_count": receipt["source_triangle_count"],
            "strand_count": receipt["strand_count"],
            "control_point_count": receipt["control_point_count"],
            "tube_triangle_count": receipt["tube_triangle_count"],
            "minimum_root_clearance_units":
                receipt["minimum_root_clearance_units"],
            "strand_data_digest_sha256":
                receipt["strand_data_digest_sha256"],
            "tube_mesh_digest_sha256":
                receipt["tube_mesh_digest_sha256"],
        }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
