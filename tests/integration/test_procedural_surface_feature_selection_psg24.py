#!/usr/bin/env python3
"""PSG-24 field-selected carrier must remain a valid PSG-19 artifact."""
from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise AssertionError("failed: " + " ".join(command))
    return result


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tangent(normal: list[float]) -> list[float]:
    # Stable frame from a nonparallel axis; the fixture's mesh normals are finite.
    axis = [1.0, 0.0, 0.0] if abs(normal[0]) < .8 else [0.0, 1.0, 0.0]
    dot = sum(a*b for a, b in zip(axis, normal))
    value = [axis[i] - dot * normal[i] for i in range(3)]
    length = sum(v*v for v in value) ** .5
    return [v / length for v in value]


def main() -> int:
    if len(sys.argv) != 7:
        return 2
    selection_tool, region_tool, inset_tool, growth_tool, stl_tool, harness = map(pathlib.Path, sys.argv[1:])
    root = pathlib.Path(__file__).resolve().parents[2]
    fixture = root / "tests/fixtures/procedural_imported_surface_inset_psg20"
    with tempfile.TemporaryDirectory(prefix="psg24_field_selection_") as temp:
        out = pathlib.Path(temp)
        run([sys.executable, str(stl_tool), "create", "--recipe",
             str(fixture / "weathered_urn.recipe.json"), "--out-root", str(out / "authored")])
        stl = out / "authored/curated/psg20_weathered_urn/source/psg20_weathered_urn.stl"
        run([str(harness), "--stl", str(stl), "--out", str(out / "imported"),
             "--asset-id", "psg20_weathered_urn", "--scene-id", "psg24_field_bridge",
             "--object-id", "psg24_urn"])
        mesh = out / "imported/assets/mesh_assets/psg20_weathered_urn.runtime.json"
        base = out / "base.region.json"
        receipt = out / "base.receipt.json"
        run([str(region_tool), "--mesh", str(mesh), "--recipe",
             str(fixture / "chipped_plaster.region_recipe.json"), "--out", str(base),
             "--summary-out", str(receipt)])
        source = json.loads(mesh.read_text())
        region_receipt = json.loads(receipt.read_text())
        triangle = source["mesh"]["triangles"][0]
        vertex = source["mesh"]["vertices"][triangle["a"]]
        points = [source["mesh"]["vertices"][triangle[key]] for key in ("a", "b", "c")]
        ab = [points[1][key] - points[0][key] for key in ("x", "y", "z")]
        ac = [points[2][key] - points[0][key] for key in ("x", "y", "z")]
        normal = [ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0]]
        normal_length = sum(value*value for value in normal) ** .5
        normal = [value / normal_length for value in normal]
        position = [vertex["x"], vertex["y"], vertex["z"]]
        t = tangent(normal)
        b = [normal[1]*t[2]-normal[2]*t[1], normal[2]*t[0]-normal[0]*t[2], normal[0]*t[1]-normal[1]*t[0]]
        field = {
            "schema": "surface_feature_field_v1", "schema_version": 1,
            "source_mesh_digest_sha256": region_receipt["source_mesh_digest_sha256"],
            "authoring_digest_sha256": "a" * 64, "seed": 24,
            "normal_compatibility_cosine": 0.5,
            "features": [{"feature_id": 2401, "population": 3,
                "source_triangle": 0, "barycentric_root": [1, 0, 0],
                "position": position, "normal": normal, "tangent": t,
                "bitangent": b, "radius": 0.35, "aspect": 1.0,
                "rotation": 0.0, "edge_softness": 0.15, "rim_width": 0.2}]}
        field_path = out / "field.json"
        field_path.write_text(json.dumps(field, separators=(",", ":")))
        first, second = out / "field_a.region.json", out / "field_b.region.json"
        for target in (first, second):
            run([str(selection_tool), "--mesh", str(mesh), "--field", str(field_path),
                 "--base-region", str(base), "--out", str(target), "--region-id",
                 "psg24_selected_feature", "--minimum-radius", "0.1"])
        assert digest(first) == digest(second)
        selected = json.loads(first.read_text())
        assert selected["source_mesh_digest_sha256"] == region_receipt["source_mesh_digest_sha256"]
        assert selected["recipe_digest_sha256"] != json.loads(base.read_text())["recipe_digest_sha256"]
        assert selected["topology_unchanged"] is True
        assert selected["source_triangle_provenance_retained"] is True
        assert selected["transition_vertex_count"] > 0
        inset_outputs = []
        for suffix in ("a", "b"):
            derived = out / f"inset_{suffix}.runtime.json"
            inset_receipt = out / f"inset_{suffix}.receipt.json"
            inset_provenance = out / f"inset_{suffix}.provenance.json"
            run([str(inset_tool), "--mesh", str(mesh), "--region", str(first),
                 "--out", str(derived), "--derived-asset-id", "psg24_field_inset",
                 "--summary-out", str(inset_receipt), "--provenance-out", str(inset_provenance),
                 "--threshold", "0.20", "--depth", "0.04", "--depth-variation", "0.1"])
            inset_outputs.append((derived, inset_receipt, inset_provenance))
        inset = json.loads(inset_outputs[0][1].read_text())
        assert all(digest(inset_outputs[0][i]) == digest(inset_outputs[1][i]) for i in range(3))
        assert inset["exact_source_and_carrier_binding"] is True
        assert inset["closed_valid_shell"] is True
        assert inset["source_mesh_immutable"] is True
        assert inset["inset_floor_triangle_count"] > 0
        growth_outputs = []
        for suffix in ("a", "b"):
            growth = out / f"growth_{suffix}.runtime.json"
            growth_receipt = out / f"growth_{suffix}.receipt.json"
            growth_provenance = out / f"growth_{suffix}.provenance.json"
            run([str(growth_tool), "--mesh", str(mesh), "--region", str(first),
                 "--out", str(growth), "--growth-asset-id", "psg24_field_growth",
                 "--summary-out", str(growth_receipt), "--provenance-out", str(growth_provenance),
                 "--threshold", "0.20", "--radius", "0.08", "--height", "0.06",
                 "--attachment-depth", "0.012", "--max-elements", "12"])
            growth_outputs.append((growth, growth_receipt, growth_provenance))
        growth = json.loads(growth_outputs[0][1].read_text())
        assert all(digest(growth_outputs[0][i]) == digest(growth_outputs[1][i]) for i in range(3))
        assert growth["exact_source_and_carrier_binding"] is True
        assert growth["closed_valid_growth_shells"] is True
        assert growth["source_mesh_immutable"] is True
        assert growth["growth_element_count"] > 0
        print(json.dumps({"status": "ok", "carrier_digest": selected["value_digest_sha256"],
                          "transition_vertices": selected["transition_vertex_count"],
                          "inset_triangles": inset["derived_triangle_count"],
                          "growth_elements": growth["growth_element_count"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
