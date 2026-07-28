#!/usr/bin/env python3
"""Generate PSG-14 native render proof for editable authored materials."""

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


FIXTURE = (
    ROOT / "tests" / "fixtures" / "procedural_solid_graphs"
    / "rounded_block_with_tunnel.json"
)


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
        "--region-binding-tool",
        type=Path,
        default=default_tool("procedural_solid_material_agent_tool"),
    )
    parser.add_argument(
        "--material-tool",
        type=Path,
        default=default_tool("procedural_solid_authored_material_agent_tool"),
    )
    parser.add_argument(
        "--authored-binding-tool",
        type=Path,
        default=default_tool("procedural_solid_authored_binding_agent_tool"),
    )
    parser.add_argument(
        "--render-cli",
        type=Path,
        default=default_tool("ray_tracing_render_headless"),
    )
    parser.add_argument("--output-root", type=Path)
    return parser.parse_args()


def run_checked(command: list[str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )
    return result


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def make_scene(
    asset_id: str,
    region_binding: Path,
    authored_binding: Path | None,
) -> dict:
    material_ref = {"binding_path": str(region_binding)}
    if authored_binding is not None:
        material_ref["authored_binding_path"] = str(authored_binding)
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"psg14_{asset_id}",
        "source_scene_id": f"psg14_{asset_id}",
        "compile_meta": {
            "compiler_version":
                "procedural_solid_authored_material_visual_proof_psg14",
            "compiled_at_ns": 0,
            "normalization": "local_diagnostic",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [{
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
            "procedural_solid_material_ref": material_ref,
            "flags": {"visible": True, "locked": False, "selectable": True},
        }],
        "materials": [{
            "id": "mat_control",
            "name": "PSG-14 preset fallback",
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


def init_material(
    tool: Path,
    template_id: str,
    material_id: str,
    path: Path,
) -> dict:
    return json.loads(run_checked([
        str(tool), "init",
        "--template", template_id,
        "--material-id", material_id,
        "--out", str(path),
    ]).stdout)


def edit_material(
    tool: Path,
    source: Path,
    source_receipt: dict,
    output: Path,
    undo: Path,
    edits: list[str],
) -> dict:
    command = [
        str(tool), "apply",
        "--material", str(source),
        "--expected-base-digest",
        source_receipt["material_digest_sha256"],
    ]
    for edit in edits:
        command.extend(["--set", edit])
    command.extend(["--out", str(output), "--undo-out", str(undo)])
    return json.loads(run_checked(command).stdout)


def author_binding(
    tool: Path,
    mesh: Path,
    region_binding: Path,
    binding_id: str,
    materials: list[tuple[str, Path]],
    output_root: Path,
) -> tuple[Path, dict]:
    base = output_root / f"{binding_id}.base.json"
    initialized = json.loads(run_checked([
        str(tool), "init",
        "--mesh", str(mesh),
        "--region-binding", str(region_binding),
        "--binding-id", binding_id,
        "--out", str(base),
    ]).stdout)
    output = output_root / f"{binding_id}.json"
    command = [
        str(tool), "apply",
        "--mesh", str(mesh),
        "--region-binding", str(region_binding),
        "--authored-binding", str(base),
        "--expected-base-digest", initialized["binding_digest_sha256"],
    ]
    for kind, path in materials:
        command.extend(["--set-kind", f"{kind}={path.resolve()}"])
    command.extend([
        "--out", str(output),
        "--undo-out", str(output_root / f"{binding_id}.undo.json"),
    ])
    return output, json.loads(run_checked(command).stdout)


def main() -> int:
    args = parse_args()
    output_root = (
        args.output_root
        if args.output_root
        else ROOT / "build" / "agent_runs" / "ray_tracing"
        / "procedural_solid" / "psg14_authored_materials"
    ).resolve()
    generated = output_root / "generated"
    assets = generated / "assets" / "mesh_assets"
    bindings = generated / "bindings"
    materials = generated / "materials"
    receipts = generated / "receipts"
    raw_runs = output_root / "raw_runs"
    review = output_root / "review"
    for directory in (
        assets, bindings, materials, receipts, raw_runs, review
    ):
        directory.mkdir(parents=True, exist_ok=True)

    solid_tool = args.solid_tool.resolve()
    region_tool = args.region_binding_tool.resolve()
    material_tool = args.material_tool.resolve()
    authored_tool = args.authored_binding_tool.resolve()
    render_cli = args.render_cli.resolve()
    asset_id = "psg14_rounded_tunnel"
    mesh = assets / f"{asset_id}.runtime.json"
    solid_receipt_path = receipts / "solid.json"
    run_checked([
        str(solid_tool),
        "--graph", str(FIXTURE),
        "--out", str(mesh),
        "--summary-out", str(solid_receipt_path),
        "--asset-id", asset_id,
        "--cells", "20",
        "--local-adaptive",
        "--maximum-cells", "40",
        "--feature-size", "0.18",
        "--collision-authority", "derived_shell",
    ])
    solid_receipt = load_json(solid_receipt_path)

    region_base = bindings / "regions.base.json"
    region_init = json.loads(run_checked([
        str(region_tool), "init",
        "--mesh", str(mesh),
        "--solid-receipt", str(solid_receipt_path),
        "--binding-id", "psg14_regions",
        "--fallback", "default",
        "--out", str(region_base),
    ]).stdout)
    region_binding = bindings / "regions.json"
    region_receipt = json.loads(run_checked([
        str(region_tool), "apply",
        "--mesh", str(mesh),
        "--binding", str(region_base),
        "--expected-base-digest", region_init["binding_digest_sha256"],
        "--set-kind", "retained=rough_metal",
        "--set-kind", "cut=glossy",
        "--out", str(region_binding),
        "--undo-out", str(bindings / "regions.undo.json"),
    ]).stdout)

    concrete_default = materials / "concrete.default.json"
    crystal_default = materials / "crystal.default.json"
    concrete_init = init_material(
        material_tool, "pitted_concrete", "concrete_default",
        concrete_default,
    )
    crystal_init = init_material(
        material_tool, "emissive_crystal", "crystal_default",
        crystal_default,
    )
    concrete_edited = materials / "concrete.edited.json"
    crystal_edited = materials / "crystal.edited.json"
    edit_material(
        material_tool, concrete_default, concrete_init,
        concrete_edited, materials / "concrete.undo.json",
        [
            "base_color.r=0.16",
            "base_color.g=0.21",
            "base_color.b=0.27",
            "roughness=0.94",
            "texture.scale_units=0.035",
            "texture.surface_damage=0.92",
            "texture.color_depth=0.62",
        ],
    )
    edit_material(
        material_tool, crystal_default, crystal_init,
        crystal_edited, materials / "crystal.undo.json",
        [
            "base_color.r=0.12",
            "base_color.g=0.48",
            "base_color.b=0.82",
            "emission.color.r=0.08",
            "emission.color.g=0.42",
            "emission.color.b=1.0",
            "emission.strength=0.72",
            "texture.scale_units=0.11",
        ],
    )
    authored_default, authored_default_receipt = author_binding(
        authored_tool, mesh, region_binding, "authored_default",
        [("retained", concrete_default), ("cut", crystal_default)],
        bindings,
    )
    authored_edited, authored_edited_receipt = author_binding(
        authored_tool, mesh, region_binding, "authored_edited",
        [("retained", concrete_edited), ("cut", crystal_edited)],
        bindings,
    )

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
    all_views = framed_views(solid_receipt)
    views = [all_views[0], all_views[2]]
    variants = [
        ("preset", None, None),
        ("authored_default", authored_default, authored_default_receipt),
        ("authored_edited", authored_edited, authored_edited_receipt),
    ]
    failures: list[str] = []
    results: list[dict] = []
    pixels_by_variant: dict[str, dict[str, list]] = {}
    cells = []
    for variant, authored_binding, authored_receipt in variants:
        scene_path = generated / f"{variant}.scene.json"
        write_json(
            scene_path,
            make_scene(asset_id, region_binding.resolve(),
                       authored_binding.resolve()
                       if authored_binding is not None else None),
        )
        pixels_by_variant[variant] = {}
        for view in views:
            cell_id = f"{variant}_{view['id']}"
            run_root = raw_runs / cell_id
            request_path = generated / f"{cell_id}.request.json"
            summary_path = run_root / "render_summary.json"
            write_json(
                request_path,
                render_request(
                    "psg14_authored_materials",
                    view,
                    scene_path,
                    request_path,
                    run_root,
                    render_contract,
                ),
            )
            run_render_cli(render_cli, request_path, summary_path)
            summary = load_json(summary_path)
            audit = object_audit(summary, f"{asset_id}_object")
            runtime = summary["procedural_solid_material_runtime"]
            authored_runtime = runtime["authored_materials"]
            frame = run_root / "frames" / "frame_0000.bmp"
            width, height, pixels = review_artifacts.read_bmp_rgb(frame)
            png = review / f"{cell_id}.png"
            review_artifacts.write_png_rgb(png, width, height, pixels)
            metrics = image_metrics(pixels)
            pixels_by_variant[variant][view["id"]] = pixels
            if audit["triangle_count"] != solid_receipt["triangle_count"]:
                failures.append(f"{cell_id}: runtime triangle parity failed")
            if audit["primary_hit_pixels"] < 1500:
                failures.append(f"{cell_id}: insufficient visible coverage")
            if metrics["luma_standard_deviation"] < 7.0:
                failures.append(f"{cell_id}: insufficient form contrast")
            if runtime["binding_digest_sha256"] != (
                region_receipt["binding_digest_sha256"]
            ):
                failures.append(f"{cell_id}: region binding digest drifted")
            if authored_receipt is None:
                if authored_runtime["loaded"]:
                    failures.append(
                        f"{cell_id}: preset control unexpectedly authored"
                    )
            elif (
                not authored_runtime["loaded"]
                or authored_runtime["asset_count"] != 1
                or authored_runtime["region_count"]
                    != authored_receipt["assignment_count"]
                or authored_runtime["bound_triangle_count"]
                    != solid_receipt["triangle_count"]
                or authored_runtime["binding_digest_sha256"]
                    != authored_receipt["binding_digest_sha256"]
            ):
                failures.append(
                    f"{cell_id}: authored runtime receipt failed"
                )
            cells.append((
                f"{variant.replace('_', ' ').title()} - {view['label']}",
                pixels,
            ))
            results.append({
                "variant": variant,
                "view": view["id"],
                "triangle_count": audit["triangle_count"],
                "primary_hit_pixels": audit["primary_hit_pixels"],
                "luma_standard_deviation":
                    metrics["luma_standard_deviation"],
                "image": str(png),
            })

    for view in views:
        preset_to_authored = changed_pixel_count(
            pixels_by_variant["preset"][view["id"]],
            pixels_by_variant["authored_default"][view["id"]],
        )
        authored_edit = changed_pixel_count(
            pixels_by_variant["authored_default"][view["id"]],
            pixels_by_variant["authored_edited"][view["id"]],
        )
        if preset_to_authored < 500:
            failures.append(
                f"{view['id']}: authored material effect not visible"
            )
        if authored_edit < 500:
            failures.append(
                f"{view['id']}: agent material edit not visible"
            )
        results.append({
            "comparison": f"preset_to_authored_{view['id']}",
            "changed_pixels": preset_to_authored,
        })
        results.append({
            "comparison": f"authored_default_to_edited_{view['id']}",
            "changed_pixels": authored_edit,
        })

    rejection_view = views[0]
    mixed_scene = make_scene(
        asset_id, region_binding.resolve(), authored_default.resolve()
    )
    mixed_object = dict(mixed_scene["objects"][0])
    mixed_object["object_id"] = f"{asset_id}_preset_only_object"
    mixed_object["procedural_solid_material_ref"] = {
        "binding_path": str(region_binding.resolve()),
    }
    mixed_scene["objects"].append(mixed_object)
    mixed_scene_path = generated / "mixed_authored_reference.scene.json"
    mixed_request_path = generated / "mixed_authored_reference.request.json"
    mixed_run_root = raw_runs / "mixed_authored_reference_rejection"
    write_json(mixed_scene_path, mixed_scene)
    write_json(
        mixed_request_path,
        render_request(
            "psg14_mixed_authored_reference_rejection",
            rejection_view,
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
            "--summary", str(mixed_run_root / "render_summary.json"),
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    if mixed_result.returncode == 0:
        failures.append("mixed authored/preset instances were not rejected")
    results.append({
        "comparison": "mixed_authored_reference_rejection",
        "rejected": mixed_result.returncode != 0,
    })

    tamper_material = materials / "tamper.material.json"
    init_material(
        material_tool, "pitted_concrete", "tamper_material",
        tamper_material,
    )
    tamper_binding, _ = author_binding(
        authored_tool, mesh, region_binding, "authored_tamper",
        [("retained", tamper_material), ("cut", crystal_default)],
        bindings,
    )
    tampered_document = load_json(tamper_material)
    tampered_document["surface"]["roughness"] = 0.17
    write_json(tamper_material, tampered_document)
    tamper_scene_path = generated / "tampered_material.scene.json"
    tamper_request_path = generated / "tampered_material.request.json"
    tamper_run_root = raw_runs / "tampered_material_rejection"
    write_json(
        tamper_scene_path,
        make_scene(
            asset_id, region_binding.resolve(), tamper_binding.resolve()
        ),
    )
    write_json(
        tamper_request_path,
        render_request(
            "psg14_tampered_material_rejection",
            rejection_view,
            tamper_scene_path,
            tamper_request_path,
            tamper_run_root,
            render_contract,
        ),
    )
    tamper_result = subprocess.run(
        [
            str(render_cli),
            "--request", str(tamper_request_path),
            "--render",
            "--summary", str(tamper_run_root / "render_summary.json"),
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    if tamper_result.returncode == 0:
        failures.append("digest-tampered authored material was not rejected")
    results.append({
        "comparison": "digest_tampered_material_rejection",
        "rejected": tamper_result.returncode != 0,
    })

    contact = review / "procedural_solid_psg14_authored_materials.png"
    write_labeled_contact_sheet(contact, cells, columns=3)
    proof = {
        "schema":
            "ray_tracing.procedural_solid_authored_material_visual_proof_psg14",
        "schema_version": 1,
        "passed": not failures,
        "authority": "local_diagnostic_only",
        "mesh_digest_sha256": solid_receipt["mesh_digest_sha256"],
        "region_digest_sha256": solid_receipt["region_digest_sha256"],
        "views": results,
        "contact_sheet": str(contact),
        "failures": failures,
    }
    write_json(output_root / "proof_summary.json", proof)
    (output_root / "index.md").write_text(
        "\n".join([
            "# PSG-14 authored material proof",
            "",
            "One unchanged closed tunnel mesh is rendered with the PSG-13 "
            "preset binding, then with digest-bound authored concrete/crystal "
            "assets, then with agent-edited authored values.",
            "",
            f"![PSG-14 authored material proof]({contact})",
            "",
            "Local diagnostic proof only; no saved scene, package, version, "
            "release, or promotion state changed.",
            "",
        ]),
        encoding="utf-8",
    )
    print(json.dumps(proof, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
