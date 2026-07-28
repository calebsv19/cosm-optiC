#!/usr/bin/env python3
"""Generate the PSG-16B weighted procedural-texture proof matrix."""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_DIR = ROOT / "tests" / "integration"
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


def default_tool(name: str) -> Path:
    return (
        ROOT / "build" / "toolchains" / "clang" / platform.machine()
        / "tools" / "cli" / name
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--solid-tool", type=Path,
                        default=default_tool("procedural_solid_asset_tool"))
    parser.add_argument("--solid-agent-tool", type=Path,
                        default=default_tool("procedural_solid_agent_tool"))
    parser.add_argument("--surface-agent-tool", type=Path,
                        default=default_tool("procedural_surface_agent_tool"))
    parser.add_argument(
        "--field-tool", type=Path,
        default=default_tool("procedural_surface_field_preset_asset_tool"))
    parser.add_argument("--region-binding-tool", type=Path,
                        default=default_tool(
                            "procedural_solid_material_agent_tool"))
    parser.add_argument("--material-tool", type=Path,
                        default=default_tool(
                            "procedural_solid_authored_material_agent_tool"))
    parser.add_argument("--authored-binding-tool", type=Path,
                        default=default_tool(
                            "procedural_solid_authored_binding_agent_tool"))
    parser.add_argument("--graph-tool", type=Path,
                        default=default_tool(
                            "procedural_solid_material_graph_agent_tool"))
    parser.add_argument("--debug-tool", type=Path,
                        default=default_tool(
                            "procedural_solid_material_debug_tool"))
    parser.add_argument("--render-cli", type=Path,
                        default=default_tool("ray_tracing_render_headless"))
    parser.add_argument(
        "--contract", type=Path,
        default=(
            ROOT / "tests" / "fixtures" /
            "procedural_solid_material_graphs" /
            "psg16b_visual_contract.json"))
    parser.add_argument(
        "--validate-contract-only", action="store_true",
        help="validate fixture provenance and exit without running tools")
    parser.add_argument(
        "--resume-completed", action="store_true",
        help="reuse completed renders from this exact output contract")
    parser.add_argument("--output-root", type=Path)
    return parser.parse_args()


def run(command: list[str]) -> str:
    result = subprocess.run(command, text=True, capture_output=True,
                            check=False)
    if result.returncode:
        raise RuntimeError(
            f"command failed: {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}")
    return result.stdout


def receipt(command: list[str]) -> dict:
    output = run(command)
    try:
        return json.loads(output)
    except json.JSONDecodeError:
        return json.loads(output.splitlines()[0])


