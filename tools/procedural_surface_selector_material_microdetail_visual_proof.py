#!/usr/bin/env python3
"""Prove a digest-bound feature selector gates material and microdetail."""

from __future__ import annotations

import argparse
import json
import os
import platform
import sys
from pathlib import Path

import procedural_imported_surface_region_visual_proof as psg19


ROOT = Path(__file__).resolve().parents[1]


def default_tool(name: str) -> Path:
    return ROOT / "build" / "toolchains" / "clang" / platform.machine() / "tools" / "cli" / name


def parse_args() -> argparse.Namespace:
    fixture = ROOT / "tests" / "fixtures" / "procedural_surface_feature_fields_psg24a"
    imported = ROOT / "tests" / "fixtures" / "procedural_imported_surface_region_psg19"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--selector-tool", type=Path, default=default_tool("procedural_surface_feature_selection_tool"))
    parser.add_argument("--region-tool", type=Path, default=default_tool("procedural_imported_surface_region_tool"))
    parser.add_argument("--region-binding-tool", type=Path, default=default_tool("procedural_solid_material_agent_tool"))
    parser.add_argument("--material-tool", type=Path, default=default_tool("procedural_solid_authored_material_agent_tool"))
    parser.add_argument("--authored-binding-tool", type=Path, default=default_tool("procedural_solid_authored_binding_agent_tool"))
    parser.add_argument("--graph-tool", type=Path, default=default_tool("procedural_solid_material_graph_agent_tool"))
    parser.add_argument("--render-cli", type=Path, default=default_tool("ray_tracing_render_headless"))
    parser.add_argument("--stl-tool", type=Path, default=ROOT.parents[1] / "tools" / "procedural_object_authoring" / "procedural_stl_tool.py")
    parser.add_argument("--import-harness", type=Path, default=ROOT.parents[1] / "line_drawing" / "build" / "toolchains" / "clang" / "bin" / "imported_mesh_harness")
    parser.add_argument("--contract", type=Path, default=fixture / "visual_contract.json")
    parser.add_argument("--recipe", type=Path, default=imported / "statue_fragment.recipe.json")
    parser.add_argument("--region-recipe", type=Path, default=imported / "plaster_peel.region_recipe.json")
    parser.add_argument("--feature-authoring", type=Path, default=fixture / "curved_plaster_spots.authoring.json")
    return parser.parse_args()


def require_tools(args: argparse.Namespace) -> None:
    missing = [str(value) for name, value in vars(args).items() if name.endswith("tool") or name in {"import_harness"}
               if isinstance(value, Path) and not value.resolve().is_file()]
    if missing:
        raise RuntimeError(f"missing selector proof tools: {missing}")


