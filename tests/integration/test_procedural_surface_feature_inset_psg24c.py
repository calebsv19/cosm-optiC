#!/usr/bin/env python3
"""PSG-24C explicit spot IDs must compile into repeatable physical insets."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def run(command: list[str]) -> None:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise AssertionError("failed: " + " ".join(command))


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    if len(sys.argv) != 8:
        return 2
    selection_tool, inset_tool, region_tool, stl_tool, harness, digest_tool, compiler = map(pathlib.Path, sys.argv[1:])
    root = pathlib.Path(__file__).resolve().parents[2]
    fixture = root / "tests/fixtures/procedural_surface_feature_fields_psg24a"
    with tempfile.TemporaryDirectory(prefix="psg24c_feature_inset_") as temp:
        out = pathlib.Path(temp)
        run([sys.executable, str(stl_tool), "create", "--recipe",
             str(fixture / "closed_curved_plaster.recipe.json"),
             "--out-root", str(out / "authored")])
        stl = out / "authored/curated/psg24_closed_curved_plaster/source/psg24_closed_curved_plaster.stl"
        run([str(harness), "--stl", str(stl), "--out", str(out / "imported"),
             "--asset-id", "psg24_closed_curved_plaster",
             "--scene-id", "psg24c_feature_inset", "--object-id", "psg24c_plaster"])
        mesh = out / "imported/assets/mesh_assets/psg24_closed_curved_plaster.runtime.json"
        region = out / "base.region.json"
        run([str(region_tool), "--mesh", str(mesh), "--recipe",
             str(fixture / "closed_curved_plaster.region_recipe.json"),
             "--out", str(region)])
        field_root = out / "field"
        run([sys.executable, str(root / "tools/procedural_surface_feature_field_authoring.py"),
             "--authoring", str(fixture / "curved_plaster_spots.authoring.json"),
             "--output-root", str(field_root), "--mesh", str(mesh),
             "--mesh-digest-tool", str(digest_tool)])
        field = field_root / "assets/surface_feature_field_v1.json"
        field_json = json.loads(field.read_text())
        selected_ids = [str(entry["feature_id"]) for entry in field_json["features"]
                        if float(entry.get("height_or_depth", 0.0)) < 0.0][:8]
        assert len(selected_ids) >= 4
        ids = ",".join(selected_ids)
        outputs = []
        for suffix in ("a", "b"):
            target = out / f"compile_{suffix}"
            run([sys.executable, str(compiler),
                 "--selection-tool", str(selection_tool),
                 "--inset-tool", str(inset_tool), "--mesh", str(mesh),
                 "--field", str(field), "--base-region", str(region),
                 "--feature-ids", ids, "--out-root", str(target),
                 "--derived-asset-id", "psg24c_feature_inset",
                 "--region-id", "psg24c_selected_spots",
                 "--threshold", "0.2", "--depth", "0.035",
                 "--depth-variation", "0.25",
                 "--minimum-component-triangles", "2"])
            outputs.append(target)
        relative_paths = [
            "bundle.authoring.json",
            "assets/psg24c_feature_inset.runtime.json",
            "carriers/psg24c_selected_spots.region.json",
            "provenance/inset.provenance.json",
            "provenance/surface_feature_inset.provenance.json",
            "receipts/feature_selection.receipt.json",
            "receipts/inset.receipt.json",
            "receipts/solid.receipt.json",
            "receipts/surface_feature_inset.receipt.json",
            "inputs/source.runtime.json",
            "inputs/surface_feature_field_v1.json",
            "inputs/selected_feature_ids.json",
        ]
        assert all(digest(outputs[0] / path) == digest(outputs[1] / path)
                   for path in relative_paths)
        receipt = json.loads((outputs[0] / "receipts/surface_feature_inset.receipt.json").read_text())
        inset = json.loads((outputs[0] / "receipts/inset.receipt.json").read_text())
        provenance = json.loads((outputs[0] / "provenance/surface_feature_inset.provenance.json").read_text())
        roles = {entry["role"] for entry in provenance["triangles"]}
        assert receipt["selected_feature_ids"] == [int(value) for value in selected_ids]
        assert all(float(entry["height_or_depth"]) < 0.0 for entry in field_json["features"]
                   if int(entry["feature_id"]) in set(map(int, selected_ids)))
        assert receipt["exact_source_field_and_selection_binding"]
        assert receipt["bounded_local_patch_extracted_before_inset"]
        assert receipt["local_patch_reduction_ratio"] > 0.5
        assert receipt["closure_stitch_ring_triangle_count"] > 0
        assert receipt["selected_component_count"] >= 2
        assert receipt["maximum_field_to_depth_range_violation_units"] <= 1.0e-12
        assert receipt["unselected_moved_vertex_count"] == 0
        assert roles == {"retained_surface", "transition_wall", "inset_floor"}
        assert all(entry["feature_id"] in ({0} | set(map(int, selected_ids)))
                   for entry in provenance["triangles"])
        assert inset["closed_valid_shell"] and inset["signed_volume_units3"] > 0.0
        assert inset["derived_mesh_digest_sha256"] != inset["source_mesh_digest_sha256"]
        assert inset["boundary_edge_count"] == 0 and inset["nonmanifold_edge_count"] == 0
        bundle = json.loads((outputs[0] / "bundle.authoring.json").read_text())
        assert set(bundle["entrypoints"]["physical_surface_inset"]) == {
            "derived_shell", "retained_surface", "transition_wall", "inset_floor"}
        print(json.dumps({
            "status": "passed", "selected_features": len(selected_ids),
            "selected_components": receipt["selected_component_count"],
            "local_patch_triangles": receipt["local_patch_triangle_count"],
            "source_triangles": receipt["source_triangle_count"],
            "derived_triangles": inset["derived_triangle_count"],
        }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