def load(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def repo_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def vec3_arg(values: list[float]) -> str:
    if len(values) != 3:
        raise ValueError("expected three-vector")
    return ",".join(str(value) for value in values)


def validate_contract(contract: dict) -> None:
    required = {
        "schema", "schema_version", "proof_id", "title", "visual_intent",
        "expected_visual_signal", "rejection_condition", "base_recipe",
        "source_passthrough_graph", "render", "lighting", "assertions",
        "weighted_transition", "families",
    }
    if set(contract) != required:
        raise ValueError("PSG-15P proof contract keys are not exact")
    if (
        contract["schema"] !=
        "ray_tracing.procedural_solid_psg16b_visual_contract" or
        contract["schema_version"] != 1 or
        len(contract["families"]) != 5
    ):
        raise ValueError("PSG-16B proof contract identity is invalid")
    transition = contract["weighted_transition"]
    if (
        transition.get("family") != "mountain_snow" or
        transition.get("node") != "snow_mask" or
        transition.get("weights") != [0.0, 0.49, 0.5, 0.51, 1.0]
    ):
        raise ValueError("PSG-16B weighted transition contract is invalid")
    ids: set[str] = set()
    provenance: set[str] = set()
    for family in contract["families"]:
        family_id = family.get("id", "")
        geometry = family.get("geometry", {})
        if not family_id or family_id in ids:
            raise ValueError("PSG-15P family ids must be unique")
        ids.add(family_id)
        if len(family.get("variants", [])) != 3:
            raise ValueError(f"{family_id}: exactly three variants are required")
        kind = geometry.get("kind")
        if kind == "solid_graph":
            source = geometry.get("graph", "")
            required_paths = ("graph",)
        elif kind == "surface_field_source":
            source = "|".join([
                geometry.get("field_graph", ""),
                geometry.get("surface_binding", ""),
            ])
            required_paths = (
                "field_graph", "parameter_manifest", "surface_binding")
        else:
            raise ValueError(f"{family_id}: unsupported geometry kind")
        if not geometry.get("authoring_edits"):
            raise ValueError(
                f"{family_id}: per-run object authoring edits are required")
        if geometry.get("remesh_mode", "local_adaptive") not in {
            "local_adaptive", "uniform"
        }:
            raise ValueError(f"{family_id}: unsupported remesh mode")
        if not source or source in provenance:
            raise ValueError(
                f"{family_id}: semantic families must have distinct provenance")
        provenance.add(source)
        for path_key in required_paths:
            path = repo_path(geometry[path_key])
            if not path.is_file():
                raise ValueError(f"{family_id}: missing fixture {path}")


def init_material(tool: Path, template: str, material_id: str,
                  path: Path) -> dict:
    return receipt([
        str(tool), "init", "--template", template,
        "--material-id", material_id, "--out", str(path)])


def edit_material(tool: Path, source: Path, source_receipt: dict,
                  path: Path, edits: list[str]) -> dict:
    command = [
        str(tool), "apply", "--material", str(source),
        "--expected-base-digest",
        source_receipt["material_digest_sha256"]]
    for edit in edits:
        command.extend(["--set", edit])
    command.extend([
        "--out", str(path), "--undo-out", str(path.with_suffix(".undo.json"))])
    return receipt(command)


def create_mesh_and_bindings(
    args: argparse.Namespace, root: Path, family: dict, contract: dict,
    base_material: Path,
) -> tuple[Path, dict, Path, Path, dict, dict]:
    family_id = family["id"]
    geometry = family["geometry"]
    source_mesh: Path | None = None
    authored_root = root / "authored_objects" / family_id
    authored_root.mkdir(parents=True, exist_ok=True)
    if geometry["kind"] == "surface_field_source":
        source_graph = repo_path(geometry["field_graph"])
        parameter_manifest = repo_path(geometry["parameter_manifest"])
        surface_binding = repo_path(geometry["surface_binding"])
        inspected = json.loads(run([
            str(args.surface_agent_tool), "inspect",
            "--graph", str(source_graph),
            "--manifest", str(parameter_manifest),
            "--binding", str(surface_binding),
        ]))
        fixture = authored_root / "surface_field.json"
        undo = authored_root / "surface_field.undo.json"
        authoring_receipt_path = authored_root / "surface_field.receipt.json"
        command = [
            str(args.surface_agent_tool), "apply",
            "--graph", str(source_graph),
            "--manifest", str(parameter_manifest),
            "--binding", str(surface_binding),
            "--expected-base-digest", inspected["graph_digest_sha256"],
        ]
        for edit in geometry["authoring_edits"]:
            command.extend(["--set", edit])
        command.extend([
            "--out", str(fixture), "--undo-out", str(undo),
            "--receipt-out", str(authoring_receipt_path),
        ])
        run(command)
        authoring_receipt = load(authoring_receipt_path)
        source_root = root / "surface_sources" / family_id
        source_root.mkdir(parents=True, exist_ok=True)
        source_mesh = source_root / "runtime_mesh.json"
        command = [
            str(args.field_tool),
            "--graph", str(fixture),
            "--binding", str(surface_binding),
            "--base-recipe", str(repo_path(contract["base_recipe"])),
            "--recipe-out", str(source_root / "recipe.json"),
            "--asset-out", str(source_mesh),
            "--material-out", str(source_root / "material.json"),
            "--manifest-out", str(source_root / "manifest.json"),
            "--summary-out", str(source_root / "summary.json"),
            "--width", str(geometry["width"]),
            "--height", str(geometry["height"]),
            "--depth", str(geometry["depth"]),
            "--target-edge", str(geometry["target_edge"]),
            "--amplitude", str(geometry["amplitude"]),
            "--edge-lock", str(geometry["edge_lock"]),
            "--asset-id", f"{family_id}_terrain_source",
            "--source-asset-id", f"{family_id}_semantic_cage",
        ]
        run(command)
        solid_fixture = repo_path(contract["source_passthrough_graph"])
    else:
        source_graph = repo_path(geometry["graph"])
        inspected = json.loads(run([
            str(args.solid_agent_tool), "inspect",
            "--graph", str(source_graph),
        ]))
        fixture = authored_root / "solid_graph.json"
        undo = authored_root / "solid_graph.undo.json"
        authoring_receipt_path = authored_root / "solid_graph.receipt.json"
        command = [
            str(args.solid_agent_tool), "apply",
            "--graph", str(source_graph),
            "--expected-base-digest", inspected["graph_digest_sha256"],
        ]
        for edit in geometry["authoring_edits"]:
            command.extend(["--set", edit])
        command.extend([
            "--out", str(fixture), "--undo-out", str(undo),
            "--receipt-out", str(authoring_receipt_path),
        ])
        run(command)
        authoring_receipt = load(authoring_receipt_path)
        solid_fixture = fixture

    mesh = root / "assets" / "mesh_assets" / f"{family_id}.runtime.json"
    solid_receipt_path = root / "receipts" / f"{family_id}.solid.json"
    command = [
        str(args.solid_tool), "--graph", str(solid_fixture),
        "--out", str(mesh),
        "--summary-out", str(solid_receipt_path), "--asset-id", family_id,
        "--cells", str(geometry["cells"]),
        "--maximum-cells", str(geometry["maximum_cells"]),
        "--feature-size", str(geometry["feature_size"]),
        "--collision-authority", "derived_shell",
    ]
    if source_mesh is not None:
        command.extend([
            "--source", f"surface_field_shell={source_mesh}",
            "--bounds-min", vec3_arg(geometry["bounds_min"]),
            "--bounds-max", vec3_arg(geometry["bounds_max"]),
            "--assign-regions",
        ])
    elif geometry.get("remesh_mode", "local_adaptive") == "local_adaptive":
        command.append("--local-adaptive")
    else:
        command.append("--assign-regions")
    run(command)
    solid = load(solid_receipt_path)
    region_base = root / "bindings" / f"{family_id}.regions.base.json"
    region_init = receipt([
        str(args.region_binding_tool), "init", "--mesh", str(mesh),
        "--solid-receipt", str(solid_receipt_path),
        "--binding-id", f"{family_id}_regions", "--fallback", "default",
        "--out", str(region_base)])
    region_binding = root / "bindings" / f"{family_id}.regions.json"
    command = [
        str(args.region_binding_tool), "apply", "--mesh", str(mesh),
        "--binding", str(region_base), "--expected-base-digest",
        region_init["binding_digest_sha256"]]
    for kind in sorted({region["kind"] for region in solid["regions"]}):
        command.extend(["--set-kind", f"{kind}=default"])
    command.extend([
        "--out", str(region_binding), "--undo-out",
        str(root / "bindings" / f"{family_id}.regions.undo.json")])
    receipt(command)
    authored_base = root / "bindings" / f"{family_id}.authored.base.json"
    authored_init = receipt([
        str(args.authored_binding_tool), "init", "--mesh", str(mesh),
        "--region-binding", str(region_binding), "--binding-id",
        f"{family_id}_authored", "--out", str(authored_base)])
    authored = root / "bindings" / f"{family_id}.authored.json"
    command = [
        str(args.authored_binding_tool), "apply", "--mesh", str(mesh),
        "--region-binding", str(region_binding), "--authored-binding",
        str(authored_base), "--expected-base-digest",
        authored_init["binding_digest_sha256"]]
    for kind in sorted({region["kind"] for region in solid["regions"]}):
        command.extend(["--set-kind", f"{kind}={base_material.resolve()}"])
    command.extend([
        "--out", str(authored), "--undo-out",
        str(root / "bindings" / f"{family_id}.authored.undo.json")])
    authored_receipt = receipt(command)
    creation = {
        "source_seed": str(source_graph),
        "source_seed_digest_sha256": digest(source_graph),
        "generated_graph": str(fixture),
        "generated_graph_digest_sha256": digest(fixture),
        "authoring_edits": geometry["authoring_edits"],
        "authoring_receipt": str(authoring_receipt_path),
        "authoring_receipt_payload": authoring_receipt,
    }
    return (
        mesh, solid, region_binding, authored, authored_receipt, creation)


def create_graph(
    args: argparse.Namespace, root: Path, graph_name: str, binding_id: str,
    template: str, authored_receipt: dict, materials: list[Path],
) -> tuple[Path, dict]:
    graph = root / "graphs" / f"{graph_name}.json"
    current = receipt([
        str(args.graph_tool), "init", "--template", template,
        "--graph-id", f"{graph_name}_graph", "--binding-id",
        binding_id, "--binding-digest",
        authored_receipt["binding_digest_sha256"], "--output", str(graph)])
    for material in materials:
        material_id = load(material)["material_id"]
        current = receipt([
            str(args.graph_tool), "bind-layer", "--graph", str(graph),
            "--expected-digest", current["graph_digest_sha256"],
            "--material-id", material_id, "--material",
            str(material.resolve())])
    return graph, current


def edit_graph(
    tool: Path, source: Path, source_receipt: dict, output: Path,
    edits: list[tuple[str, str, str]],
) -> dict:
    write_json(output, load(source))
    current = dict(source_receipt)
    for node, parameter, value in edits:
        current = receipt([
            str(tool), "set", "--graph", str(output),
            "--expected-digest", current["graph_digest_sha256"],
            "--node", node, "--parameter", parameter, "--value", value,
            "--snapshot", str(output.with_suffix(".undo.json"))])
    return current


def create_constant_weight_graph(
    tool: Path, source: Path, output: Path, node_id: str, weight: float,
) -> dict:
    document = load(source)
    matched = False
    for node in document["nodes"]:
        if node["node_id"] != node_id:
            continue
        node["kind"] = "constant"
        node["inputs"] = {}
        node["parameters"]["value"] = weight
        matched = True
        break
    if not matched:
        raise ValueError(f"missing transition node: {node_id}")
    write_json(output, document)
    compiled = receipt([
        str(tool), "compile", "--graph", str(output)])
    if not compiled.get("graph_digest_sha256"):
        raise RuntimeError("constant-weight graph compile omitted identity")
    return compiled


def make_scene(asset_id: str, region_binding: Path, authored: Path,
               graph: Path) -> dict:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"psg15_{asset_id}",
        "source_scene_id": f"psg15_{asset_id}",
        "compile_meta": {
            "compiler_version": "psg15_material_graph_visual_proof",
            "compiled_at_ns": 0, "normalization": "local_diagnostic"},
        "space_mode_default": "3d", "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [{
            "object_id": f"{asset_id}_object",
            "object_type": "mesh_asset_instance",
            "dimensional_mode": "full_3d",
            "transform": {
                "position": {"x": 0.0, "y": 0.0, "z": 0.0},
                "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                "scale": {"x": 1.0, "y": 1.0, "z": 1.0}},
            "geometry_ref": {"kind": "mesh_asset", "id": asset_id},
            "material_ref": {"id": "fallback"},
            "procedural_solid_material_ref": {
                "binding_path": str(region_binding.resolve()),
                "authored_binding_path": str(authored.resolve()),
                "graph_path": str(graph.resolve())},
            "flags": {"visible": True, "locked": False,
                      "selectable": True}}],
        "materials": [{
            "id": "fallback", "name": "PSG-15 fallback",
            "base_color": {"r": 0.4, "g": 0.45, "b": 0.5},
            "roughness": 0.7, "metallic": 0.0}],
        "lights": [], "extensions": {},
    }