def main() -> int:
    args = parse_args()
    require_tools(args)
    output = (args.output_root or ROOT / "build" / "agent_runs" / "surface_authoring_contract_matrix" / "selector_material_microdetail" / "composed").resolve()
    generated, review = output / "generated", output / "review"
    for name in ("assets/mesh_assets", "bindings", "materials", "graphs", "requests", "raw", "regions", "receipts", "fresh_stl"):
        (generated / name).mkdir(parents=True, exist_ok=True)
    review.mkdir(parents=True, exist_ok=True)
    contract = psg19.load(args.contract)
    contract["render"] = {**contract["render"], "width": 900, "height": 700,
                          "temporal_frames": 1}

    # Fresh source creation mirrors the PSG-24 field proof. The selected carrier
    # is a separate digest-bound region, never a 2-D image mask.
    # The imported-mesh authoring contract has bounded URI storage; keep its
    # transient source path short while copying the immutable runtime asset into
    # the proof root immediately afterward.
    fresh_root = Path("/private/tmp") / f"surface_selector_{os.getpid()}"
    psg19.run([sys.executable, str(args.stl_tool), "create", "--recipe", str(args.recipe), "--out-root", str(fresh_root)])
    asset_id = psg19.load(args.recipe)["asset_id"]
    stl = fresh_root / "curated" / asset_id / "source" / f"{asset_id}.stl"
    psg19.run([str(args.import_harness), "--stl", str(stl), "--out", str(fresh_root / "imported"), "--asset-id", asset_id,
               "--scene-id", "surface_authoring_selector", "--object-id", "selector_statue"])
    mesh = generated / "assets" / "mesh_assets" / f"{asset_id}.runtime.json"
    mesh.write_bytes((fresh_root / "imported" / "assets" / "mesh_assets" / mesh.name).read_bytes())
    base_region = generated / "regions" / "base.region.json"
    base_receipt_path = generated / "receipts" / "base_region.json"
    solid_receipt_path = generated / "receipts" / "solid.json"
    base_receipt = psg19.receipt([str(args.region_tool), "--mesh", str(mesh), "--recipe", str(args.region_recipe), "--out", str(base_region),
                                  "--summary-out", str(base_receipt_path), "--solid-receipt-out", str(solid_receipt_path)])
    # Three continuous authored regions align with the object's distinct stepped
    # forms.  They replace the earlier scattered feature-threshold fixture.
    bands = {"base": (0.05, 0.10), "middle": (0.35, 0.13), "top": (0.80, 0.11)}
    selected_regions, selector_receipts = {}, {}
    for name, (height, half_height) in bands.items():
        recipe = {"schema": "ray_tracing.procedural_imported_surface_region_recipe", "schema_version": 1,
                  "region_id": f"stepped_{name}_band", "source_asset_id": asset_id,
                  "patches": [{"center": [0.5, 0.5, height], "radius": [1.0, 1.0, half_height], "feather": 0.14,
                               "noise_scale": 1.0, "noise_strength": 0.0, "strength": 1.0, "seed": 24100}]}
        recipe_path = generated / "regions" / f"{name}.recipe.json"
        region_path = generated / "regions" / f"{name}.region.json"
        receipt_path = generated / "receipts" / f"{name}.selector.json"
        psg19.write_json(recipe_path, recipe)
        selector_receipts[name] = psg19.receipt([str(args.region_tool), "--mesh", str(mesh), "--recipe", str(recipe_path),
                                                 "--out", str(region_path), "--summary-out", str(receipt_path)])
        selected_regions[name] = region_path

    materials = generated / "materials"
    base = psg19.init_and_edit_material(args.material_tool, materials, "weathered_rock", "base_material", "clean_plaster", [
        "base_color.r=0.78", "base_color.g=0.70", "base_color.b=0.59", "roughness=0.86", "reflectivity=0.025",
        "texture.enabled=false", "texture.microdetail_normal_strength=0.0"])
    overlay = psg19.init_and_edit_material(args.material_tool, materials, "pitted_concrete", "pore_material", "selected_mud_microdetail", [
        "base_color.r=0.18", "base_color.g=0.12", "base_color.b=0.075", "roughness=0.97", "reflectivity=0.01",
        "texture.scale_units=0.04", "texture.strength=0.92", "texture.coverage=0.78", "texture.grain=0.9",
        "texture.contrast=0.82", "texture.microdetail_normal_strength=0.88", "texture.seed=24081"])
    black, white = psg19.init_and_edit_material(args.material_tool, materials, "weathered_rock", "base_material", "selector_mask_black", [
        "base_color.r=0.01", "base_color.g=0.01", "base_color.b=0.01", "roughness=1.0", "reflectivity=0.0", "texture.enabled=false"]), psg19.init_and_edit_material(args.material_tool, materials, "pitted_concrete", "pore_material", "selector_mask_white", [
        "base_color.r=0.99", "base_color.g=0.99", "base_color.b=0.99", "roughness=1.0", "reflectivity=0.0", "texture.enabled=false"])

    bindings = generated / "bindings"
    region_base = bindings / "regions.base.json"
    region_init = psg19.receipt([str(args.region_binding_tool), "init", "--mesh", str(mesh), "--solid-receipt", str(solid_receipt_path),
                                 "--binding-id", "selector_regions", "--fallback", "default", "--out", str(region_base)])
    region_binding = bindings / "regions.json"
    region_applied = psg19.receipt([str(args.region_binding_tool), "apply", "--mesh", str(mesh), "--binding", str(region_base),
                                    "--expected-base-digest", region_init["binding_digest_sha256"], "--set-kind", "retained=default",
                                    "--out", str(region_binding), "--undo-out", str(bindings / "regions.undo.json")])
    authored_base = bindings / "authored.base.json"
    authored_init = psg19.receipt([str(args.authored_binding_tool), "init", "--mesh", str(mesh), "--region-binding", str(region_binding),
                                   "--binding-id", "selector_authored", "--out", str(authored_base)])
    authored = bindings / "authored.json"
    authored_receipt = psg19.receipt([str(args.authored_binding_tool), "apply", "--mesh", str(mesh), "--region-binding", str(region_binding),
                                      "--authored-binding", str(authored_base), "--expected-base-digest", authored_init["binding_digest_sha256"],
                                      "--set-kind", f"retained={base.resolve()}", "--out", str(authored), "--undo-out", str(bindings / "authored.undo.json")])
    graphs = generated / "graphs"
    control_graph, control_receipt = psg19.create_graph(args.graph_tool, graphs, "selector_control", "selector_authored", authored_receipt["binding_digest_sha256"], base, overlay, False)
    selected_graph, selected_receipt = psg19.create_graph(args.graph_tool, graphs, "selector_material_microdetail", "selector_authored", authored_receipt["binding_digest_sha256"], base, overlay, True)
    mask_graph, mask_receipt = psg19.create_graph(args.graph_tool, graphs, "selector_mask", "selector_authored", authored_receipt["binding_digest_sha256"], black, white, True)
    mesh_document = psg19.load(mesh)
    hero = psg19.views(mesh_document)["hero"]
    specs = {"control": (control_graph, base_region)}
    for name, region in selected_regions.items():
        specs[f"{name}_selector_mask"] = (mask_graph, region)
        specs[f"{name}_material_microdetail"] = (selected_graph, region)
    specs["base_material_microdetail_repeat"] = (selected_graph, selected_regions["base"])
    renders = {name: psg19.render(args.render_cli, contract, generated, review, name, graph, hero, region_binding, authored, region, None, False)
               for name, (graph, region) in specs.items()}
    pixels = {name: value[0] for name, value in renders.items()}
    changed = {name: psg19.changed_pixels(pixels["control"], pixels[f"{name}_material_microdetail"]) for name in bands}
    mask_changed = {name: psg19.changed_pixels(pixels["control"], pixels[f"{name}_selector_mask"]) for name in bands}
    repeat_changed = psg19.changed_pixels(pixels["base_material_microdetail"], pixels["base_material_microdetail_repeat"])
    raw_masks, raw_mask_paths = {}, {}
    for name, region in selected_regions.items():
        raw_masks[name] = psg19.diagnostic_projection(mesh_document, psg19.load(region), contract["render"]["width"], contract["render"]["height"], wireframe=False)
        raw_mask_paths[name] = review / f"{name}_selector_carrier.png"
        psg19.review_artifacts.write_png_rgb(raw_mask_paths[name], contract["render"]["width"], contract["render"]["height"], raw_masks[name])
    contact = review / "selector_material_microdetail_matrix.png"
    psg19.write_labeled_contact_sheet(contact, [("CLEAN CONTROL", pixels["control"])] +
        [(f"{name.upper()} SELECTOR", pixels[f"{name}_selector_mask"]) for name in bands] +
        [(f"{name.upper()} MATERIAL + MICRODETAIL", pixels[f"{name}_material_microdetail"]) for name in bands], columns=2)
    audits = {name: value[1] for name, value in renders.items()}
    summaries = {name: psg19.load(generated / "raw" / name / "render_summary.json") for name in specs}
    runtime_digest = summaries["control"]["procedural_solid_material_runtime"]["mesh_digest_sha256"]
    failures = []
    if any(value < 5000 for value in changed.values()) or any(value < 5000 for value in mask_changed.values()):
        failures.append("a selector band produced insufficient visible material or mask signal")
    if repeat_changed != 0:
        failures.append("selected material+microdetail repeat is not deterministic")
    for name, audit in audits.items():
        summary = summaries[name]
        if audit["triangle_count"] != base_receipt["triangle_count"] or audit["primary_hit_pixels"] != audits["control"]["primary_hit_pixels"]:
            failures.append(f"{name}: source topology or hit coverage changed")
        if summary["procedural_solid_material_runtime"]["mesh_digest_sha256"] != runtime_digest or summary["prepared_acceleration"]["active_trace_route"] != "tlas_blas":
            failures.append(f"{name}: runtime mesh or acceleration identity changed")
    report = {"schema": "ray_tracing.surface_authoring_selector_material_microdetail_visual_proof", "schema_version": 1,
              "status": "passed" if not failures else "failed", "authority": {"classification": "local_diagnostic_proof", "promotion_authorized": False, "package_or_release_changed": False},
              "claim": {"demonstrated": ["digest-bound selected carrier gates overlay material and shading-normal microdetail", "unchanged source mesh, triangle count, hit coverage, and TLAS/BLAS route", "deterministic selected composition"],
                        "not_demonstrated": ["geometry displacement", "silhouette displacement", "remeshing", "physical relief", "saved-scene promotion"]},
              "mesh": str(mesh), "mesh_digest_sha256": psg19.digest(mesh), "triangle_count": base_receipt["triangle_count"], "primary_hit_pixels": audits["control"]["primary_hit_pixels"],
              "selector": {"kind": "continuous_authored_stepped_bands", "regions": {name: {"path": str(path), "digest_sha256": psg19.digest(path), "receipt": selector_receipts[name]} for name, path in selected_regions.items()}},
              "bindings": {"region_binding_digest_sha256": region_applied["binding_digest_sha256"], "authored_binding_digest_sha256": authored_receipt["binding_digest_sha256"]},
              "graphs": {"control": control_receipt["graph_digest_sha256"], "selected_material_microdetail": selected_receipt["graph_digest_sha256"], "selector_mask": mask_receipt["graph_digest_sha256"]},
              "images": {**{name: str(value[3]) for name, value in renders.items()}, "raw_selector_carriers": {name: str(path) for name, path in raw_mask_paths.items()}, "contact_sheet": str(contact)},
              "metrics": {"control_to_selected_material_microdetail": {name: {"changed_pixels": value} for name, value in changed.items()}, "control_to_selector_mask": {name: {"changed_pixels": value} for name, value in mask_changed.items()}, "selected_repeat": {"changed_pixels": repeat_changed}}, "failures": failures}
    psg19.write_json(review / "selector_material_microdetail_visual_proof.json", report)
    print(json.dumps(report, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
