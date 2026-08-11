#!/usr/bin/env python3
"""PSG-22 imported-surface attached-growth contract proof."""

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


def compile_growth(
    tool: pathlib.Path,
    mesh: pathlib.Path,
    region: pathlib.Path,
    root: pathlib.Path,
    suffix: str,
    *,
    radius: str = "0.15",
    success: bool = True,
) -> dict | None:
    growth = root / f"growth_{suffix}.runtime.json"
    receipt = root / f"growth_{suffix}.receipt.json"
    provenance = root / f"growth_{suffix}.provenance.json"
    run([
        str(tool),
        "--mesh", str(mesh),
        "--region", str(region),
        "--out", str(growth),
        "--growth-asset-id", "psg22_finial_moss_growth",
        "--summary-out", str(receipt),
        "--provenance-out", str(provenance),
        "--threshold", "0.62",
        "--radius", radius,
        "--height", "0.120",
        "--attachment-depth", "0.025",
        "--max-elements", "12",
    ], success=success)
    if not success:
        return None
    return {
        "growth_path": growth,
        "receipt_path": receipt,
        "provenance_path": provenance,
        "growth": json.loads(growth.read_text(encoding="utf-8")),
        "receipt": json.loads(receipt.read_text(encoding="utf-8")),
        "provenance": json.loads(provenance.read_text(encoding="utf-8")),
    }


