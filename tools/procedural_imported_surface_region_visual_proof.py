#!/usr/bin/env python3
"""Generate the PSG-19 fresh imported-STL coating-region visual proof."""

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
from procedural_surface_feature_spot_compiler import mesh_analysis  # noqa: E402
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
        / "procedural_imported_surface_region_psg19"
    )
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--region-tool", type=Path,
                        default=default_tool(
                            "procedural_imported_surface_region_tool"))
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
    parser.add_argument("--render-cli", type=Path,
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
    parser.add_argument("--contract", type=Path,
                        default=fixture / "visual_contract.json")
    parser.add_argument("--recipe", type=Path,
                        default=fixture / "statue_fragment.recipe.json")
    parser.add_argument("--region-recipe", type=Path,
                        default=fixture / "plaster_peel.region_recipe.json")
    parser.add_argument("--surface-feature-field", type=Path,
                        help="Optional digest-bound PSG-24 field used as the overlay weight.")
    parser.add_argument("--surface-feature-authoring", type=Path,
                        help="Compile a PSG-24A field against the fresh proof mesh.")
    parser.add_argument("--force-render", action="store_true",
                        help="Regenerate native frames even when prior proof files exist.")
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


def create_graph(
    graph_tool: Path,
    root: Path,
    name: str,
    binding_id: str,
    binding_digest: str,
    base_material: Path,
    overlay_material: Path,
    authored_weight: bool,
    feature_weight: bool = False,
    feature_channel: str = "feature_coverage",
) -> tuple[Path, dict]:
    graph = root / f"{name}.json"
    current = receipt([
        str(graph_tool), "init", "--template", "concrete_pores",
        "--graph-id", name, "--binding-id", binding_id,
        "--binding-digest", binding_digest, "--output", str(graph)])
    for material in (base_material, overlay_material):
        material_id = load(material)["material_id"]
        current = receipt([
            str(graph_tool), "bind-layer", "--graph", str(graph),
            "--expected-digest", current["graph_digest_sha256"],
            "--material-id", material_id, "--material",
            str(material.resolve())])
    document = load(graph)
    document["nodes"] = [
        {
            "node_id": "base",
            "kind": "constant",
            "inputs": {},
            "parameters": {
                "value": 1.0, "minimum": 0.0, "maximum": 1.0,
                "scale": 1.0, "offset": 0.0, "seed": 1,
                "region_kind": "",
            },
        },
        {
            "node_id": "coating_exposure",
            "kind": (feature_channel if feature_weight else
                     "authored_region" if authored_weight else "constant"),
            "inputs": {},
            "parameters": {
                "value": 0.0, "minimum": 0.0, "maximum": 1.0,
                "scale": 1.0, "offset": 0.0, "seed": 1,
                "region_kind": "",
            },
        },
    ]
    document["layers"][0]["weight_node_id"] = "base"
    document["layers"][1]["weight_node_id"] = "coating_exposure"
    write_json(graph, document)
    compiled = receipt([str(graph_tool), "compile", "--graph", str(graph)])
    return graph, compiled


def make_scene(
    asset_id: str,
    region_binding: Path,
    authored_binding: Path,
    graph: Path,
    surface_region: Path,
    surface_feature_field: Path | None = None,
) -> dict:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": f"psg19_{graph.stem}",
        "source_scene_id": f"psg19_{graph.stem}",
        "compile_meta": {
            "compiler_version": "psg19_imported_surface_region_visual_proof",
            "compiled_at_ns": 0,
            "normalization": "fresh_imported_stl_diagnostic",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [{
            "object_id": "psg19_statue_object",
            "object_type": "mesh_asset_instance",
            "dimensional_mode": "full_3d",
            "transform": {
                "position": {"x": 0.0, "y": 0.0, "z": 0.0},
                "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
            },
            "geometry_ref": {"kind": "mesh_asset", "id": asset_id},
            "material_ref": {"id": "fallback"},
            "procedural_solid_material_ref": {
                "binding_path": str(region_binding.resolve()),
                "authored_binding_path": str(authored_binding.resolve()),
                "graph_path": str(graph.resolve()),
                "surface_region_path": str(surface_region.resolve()),
            },
            "flags": {"visible": True, "locked": False, "selectable": True},
        }],
        "materials": [{
            "id": "fallback",
            "name": "PSG-19 fallback",
            "base_color": {"r": 0.5, "g": 0.5, "b": 0.5},
            "roughness": 0.8,
            "metallic": 0.0,
        }],
        "lights": [],
        "extensions": {},
    }


def views(mesh: dict) -> dict[str, dict]:
    low = mesh["local_bounds"]["min"]
    high = mesh["local_bounds"]["max"]
    center = {axis: (low[axis] + high[axis]) * 0.5
              for axis in ("x", "y", "z")}
    radius = max(high[axis] - low[axis] for axis in ("x", "y", "z"))
    return {
        "hero": {
            "id": "hero",
            "camera_position": {
                "x": center["x"] + 1.12 * radius,
                "y": center["y"] - 1.62 * radius,
                "z": center["z"] + 0.52 * radius,
            },
            "camera_look_at": center,
        },
        "detail": {
            "id": "detail",
            "camera_position": {
                "x": center["x"] - 1.45 * radius,
                "y": center["y"] - 1.25 * radius,
                "z": center["z"] + 0.18 * radius,
            },
            "camera_look_at": {
                "x": center["x"],
                "y": center["y"],
                "z": center["z"] + 0.12 * radius,
            },
        },
        "grazing": {
            "id": "grazing",
            "camera_position": {
                "x": center["x"] + 1.72 * radius,
                "y": center["y"] - 0.72 * radius,
                "z": center["z"] + 0.08 * radius,
            },
            "camera_look_at": {
                "x": center["x"],
                "y": center["y"],
                "z": center["z"] - 0.04 * radius,
            },
        },
    }


def render(
    render_cli: Path,
    contract: dict,
    generated: Path,
    review: Path,
    name: str,
    graph: Path,
    view: dict,
    region_binding: Path,
    authored_binding: Path,
    surface_region: Path,
    surface_feature_field: Path | None = None,
    force_render: bool = False,
) -> tuple[list, dict, dict, Path]:
    scene = generated / f"{name}.scene.json"
    request = generated / "requests" / f"{name}.request.json"
    raw = generated / "raw" / name
    scene_document = make_scene(
        load(surface_region)["source_asset_id"], region_binding,
        authored_binding, graph, surface_region, surface_feature_field)
    if surface_feature_field:
        scene_document["objects"][0]["procedural_solid_material_ref"][
            "surface_feature_field_path"] = str(surface_feature_field.resolve())
    write_json(scene, scene_document)
    request_contract = {
        "render": contract["render"],
        "lighting": contract["lighting"],
    }
    write_json(request, render_request(
        contract["proof_id"], {**view, "id": name},
        scene, request, raw, request_contract))
    summary_path = raw / "render_summary.json"
    frame = raw / "frames" / "frame_0000.bmp"
    if force_render or not (summary_path.is_file() and frame.is_file()):
        run_render_cli(render_cli, request, summary_path)
    summary = load(summary_path)
    audit = object_audit(summary, "psg19_statue_object")
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


def read_ppm_rgb(path: Path) -> tuple[int, int, list]:
    data = path.read_bytes()
    header, dimensions, maximum, payload = data.split(b"\n", 3)
    if header != b"P6" or maximum != b"255":
        raise ValueError(f"{path}: unsupported PPM")
    width_text, height_text = dimensions.split()
    width_value, height_value = int(width_text), int(height_text)
    if len(payload) != width_value * height_value * 3:
        raise ValueError(f"{path}: truncated PPM")
    pixels = []
    for y in range(height_value):
        row = []
        for x in range(width_value):
            offset = (y * width_value + x) * 3
            row.append(tuple(payload[offset:offset + 3]))
        pixels.append(row)
    return width_value, height_value, pixels


def resize_nearest(pixels: list, width: int, height: int) -> list:
    source_height = len(pixels)
    source_width = len(pixels[0])
    return [[
        pixels[min(source_height - 1, y * source_height // height)][
            min(source_width - 1, x * source_width // width)]
        for x in range(width)
    ] for y in range(height)]


def feature_diagnostic_projection(
    mesh: dict, field: dict, analysis: dict, width: int, height: int, mode: str,
) -> list[list[tuple[int, int, int]]]:
    vertices = mesh["mesh"]["vertices"]
    triangles = mesh["mesh"]["triangles"]
    low, high = mesh["local_bounds"]["min"], mesh["local_bounds"]["max"]
    span_x, span_z = high["x"] - low["x"], high["z"] - low["z"]
    scale_value = min((width - 80) / span_x, (height - 80) / span_z)
    offset_x = (width - span_x * scale_value) * 0.5
    offset_y = (height - span_z * scale_value) * 0.5
    projected = [(
        offset_x + (vertex["x"] - low["x"]) * scale_value,
        height - offset_y - (vertex["z"] - low["z"]) * scale_value,
        vertex["y"],
    ) for vertex in vertices]
    canvas = [[(9, 11, 14) for _ in range(width)] for _ in range(height)]
    depth = [[math.inf for _ in range(width)] for _ in range(height)]
    for triangle_index, triangle in enumerate(triangles):
        indices = (triangle["a"], triangle["b"], triangle["c"])
        points = [projected[index] for index in indices]
        x0 = max(0, int(math.floor(min(point[0] for point in points))))
        x1 = min(width - 1, int(math.ceil(max(point[0] for point in points))))
        y0 = max(0, int(math.floor(min(point[1] for point in points))))
        y1 = min(height - 1, int(math.ceil(max(point[1] for point in points))))
        denominator = (
            (points[1][1] - points[2][1]) * (points[0][0] - points[2][0])
            + (points[2][0] - points[1][0]) * (points[0][1] - points[2][1]))
        if abs(denominator) < 1.0e-12:
            continue
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                px, py = x + 0.5, y + 0.5
                a = ((points[1][1] - points[2][1]) * (px - points[2][0])
                     + (points[2][0] - points[1][0]) * (py - points[2][1])) / denominator
                b = ((points[2][1] - points[0][1]) * (px - points[2][0])
                     + (points[0][0] - points[2][0]) * (py - points[2][1])) / denominator
                c = 1.0 - a - b
                if min(a, b, c) < -1.0e-8:
                    continue
                z_depth = a * points[0][2] + b * points[1][2] + c * points[2][2]
                if z_depth >= depth[y][x]:
                    continue
                depth[y][x] = z_depth
                if mode == "macro_envelope":
                    value = analysis["weights"][triangle_index]
                    canvas[y][x] = (
                        int(22 + 220 * value), int(28 + 132 * value),
                        int(38 + 42 * value))
                else:
                    if mode == "geometric_normal":
                        normal = analysis["face_normals"][triangle_index]
                    else:
                        smooth = analysis["smooth_normals"]
                        normal = [
                            a * smooth[indices[0]][axis]
                            + b * smooth[indices[1]][axis]
                            + c * smooth[indices[2]][axis]
                            for axis in range(3)]
                        magnitude = math.sqrt(sum(value * value for value in normal))
                        normal = [value / magnitude for value in normal]
                    canvas[y][x] = tuple(
                        max(0, min(255, int((value * 0.5 + 0.5) * 255)))
                        for value in normal)
    if mode == "feature_id":
        # Not reached through the triangle fill above; retained for callers
        # that want a dark source silhouette with stable-ID root markers.
        pass
    return canvas


def feature_id_projection(
    mesh: dict, field: dict, width: int, height: int,
) -> list[list[tuple[int, int, int]]]:
    background = diagnostic_projection(
        mesh, {"vertex_weights": [0.08] * len(mesh["mesh"]["vertices"])},
        width, height, wireframe=False)
    low, high = mesh["local_bounds"]["min"], mesh["local_bounds"]["max"]
    scale_value = min(
        (width - 80) / (high["x"] - low["x"]),
        (height - 80) / (high["z"] - low["z"]))
    offset_x = (width - (high["x"] - low["x"]) * scale_value) * 0.5
    offset_y = (height - (high["z"] - low["z"]) * scale_value) * 0.5
    for feature in field["features"]:
        # The diagnostic camera looks from negative Y; rear roots are omitted
        # so the ID view reads as deposits on the visible object, not a flat map.
        if feature["position"][1] > 0.12:
            continue
        center_x = int(offset_x + (feature["position"][0] - low["x"]) * scale_value)
        center_y = int(height - offset_y - (feature["position"][2] - low["z"]) * scale_value)
        radius = max(2, int(feature["radius"] * scale_value))
        value = feature["feature_id"]
        color = ((value * 67) % 224 + 31, (value * 131) % 224 + 31,
                 (value * 197) % 224 + 31)
        for y in range(max(0, center_y - radius), min(height, center_y + radius + 1)):
            for x in range(max(0, center_x - radius), min(width, center_x + radius + 1)):
                if (x - center_x) ** 2 + (y - center_y) ** 2 <= radius ** 2:
                    background[y][x] = color
    return background


def diagnostic_projection(
    mesh: dict,
    region: dict,
    width: int,
    height: int,
    *,
    wireframe: bool,
) -> list[list[tuple[int, int, int]]]:
    vertices = mesh["mesh"]["vertices"]
    triangles = mesh["mesh"]["triangles"]
    weights = region["vertex_weights"]
    low = mesh["local_bounds"]["min"]
    high = mesh["local_bounds"]["max"]
    span_x = high["x"] - low["x"]
    span_z = high["z"] - low["z"]
    scale = min((width - 80) / span_x, (height - 80) / span_z)
    offset_x = (width - span_x * scale) * 0.5
    offset_y = (height - span_z * scale) * 0.5
    projected = [
        (
            offset_x + (vertex["x"] - low["x"]) * scale,
            height - offset_y - (vertex["z"] - low["z"]) * scale,
            vertex["y"],
        )
        for vertex in vertices
    ]
    canvas = [[(13, 16, 21) for _ in range(width)] for _ in range(height)]
    depth = [[math.inf for _ in range(width)] for _ in range(height)]
    for triangle in triangles:
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
                    (points[1][1] - points[2][1]) * (px - points[2][0]) +
                    (points[2][0] - points[1][0]) * (py - points[2][1])
                ) / denominator
                b = (
                    (points[2][1] - points[0][1]) * (px - points[2][0]) +
                    (points[0][0] - points[2][0]) * (py - points[2][1])
                ) / denominator
                c = 1.0 - a - b
                if min(a, b, c) < -1e-8:
                    continue
                z = a * points[0][2] + b * points[1][2] + c * points[2][2]
                if z >= depth[y][x]:
                    continue
                depth[y][x] = z
                value = max(0.0, min(
                    1.0,
                    a * weights[indices[0]] +
                    b * weights[indices[1]] +
                    c * weights[indices[2]],
                ))
                shade = int(round(value * 255.0))
                canvas[y][x] = (
                    (
                        min(255, 26 + int(0.70 * shade)),
                        min(255, 30 + int(0.76 * shade)),
                        min(255, 38 + shade),
                    )
                    if wireframe else (shade, shade, shade)
                )
    if wireframe:
        def line(left: tuple[float, float, float],
                 right: tuple[float, float, float]) -> None:
            x0, y0 = int(round(left[0])), int(round(left[1]))
            x1, y1 = int(round(right[0])), int(round(right[1]))
            dx, dy = abs(x1 - x0), -abs(y1 - y0)
            sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
            error = dx + dy
            while True:
                if 0 <= x0 < width and 0 <= y0 < height:
                    canvas[y0][x0] = (242, 136, 54)
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
            for index in range(3):
                line(projected[indices[index]],
                     projected[indices[(index + 1) % 3]])
    return canvas


def main() -> int:
    args = parse_args()
    fixture_contract = load(args.contract.resolve())
    output = (args.output_root or (
        ROOT / "build" / "agent_runs" / "ray_tracing"
        / "procedural_solid" / "psg19_imported_surface_region_v2")).resolve()
    generated = output / "generated"
    review = output / "review"
    run_token = f"run_{os.getpid()}"
    staging_root = Path("/private/tmp") / f"psg19_visual_{os.getpid()}"
    fresh_stl_root = staging_root / "fresh_stl"
    for directory in (
        fresh_stl_root, generated / "fresh_stl", generated / "imported",
        generated / "assets" / "mesh_assets",
        generated / "receipts", generated / "bindings",
        generated / "materials", generated / "graphs",
        generated / "requests", generated / "raw", review,
    ):
        directory.mkdir(parents=True, exist_ok=True)

    tools = (
        args.region_tool, args.region_binding_tool, args.material_tool,
        args.authored_binding_tool, args.graph_tool, args.render_cli,
        args.stl_tool, args.import_harness,
    )
    if any(not tool.resolve().exists() for tool in tools):
        missing = [str(tool.resolve()) for tool in tools
                   if not tool.resolve().exists()]
        raise RuntimeError(f"missing PSG-19 tools: {missing}")

    run([
        sys.executable, str(args.stl_tool.resolve()), "create",
        "--recipe", str(args.recipe.resolve()),
        "--out-root", str(fresh_stl_root)])
    recipe_document = load(args.recipe.resolve())
    asset_id = recipe_document["asset_id"]
    stl = fresh_stl_root / "curated" / asset_id / "source" / f"{asset_id}.stl"
    durable_stl = generated / "fresh_stl" / f"{run_token}.stl"
    durable_stl.write_bytes(stl.read_bytes())
    imported = staging_root / "imported"
    run([
        str(args.import_harness.resolve()), "--stl", str(stl),
        "--out", str(imported),
        "--asset-id", asset_id,
        "--scene-id", "psg19_imported_surface_region",
        "--object-id", "psg19_statue"])
    imported_mesh = (
        imported / "assets" / "mesh_assets"
        / f"{asset_id}.runtime.json")
    mesh = generated / "assets" / "mesh_assets" / imported_mesh.name
    mesh.write_bytes(imported_mesh.read_bytes())

    surface_region = generated / "regions" / "plaster_peel.region.json"
    surface_region.parent.mkdir(parents=True, exist_ok=True)
    region_receipt_path = generated / "receipts" / "surface_region.json"
    solid_receipt_path = generated / "receipts" / "solid.json"
    region_receipt = receipt([
        str(args.region_tool.resolve()), "--mesh", str(mesh),
        "--recipe", str(args.region_recipe.resolve()),
        "--out", str(surface_region),
        "--summary-out", str(region_receipt_path),
        "--solid-receipt-out", str(solid_receipt_path)])
    mesh_document = load(mesh)
    surface_region_document = load(surface_region)
    feature_proof_root = None
    if args.surface_feature_authoring:
        feature_proof_root = output / "field"
        run([
            sys.executable,
            str((ROOT / "tools" / "procedural_surface_feature_field_authoring.py").resolve()),
            "--authoring", str(args.surface_feature_authoring.resolve()),
            "--mesh", str(mesh),
            "--solid-receipt", str(solid_receipt_path),
            "--output-root", str(feature_proof_root),
        ])
        args.surface_feature_field = (
            feature_proof_root / "assets" / "surface_feature_field_v1.json")

    materials = generated / "materials"
    plaster = init_and_edit_material(
        args.material_tool.resolve(), materials, "weathered_rock",
        "base_material", "aged_plaster", [
            "base_color.r=0.89", "base_color.g=0.81",
            "base_color.b=0.68", "roughness=0.88",
            "reflectivity=0.035", "texture.scale_units=0.065",
            "texture.strength=0.26", "texture.coverage=0.74",
            "texture.grain=0.52", "texture.edge_softness=0.72",
            "texture.contrast=0.24", "texture.color_depth=0.16",
            "texture.surface_damage=0.24",
            "texture.microdetail_normal_strength=0.28",
            "texture.seed=19019",
        ])
    concrete = init_and_edit_material(
        args.material_tool.resolve(), materials, "pitted_concrete",
        "pore_material", "exposed_concrete", [
            "base_color.r=0.20", "base_color.g=0.23",
            "base_color.b=0.25", "roughness=0.96",
            "reflectivity=0.025", "texture.scale_units=0.052",
            "texture.strength=0.88", "texture.coverage=0.66",
            "texture.grain=0.90", "texture.edge_softness=0.25",
            "texture.contrast=0.82", "texture.color_depth=0.52",
            "texture.surface_damage=0.78",
            "texture.microdetail_normal_strength=0.62",
            "texture.seed=19023",
        ])
    black = init_and_edit_material(
        args.material_tool.resolve(), materials, "weathered_rock",
        "base_material", "mask_black", [
            "base_color.r=0.01", "base_color.g=0.01",
            "base_color.b=0.01", "roughness=1.0",
            "reflectivity=0.0", "texture.enabled=false",
        ])
    white = init_and_edit_material(
        args.material_tool.resolve(), materials, "pitted_concrete",
        "pore_material", "mask_white", [
            "base_color.r=0.99", "base_color.g=0.99",
            "base_color.b=0.99", "roughness=1.0",
            "reflectivity=0.0", "texture.enabled=false",
        ])

    bindings = generated / "bindings"
    region_base = bindings / "regions.base.json"
    region_init = receipt([
        str(args.region_binding_tool.resolve()), "init",
        "--mesh", str(mesh), "--solid-receipt", str(solid_receipt_path),
        "--binding-id", "psg19_imported_regions",
        "--fallback", "default", "--out", str(region_base)])
    region_binding = bindings / "regions.json"
    region_applied = receipt([
        str(args.region_binding_tool.resolve()), "apply",
        "--mesh", str(mesh), "--binding", str(region_base),
        "--expected-base-digest", region_init["binding_digest_sha256"],
        "--set-kind", "retained=default", "--out", str(region_binding),
        "--undo-out", str(bindings / "regions.undo.json")])
    authored_base = bindings / "authored.base.json"
    authored_init = receipt([
        str(args.authored_binding_tool.resolve()), "init",
        "--mesh", str(mesh), "--region-binding", str(region_binding),
        "--binding-id", "psg19_imported_authored",
        "--out", str(authored_base)])
    authored_binding = bindings / "authored.json"
    authored_receipt = receipt([
        str(args.authored_binding_tool.resolve()), "apply",
        "--mesh", str(mesh), "--region-binding", str(region_binding),
        "--authored-binding", str(authored_base),
        "--expected-base-digest", authored_init["binding_digest_sha256"],
        "--set-kind", f"retained={plaster.resolve()}",
        "--out", str(authored_binding),
        "--undo-out", str(bindings / "authored.undo.json")])

    graphs = generated / "graphs"
    beauty_graph, beauty_graph_receipt = create_graph(
        args.graph_tool.resolve(), graphs, "psg19_plaster_concrete",
        "psg19_imported_authored",
        authored_receipt["binding_digest_sha256"],
        plaster, concrete, True, args.surface_feature_field is not None)
    control_graph, control_graph_receipt = create_graph(
        args.graph_tool.resolve(), graphs, "psg19_all_plaster_control",
        "psg19_imported_authored",
        authored_receipt["binding_digest_sha256"],
        plaster, concrete, False)
    mask_graph, mask_graph_receipt = create_graph(
        args.graph_tool.resolve(), graphs, "psg19_region_mask",
        "psg19_imported_authored",
        authored_receipt["binding_digest_sha256"],
        black, white, True)

    feature_mask_graphs: dict[str, tuple[Path, dict]] = {}
    if args.surface_feature_field:
        for channel in ("feature_coverage", "feature_interior", "feature_rim"):
            feature_mask_graphs[channel] = create_graph(
                args.graph_tool.resolve(), graphs, f"psg24_{channel}_mask",
                "psg19_imported_authored",
                authored_receipt["binding_digest_sha256"],
                black, white, False, True, channel)

    view_set = views(mesh_document)
    renders: dict[str, tuple[list, dict, dict, Path]] = {}
    render_specs = [
        ("all_plaster_control", control_graph, "hero"),
        ("plaster_concrete_layered", beauty_graph, "hero"),
        ("plaster_concrete_detail", beauty_graph, "detail"),
        ("plaster_concrete_grazing", beauty_graph, "grazing"),
        ("plaster_concrete_repeat", beauty_graph, "hero"),
    ]
    if args.surface_feature_field:
        render_specs.extend(
            (f"{channel}_mask", value[0], "hero")
            for channel, value in feature_mask_graphs.items()
        )
    else:
        render_specs.append(("native_region_mask", mask_graph, "hero"))
    feature_graph_paths = {
        value[0] for value in feature_mask_graphs.values()
    }
    for name, graph, view_name in render_specs:
        renders[name] = render(
            args.render_cli.resolve(), fixture_contract,
            generated, review, name, graph, view_set[view_name],
            region_binding, authored_binding, surface_region,
            (args.surface_feature_field
             if graph == beauty_graph or graph in feature_graph_paths
             else None),
            args.force_render)

    width = fixture_contract["render"]["width"]
    height = fixture_contract["render"]["height"]
    raw_mask = diagnostic_projection(
        mesh_document, surface_region_document, width, height,
        wireframe=False)
    wire = diagnostic_projection(
        mesh_document, surface_region_document, width, height,
        wireframe=True)
    raw_mask_path = review / "raw_authored_region.png"
    wire_path = review / "source_triangle_provenance.png"
    review_artifacts.write_png_rgb(raw_mask_path, width, height, raw_mask)
    review_artifacts.write_png_rgb(wire_path, width, height, wire)

    control_pixels = renders["all_plaster_control"][0]
    layered_pixels = renders["plaster_concrete_layered"][0]
    repeat_pixels = renders["plaster_concrete_repeat"][0]
    changed = changed_pixels(control_pixels, layered_pixels)
    repeat_changed = changed_pixels(layered_pixels, repeat_pixels)
    mask_separation = (
        changed_pixels(
            renders["feature_interior_mask"][0],
            renders["feature_rim_mask"][0])
        if args.surface_feature_field else 0
    )
    assertions = fixture_contract["assertions"]
    failures: list[str] = []
    feature_receipt = (
        load(feature_proof_root / "receipts" / "surface_feature_field.receipt.json")
        if feature_proof_root else None
    )
    for name, (_, audit, metrics, _) in renders.items():
        if audit["triangle_count"] != region_receipt["triangle_count"]:
            failures.append(f"{name}: source triangle count drift")
        if audit["primary_hit_pixels"] < assertions["minimum_primary_hit_pixels"]:
            failures.append(f"{name}: insufficient source-mesh coverage")
        minimum_luma = (
            assertions["minimum_mask_luma_standard_deviation"]
            if name.endswith("_mask") else 8.0
        )
        if metrics["luma_standard_deviation"] < minimum_luma:
            failures.append(f"{name}: visually flat")
    if changed < assertions["minimum_beauty_changed_pixels"]:
        failures.append("layered beauty is too similar to all-plaster control")
    if repeat_changed > assertions["maximum_repeat_changed_pixels"]:
        failures.append("exact repeat pixels differ")
    if args.surface_feature_field and mask_separation == 0:
        failures.append("feature interior and rim masks are identical")
    mask_names = (
        [f"{channel}_mask" for channel in feature_mask_graphs]
        if args.surface_feature_field else ["native_region_mask"]
    )
    for name in mask_names:
        if image_metrics(renders[name][0])["luma_standard_deviation"] < (
                assertions["minimum_mask_luma_standard_deviation"]):
            failures.append(f"{name} is visually flat")
    if region_receipt["transition_vertex_count"] < (
            assertions["minimum_transition_vertices"]):
        failures.append("continuous transition band is undersampled")
    if not (
        region_receipt["topology_unchanged"] and
        region_receipt["source_triangle_provenance_retained"] and
        region_receipt["source_file_digest_sha256"] == digest(mesh)
    ):
        failures.append("source identity or provenance contract failed")
    if feature_receipt:
        if not all(
                population["accepted_count"] == population["requested_count"]
                for population in feature_receipt["populations"]):
            failures.append("feature population count or density target was not met")
        if not all(
                population["declared_radius_range"][0]
                <= population["radius_quantiles"]["minimum"]
                <= population["radius_quantiles"]["maximum"]
                <= population["declared_radius_range"][1]
                for population in feature_receipt["populations"]):
            failures.append("feature population radius quantile is out of range")
        coverage = feature_receipt["coverage"]
        if not (0.03 <= coverage["eligible_measured"] <= 0.55 and
                coverage["clean_base_measured"] >= 0.45 and
                coverage["interior_positive_area_fraction"] > 0.0 and
                coverage["rim_positive_area_fraction"] > 0.0):
            failures.append("feature coverage is outside the bounded acceptance range")
        provenance = feature_receipt["provenance"]
        if not (provenance["all_source_triangles_in_range"] and
                provenance["stable_feature_ids_unique"] and
                provenance["maximum_barycentric_sum_error"] <= 1.0e-8 and
                provenance["maximum_frame_orthonormal_error"] <= 1.0e-8):
            failures.append("feature provenance or local tangent frame is invalid")
        if (feature_receipt["normal_compatibility"]
                ["opposing_fold_incompatible_assignments"] != 0):
            failures.append("normal-incompatible opposing fold assignment detected")
        search = feature_receipt["candidate_search"]
        if not (search["capacity_respected"] and
                search["observed_max_cell_candidates"] < feature_receipt["feature_count"]):
            failures.append("feature candidate search is not bounded")

    if args.surface_feature_field:
        contact = review / "psg24_surface_feature_field_review.png"
        write_labeled_contact_sheet(contact, [
            ("CLEAN PLASTER CONTROL", control_pixels),
            ("SPOT BEAUTY - HERO", layered_pixels),
            ("SPOT BEAUTY - DETAIL", renders["plaster_concrete_detail"][0]),
            ("SPOT BEAUTY - GRAZING", renders["plaster_concrete_grazing"][0]),
            ("SPOT COVERAGE", renders["feature_coverage_mask"][0]),
            ("SPOT INTERIOR", renders["feature_interior_mask"][0]),
            ("SPOT RIM", renders["feature_rim_mask"][0]),
        ], columns=4)
        diagnostic_contact = None
        diagnostic_images = {}
        if feature_proof_root:
            field_document = load(args.surface_feature_field)
            field_analysis = mesh_analysis(
                mesh_document, load(args.surface_feature_authoring)["macro_envelope"])
            projected_diagnostics = {
                "feature_id": feature_id_projection(
                    mesh_document, field_document, width, height),
                "envelope": feature_diagnostic_projection(
                    mesh_document, field_document, field_analysis,
                    width, height, "macro_envelope"),
                "geometric_normal": feature_diagnostic_projection(
                    mesh_document, field_document, field_analysis,
                    width, height, "geometric_normal"),
                "shading_normal": feature_diagnostic_projection(
                    mesh_document, field_document, field_analysis,
                    width, height, "shading_normal"),
            }
            _, _, repeat_pixels_diagnostic = read_ppm_rgb(
                feature_proof_root / "proof" / "repeat_difference.ppm")
            projected_diagnostics["repeat_difference"] = resize_nearest(
                repeat_pixels_diagnostic, width, height)
            for name, resized in projected_diagnostics.items():
                path = review / f"compiler_{name}.png"
                review_artifacts.write_png_rgb(path, width, height, resized)
                diagnostic_images[name] = (resized, path)
            diagnostic_contact = review / "psg24_surface_feature_diagnostics_review.png"
            write_labeled_contact_sheet(diagnostic_contact, [
                ("STABLE FEATURE ROOT IDS", diagnostic_images["feature_id"][0]),
                ("MACRO ENVELOPE WEIGHT", diagnostic_images["envelope"][0]),
                ("GEOMETRIC NORMAL CARRIER", diagnostic_images["geometric_normal"][0]),
                ("SHADING NORMAL CARRIER", diagnostic_images["shading_normal"][0]),
                ("EXACT-REPEAT DIFFERENCE", diagnostic_images["repeat_difference"][0]),
                ("SOURCE TRIANGLE PROVENANCE", wire),
            ], columns=3)
        lineage_contact = review / "psg24_source_lineage_review.png"
        write_labeled_contact_sheet(lineage_contact, [
            ("LEGACY PSG-19 CARRIER - NOT SPOTS", raw_mask),
            ("SOURCE TRIANGLES", wire),
        ], columns=2)
    else:
        contact = review / "psg19_plaster_concrete_high_quality_matrix.png"
        lineage_contact = None
        write_labeled_contact_sheet(contact, [
            ("ALL PLASTER CONTROL", control_pixels),
            ("LAYERED HERO", layered_pixels),
            ("LAYERED DETAIL", renders["plaster_concrete_detail"][0]),
            ("NATIVE REGION MASK", renders["native_region_mask"][0]),
            ("RAW CONTINUOUS FIELD", raw_mask),
            ("SOURCE TRIANGLES", wire),
        ], columns=3)
    summary = {
        "schema": (
            "ray_tracing.surface_feature_fields_psg24a_proof"
            if args.surface_feature_field else
            "ray_tracing.procedural_imported_surface_region_psg19_proof"
        ),
        "schema_version": 1,
        "status": "passed" if not failures else "failed",
        "failures": failures,
        "contract": str(args.contract.resolve()),
        "contract_digest_sha256": digest(args.contract.resolve()),
        "fresh_object": {
            "generation_policy": "generated_from_recipe_every_proof_run",
            "recipe": str(args.recipe.resolve()),
            "recipe_digest_sha256": digest(args.recipe.resolve()),
            "generated_stl": str(durable_stl),
            "generated_stl_digest_sha256": digest(durable_stl),
            "runtime_mesh": str(mesh),
            "runtime_mesh_file_digest_sha256": digest(mesh),
        },
        "surface_region": region_receipt,
        "surface_feature_field": ({
            "path": str(args.surface_feature_field.resolve()),
            "digest_sha256": digest(args.surface_feature_field.resolve()),
            "compile_receipt": feature_receipt,
        } if args.surface_feature_field else None),
        "bindings": {
            "region_binding_digest_sha256":
                region_applied["binding_digest_sha256"],
            "authored_binding_digest_sha256":
                authored_receipt["binding_digest_sha256"],
        },
        "graphs": {
            "beauty": beauty_graph_receipt,
            "control": control_graph_receipt,
            "mask": mask_graph_receipt,
            "feature_masks": {
                name: value[1] for name, value in feature_mask_graphs.items()
            },
        },
        "acceptance": {
            "beauty_changed_pixels": changed,
            "repeat_changed_pixels": repeat_changed,
            "interior_rim_changed_pixels": mask_separation,
            "source_triangle_count": region_receipt["triangle_count"],
            "render_triangle_counts": {
                name: value[1]["triangle_count"]
                for name, value in renders.items()
            },
            "topology_unchanged": region_receipt["topology_unchanged"],
            "source_triangle_provenance_retained":
                region_receipt["source_triangle_provenance_retained"],
            "resolved_environment_lighting": load(
                generated / "raw" / "plaster_concrete_layered"
                / "render_summary.json")["environment_lighting"],
        },
        "images": {
            name: str(value[3]) for name, value in renders.items()
        } | {
            "raw_authored_region": str(raw_mask_path),
            "source_triangle_provenance": str(wire_path),
            "contact_sheet": str(contact),
        } | ({
            "diagnostic_contact_sheet": str(diagnostic_contact),
            **{
                f"compiler_{name}": str(value[1])
                for name, value in diagnostic_images.items()
            },
        } if args.surface_feature_field and diagnostic_contact else {}) | (
            {"source_lineage_contact_sheet": str(lineage_contact)}
             if lineage_contact else {}),
    }
    write_json(output / "proof_summary.json", summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