def create_mask_materials(
    args: argparse.Namespace, root: Path, family_id: str,
    base_material_id: str, overlay_material_id: str,
) -> tuple[Path, Path]:
    paths: list[Path] = []
    for role, material_id, value in (
        ("base", base_material_id, "0.02"),
        ("overlay", overlay_material_id, "0.98"),
    ):
        base = root / "materials" / f"{family_id}.mask_{role}.base.json"
        initialized = init_material(
            args.material_tool, "weathered_rock", material_id, base)
        output = root / "materials" / f"{family_id}.mask_{role}.json"
        edit_material(args.material_tool, base, initialized, output, [
            f"base_color.r={value}",
            f"base_color.g={value}",
            f"base_color.b={value}",
            "roughness=0.8",
            "metallic=0.0",
            "reflectivity=0.0",
            "texture.enabled=false",
        ])
        paths.append(output)
    return paths[0], paths[1]


def render_variant(
    args: argparse.Namespace, generated: Path, review: Path,
    contract: dict, family_id: str, suffix: str, solid: dict,
    region_binding: Path, authored: Path, graph: Path,
) -> tuple[list, dict, dict, Path]:
    view = framed_views(solid)[0]
    scene = generated / f"{family_id}.{suffix}.scene.json"
    request = generated / "requests" / f"{family_id}.{suffix}.json"
    raw = generated / "raw" / f"{family_id}.{suffix}"
    write_json(scene, make_scene(
        family_id, region_binding, authored, graph))
    write_json(request, render_request(
        contract["proof_id"], view, scene, request, raw, {
            "render": contract["render"],
            "lighting": contract["lighting"],
        }))
    summary_path = raw / "render_summary.json"
    frame = raw / "frames" / "frame_0000.bmp"
    reusable = False
    if args.resume_completed and summary_path.is_file() and frame.is_file():
        previous = load(summary_path)
        previous_render = previous.get("render", {})
        reusable = (
            previous.get("run_id") == f"{contract['proof_id']}_hero" and
            previous.get("scene_applied") is True and
            previous.get("rendered_frames") is True and
            previous_render.get("width") == contract["render"]["width"] and
            previous_render.get("height") == contract["render"]["height"] and
            previous_render.get("temporal_frames") ==
                contract["render"]["temporal_frames"]
        )
    if not reusable:
        run_render_cli(args.render_cli, request, summary_path)
    summary = load(summary_path)
    audit = object_audit(summary, f"{family_id}_object")
    width, height, pixels = review_artifacts.read_bmp_rgb(frame)
    png = review / f"{family_id}.{suffix}.png"
    review_artifacts.write_png_rgb(png, width, height, pixels)
    return pixels, audit, image_metrics(pixels), png


