#!/usr/bin/env python3
"""Prove one mountain source with material layers and microdetail active together."""

from __future__ import annotations

import json

import procedural_solid_material_graph_visual_proof as psg


def delta_image(left: list, right: list, multiplier: int = 10) -> list:
    return [[tuple(min(255, abs(a[channel] - b[channel]) * multiplier)
                    for channel in range(3))
             for a, b in zip(left_row, right_row)]
            for left_row, right_row in zip(left, right)]


def main() -> int:
    args = psg.parse_args()
    args.contract = args.contract.resolve()
    contract = psg.load(args.contract)
    psg.validate_contract(contract)
    contract["proof_id"] = "surface_authoring_material_microdetail"
    contract["title"] = "Surface Authoring Material + Microdetail"
    contract["render"] = dict(contract["render"])
    contract["render"].update({"width": 1200, "height": 900, "temporal_frames": 12})
    if args.validate_contract_only:
        print(json.dumps({"status": "passed", "proof_id": contract["proof_id"]}, indent=2))
        return 0

    output = (args.output_root or (
        psg.ROOT / "build" / "agent_runs" / "ray_tracing" / "procedural_solid"
        / "surface_authoring_material_microdetail")).resolve()
    generated, review = output / "generated", output / "review"
    for name in ("assets/mesh_assets", "surface_sources", "authored_objects", "receipts",
                 "bindings", "materials", "graphs", "requests", "raw"):
        (generated / name).mkdir(parents=True, exist_ok=True)
    review.mkdir(parents=True, exist_ok=True)
    for name in ("solid_tool", "solid_agent_tool", "surface_agent_tool", "field_tool",
                 "region_binding_tool", "material_tool", "authored_binding_tool",
                 "graph_tool", "debug_tool", "render_cli"):
        setattr(args, name, getattr(args, name).resolve())

    family = next(item for item in contract["families"] if item["id"] == "mountain_snow")
    rock_control = generated / "materials" / "rock.control.json"
    rock_receipt = psg.init_material(args.material_tool, "weathered_rock", "base_material", rock_control)
    snow_control = generated / "materials" / "snow.control.json"
    snow_receipt = psg.init_material(args.material_tool, "snow", "snow_material", snow_control)
    rock_detail = generated / "materials" / "rock.microdetail.json"
    psg.edit_material(args.material_tool, rock_control, rock_receipt, rock_detail,
                      ["texture.microdetail_normal_strength=0.82"])
    snow_detail = generated / "materials" / "snow.microdetail.json"
    psg.edit_material(args.material_tool, snow_control, snow_receipt, snow_detail,
                      ["texture.microdetail_normal_strength=0.58"])

    mesh, solid, region_binding, authored, authored_receipt, creation = psg.create_mesh_and_bindings(
        args, generated, family, contract, rock_control)
    control_graph, control_receipt = psg.create_graph(
        args, generated, "mountain_snow.control", "mountain_snow_authored",
        family["template"], authored_receipt, [rock_control, snow_control])
    material_graph = generated / "graphs" / "mountain_snow.material.json"
    material_receipt = psg.edit_graph(args.graph_tool, control_graph, control_receipt, material_graph,
                                      [("snowline", "minimum", "0.28"),
                                       ("snowline", "maximum", "0.48")])
    microdetail_graph, microdetail_receipt = psg.create_graph(
        args, generated, "mountain_snow.microdetail", "mountain_snow_authored",
        family["template"], authored_receipt, [rock_detail, snow_detail])
    combined_base, combined_base_receipt = psg.create_graph(
        args, generated, "mountain_snow.combined_base", "mountain_snow_authored",
        family["template"], authored_receipt, [rock_detail, snow_detail])
    combined_graph = generated / "graphs" / "mountain_snow.combined.json"
    combined_receipt = psg.edit_graph(args.graph_tool, combined_base, combined_base_receipt,
                                      combined_graph,
                                      [("snowline", "minimum", "0.28"),
                                       ("snowline", "maximum", "0.48")])

    variants = {
        "control": control_graph,
        "material_only": material_graph,
        "microdetail_only": microdetail_graph,
        "material_microdetail": combined_graph,
        "material_microdetail_repeat": combined_graph,
    }
    renders = {}
    for name, graph in variants.items():
        renders[name] = psg.render_variant(args, generated, review, contract, family["id"], name,
                                           solid, region_binding, authored, graph)
    pixels = {name: item[0] for name, item in renders.items()}
    audits = {name: item[1] for name, item in renders.items()}
    metrics = {name: item[2] for name, item in renders.items()}
    images = {name: str(item[3]) for name, item in renders.items()}
    summaries = {name: psg.load(generated / "raw" / f"{family['id']}.{name}" / "render_summary.json")
                 for name in variants}
    deltas = {
        "control_to_material": psg.pixel_delta(pixels["control"], pixels["material_only"]),
        "control_to_microdetail": psg.pixel_delta(pixels["control"], pixels["microdetail_only"]),
        "material_to_combined": psg.pixel_delta(pixels["material_only"], pixels["material_microdetail"]),
        "microdetail_to_combined": psg.pixel_delta(pixels["microdetail_only"], pixels["material_microdetail"]),
        "combined_repeat": psg.pixel_delta(pixels["material_microdetail"], pixels["material_microdetail_repeat"]),
    }
    effect = review / "mountain_snow.material_microdetail_effect_10x.png"
    psg.review_artifacts.write_png_rgb(effect, len(pixels["material_microdetail"][0]),
                                       len(pixels["material_microdetail"]),
                                       delta_image(pixels["control"], pixels["material_microdetail"]))
    contact = review / "material_microdetail_matrix.png"
    psg.write_labeled_contact_sheet(contact, [
        ("CONTROL", pixels["control"]),
        ("MATERIAL LAYERS", pixels["material_only"]),
        ("MICRODETAIL NORMAL", pixels["microdetail_only"]),
        ("MATERIAL + MICRODETAIL", pixels["material_microdetail"]),
        ("COMBINED EFFECT - 10X", delta_image(pixels["control"], pixels["material_microdetail"])),
    ], columns=3)

    failures: list[str] = []
    runtime_mesh_digest = summaries["control"].get(
        "procedural_solid_material_runtime", {}).get("mesh_digest_sha256")
    for name, audit in audits.items():
        if audit["triangle_count"] != solid["triangle_count"]:
            failures.append(f"{name}: triangle identity changed")
        if audit["primary_hit_pixels"] != audits["control"]["primary_hit_pixels"]:
            failures.append(f"{name}: silhouette or hit coverage changed")
        summary = summaries[name]
        if (summary.get("procedural_solid_material_runtime", {}).get("mesh_digest_sha256") != runtime_mesh_digest
                or summary.get("prepared_acceleration", {}).get("active_trace_route") != "tlas_blas"):
            failures.append(f"{name}: mesh or acceleration identity changed")
    for name in ("control_to_material", "control_to_microdetail",
                 "material_to_combined", "microdetail_to_combined"):
        if deltas[name]["changed_pixels"] < 5000:
            failures.append(f"{name}: insufficient visible signal")
    if deltas["combined_repeat"]["changed_pixels"] != 0:
        failures.append("combined repeat is not deterministic")

    report = {
        "schema": "ray_tracing.surface_authoring_material_microdetail_visual_proof",
        "schema_version": 1,
        "status": "passed" if not failures else "failed",
        "authority": {"classification": "local_diagnostic_proof", "promotion_authorized": False,
                      "package_or_release_changed": False},
        "claim": {"demonstrated": ["material-layer and shading-normal microdetail composition",
                                     "unchanged source mesh, triangle count, hit coverage, and TLAS/BLAS route",
                                     "deterministic combined render"],
                  "not_demonstrated": ["geometry displacement", "silhouette displacement", "remeshing",
                                        "physical relief", "saved-scene promotion"]},
        "render": contract["render"], "object_creation": creation,
        "mesh": str(mesh), "mesh_digest_sha256": psg.digest(mesh),
        "triangle_count": solid["triangle_count"],
        "primary_hit_pixels": audits["control"]["primary_hit_pixels"],
        "graphs": {"control": control_receipt["graph_digest_sha256"],
                   "material_only": material_receipt["graph_digest_sha256"],
                   "microdetail_only": microdetail_receipt["graph_digest_sha256"],
                   "material_microdetail": combined_receipt["graph_digest_sha256"]},
        "images": {**images, "contact_sheet": str(contact), "combined_effect_10x": str(effect)},
        "metrics": {**deltas, "luma_standard_deviation": metrics}, "failures": failures,
    }
    psg.write_json(review / "material_microdetail_visual_proof.json", report)
    print(json.dumps(report, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