def main() -> int:
    if len(sys.argv) != 5:
        print(
            f"usage: {sys.argv[0]} GROWTH_TOOL REGION_TOOL STL_TOOL "
            "IMPORT_HARNESS",
            file=sys.stderr,
        )
        return 2
    root = pathlib.Path(__file__).resolve().parents[2]
    growth_tool = pathlib.Path(sys.argv[1]).resolve()
    region_tool = pathlib.Path(sys.argv[2]).resolve()
    stl_tool = pathlib.Path(sys.argv[3]).resolve()
    harness = pathlib.Path(sys.argv[4]).resolve()
    fixture = (
        root / "tests/fixtures/procedural_imported_surface_growth_psg22"
    )
    contract = json.loads(
        (fixture / "visual_contract.json").read_text(encoding="utf-8"))
    assertions = contract["assertions"]
    with tempfile.TemporaryDirectory(prefix="psg22_growth_") as temp:
        out = pathlib.Path(temp)
        authored = out / "authored"
        run([
            sys.executable, str(stl_tool), "create",
            "--recipe", str(fixture / "mossy_garden_finial.recipe.json"),
            "--out-root", str(authored),
        ])
        stl = (
            authored
            / "curated/psg22_garden_finial/source/psg22_garden_finial.stl"
        )
        assert stl.is_file()
        imported = out / "imported"
        run([
            str(harness),
            "--stl", str(stl),
            "--out", str(imported),
            "--asset-id", "psg22_garden_finial",
            "--scene-id", "psg22_imported_surface_growth",
            "--object-id", "psg22_finial",
        ])
        mesh = (
            imported
            / "assets/mesh_assets/psg22_garden_finial.runtime.json"
        )
        source_sha = sha(mesh)
        source = json.loads(mesh.read_text(encoding="utf-8"))
        region = out / "moss_growth.region.json"
        region_receipt = out / "moss_growth.region.receipt.json"
        run([
            str(region_tool),
            "--mesh", str(mesh),
            "--recipe", str(fixture / "moss_growth.region_recipe.json"),
            "--out", str(region),
            "--summary-out", str(region_receipt),
        ])
        first = compile_growth(growth_tool, mesh, region, out, "a")
        second = compile_growth(growth_tool, mesh, region, out, "b")
        assert first is not None and second is not None
        receipt = first["receipt"]
        growth = first["growth"]
        provenance = first["provenance"]

        assert first["receipt"] == second["receipt"]
        assert sha(first["growth_path"]) == sha(second["growth_path"])
        assert sha(first["provenance_path"]) == sha(
            second["provenance_path"])
        assert sha(mesh) == source_sha
        assert receipt["source_file_digest_sha256"] == source_sha
        assert receipt["source_vertex_count"] == source["mesh"]["vertex_count"]
        assert (
            receipt["source_triangle_count"]
            == source["mesh"]["triangle_count"]
        )
        assert receipt["source_mesh_immutable"] is True
        assert receipt["exact_source_and_carrier_binding"] is True
        assert receipt["source_triangle_mapping_retained"] is True
        assert receipt["attachment_penetration_verified"] is True
        assert receipt["overlap_gate_passed"] is True
        assert receipt["self_intersection_gate_passed"] is True
        assert receipt["closed_valid_growth_shells"] is True
        assert receipt["replaceable_attached_geometry"] is True
        assert (
            receipt["growth_element_count"]
            >= assertions["minimum_growth_elements"]
        )
        assert receipt["connected_component_count"] == (
            receipt["growth_element_count"]
        )
        assert receipt["euler_characteristic"] == (
            2 * receipt["growth_element_count"]
        )
        assert receipt["boundary_edge_count"] == 0
        assert receipt["nonmanifold_edge_count"] == 0
        assert receipt["signed_volume_units3"] > 0.0
        assert receipt["minimum_attachment_depth_units"] > 0.0
        assert receipt["maximum_growth_height_units"] > 0.0
        assert receipt["minimum_inter_element_clearance_units"] > 0.0
        assert receipt["inter_element_overlap_pair_count"] <= (
            assertions["maximum_overlap_pairs"]
        )
        assert receipt["self_intersection_pair_count"] <= (
            assertions["maximum_self_intersection_pairs"]
        )
        assert receipt["exposed_growth_triangle_count"] >= (
            assertions["minimum_exposed_growth_triangles"]
        )
        assert receipt["attachment_base_triangle_count"] >= (
            assertions["minimum_attachment_base_triangles"]
        )
        assert receipt["growth_vertex_count"] == growth["mesh"]["vertex_count"]
        assert (
            receipt["growth_triangle_count"]
            == growth["mesh"]["triangle_count"]
        )
        groups = {
            group["group_id"]: group["triangle_span"]["count"]
            for group in growth["surface_groups"]
        }
        assert groups == {
            "exposed_growth": receipt["exposed_growth_triangle_count"],
            "attachment_base": receipt["attachment_base_triangle_count"],
        }
        assert provenance["triangle_count"] == receipt["growth_triangle_count"]
        assert provenance["provenance_digest_sha256"] == (
            receipt["provenance_digest_sha256"]
        )
        assert all(
            triangle["source_triangle_index"]
            < receipt["source_triangle_count"]
            for triangle in provenance["triangles"]
        )
        assert {
            triangle["role"] for triangle in provenance["triangles"]
        } == {"exposed_growth", "attachment_base"}

        stale = out / "stale.runtime.json"
        stale.write_text(mesh.read_text(encoding="utf-8"), encoding="utf-8")
        stale_document = json.loads(stale.read_text(encoding="utf-8"))
        stale_document["mesh"]["vertices"][0]["x"] += 0.01
        stale.write_text(
            json.dumps(stale_document, indent=2) + "\n", encoding="utf-8")
        compile_growth(
            growth_tool, stale, region, out, "stale", success=False)
        compile_growth(
            growth_tool, mesh, region, out, "oversized",
            radius="0.80", success=False)

        print(json.dumps({
            "status": "ok",
            "fixture_source": "fresh_generated_stl",
            "source_triangle_count": receipt["source_triangle_count"],
            "growth_element_count": receipt["growth_element_count"],
            "growth_triangle_count": receipt["growth_triangle_count"],
            "attachment_base_triangle_count":
                receipt["attachment_base_triangle_count"],
            "minimum_clearance_units":
                receipt["minimum_inter_element_clearance_units"],
            "growth_mesh_digest_sha256":
                receipt["growth_mesh_digest_sha256"],
        }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