def changed_pixels(left: list, right: list) -> int:
    return sum(
        left_pixel != right_pixel
        for left_row, right_row in zip(left, right)
        for left_pixel, right_pixel in zip(left_row, right_row)
    )


def pixel_delta(left: list, right: list) -> dict:
    channel_deltas = [
        abs(left_channel - right_channel)
        for left_row, right_row in zip(left, right)
        for left_pixel, right_pixel in zip(left_row, right_row)
        for left_channel, right_channel in zip(left_pixel, right_pixel)
    ]
    return {
        "changed_pixels": changed_pixels(left, right),
        "mean_absolute_channel_delta":
            sum(channel_deltas) / len(channel_deltas),
        "maximum_channel_delta": max(channel_deltas),
    }

def read_pgm(path: Path) -> list[list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if not data.startswith(b"P5\n"):
        raise ValueError(f"unsupported raw debug image: {path}")
    header, payload = data.split(b"\n255\n", 1)
    parts = header.split()
    if len(parts) != 3:
        raise ValueError(f"invalid raw debug header: {path}")
    width = int(parts[1])
    height = int(parts[2])
    if len(payload) != width * height:
        raise ValueError(f"invalid raw debug payload: {path}")
    return [
        [
            (value, value, value)
            for value in payload[y * width:(y + 1) * width]
        ]
        for y in range(height)
    ]


def main() -> int:
    args = parse_args()
    args.contract = args.contract.resolve()
    contract = load(args.contract)
    validate_contract(contract)
    if args.validate_contract_only:
        print(json.dumps({
            "status": "passed",
            "proof_id": contract["proof_id"],
            "contract": str(args.contract),
            "contract_digest_sha256": digest(args.contract),
            "family_ids": [
                family["id"] for family in contract["families"]],
        }, indent=2))
        return 0

    output = (args.output_root or (
        ROOT / "build" / "agent_runs" / "ray_tracing"
        / "procedural_solid" / "psg16b_weighted_texture_layers")).resolve()
    generated = output / "generated"
    review = output / "review"
    for name in (
        "assets/mesh_assets", "surface_sources", "authored_objects",
        "receipts", "bindings",
        "materials", "graphs", "requests", "raw",
    ):
        (generated / name).mkdir(parents=True, exist_ok=True)
    review.mkdir(parents=True, exist_ok=True)
    args.solid_tool = args.solid_tool.resolve()
    args.solid_agent_tool = args.solid_agent_tool.resolve()
    args.surface_agent_tool = args.surface_agent_tool.resolve()
    args.field_tool = args.field_tool.resolve()
    args.region_binding_tool = args.region_binding_tool.resolve()
    args.material_tool = args.material_tool.resolve()
    args.authored_binding_tool = args.authored_binding_tool.resolve()
    args.graph_tool = args.graph_tool.resolve()
    args.debug_tool = args.debug_tool.resolve()
    args.render_cli = args.render_cli.resolve()

    material_specs = {
        "rock": ("weathered_rock", "base_material", []),
        "concrete": ("pitted_concrete", "base_material", []),
        "sand": ("wind_sand", "base_material", []),
        "snow": ("snow", "snow_material", []),
        "pore": ("weathered_rock", "pore_material", [
            "base_color.r=0.08", "base_color.g=0.07",
            "base_color.b=0.06", "roughness=0.98"]),
        "sediment": ("wind_sand", "sediment_material", [
            "base_color.r=0.52", "base_color.g=0.31",
            "base_color.b=0.13"]),
        "dune": ("wind_sand", "dune_band_material", [
            "base_color.r=0.91", "base_color.g=0.72",
            "base_color.b=0.41"]),
        "strata": ("polished_stone", "strata_material", [
            "base_color.r=0.16", "base_color.g=0.20",
            "base_color.b=0.24", "roughness=0.48"]),
    }
    material_paths: dict[str, Path] = {}
    for key, (template, material_id, edits) in material_specs.items():
        base = generated / "materials" / f"{key}.base.json"
        initialized = init_material(
            args.material_tool, template, material_id, base)
        if edits:
            path = generated / "materials" / f"{key}.json"
            edit_material(args.material_tool, base, initialized, path, edits)
        else:
            path = base
        material_paths[key] = path

    labels = ("Subtle", "Balanced", "Strong")
    beauty_cells: list[tuple[str, list]] = []
    mask_cells: list[tuple[str, list]] = []
    family_results: list[dict] = []
    raw_debug_result: dict | None = None
    weighted_transition_result: dict | None = None
    failures: list[str] = []
    assertions = contract["assertions"]
    for family in contract["families"]:
        family_id = family["id"]
        base_key = family["base_material"]
        overlay_key = family["overlay_material"]
        mesh, solid, region_binding, authored, authored_receipt, creation = (
            create_mesh_and_bindings(
                args, generated, family, contract,
                material_paths[base_key]))
        graph, graph_receipt = create_graph(
            args, generated, f"{family_id}.beauty",
            f"{family_id}_authored", family["template"], authored_receipt,
            [material_paths[base_key], material_paths[overlay_key]])
        base_material_id = load(material_paths[base_key])["material_id"]
        overlay_material_id = load(
            material_paths[overlay_key])["material_id"]
        mask_base, mask_overlay = create_mask_materials(
            args, generated, family_id, base_material_id,
            overlay_material_id)
        mask_graph, mask_graph_receipt = create_graph(
            args, generated, f"{family_id}.mask",
            f"{family_id}_authored", family["template"], authored_receipt,
            [mask_base, mask_overlay])

        family_pixels: list[list] = []
        family_cells: list[tuple[str, list]] = []
        balanced_graph_path: Path | None = None
        variant_results: list[dict] = []
        for index, variant in enumerate(family["variants"]):
            graph_variant = generated / "graphs" / (
                f"{family_id}.beauty.{index}.json")
            variant_receipt = edit_graph(
                args.graph_tool, graph, graph_receipt, graph_variant,
                [tuple(edit) for edit in variant["edits"]])
            if index == 1:
                balanced_graph_path = graph_variant
            compile_receipt = receipt([
                str(args.graph_tool), "compile", "--graph",
                str(graph_variant)])
            if compile_receipt["graph_digest_sha256"] != (
                    variant_receipt["graph_digest_sha256"]):
                failures.append(
                    f"{family_id}/{labels[index]} compile drift")
            pixels, audit, metrics, png = render_variant(
                args, generated, review, contract, family_id,
                f"beauty.{index}", solid, region_binding, authored,
                graph_variant)
            if audit["triangle_count"] != solid["triangle_count"]:
                failures.append(
                    f"{family_id}/{labels[index]} triangle drift")
            if audit["primary_hit_pixels"] < (
                    assertions["minimum_primary_hit_pixels"]):
                failures.append(
                    f"{family_id}/{labels[index]} low coverage")
            if metrics["luma_standard_deviation"] < (
                    assertions[
                        "minimum_image_luma_standard_deviation"]):
                failures.append(
                    f"{family_id}/{labels[index]} flat image")
            beauty_cells.append((variant["label"], pixels))
            family_cells.append((variant["label"], pixels))
            family_pixels.append(pixels)
            variant_results.append({
                "variant": labels[index].lower(),
                "graph_digest_sha256":
                    variant_receipt["graph_digest_sha256"],
                "node_count": variant_receipt["node_count"],
                "parameter_summary": variant["label"],
                "triangle_count": audit["triangle_count"],
                "primary_hit_pixels": audit["primary_hit_pixels"],
                "luma_standard_deviation":
                    metrics["luma_standard_deviation"],
                "image": str(png)})

        changed = changed_pixels(family_pixels[0], family_pixels[2])
        if changed < assertions[
                "minimum_subtle_to_strong_changed_pixels"]:
            failures.append(
                f"{family_id}: node edits not visually distinct")

        balanced = family["variants"][1]
        mask_variant = generated / "graphs" / (
            f"{family_id}.mask.balanced.json")
        mask_receipt = edit_graph(
            args.graph_tool, mask_graph, mask_graph_receipt, mask_variant,
            [tuple(edit) for edit in balanced["edits"]])
        compile_receipt = receipt([
            str(args.graph_tool), "compile", "--graph", str(mask_variant)])
        if compile_receipt["graph_digest_sha256"] != (
                mask_receipt["graph_digest_sha256"]):
            failures.append(f"{family_id}/mask compile drift")
        mask_pixels, mask_audit, mask_metrics, mask_png = render_variant(
            args, generated, review, contract, family_id, "mask",
            solid, region_binding, authored, mask_variant)
        if mask_audit["triangle_count"] != solid["triangle_count"]:
            failures.append(f"{family_id}/mask triangle drift")
        if mask_audit["primary_hit_pixels"] < (
                assertions["minimum_primary_hit_pixels"]):
            failures.append(f"{family_id}/mask low coverage")
        if mask_metrics["luma_standard_deviation"] < (
                assertions["minimum_mask_luma_standard_deviation"]):
            failures.append(f"{family_id}/mask is flat")
        mask_changed = changed_pixels(mask_pixels, family_pixels[1])
        if mask_changed < assertions[
                "minimum_mask_changed_pixels_from_balanced_beauty"]:
            failures.append(
                f"{family_id}/mask does not expose material weight")
        mask_cells.append((family["label"], mask_pixels))
        family_sheet = (
            review / f"{family_id}.high_quality_variants.png")
        write_labeled_contact_sheet(family_sheet, family_cells, columns=3)
        if family_id == "mountain_snow":
            if balanced_graph_path is None:
                raise RuntimeError("mountain balanced graph was not created")
            raw_cells: list[tuple[str, list]] = []
            raw_receipts: dict[str, dict] = {}
            raw_images: dict[str, str] = {}
            for channel, label in (
                ("height", "RAW HEIGHT"),
                ("signed_up_slope", "RAW SIGNED-UP SLOPE"),
                ("layer_weight", "RAW SNOW WEIGHT"),
            ):
                pgm = review / f"mountain_snow.{channel}.1024.pgm"
                debug_receipt = receipt([
                    str(args.debug_tool),
                    "--graph", str(balanced_graph_path),
                    "--mesh", str(mesh),
                    "--channel", channel,
                    "--layer", "1",
                    "--width", "1024", "--height", "1024",
                    "--output", str(pgm),
                ])
                if (
                    debug_receipt["covered_pixels"] < 100000 or
                    debug_receipt["compared_internal_edges"] < 100 or
                    debug_receipt["maximum_internal_edge_jump"] > 1e-12
                ):
                    failures.append(
                        f"mountain_snow/{channel} raw continuity failed")
                pixels = read_pgm(pgm)
                png = review / f"mountain_snow.{channel}.1024.png"
                review_artifacts.write_png_rgb(
                    png, len(pixels[0]), len(pixels), pixels)
                raw_cells.append((label, pixels))
                raw_receipts[channel] = debug_receipt
                raw_images[channel] = str(png)
            raw_sheet = review / "mountain_snow.raw_continuous_inputs.png"
            write_labeled_contact_sheet(raw_sheet, raw_cells, columns=1)
            raw_debug_result = {
                "matrix": str(raw_sheet),
                "images": raw_images,
                "receipts": raw_receipts,
                "acceptance": {
                    "maximum_internal_edge_jump": 1e-12,
                    "geometry_identity_changed": False,
                    "texture_blending_changed": False,
                },
            }
            transition = contract["weighted_transition"]
            transition_cells: list[tuple[str, list]] = []
            transition_pixels: dict[float, list] = {}
            transition_variants: list[dict] = []
            for weight in transition["weights"]:
                weight_token = f"{weight:.2f}".replace(".", "_")
                constant_graph = generated / "graphs" / (
                    f"mountain_snow.texture_weight_{weight_token}.json")
                constant_receipt = create_constant_weight_graph(
                    args.graph_tool, balanced_graph_path, constant_graph,
                    transition["node"], weight)
                pixels, audit, metrics, png = render_variant(
                    args, generated, review, contract, family_id,
                    f"texture_weight_{weight_token}", solid,
                    region_binding, authored, constant_graph)
                if audit["triangle_count"] != solid["triangle_count"]:
                    failures.append(
                        f"mountain_snow/weight {weight:.2f} triangle drift")
                transition_pixels[weight] = pixels
                transition_cells.append((f"WEIGHT {weight:.2f}", pixels))
                transition_variants.append({
                    "weight": weight,
                    "graph_digest_sha256":
                        constant_receipt["graph_digest_sha256"],
                    "triangle_count": audit["triangle_count"],
                    "primary_hit_pixels": audit["primary_hit_pixels"],
                    "luma_standard_deviation":
                        metrics["luma_standard_deviation"],
                    "image": str(png),
                })
            repeat_pixels, repeat_audit, _, repeat_png = render_variant(
                args, generated, review, contract, family_id,
                "texture_weight_0_50_repeat", solid,
                region_binding, authored,
                generated / "graphs" /
                "mountain_snow.texture_weight_0_50.json")
            transition_sheet = (
                review / "mountain_snow.weighted_texture_transition.png")
            write_labeled_contact_sheet(
                transition_sheet, transition_cells, columns=5)
            low_delta = pixel_delta(
                transition_pixels[0.49], transition_pixels[0.5])
            high_delta = pixel_delta(
                transition_pixels[0.5], transition_pixels[0.51])
            endpoint_delta = pixel_delta(
                transition_pixels[0.0], transition_pixels[1.0])
            repeat_delta = pixel_delta(
                transition_pixels[0.5], repeat_pixels)
            maximum_step_fraction = transition[
                "maximum_neighbor_step_fraction_of_endpoint"]
            balance_ratio = transition["maximum_neighbor_step_ratio"]
            low_mean = low_delta["mean_absolute_channel_delta"]
            high_mean = high_delta["mean_absolute_channel_delta"]
            endpoint_mean = endpoint_delta[
                "mean_absolute_channel_delta"]
            if (
                low_delta["changed_pixels"] <
                    transition["minimum_neighbor_changed_pixels"] or
                high_delta["changed_pixels"] <
                    transition["minimum_neighbor_changed_pixels"] or
                endpoint_mean <= 0.0 or
                low_mean >= endpoint_mean * maximum_step_fraction or
                high_mean >= endpoint_mean * maximum_step_fraction or
                low_mean >= high_mean * balance_ratio or
                high_mean >= low_mean * balance_ratio
            ):
                failures.append(
                    "mountain_snow weighted texture transition is not smooth")
            if repeat_delta["changed_pixels"] != 0:
                failures.append(
                    "mountain_snow weight 0.50 repeat is not deterministic")
            weighted_transition_result = {
                "matrix": str(transition_sheet),
                "variants": transition_variants,
                "repeat_image": str(repeat_png),
                "repeat_triangle_count": repeat_audit["triangle_count"],
                "deltas": {
                    "weight_0_49_to_0_50": low_delta,
                    "weight_0_50_to_0_51": high_delta,
                    "weight_0_00_to_1_00": endpoint_delta,
                    "weight_0_50_repeat": repeat_delta,
                },
                "acceptance": transition,
            }
        geometry = family["geometry"]
        if geometry["kind"] == "solid_graph":
            provenance = {
                "kind": "solid_graph",
                "graph": str(repo_path(geometry["graph"])),
                "graph_digest_sha256":
                    digest(repo_path(geometry["graph"])),
            }
        else:
            provenance = {
                "kind": "surface_field_source",
                "field_graph": str(repo_path(geometry["field_graph"])),
                "field_graph_digest_sha256":
                    digest(repo_path(geometry["field_graph"])),
                "surface_binding":
                    str(repo_path(geometry["surface_binding"])),
                "surface_binding_digest_sha256":
                    digest(repo_path(geometry["surface_binding"])),
                "solid_adapter":
                    str(repo_path(contract["source_passthrough_graph"])),
            }
        family_results.append({
            "family": family_id,
            "claim": family["claim"],
            "geometry_provenance": provenance,
            "derived_mesh": str(mesh),
            "derived_mesh_digest_sha256": digest(mesh),
            "object_creation": creation,
            "triangle_count": solid["triangle_count"],
            "high_quality_variant_matrix": str(family_sheet),
            "variants": variant_results,
            "subtle_to_strong_changed_pixels": changed,
            "mask": {
                "graph_digest_sha256":
                    mask_receipt["graph_digest_sha256"],
                "luma_standard_deviation":
                    mask_metrics["luma_standard_deviation"],
                "changed_pixels_from_balanced_beauty": mask_changed,
                "image": str(mask_png),
            },
        })

    beauty_sheet = (
        review / "procedural_solid_psg16b_material_graph_matrix.png")
    mask_sheet = (
        review / "procedural_solid_psg16b_material_weight_masks.png")
    write_labeled_contact_sheet(beauty_sheet, beauty_cells, columns=3)
    write_labeled_contact_sheet(mask_sheet, mask_cells, columns=2)
    report = {
        "schema": "ray_tracing.procedural_solid_psg16b_visual_proof",
        "schema_version": 1,
        "status": "passed" if not failures else "failed",
        "authority": {
            "classification": "local_diagnostic_proof",
            "promotion_authorized": False,
            "saved_scene_replaced": False,
            "package_or_release_changed": False,
        },
        "contract": {
            "path": str(args.contract),
            "digest_sha256": digest(args.contract),
            "visual_intent": contract["visual_intent"],
            "expected_visual_signal":
                contract["expected_visual_signal"],
            "rejection_condition": contract["rejection_condition"],
        },
        "claim_boundary": {
            "demonstrated": [
                "per-run object creation plus authored material composition "
                "from geometry-derived inputs",
                "native-hit barycentric height, signed-up-slope, and "
                "object-position evaluation",
                "continuous mountain layer weights with zero measured "
                "internal shared-edge jump",
                "deterministic graph-order procedural texture layers whose "
                "effective opacity follows continuous graph weights",
            ],
            "compatibility_inputs": [
                "curvature",
                "cavity",
                "boundary_distance",
                "region",
            ],
            "not_demonstrated": [
                "bump-normal or microdetail-normal output",
                "material-driven displacement or remeshing",
            ],
        },
        "geometry_inputs": [
            "height", "slope", "curvature", "cavity", "region",
            "boundary_distance"],
        "beauty_matrix": str(beauty_sheet),
        "material_weight_mask_matrix": str(mask_sheet),
        "raw_continuous_debug": raw_debug_result,
        "weighted_texture_transition": weighted_transition_result,
        "families": family_results,
        "failures": failures,
    }
    write_json(review / "psg16b_visual_proof.json", report)
    print(json.dumps(report, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
