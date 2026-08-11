#!/usr/bin/env python3
"""PSG-24D positive spot roots compile into separate PSG-22 assets."""

from __future__ import annotations

import hashlib
import json
import math
import pathlib
import subprocess
import sys
import tempfile


def run(command: list[str], *, success: bool = True) -> None:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if (result.returncode == 0) != success:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise AssertionError("unexpected result: " + " ".join(command))


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def choose_clear_features(features: list[dict]) -> list[int]:
    accepted: list[tuple[list[float], float, int]] = []
    for feature in features:
        radius = float(feature["radius"]) * 0.32
        height = float(feature["height_or_depth"]) * 0.85
        attachment = max(radius * 0.22, height * 1.10)
        center = [
            float(feature["position"][axis]) +
            float(feature["normal"][axis]) * (height - attachment) * 0.5
            for axis in range(3)]
        bound = max(radius * max(1.0, float(feature["aspect"])),
                    (height + attachment) * 0.5)
        if all(math.dist(center, prior_center) >= 1.02 * (bound + prior_bound)
               for prior_center, prior_bound, _ in accepted):
            accepted.append((center, bound, int(feature["feature_id"])))
    return [feature_id for _, _, feature_id in accepted]


def main() -> int:
    if len(sys.argv) != 9:
        return 2
    selection_tool, growth_tool, region_tool, stl_tool, harness, digest_tool, compiler, bundle_tool = map(
        pathlib.Path, sys.argv[1:])
    root = pathlib.Path(__file__).resolve().parents[2]
    fixture = root / "tests/fixtures/procedural_surface_feature_fields_psg24a"
    with tempfile.TemporaryDirectory(prefix="psg24d_feature_deposit_") as temp:
        out = pathlib.Path(temp)
        run([sys.executable, str(stl_tool), "create", "--recipe",
             str(fixture / "closed_curved_plaster.recipe.json"),
             "--out-root", str(out / "authored")])
        stl = out / "authored/curated/psg24_closed_curved_plaster/source/psg24_closed_curved_plaster.stl"
        run([str(harness), "--stl", str(stl), "--out", str(out / "imported"),
             "--asset-id", "psg24_closed_curved_plaster",
             "--scene-id", "psg24d_feature_deposit", "--object-id", "psg24d_plaster"])
        mesh = out / "imported/assets/mesh_assets/psg24_closed_curved_plaster.runtime.json"
        source_digest = digest(mesh)
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
        large = [entry for entry in field_json["features"]
                 if float(entry.get("height_or_depth", 0.0)) > 0.0]
        selected_ids = choose_clear_features(large)
        assert len(selected_ids) >= 4
        ids = ",".join(str(value) for value in selected_ids)
        outputs: list[pathlib.Path] = []
        for suffix in ("a", "b"):
            target = out / f"compile_{suffix}"
            run([sys.executable, str(compiler),
                 "--selection-tool", str(selection_tool),
                 "--growth-tool", str(growth_tool), "--mesh", str(mesh),
                 "--field", str(field), "--base-region", str(region),
                 "--feature-ids", ids, "--out-root", str(target),
                 "--asset-prefix", "psg24d_curved_plaster_deposit",
                 "--material-semantic", "dried_mud_deposit",
                 "--radius-scale", "0.32", "--height-scale", "0.85",
                 "--attachment-to-radius", "0.22",
                 "--minimum-attachment-to-height", "1.10"])
            run([sys.executable, str(bundle_tool), "compile",
                 "--spec", str(target / "bundle.authoring.json"),
                 "--output", str(target / "bundle.json")])
            run([sys.executable, str(bundle_tool), "validate",
                 "--bundle", str(target / "bundle.json")])
            outputs.append(target)

        paths = sorted(
            str(path.relative_to(outputs[0])) for path in outputs[0].rglob("*")
            if path.is_file())
        assert paths == sorted(
            str(path.relative_to(outputs[1])) for path in outputs[1].rglob("*")
            if path.is_file())
        assert all(digest(outputs[0] / path) == digest(outputs[1] / path)
                   for path in paths)
        assert digest(mesh) == source_digest

        receipt = json.loads((outputs[0] / "receipts/surface_feature_deposit.receipt.json").read_text())
        provenance = json.loads((outputs[0] / "provenance/surface_feature_deposit.provenance.json").read_text())
        assert receipt["accepted_positive_height_feature_count"] == len(selected_ids)
        assert receipt["separate_growth_asset_count"] == len(selected_ids)
        assert receipt["closed_positive_volume_component_count"] == len(selected_ids)
        assert receipt["minimum_attachment_depth_units"] > 0.0
        assert receipt["minimum_cross_asset_clearance_units"] > 0.0
        assert receipt["forbidden_overlap_pair_count"] == 0
        assert receipt["self_intersection_pair_count"] == 0
        assert receipt["material_agreement_verified"]
        assert receipt["replaceable_not_boolean_unioned"]
        assert {entry["feature_id"] for entry in provenance["elements"]} == set(selected_ids)
        assert all(entry["feature_source_triangle_index"] ==
                   entry["attachment_source_triangle_index"]
                   for entry in provenance["triangles"])
        assert {entry["role"] for entry in provenance["triangles"]} == {
            "exposed_growth", "attachment_base"}
        assert all(entry["material_semantic"] == "dried_mud_deposit"
                   for entry in provenance["triangles"])
        assert all(element["positive_height_at_root"] > 0.0
                   for element in provenance["elements"])
        assert all(element["authored_height_or_depth"] > 0.0
                   for element in provenance["elements"])
        assert all(element["equator_at_or_below_root_plane"]
                   for element in provenance["elements"])
        bundle = json.loads((outputs[0] / "bundle.json").read_text())
        assert len(bundle["entrypoints"]["attached_deposits"]["assets"]) == len(selected_ids)

        overlapping_ids = ",".join(str(entry["feature_id"]) for entry in large)
        run([sys.executable, str(compiler),
             "--selection-tool", str(selection_tool),
             "--growth-tool", str(growth_tool), "--mesh", str(mesh),
             "--field", str(field), "--base-region", str(region),
             "--feature-ids", overlapping_ids,
             "--out-root", str(out / "overlap_rejected"),
             "--radius-scale", "0.55"], success=False)
        print(json.dumps({
            "status": "passed", "selected_features": len(selected_ids),
            "separate_assets": receipt["separate_growth_asset_count"],
            "growth_triangles": receipt["growth_triangle_count"],
            "minimum_clearance": receipt["minimum_cross_asset_clearance_units"],
            "repeat_artifacts": len(paths),
        }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
