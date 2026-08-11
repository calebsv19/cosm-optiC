#!/usr/bin/env python3
"""Generate the PSG-21 adaptive imported-STL physical-inset visual proof."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import platform
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_DIR = ROOT / "tests" / "integration"
sys.path.insert(0, str(INTEGRATION_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
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
    fixture = (
        ROOT / "tests" / "fixtures"
        / "procedural_imported_surface_inset_psg20"
    )
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--inset-tool", type=Path,
        default=default_tool("procedural_imported_surface_inset_tool"))
    parser.add_argument(
        "--region-tool", type=Path,
        default=default_tool("procedural_imported_surface_region_tool"))
    parser.add_argument(
        "--region-binding-tool", type=Path,
        default=default_tool("procedural_solid_material_agent_tool"))
    parser.add_argument(
        "--material-tool", type=Path,
        default=default_tool(
            "procedural_solid_authored_material_agent_tool"))
    parser.add_argument(
        "--authored-binding-tool", type=Path,
        default=default_tool(
            "procedural_solid_authored_binding_agent_tool"))
    parser.add_argument(
        "--graph-tool", type=Path,
        default=default_tool(
            "procedural_solid_material_graph_agent_tool"))
    parser.add_argument(
        "--render-cli", type=Path,
        default=default_tool("ray_tracing_render_headless"))
    parser.add_argument(
        "--stl-tool", type=Path,
        default=(
            ROOT.parent / "tools" / "procedural_object_authoring"
            / "procedural_stl_tool.py"))
    parser.add_argument(
        "--import-harness", type=Path,
        default=(
            ROOT.parent / "line_drawing" / "build" / "toolchains"
            / "clang" / "bin" / "imported_mesh_harness"))
    parser.add_argument(
        "--contract", type=Path, default=fixture / "visual_contract.json")
    parser.add_argument(
        "--recipe", type=Path, default=fixture / "weathered_urn.recipe.json")
    parser.add_argument(
        "--region-recipe", type=Path,
        default=fixture / "chipped_plaster.region_recipe.json")
    parser.add_argument("--output-root", type=Path)
    return parser.parse_args()


def run(command: list[str]) -> str:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
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


def init_and_edit_material(
    tool: Path,
    root: Path,
    template: str,
    material_id: str,
    name: str,
    edits: list[str],
) -> Path:
    base = root / f"{name}.base.json"
    initialized = receipt([
        str(tool), "init", "--template", template,
        "--material-id", material_id, "--out", str(base)])
    output = root / f"{name}.json"
    command = [
        str(tool), "apply", "--material", str(base),
        "--expected-base-digest", initialized["material_digest_sha256"]]
    for edit in edits:
        command.extend(["--set", edit])
    command.extend([
        "--out", str(output), "--undo-out",
        str(root / f"{name}.undo.json")])
    receipt(command)
    return output


def create_role_graph(
    graph_tool: Path,
    material_tool: Path,
    root: Path,
    authored_binding_id: str,
    authored_binding_digest: str,
    materials: list[tuple[Path, str]],
) -> tuple[Path, dict]:
    graph = root / "psg20_inset_role_materials.json"
    document = {
        "schema":
            "ray_tracing.procedural_solid_material_composition_graph",
        "schema_version": 1,
        "graph_id": "psg20_inset_role_materials",
        "authored_binding": {
            "binding_id": authored_binding_id,
            "binding_digest_sha256": authored_binding_digest,
        },
        "nodes": [
        {
            "node_id": f"{role}_weight",
            "kind": "region",
            "inputs": {},
            "parameters": {
                "value": 0.0,
                "minimum": 0.0,
                "maximum": 1.0,
                "scale": 1.0,
                "offset": 0.0,
                "seed": 1,
                "region_kind": role,
            },
        }
        for _, role in materials
        ],
        "layers": [],
    }
    for material, role in materials:
        material_document = load(material)
        material_receipt = receipt([
            str(material_tool), "inspect", "--material", str(material)])
        document["layers"].append({
            "material_id": material_document["material_id"],
            "material_path": str(material.resolve()),
            "material_digest_sha256":
                material_receipt["material_digest_sha256"],
            "weight_node_id": f"{role}_weight",
        })
    write_json(graph, document)
    return graph, receipt([
        str(graph_tool), "compile", "--graph", str(graph)])


def make_scene(
    scene_id: str,
    asset_id: str,
    *,
    procedural_refs: tuple[Path, Path, Path] | None,
) -> dict:
    object_document: dict = {
        "object_id": f"{scene_id}_object",
        "object_type": "mesh_asset_instance",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": {"x": 0.0, "y": 0.0, "z": 0.0},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "geometry_ref": {"kind": "mesh_asset", "id": asset_id},
        "material_ref": {"id": "fallback_plaster"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }
    if procedural_refs is not None:
        region_binding, authored_binding, graph = procedural_refs
        object_document["procedural_solid_material_ref"] = {
            "binding_path": str(region_binding.resolve()),
            "authored_binding_path": str(authored_binding.resolve()),
            "graph_path": str(graph.resolve()),
        }
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": scene_id,
        "source_scene_id": scene_id,
        "compile_meta": {
            "compiler_version":
                "psg20_imported_surface_inset_visual_proof",
            "compiled_at_ns": 0,
            "normalization": "fresh_imported_stl_physical_inset_diagnostic",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [object_document],
        "materials": [{
            "id": "fallback_plaster",
            "name": "PSG-20 source plaster control",
            "base_color": {"r": 0.79, "g": 0.72, "b": 0.62},
            "roughness": 0.90,
            "metallic": 0.0,
        }],
        "lights": [],
        "extensions": {},
    }


def proof_views() -> dict[str, dict]:
    return {
        "hero": {
            "id": "hero",
            "camera_position": {"x": 2.48, "y": -2.58, "z": 1.78},
            "camera_look_at": {"x": 0.38, "y": -0.34, "z": 1.20},
        },
        "grazing": {
            "id": "grazing",
            "camera_position": {"x": 3.20, "y": 0.25, "z": 1.58},
            "camera_look_at": {"x": 0.54, "y": -0.46, "z": 1.21},
        },
    }


def render(
    render_cli: Path,
    contract: dict,
    generated: Path,
    review: Path,
    name: str,
    asset_id: str,
    view: dict,
    procedural_refs: tuple[Path, Path, Path] | None,
) -> tuple[list, dict, dict, Path]:
    scene = generated / f"{name}.scene.json"
    request = generated / "requests" / f"{name}.request.json"
    raw = generated / "raw" / name
    object_id = f"{name}_object"
    write_json(scene, make_scene(
        name, asset_id, procedural_refs=procedural_refs))
    request_contract = {
        "render": contract["render"],
        "lighting": contract["lighting"],
    }
    write_json(request, render_request(
        contract["proof_id"], {**view, "id": name},
        scene, request, raw, request_contract))
    summary_path = raw / "render_summary.json"
    frame = raw / "frames" / "frame_0000.bmp"
    if not (summary_path.is_file() and frame.is_file()):
        run_render_cli(render_cli, request, summary_path)
    summary = load(summary_path)
    audit = object_audit(summary, object_id)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame)
    png = review / f"{name}.png"
    review_artifacts.write_png_rgb(png, width, height, pixels)
    return pixels, audit, image_metrics(pixels), png


def changed_pixels(left: list, right: list) -> int:
    return sum(
        a != b
        for left_row, right_row in zip(left, right)
        for a, b in zip(left_row, right_row)
    )


def project_mesh(
    mesh: dict,
    width: int,
    height: int,
) -> tuple[list[tuple[float, float, float]], float, float]:
    low = mesh["local_bounds"]["min"]
    high = mesh["local_bounds"]["max"]
    span_x = high["x"] - low["x"]
    span_z = high["z"] - low["z"]
    scale = min((width - 96) / span_x, (height - 96) / span_z)
    offset_x = (width - span_x * scale) * 0.5
    offset_y = (height - span_z * scale) * 0.5
    return [
        (
            offset_x + (vertex["x"] - low["x"]) * scale,
            height - offset_y - (vertex["z"] - low["z"]) * scale,
            vertex["y"],
        )
        for vertex in mesh["mesh"]["vertices"]
    ], scale, offset_x


def rasterize_depth(
    mesh: dict,
    width: int,
    height: int,
    colors: list[tuple[int, int, int]] | None = None,
    *,
    wireframe: bool = False,
) -> tuple[list[list[tuple[int, int, int]]], list[list[float]]]:
    projected, _, _ = project_mesh(mesh, width, height)
    canvas = [[(13, 16, 21) for _ in range(width)] for _ in range(height)]
    depth = [[math.inf for _ in range(width)] for _ in range(height)]
    triangles = mesh["mesh"]["triangles"]
    for triangle_index, triangle in enumerate(triangles):
        indices = (triangle["a"], triangle["b"], triangle["c"])
        points = [projected[index] for index in indices]
        x0 = max(0, int(math.floor(min(point[0] for point in points))))
        x1 = min(width - 1, int(math.ceil(max(point[0] for point in points))))
        y0 = max(0, int(math.floor(min(point[1] for point in points))))
        y1 = min(height - 1, int(math.ceil(max(point[1] for point in points))))
        denominator = (
            (points[1][1] - points[2][1]) *
            (points[0][0] - points[2][0]) +
            (points[2][0] - points[1][0]) *
            (points[0][1] - points[2][1])
        )
        if abs(denominator) < 1e-12:
            continue
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                px, py = x + 0.5, y + 0.5
                a = (
                    (points[1][1] - points[2][1]) *
                    (px - points[2][0]) +
                    (points[2][0] - points[1][0]) *
                    (py - points[2][1])
                ) / denominator
                b = (
                    (points[2][1] - points[0][1]) *
                    (px - points[2][0]) +
                    (points[0][0] - points[2][0]) *
                    (py - points[2][1])
                ) / denominator
                c = 1.0 - a - b
                if min(a, b, c) < -1e-8:
                    continue
                value = (
                    a * points[0][2] +
                    b * points[1][2] +
                    c * points[2][2]
                )
                if value >= depth[y][x]:
                    continue
                depth[y][x] = value
                canvas[y][x] = (
                    colors[triangle_index]
                    if colors is not None else (185, 190, 199)
                )
    if wireframe:
        def line(
            left: tuple[float, float, float],
            right: tuple[float, float, float],
        ) -> None:
            x0, y0 = int(round(left[0])), int(round(left[1]))
            x1, y1 = int(round(right[0])), int(round(right[1]))
            dx, dy = abs(x1 - x0), -abs(y1 - y0)
            sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
            error = dx + dy
            while True:
                if 0 <= x0 < width and 0 <= y0 < height:
                    canvas[y0][x0] = (245, 222, 167)
                if x0 == x1 and y0 == y1:
                    break
                twice = 2 * error
                if twice >= dy:
                    error += dy
                    x0 += sx
                if twice <= dx:
                    error += dx
                    y0 += sy
        for triangle in triangles:
            indices = (triangle["a"], triangle["b"], triangle["c"])
            for side in range(3):
                line(
                    projected[indices[side]],
                    projected[indices[(side + 1) % 3]],
                )
    return canvas, depth


def role_debug(
    mesh: dict,
    provenance: dict,
    width: int,
    height: int,
    *,
    wireframe: bool,
) -> list[list[tuple[int, int, int]]]:
    palette = {
        "retained_surface": (191, 175, 149),
        "transition_wall": (232, 126, 56),
        "inset_floor": (70, 104, 120),
    }
    colors = [
        palette[entry["role"]] for entry in provenance["triangles"]
    ]
    return rasterize_depth(
        mesh, width, height, colors, wireframe=wireframe)[0]


def depth_delta_debug(
    source: dict,
    derived: dict,
    width: int,
    height: int,
) -> tuple[list[list[tuple[int, int, int]]], int]:
    _, source_depth = rasterize_depth(source, width, height)
    _, derived_depth = rasterize_depth(derived, width, height)
    canvas = [[(13, 16, 21) for _ in range(width)] for _ in range(height)]
    changed = 0
    for y in range(height):
        for x in range(width):
            left = source_depth[y][x]
            right = derived_depth[y][x]
            if math.isinf(left) and math.isinf(right):
                continue
            if math.isinf(left) != math.isinf(right):
                canvas[y][x] = (255, 202, 82)
                changed += 1
                continue
            delta = abs(left - right)
            if delta > 1e-5:
                changed += 1
                strength = min(1.0, delta / 0.075)
                canvas[y][x] = (
                    int(35 + 220 * strength),
                    int(55 + 110 * strength),
                    int(72 + 70 * (1.0 - strength)),
                )
            else:
                canvas[y][x] = (48, 54, 63)
    return canvas, changed


def main() -> int:
    args = parse_args()
    contract = load(args.contract.resolve())
    output = (args.output_root or (
        ROOT / "build" / "agent_runs" / "ray_tracing"
        / "procedural_solid"
        / "psg21_adaptive_imported_surface_inset_v1")).resolve()
    generated = output / "generated"
    review = output / "review"
    staging = Path("/private/tmp") / f"psg20_visual_{os.getpid()}"
    for directory in (
        staging, generated / "fresh_stl",
        generated / "assets" / "mesh_assets",
        generated / "regions", generated / "receipts",
        generated / "bindings", generated / "materials",
        generated / "graphs", generated / "requests",
        generated / "raw", review,
    ):
        directory.mkdir(parents=True, exist_ok=True)
    tools = (
        args.inset_tool, args.region_tool, args.region_binding_tool,
        args.material_tool, args.authored_binding_tool, args.graph_tool,
        args.render_cli, args.stl_tool, args.import_harness,
    )
    missing = [str(tool.resolve()) for tool in tools
               if not tool.resolve().exists()]
    if missing:
        raise RuntimeError(f"missing PSG-21 tools: {missing}")

    authored = staging / "authored"
    run([
        sys.executable, str(args.stl_tool.resolve()), "create",
        "--recipe", str(args.recipe.resolve()),
        "--out-root", str(authored)])
    stl = (
        authored / "curated/psg20_weathered_urn"
        / "source/psg20_weathered_urn.stl"
    )
    durable_stl = generated / "fresh_stl" / f"run_{os.getpid()}.stl"
    durable_stl.write_bytes(stl.read_bytes())
    imported = staging / "imported"
    run([
        str(args.import_harness.resolve()),
        "--stl", str(stl),
        "--out", str(imported),
        "--asset-id", "psg20_weathered_urn",
        "--scene-id", "psg20_imported_surface_inset",
        "--object-id", "psg20_urn",
    ])
    source_mesh = (
        generated / "assets/mesh_assets"
        / "psg20_weathered_urn.runtime.json"
    )
    source_mesh.write_bytes((
        imported / "assets/mesh_assets"
        / "psg20_weathered_urn.runtime.json"
    ).read_bytes())
    source_sha_before = digest(source_mesh)
    surface_region = generated / "regions/chipped_plaster.region.json"
    region_receipt_path = generated / "receipts/surface_region.json"
    receipt([
        str(args.region_tool.resolve()),
        "--mesh", str(source_mesh),
        "--recipe", str(args.region_recipe.resolve()),
        "--out", str(surface_region),
        "--summary-out", str(region_receipt_path),
    ])
    inset = contract["inset"]
    derived_mesh = (
        generated / "assets/mesh_assets"
        / "psg20_weathered_urn_inset.runtime.json"
    )
    inset_receipt_path = generated / "receipts/inset.json"
    provenance_path = generated / "receipts/provenance.json"
    solid_receipt_path = generated / "receipts/solid.json"
    inset_command = [
        str(args.inset_tool.resolve()),
        "--mesh", str(source_mesh),
        "--region", str(surface_region),
        "--out", str(derived_mesh),
        "--derived-asset-id", "psg20_weathered_urn_inset",
        "--summary-out", str(inset_receipt_path),
        "--provenance-out", str(provenance_path),
        "--solid-receipt-out", str(solid_receipt_path),
        "--threshold", str(inset["threshold"]),
        "--depth", str(inset["depth_units"]),
        "--depth-variation", str(inset["depth_variation"]),
    ]
    inset_receipt = receipt(inset_command)
    repeat_root = staging / "repeat"
    repeat_root.mkdir(parents=True, exist_ok=True)
    repeat_command = inset_command.copy()
    replacements = {
        str(derived_mesh): str(repeat_root / "derived.runtime.json"),
        str(inset_receipt_path): str(repeat_root / "inset.json"),
        str(provenance_path): str(repeat_root / "provenance.json"),
        str(solid_receipt_path): str(repeat_root / "solid.json"),
    }
    repeat_command = [replacements.get(value, value)
                      for value in repeat_command]
    repeat_receipt = receipt(repeat_command)
    if inset_receipt != repeat_receipt:
        raise RuntimeError("repeat inset receipt differs")
    for durable, repeated in (
        (derived_mesh, repeat_root / "derived.runtime.json"),
        (provenance_path, repeat_root / "provenance.json"),
        (solid_receipt_path, repeat_root / "solid.json"),
    ):
        if digest(durable) != digest(repeated):
            raise RuntimeError(f"repeat artifact differs: {durable.name}")
    if digest(source_mesh) != source_sha_before:
        raise RuntimeError("source runtime mesh changed during inset compile")

    multi_region = generated / "regions/multiple_chips.region.json"
    multi_region_receipt_path = generated / "receipts/multiple_region.json"
    receipt([
        str(args.region_tool.resolve()),
        "--mesh", str(source_mesh),
        "--recipe", str(
            args.region_recipe.resolve().parent
            / "multiple_chips.region_recipe.json"),
        "--out", str(multi_region),
        "--summary-out", str(multi_region_receipt_path),
    ])
    multi_derived_mesh = (
        generated / "assets/mesh_assets"
        / "psg21_weathered_urn_multi_inset.runtime.json"
    )
    multi_receipt_path = generated / "receipts/multiple_inset.json"
    multi_provenance_path = generated / "receipts/multiple_provenance.json"
    multi_solid_receipt_path = generated / "receipts/multiple_solid.json"
    multi_receipt = receipt([
        str(args.inset_tool.resolve()),
        "--mesh", str(source_mesh),
        "--region", str(multi_region),
        "--out", str(multi_derived_mesh),
        "--derived-asset-id", "psg21_weathered_urn_multi_inset",
        "--summary-out", str(multi_receipt_path),
        "--provenance-out", str(multi_provenance_path),
        "--solid-receipt-out", str(multi_solid_receipt_path),
        "--threshold", str(inset["threshold"]),
        "--depth", str(inset["depth_units"]),
        "--depth-variation", str(inset["depth_variation"]),
    ])
    if digest(source_mesh) != source_sha_before:
        raise RuntimeError(
            "source runtime mesh changed during multi-region inset compile")

    materials = generated / "materials"
    plaster = init_and_edit_material(
        args.material_tool.resolve(), materials, "weathered_rock",
        "psg20_plaster_material", "aged_plaster", [
            "base_color.r=0.86", "base_color.g=0.78",
            "base_color.b=0.66", "roughness=0.91",
            "reflectivity=0.03", "texture.enabled=false",
            "emission.color.r=0.86", "emission.color.g=0.78",
            "emission.color.b=0.66", "emission.strength=0.10",
            "texture.scale_units=0.055",
            "texture.strength=0.34", "texture.coverage=0.72",
            "texture.grain=0.56", "texture.edge_softness=0.70",
            "texture.contrast=0.30", "texture.color_depth=0.18",
            "texture.surface_damage=0.30",
            "texture.microdetail_normal_strength=0.30",
            "texture.seed=20020",
        ])
    wall = init_and_edit_material(
        args.material_tool.resolve(), materials, "weathered_rock",
        "psg20_wall_material", "chipped_wall", [
            "base_color.r=0.82", "base_color.g=0.44",
            "base_color.b=0.18", "roughness=0.94",
            "reflectivity=0.02", "texture.enabled=false",
            "emission.color.r=0.82", "emission.color.g=0.44",
            "emission.color.b=0.18", "emission.strength=0.40",
            "texture.scale_units=0.030",
            "texture.strength=0.62", "texture.coverage=0.78",
            "texture.grain=0.78", "texture.edge_softness=0.30",
            "texture.contrast=0.62", "texture.color_depth=0.40",
            "texture.surface_damage=0.68",
            "texture.microdetail_normal_strength=0.54",
            "texture.seed=20021",
        ])
    concrete = init_and_edit_material(
        args.material_tool.resolve(), materials, "pitted_concrete",
        "psg20_concrete_material", "inset_concrete", [
            "base_color.r=0.28", "base_color.g=0.42",
            "base_color.b=0.48", "roughness=0.98",
            "reflectivity=0.015", "texture.enabled=false",
            "emission.color.r=0.28", "emission.color.g=0.42",
            "emission.color.b=0.48", "emission.strength=0.34",
            "texture.scale_units=0.045",
            "texture.strength=0.90", "texture.coverage=0.68",
            "texture.grain=0.92", "texture.edge_softness=0.20",
            "texture.contrast=0.86", "texture.color_depth=0.58",
            "texture.surface_damage=0.82",
            "texture.microdetail_normal_strength=0.68",
            "texture.seed=20022",
        ])

    bindings = generated / "bindings"
    region_base = bindings / "regions.base.json"
    region_init = receipt([
        str(args.region_binding_tool.resolve()), "init",
        "--mesh", str(derived_mesh),
        "--solid-receipt", str(solid_receipt_path),
        "--binding-id", "psg20_inset_regions",
        "--fallback", "default",
        "--out", str(region_base),
    ])
    region_binding = bindings / "regions.json"
    region_applied = receipt([
        str(args.region_binding_tool.resolve()), "apply",
        "--mesh", str(derived_mesh),
        "--binding", str(region_base),
        "--expected-base-digest", region_init["binding_digest_sha256"],
        "--set-kind", "retained=default",
        "--set-kind", "blend=glossy",
        "--set-kind", "cut=rough_metal",
        "--out", str(region_binding),
        "--undo-out", str(bindings / "regions.undo.json"),
    ])
    authored_base = bindings / "authored.base.json"
    authored_init = receipt([
        str(args.authored_binding_tool.resolve()), "init",
        "--mesh", str(derived_mesh),
        "--region-binding", str(region_binding),
        "--binding-id", "psg20_inset_authored",
        "--out", str(authored_base),
    ])
    authored_binding = bindings / "authored.json"
    authored_command = [
        str(args.authored_binding_tool.resolve()), "apply",
        "--mesh", str(derived_mesh),
        "--region-binding", str(region_binding),
        "--authored-binding", str(authored_base),
        "--expected-base-digest", authored_init["binding_digest_sha256"],
    ]
    for kind, material in (
        ("retained", plaster), ("blend", wall), ("cut", concrete),
    ):
        authored_command.extend(["--set-kind", f"{kind}={material.resolve()}"])
    authored_command.extend([
        "--out", str(authored_binding),
        "--undo-out", str(bindings / "authored.undo.json"),
    ])
    authored_receipt = receipt(authored_command)
    graph, graph_receipt = create_role_graph(
        args.graph_tool.resolve(), args.material_tool.resolve(),
        generated / "graphs",
        "psg20_inset_authored",
        authored_receipt["binding_digest_sha256"],
        [(plaster, "retained"), (wall, "blend"), (concrete, "cut")],
    )
    procedural_refs = (region_binding, authored_binding, graph)

    view_set = proof_views()
    renders = {
        "source_control_hero": render(
            args.render_cli.resolve(), contract, generated, review,
            "source_control_hero", "psg20_weathered_urn",
            view_set["hero"], None),
        "physical_inset_hero": render(
            args.render_cli.resolve(), contract, generated, review,
            "physical_inset_hero", "psg20_weathered_urn_inset",
            view_set["hero"], procedural_refs),
        "physical_inset_grazing": render(
            args.render_cli.resolve(), contract, generated, review,
            "physical_inset_grazing", "psg20_weathered_urn_inset",
            view_set["grazing"], procedural_refs),
        "physical_inset_repeat": render(
            args.render_cli.resolve(), contract, generated, review,
            "physical_inset_repeat", "psg20_weathered_urn_inset",
            view_set["hero"], procedural_refs),
    }
    width = contract["render"]["width"]
    height = contract["render"]["height"]
    source_document = load(source_mesh)
    derived_document = load(derived_mesh)
    provenance = load(provenance_path)
    multi_document = load(multi_derived_mesh)
    multi_provenance = load(multi_provenance_path)
    role_image = role_debug(
        derived_document, provenance, width, height, wireframe=False)
    wire_image = role_debug(
        derived_document, provenance, width, height, wireframe=True)
    delta_image, geometry_changed_pixels = depth_delta_debug(
        source_document, derived_document, width, height)
    multi_role_image = role_debug(
        multi_document, multi_provenance, width, height, wireframe=False)
    multi_wire_image = role_debug(
        multi_document, multi_provenance, width, height, wireframe=True)
    role_path = review / "topology_roles.png"
    wire_path = review / "source_triangle_provenance.png"
    delta_path = review / "source_vs_derived_depth_delta.png"
    multi_role_path = review / "multiple_region_topology.png"
    multi_wire_path = review / "multiple_region_provenance.png"
    review_artifacts.write_png_rgb(role_path, width, height, role_image)
    review_artifacts.write_png_rgb(wire_path, width, height, wire_image)
    review_artifacts.write_png_rgb(delta_path, width, height, delta_image)
    review_artifacts.write_png_rgb(
        multi_role_path, width, height, multi_role_image)
    review_artifacts.write_png_rgb(
        multi_wire_path, width, height, multi_wire_image)

    beauty_changed = changed_pixels(
        renders["source_control_hero"][0],
        renders["physical_inset_hero"][0])
    repeat_changed = changed_pixels(
        renders["physical_inset_hero"][0],
        renders["physical_inset_repeat"][0])
    assertions = contract["assertions"]
    failures: list[str] = []
    if inset_receipt["boundary_edge_count"] != 0:
        failures.append("derived shell has open edges")
    if inset_receipt["nonmanifold_edge_count"] != 0:
        failures.append("derived shell has nonmanifold edges")
    if inset_receipt["boundary_loop_count"] != 1:
        failures.append("selected patch is not one boundary loop")
    if inset_receipt["selected_component_count"] != 1:
        failures.append("single-region proof did not retain one component")
    if not inset_receipt["transition_refinement_active"]:
        failures.append("transition refinement was not active")
    if not inset_receipt["adaptive_refinement_active"]:
        failures.append("adaptive carrier refinement was not active")
    if not inset_receipt["adaptive_refinement_converged"]:
        failures.append("adaptive carrier refinement did not converge")
    if inset_receipt["adaptive_refinement_pass_count"] < 2:
        failures.append("adaptive carrier refinement did not perform two passes")
    if (
        inset_receipt["final_max_boundary_edge_length_units"]
        >= inset_receipt["initial_max_boundary_edge_length_units"]
    ):
        failures.append("adaptive refinement did not reduce boundary edge scale")
    if (
        inset_receipt["final_max_boundary_edge_length_units"]
        > inset_receipt["target_boundary_edge_length_units"] + 1e-9
    ):
        failures.append("adaptive refinement missed its boundary edge target")
    if multi_receipt["selected_component_count"] != 2:
        failures.append("multi-region proof did not retain two components")
    if multi_receipt["boundary_loop_count"] != 2:
        failures.append("multi-region proof did not produce two boundary loops")
    if not multi_receipt["adaptive_refinement_converged"]:
        failures.append("multi-region adaptive refinement did not converge")
    if multi_receipt["boundary_edge_count"] != 0:
        failures.append("multi-region shell has open edges")
    if multi_receipt["nonmanifold_edge_count"] != 0:
        failures.append("multi-region shell has nonmanifold edges")
    if geometry_changed_pixels < assertions[
            "minimum_primary_hit_changed_pixels"]:
        failures.append("source/derived projected depth changed too little")
    if beauty_changed < assertions["minimum_beauty_changed_pixels"]:
        failures.append("source and physical-inset beauty changed too little")
    if repeat_changed > assertions["maximum_repeat_changed_pixels"]:
        failures.append("exact repeat pixels differ")
    source_audit = renders["source_control_hero"][1]
    for name, (_, audit, metrics, _) in renders.items():
        expected = (
            inset_receipt["source_triangle_count"]
            if name == "source_control_hero"
            else inset_receipt["derived_triangle_count"]
        )
        if audit["triangle_count"] != expected:
            failures.append(f"{name}: runtime triangle count drift")
        if audit["primary_hit_pixels"] < 120000:
            failures.append(f"{name}: insufficient object coverage")
        if metrics["luma_standard_deviation"] < 8.0:
            failures.append(f"{name}: visually flat")
    if (
        renders["physical_inset_hero"][1]["triangle_count"]
        == source_audit["triangle_count"]
    ):
        failures.append("runtime did not consume topology-changing shell")

    matrix = review / "psg21_adaptive_inset_high_quality_matrix.png"
    write_labeled_contact_sheet(matrix, [
        ("SOURCE CONTROL", renders["source_control_hero"][0]),
        ("PHYSICAL INSET", renders["physical_inset_hero"][0]),
        ("GRAZING DEPTH", renders["physical_inset_grazing"][0]),
        ("TOPOLOGY ROLES", role_image),
        ("DEPTH DELTA", delta_image),
        ("TRIANGLE PROVENANCE", wire_image),
        ("MULTI-REGION LOOPS", multi_role_image),
        ("MULTI-REGION MESH", multi_wire_image),
        ("EXACT REPEAT", renders["physical_inset_repeat"][0]),
    ], columns=3)
    summary = {
        "schema":
            "ray_tracing.procedural_imported_surface_inset_visual_proof",
        "schema_version": 2,
        "passed": not failures,
        "render_resolution": [width, height],
        "temporal_frames": contract["render"]["temporal_frames"],
        "fresh_source_stl_sha256": digest(durable_stl),
        "source_runtime_file_sha256": source_sha_before,
        "source_mesh_digest_sha256":
            inset_receipt["source_mesh_digest_sha256"],
        "carrier_value_digest_sha256":
            inset_receipt["carrier_value_digest_sha256"],
        "derived_mesh_digest_sha256":
            inset_receipt["derived_mesh_digest_sha256"],
        "provenance_digest_sha256":
            inset_receipt["provenance_digest_sha256"],
        "source_triangle_count": inset_receipt["source_triangle_count"],
        "derived_triangle_count": inset_receipt["derived_triangle_count"],
        "transition_wall_triangle_count":
            inset_receipt["transition_wall_triangle_count"],
        "inset_floor_triangle_count":
            inset_receipt["inset_floor_triangle_count"],
        "boundary_ring_edge_count":
            inset_receipt["boundary_ring_edge_count"],
        "selected_component_count":
            inset_receipt["selected_component_count"],
        "boundary_loop_count":
            inset_receipt["boundary_loop_count"],
        "adaptive_refinement_pass_count":
            inset_receipt["adaptive_refinement_pass_count"],
        "target_boundary_edge_length_units":
            inset_receipt["target_boundary_edge_length_units"],
        "initial_max_boundary_edge_length_units":
            inset_receipt["initial_max_boundary_edge_length_units"],
        "final_max_boundary_edge_length_units":
            inset_receipt["final_max_boundary_edge_length_units"],
        "adaptive_refinement_converged":
            inset_receipt["adaptive_refinement_converged"],
        "multiple_region_selected_component_count":
            multi_receipt["selected_component_count"],
        "multiple_region_boundary_loop_count":
            multi_receipt["boundary_loop_count"],
        "multiple_region_derived_mesh_digest_sha256":
            multi_receipt["derived_mesh_digest_sha256"],
        "minimum_inset_depth_units":
            inset_receipt["minimum_inset_depth_units"],
        "maximum_inset_depth_units":
            inset_receipt["maximum_inset_depth_units"],
        "projected_geometry_changed_pixels": geometry_changed_pixels,
        "beauty_changed_pixels": beauty_changed,
        "repeat_changed_pixels": repeat_changed,
        "region_binding_digest_sha256":
            region_applied["binding_digest_sha256"],
        "authored_binding_digest_sha256":
            authored_receipt["binding_digest_sha256"],
        "material_graph_digest_sha256":
            graph_receipt["graph_digest_sha256"],
        "contact_sheet": str(matrix.relative_to(output)),
        "review_images": {
            name: str(data[3].relative_to(output))
            for name, data in renders.items()
        } | {
            "topology_roles": str(role_path.relative_to(output)),
            "source_vs_derived_depth_delta": str(delta_path.relative_to(output)),
            "source_triangle_provenance": str(wire_path.relative_to(output)),
            "multiple_region_topology":
                str(multi_role_path.relative_to(output)),
            "multiple_region_provenance":
                str(multi_wire_path.relative_to(output)),
        },
        "failures": failures,
        "authority": {
            "local_diagnostic_only": True,
            "fresh_imported_object_created": True,
            "source_runtime_mesh_immutable": True,
            "replaceable_derived_shell_created": True,
            "saved_scene_mutated": False,
            "package_or_release_mutated": False,
        },
    }
    write_json(output / "proof_summary.json", summary)
    if failures:
        raise RuntimeError("; ".join(failures))
    print(output / "proof_summary.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
