#!/usr/bin/env python3
"""Generate PSG-13 retained/cut/blend material-binding render proof."""

from __future__ import annotations

import argparse
import json
import platform
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_DIR = ROOT / "tests" / "integration"
if str(INTEGRATION_DIR) not in sys.path:
    sys.path.insert(0, str(INTEGRATION_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
from procedural_solid_visual_proof import framed_views  # noqa: E402
from procedural_surface_visual_proof import (  # noqa: E402
    image_metrics,
    object_audit,
    render_request,
    run_render_cli,
    write_json,
    write_labeled_contact_sheet,
)


FIXTURE_ROOT = ROOT / "tests" / "fixtures" / "procedural_solid_graphs"


def default_tool(name: str) -> Path:
    return (
        ROOT / "build" / "toolchains" / "clang" / platform.machine()
        / "tools" / "cli" / name
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--solid-tool",
        type=Path,
        default=default_tool("procedural_solid_asset_tool"),
    )
    parser.add_argument(
        "--material-agent-tool",
        type=Path,
        default=default_tool("procedural_solid_material_agent_tool"),
    )
    parser.add_argument(
        "--render-cli",
        type=Path,
        default=default_tool("ray_tracing_render_headless"),
    )
    parser.add_argument("--output-root", type=Path)
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def run_checked(command: list[str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )
    return result


def make_scene(
    scene_id: str,
    asset_id: str,
    binding_path: Path | None,
) -> dict:
    mesh_object = {
        "object_id": f"{asset_id}_object",
        "object_type": "mesh_asset_instance",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": {"x": 0.0, "y": 0.0, "z": 0.0},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "geometry_ref": {"kind": "mesh_asset", "id": asset_id},
        "material_ref": {"id": "mat_control"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }
    if binding_path is not None:
        mesh_object["procedural_solid_material_ref"] = {
            "binding_path": str(binding_path),
        }
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": scene_id,
        "source_scene_id": scene_id,
        "compile_meta": {
            "compiler_version": "procedural_solid_material_visual_proof_psg13",
            "compiled_at_ns": 0,
            "normalization": "local_diagnostic",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [mesh_object],
        "materials": [{
            "id": "mat_control",
            "name": "PSG-13 geometry control",
            "base_color": {"r": 0.46, "g": 0.52, "b": 0.58},
            "roughness": 0.72,
            "metallic": 0.0,
        }],
        "lights": [],
        "extensions": {},
    }


def changed_pixel_count(left: list, right: list) -> int:
    return sum(
        left_pixel != right_pixel
        for left_row, right_row in zip(left, right)
        for left_pixel, right_pixel in zip(left_row, right_row)
    )


def main() -> int:
    args = parse_args()
    output_root = (
        args.output_root
        if args.output_root
        else ROOT / "build" / "agent_runs" / "ray_tracing"
        / "procedural_solid" / "psg13_material_binding"
    ).resolve()
    solid_tool = args.solid_tool.resolve()
    material_agent = args.material_agent_tool.resolve()
    render_cli = args.render_cli.resolve()
    generated = output_root / "generated"
    assets = generated / "assets" / "mesh_assets"
    receipts = generated / "receipts"
    bindings = generated / "bindings"
    review = output_root / "review"
    raw_runs = output_root / "raw_runs"
    for directory in (assets, receipts, bindings, review, raw_runs):
        directory.mkdir(parents=True, exist_ok=True)

    cases = [
        {
            "id": "rounded_block_with_tunnel",
            "label": "Tunnel cut",
            "graph": "rounded_block_with_tunnel.json",
            "assignments": ["retained=rough_metal", "cut=emissive"],
            "required_kind": "cut",
            "required_count": "cut_triangle_count",
        },
        {
            "id": "blended_double_sphere",
            "label": "Smooth blend",
            "graph": "blended_double_sphere.json",
            "assignments": ["retained=rough_metal", "blend=glossy"],
            "required_kind": "blend",
            "required_count": "blend_triangle_count",
        },
    ]
    render_contract = {
        "render": {
            "width": 360,
            "height": 270,
            "temporal_frames": 1,
            "integrator_3d": "disney_v2",
            "camera_zoom": 1.12,
        },
        "lighting": {
            "environment_light_mode": "ambient",
            "ambient_strength": 0.38,
            "top_fill_strength": 1.3,
            "light_intensity": 4.2,
            "light_radius": 0.14,
        },
    }
    failures: list[str] = []
    results: list[dict] = []
    cells = []

    for case in cases:
        asset_id = f"psg13_{case['id']}"
        mesh_path = assets / f"{asset_id}.runtime.json"
        solid_receipt_path = receipts / f"{case['id']}.solid.json"
        run_checked([
            str(solid_tool),
            "--graph", str(FIXTURE_ROOT / case["graph"]),
            "--out", str(mesh_path),
            "--summary-out", str(solid_receipt_path),
            "--asset-id", asset_id,
            "--cells", "20",
            "--local-adaptive",
            "--maximum-cells", "40",
            "--feature-size", "0.18",
            "--collision-authority", "derived_shell",
        ])
        solid_receipt = load_json(solid_receipt_path)
        if (
            solid_receipt["boundary_edge_count"] != 0
            or solid_receipt["nonmanifold_edge_count"] != 0
            or solid_receipt["connected_component_count"] != 1
            or solid_receipt["signed_volume_units3"] <= 0.0
            or solid_receipt[case["required_count"]] <= 0
        ):
            failures.append(f"{case['id']}: solid topology/region contract failed")

        base_binding = bindings / f"{case['id']}.base.json"
        init_receipt = json.loads(run_checked([
            str(material_agent),
            "init",
            "--mesh", str(mesh_path),
            "--solid-receipt", str(solid_receipt_path),
            "--binding-id", f"{asset_id}_materials",
            "--fallback", "default",
            "--out", str(base_binding),
        ]).stdout)
        edited_binding = bindings / f"{case['id']}.json"
        undo_binding = bindings / f"{case['id']}.undo.json"
        apply_command = [
            str(material_agent),
            "apply",
            "--mesh", str(mesh_path),
            "--binding", str(base_binding),
            "--expected-base-digest", init_receipt["binding_digest_sha256"],
        ]
        for assignment in case["assignments"]:
            apply_command.extend(["--set-kind", assignment])
        apply_command.extend([
            "--out", str(edited_binding),
            "--undo-out", str(undo_binding),
        ])
        binding_receipt = json.loads(run_checked(apply_command).stdout)
        region_materials = {
            region["kind"]: region["material"]
            for region in binding_receipt["regions"]
        }
        if (
            binding_receipt["mesh_digest_sha256"]
            != solid_receipt["mesh_digest_sha256"]
            or binding_receipt["region_digest_sha256"]
            != solid_receipt["region_digest_sha256"]
            or case["required_kind"] not in region_materials
        ):
            failures.append(f"{case['id']}: binding identity contract failed")

        all_views = framed_views(solid_receipt)
        views = (
            [all_views[0], all_views[2]]
            if case["id"] == "rounded_block_with_tunnel"
            else all_views[:2]
        )
        if case["id"] == "rounded_block_with_tunnel":
            mixed_scene = make_scene(
                "psg13_mixed_binding_rejection",
                asset_id,
                edited_binding.resolve(),
            )
            mixed_object = dict(mixed_scene["objects"][0])
            mixed_object["object_id"] = f"{asset_id}_unbound_object"
            mixed_object.pop("procedural_solid_material_ref", None)
            mixed_scene["objects"].append(mixed_object)
            mixed_scene_path = generated / "mixed_binding.scene.json"
            mixed_request_path = generated / "mixed_binding.request.json"
            mixed_run_root = raw_runs / "mixed_binding_rejection"
            mixed_summary_path = mixed_run_root / "render_summary.json"
            write_json(mixed_scene_path, mixed_scene)
            write_json(
                mixed_request_path,
                render_request(
                    "psg13_mixed_binding_rejection",
                    views[0],
                    mixed_scene_path,
                    mixed_request_path,
                    mixed_run_root,
                    render_contract,
                ),
            )
            mixed_result = subprocess.run(
                [
                    str(render_cli),
                    "--request", str(mixed_request_path),
                    "--render",
                    "--summary", str(mixed_summary_path),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            if mixed_result.returncode == 0:
                failures.append(
                    "mixed bound/unbound instances were not rejected"
                )
            results.append({
                "case": case["id"],
                "comparison": "mixed_binding_rejection",
                "rejected": mixed_result.returncode != 0,
            })
        variant_pixels: dict[str, dict[str, list]] = {
            "control": {},
            "bound": {},
        }
        for variant, binding_path in (
            ("control", None),
            ("bound", edited_binding.resolve()),
        ):
            scene_path = generated / f"{case['id']}.{variant}.scene.json"
            write_json(
                scene_path,
                make_scene(
                    f"psg13_{case['id']}_{variant}",
                    asset_id,
                    binding_path,
                ),
            )
            for view in views:
                cell_id = f"{case['id']}_{variant}_{view['id']}"
                run_root = raw_runs / cell_id
                request_path = generated / f"{cell_id}.request.json"
                summary_path = run_root / "render_summary.json"
                write_json(
                    request_path,
                    render_request(
                        "psg13_region_material_binding",
                        view,
                        scene_path,
                        request_path,
                        run_root,
                        render_contract,
                    ),
                )
                run_render_cli(render_cli, request_path, summary_path)
                render_summary = load_json(summary_path)
                audit = object_audit(
                    render_summary, f"{asset_id}_object"
                )
                material_runtime = render_summary[
                    "procedural_solid_material_runtime"
                ]
                frame = run_root / "frames" / "frame_0000.bmp"
                width, height, pixels = review_artifacts.read_bmp_rgb(frame)
                png = review / f"{cell_id}.png"
                review_artifacts.write_png_rgb(png, width, height, pixels)
                metrics = image_metrics(pixels)
                variant_pixels[variant][view["id"]] = pixels
                if audit["triangle_count"] != solid_receipt["triangle_count"]:
                    failures.append(f"{cell_id}: runtime triangle parity failed")
                if audit["primary_hit_pixels"] < 1500:
                    failures.append(f"{cell_id}: insufficient visible coverage")
                if metrics["luma_standard_deviation"] < 7.0:
                    failures.append(f"{cell_id}: insufficient form contrast")
                if variant == "bound" and (
                    not material_runtime["loaded"]
                    or material_runtime["asset_count"] != 1
                    or material_runtime["region_count"]
                    != binding_receipt["region_count"]
                    or material_runtime["bound_triangle_count"]
                    != solid_receipt["triangle_count"]
                    or material_runtime["binding_digest_sha256"]
                    != binding_receipt["binding_digest_sha256"]
                    or material_runtime["mesh_digest_sha256"]
                    != solid_receipt["mesh_digest_sha256"]
                    or material_runtime["region_digest_sha256"]
                    != solid_receipt["region_digest_sha256"]
                ):
                    failures.append(f"{cell_id}: runtime binding receipt failed")
                if variant == "control" and material_runtime["loaded"]:
                    failures.append(f"{cell_id}: control unexpectedly bound")
                cells.append((
                    f"{case['label']} - {variant.title()} - {view['label']}",
                    pixels,
                ))
                results.append({
                    "case": case["id"],
                    "variant": variant,
                    "view": view["id"],
                    "triangle_count": audit["triangle_count"],
                    "primary_hit_pixels": audit["primary_hit_pixels"],
                    "luma_standard_deviation":
                        metrics["luma_standard_deviation"],
                    "image": str(png),
                })

        for view in views:
            changed = changed_pixel_count(
                variant_pixels["control"][view["id"]],
                variant_pixels["bound"][view["id"]],
            )
            if changed < 500:
                failures.append(
                    f"{case['id']}_{view['id']}: material binding not visible"
                )
            results.append({
                "case": case["id"],
                "comparison": f"control_to_bound_{view['id']}",
                "changed_pixels": changed,
            })

    contact = review / "procedural_solid_psg13_material_binding.png"
    write_labeled_contact_sheet(contact, cells, columns=4)
    summary = {
        "schema": "ray_tracing.procedural_solid_material_visual_proof_psg13",
        "schema_version": 1,
        "passed": not failures,
        "authority": "local_diagnostic_only",
        "cases": cases,
        "views": results,
        "contact_sheet": str(contact),
        "failures": failures,
    }
    write_json(output_root / "proof_summary.json", summary)
    (output_root / "index.md").write_text(
        "\n".join([
            "# PSG-13 region material binding proof",
            "",
            "The same closed solid meshes are rendered first with the scene "
            "fallback material and then with digest-bound retained/cut/blend "
            "region assignments. Mesh and region identities remain unchanged.",
            "",
            f"![PSG-13 material binding proof]({contact})",
            "",
            "Local diagnostic proof only; no saved scene, package, version, "
            "release, or promotion state changed.",
            "",
        ]),
        encoding="utf-8",
    )
    print(json.dumps(summary, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
