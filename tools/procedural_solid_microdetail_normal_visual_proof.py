#!/usr/bin/env python3
"""Generate the high-resolution PSG-17 microdetail-normal proof."""

from __future__ import annotations

import json
from pathlib import Path

import procedural_solid_material_graph_visual_proof as psg


def amplified_delta(left: list, right: list) -> list:
    return [
        [
            (
                min(255, abs(a[0] - b[0]) * 10),
                min(255, abs(a[1] - b[1]) * 10),
                min(255, abs(a[2] - b[2]) * 10),
            )
            for a, b in zip(left_row, right_row)
        ]
        for left_row, right_row in zip(left, right)
    ]


def main() -> int:
    args = psg.parse_args()
    args.contract = args.contract.resolve()
    contract = psg.load(args.contract)
    psg.validate_contract(contract)
    contract["proof_id"] = "psg17_microdetail_normal"
    contract["title"] = "PSG-17 Microdetail Normal"
    contract["render"] = dict(contract["render"])
    contract["render"].update({
        "width": 1200,
        "height": 900,
        "temporal_frames": 12,
    })
    if args.validate_contract_only:
        print(json.dumps({
            "status": "passed",
            "proof_id": contract["proof_id"],
            "resolution": [
                contract["render"]["width"],
                contract["render"]["height"],
            ],
        }, indent=2))
        return 0

    output = (args.output_root or (
        psg.ROOT / "build" / "agent_runs" / "ray_tracing"
        / "procedural_solid" / "psg17_microdetail_normal")).resolve()
    generated = output / "generated"
    review = output / "review"
    for name in (
        "assets/mesh_assets", "surface_sources", "authored_objects",
        "receipts", "bindings", "materials", "graphs", "requests", "raw",
    ):
        (generated / name).mkdir(parents=True, exist_ok=True)
    review.mkdir(parents=True, exist_ok=True)
    for name in (
        "solid_tool", "solid_agent_tool", "surface_agent_tool", "field_tool",
        "region_binding_tool", "material_tool", "authored_binding_tool",
        "graph_tool", "debug_tool", "render_cli",
    ):
        setattr(args, name, getattr(args, name).resolve())

    family = next(
        item for item in contract["families"]
        if item["id"] == "mountain_snow")
    rock_base = generated / "materials" / "rock.control.json"
    rock_receipt = psg.init_material(
        args.material_tool, "weathered_rock", "base_material", rock_base)
    snow_base = generated / "materials" / "snow.control.json"
    snow_receipt = psg.init_material(
        args.material_tool, "snow", "snow_material", snow_base)
    rock_detail = generated / "materials" / "rock.microdetail.json"
    psg.edit_material(
        args.material_tool, rock_base, rock_receipt, rock_detail,
        ["texture.microdetail_normal_strength=0.82"])
    snow_detail = generated / "materials" / "snow.microdetail.json"
    psg.edit_material(
        args.material_tool, snow_base, snow_receipt, snow_detail,
        ["texture.microdetail_normal_strength=0.58"])

    mesh, solid, region_binding, authored, authored_receipt, creation = (
        psg.create_mesh_and_bindings(
            args, generated, family, contract, rock_base))
    control_graph, control_graph_receipt = psg.create_graph(
        args, generated, "mountain_snow.control",
        "mountain_snow_authored", family["template"], authored_receipt,
        [rock_base, snow_base])
    detail_graph, detail_graph_receipt = psg.create_graph(
        args, generated, "mountain_snow.microdetail",
        "mountain_snow_authored", family["template"], authored_receipt,
        [rock_detail, snow_detail])

    control_pixels, control_audit, control_metrics, control_png = (
        psg.render_variant(
            args, generated, review, contract, family["id"], "control",
            solid, region_binding, authored, control_graph))
    detail_pixels, detail_audit, detail_metrics, detail_png = (
        psg.render_variant(
            args, generated, review, contract, family["id"], "microdetail",
            solid, region_binding, authored, detail_graph))
    repeat_pixels, repeat_audit, _, repeat_png = psg.render_variant(
        args, generated, review, contract, family["id"], "microdetail_repeat",
        solid, region_binding, authored, detail_graph)

    effect_pixels = amplified_delta(control_pixels, detail_pixels)
    effect_png = review / "mountain_snow.microdetail_effect_10x.png"
    psg.review_artifacts.write_png_rgb(
        effect_png, len(effect_pixels[0]), len(effect_pixels), effect_pixels)
    matrix = review / "psg17_microdetail_normal_comparison.png"
    psg.write_labeled_contact_sheet(matrix, [
        ("CONTROL - NORMAL OFF", control_pixels),
        ("MICRODETAIL NORMAL ON", detail_pixels),
        ("SHADING EFFECT - 10X", effect_pixels),
    ], columns=3)

    control_summary = psg.load(
        generated / "raw" / "mountain_snow.control" /
        "render_summary.json")
    detail_summary = psg.load(
        generated / "raw" / "mountain_snow.microdetail" /
        "render_summary.json")
    delta = psg.pixel_delta(control_pixels, detail_pixels)
    repeat_delta = psg.pixel_delta(detail_pixels, repeat_pixels)
    failures: list[str] = []
    if delta["changed_pixels"] < 5000:
        failures.append("microdetail normal did not produce enough shading signal")
    if repeat_delta["changed_pixels"] != 0:
        failures.append("microdetail normal render is not deterministic")
    if (
        control_audit["triangle_count"] != detail_audit["triangle_count"] or
        control_audit["triangle_count"] != repeat_audit["triangle_count"] or
        control_audit["primary_hit_pixels"] !=
            detail_audit["primary_hit_pixels"] or
        control_audit["primary_hit_pixels"] !=
            repeat_audit["primary_hit_pixels"]
    ):
        failures.append("triangle or silhouette identity changed")
    if (
        control_summary.get("procedural_solid_material_runtime", {}).get(
            "mesh_digest_sha256") !=
        detail_summary.get("procedural_solid_material_runtime", {}).get(
            "mesh_digest_sha256") or
        control_summary.get("prepared_acceleration", {}).get(
            "active_trace_route") != "tlas_blas" or
        detail_summary.get("prepared_acceleration", {}).get(
            "active_trace_route") != "tlas_blas"
    ):
        failures.append("mesh or acceleration identity changed")

    report = {
        "schema": "ray_tracing.procedural_solid_psg17_visual_proof",
        "schema_version": 1,
        "status": "passed" if not failures else "failed",
        "authority": {
            "classification": "local_diagnostic_proof",
            "promotion_authorized": False,
            "package_or_release_changed": False,
        },
        "claim": {
            "demonstrated": [
                "explicit weighted texture microdetail normal control",
                "deterministic shading-only normal perturbation",
                "unchanged mesh digest, triangle count, silhouette pixel count, "
                "and TLAS/BLAS trace route",
            ],
            "not_demonstrated": [
                "geometry displacement", "silhouette displacement",
                "remeshing", "closed-shell deformation",
            ],
        },
        "render": contract["render"],
        "object_creation": creation,
        "mesh": str(mesh),
        "mesh_digest_sha256": psg.digest(mesh),
        "runtime_mesh_digest_sha256":
            control_summary.get(
                "procedural_solid_material_runtime", {}).get(
                    "mesh_digest_sha256"),
        "triangle_count": control_audit["triangle_count"],
        "primary_hit_pixels": control_audit["primary_hit_pixels"],
        "active_trace_route":
            control_summary.get("prepared_acceleration", {}).get(
                "active_trace_route"),
        "control_graph_digest_sha256":
            control_graph_receipt["graph_digest_sha256"],
        "microdetail_graph_digest_sha256":
            detail_graph_receipt["graph_digest_sha256"],
        "images": {
            "comparison": str(matrix),
            "control": str(control_png),
            "microdetail": str(detail_png),
            "microdetail_repeat": str(repeat_png),
            "shading_effect_10x": str(effect_png),
        },
        "metrics": {
            "control_luma_standard_deviation":
                control_metrics["luma_standard_deviation"],
            "microdetail_luma_standard_deviation":
                detail_metrics["luma_standard_deviation"],
            "control_to_microdetail": delta,
            "microdetail_repeat": repeat_delta,
        },
        "failures": failures,
    }
    psg.write_json(review / "psg17_visual_proof.json", report)
    print(json.dumps(report, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
