#!/usr/bin/env python3
"""Generate and promote the M12 layer-control material preview grid."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path


PROOF_ID = "m12_s5_layer_control_preview_grid"

ROWS = [
    {
        "id": "metal_opacity",
        "title": "Rough Metal - Rust Opacity",
        "material_id": 2,
        "object_color": 0xB88946,
        "alpha": 1.0,
        "roughness": 0.52,
        "reflectivity": 0.72,
        "variants": [
            {"label": "base_no_overlay", "roughness": 0.52, "reflectivity": 0.72},
            {
                "label": "rust_op025",
                "preview_overlay": {
                    "kind": "rust",
                    "opacity": 0.25,
                    "strength": 0.80,
                    "coverage": 0.62,
                    "grain": 0.52,
                    "contrast": 0.72,
                    "color_depth": 0.82,
                    "surface_damage": 0.70,
                    "seed": 31,
                    "roughness_influence": 0.70,
                    "reflectivity_influence": -0.35,
                    "specular_influence": -0.25,
                },
            },
            {
                "label": "rust_op060",
                "preview_overlay": {
                    "kind": "rust",
                    "opacity": 0.60,
                    "strength": 0.80,
                    "coverage": 0.62,
                    "grain": 0.52,
                    "contrast": 0.72,
                    "color_depth": 0.82,
                    "surface_damage": 0.70,
                    "seed": 31,
                    "roughness_influence": 0.70,
                    "reflectivity_influence": -0.35,
                    "specular_influence": -0.25,
                },
            },
            {
                "label": "rust_op095",
                "preview_overlay": {
                    "kind": "rust",
                    "opacity": 0.95,
                    "strength": 0.80,
                    "coverage": 0.62,
                    "grain": 0.52,
                    "contrast": 0.72,
                    "color_depth": 0.82,
                    "surface_damage": 0.70,
                    "seed": 31,
                    "roughness_influence": 0.70,
                    "reflectivity_influence": -0.35,
                    "specular_influence": -0.25,
                },
            },
        ],
    },
    {
        "id": "metal_influence",
        "title": "Rough Metal - Signed Influence",
        "material_id": 2,
        "object_color": 0xB88946,
        "alpha": 1.0,
        "roughness": 0.46,
        "reflectivity": 0.78,
        "variants": [
            {"label": "neutral_ref078", "roughness": 0.46, "reflectivity": 0.78},
            {
                "label": "attenuate_refl",
                "preview_overlay": {
                    "kind": "grime",
                    "opacity": 0.78,
                    "strength": 0.86,
                    "coverage": 0.58,
                    "grain": 0.64,
                    "contrast": 0.78,
                    "surface_damage": 0.68,
                    "flow": 0.42,
                    "seed": 43,
                    "roughness_influence": 0.82,
                    "reflectivity_influence": -0.55,
                    "specular_influence": -0.45,
                    "diffuse_influence": 0.20,
                },
            },
            {
                "label": "boost_spec",
                "preview_overlay": {
                    "kind": "oil",
                    "opacity": 0.76,
                    "strength": 0.86,
                    "coverage": 0.60,
                    "grain": 0.48,
                    "contrast": 0.70,
                    "surface_damage": 0.72,
                    "flow": 0.82,
                    "seed": 67,
                    "roughness_influence": -0.40,
                    "reflectivity_influence": 0.92,
                    "specular_influence": 0.88,
                    "diffuse_influence": -0.30,
                },
            },
            {
                "label": "strength_035",
                "preview_overlay": {
                    "kind": "rust",
                    "opacity": 0.86,
                    "strength": 0.35,
                    "coverage": 0.62,
                    "grain": 0.52,
                    "contrast": 0.72,
                    "surface_damage": 0.70,
                    "seed": 31,
                    "roughness_influence": 0.74,
                    "reflectivity_influence": -0.45,
                },
            },
        ],
    },
    {
        "id": "family_layer_examples",
        "title": "Family Layer Examples",
        "material_id": 5,
        "object_color": 0xB6D7FF,
        "alpha": 0.68,
        "roughness": 0.18,
        "reflectivity": 0.04,
        "variants": [
            {
                "label": "glass_fog_trans",
                "alpha": 0.68,
                "roughness": 0.18,
                "reflectivity": 0.04,
                "preview_overlay": {
                    "kind": "fog",
                    "opacity": 0.74,
                    "strength": 0.84,
                    "coverage": 0.56,
                    "grain": 0.36,
                    "contrast": 0.44,
                    "color_depth": 0.70,
                    "surface_damage": 0.52,
                    "seed": 53,
                    "roughness_influence": 0.68,
                    "transparency_influence": -0.30,
                },
            },
            {
                "label": "mirror_oil_spec",
                "material_id": 1,
                "object_color": 0xD8E0EE,
                "alpha": 1.0,
                "roughness": 0.08,
                "reflectivity": 0.92,
                "preview_overlay": {
                    "kind": "oil",
                    "opacity": 0.70,
                    "strength": 0.82,
                    "coverage": 0.60,
                    "grain": 0.48,
                    "contrast": 0.70,
                    "surface_damage": 0.72,
                    "flow": 0.82,
                    "seed": 67,
                    "roughness_influence": -0.30,
                    "reflectivity_influence": 0.94,
                    "specular_influence": 0.92,
                },
            },
            {
                "label": "metal_rust_rough",
                "material_id": 2,
                "object_color": 0xB88946,
                "alpha": 1.0,
                "roughness": 0.58,
                "reflectivity": 0.70,
                "preview_overlay": {
                    "kind": "rust",
                    "opacity": 0.82,
                    "strength": 0.84,
                    "coverage": 0.62,
                    "grain": 0.52,
                    "contrast": 0.72,
                    "surface_damage": 0.70,
                    "color_depth": 0.82,
                    "seed": 31,
                    "roughness_influence": 0.82,
                    "reflectivity_influence": -0.38,
                    "specular_influence": -0.30,
                },
            },
            {
                "label": "metal_grime_mute",
                "material_id": 2,
                "object_color": 0xB88946,
                "alpha": 1.0,
                "roughness": 0.58,
                "reflectivity": 0.70,
                "preview_overlay": {
                    "kind": "grime",
                    "opacity": 0.55,
                    "strength": 0.70,
                    "coverage": 0.54,
                    "grain": 0.64,
                    "contrast": 0.78,
                    "surface_damage": 0.68,
                    "seed": 43,
                    "roughness_influence": 0.64,
                    "reflectivity_influence": -0.52,
                    "diffuse_influence": 0.25,
                },
            },
        ],
    },
]


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[2]


def tool_path(root: Path) -> Path:
    arch = os.uname().machine
    toolchain = root / "build" / "toolchains" / "clang" / arch / "tools" / "cli" / "ray_tracing_material_preview_headless"
    fallback = root / "build" / arch / "tools" / "cli" / "ray_tracing_material_preview_headless"
    return toolchain if toolchain.exists() else fallback


def load_fixture(root: Path) -> dict:
    fixture = root / "tests" / "fixtures" / "disney_v2_visual_matrix" / "transparent_interior_stack" / "scene_runtime.json"
    with fixture.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def update_scene(scene: dict, row: dict, variant: dict | None = None) -> dict:
    scene = json.loads(json.dumps(scene))
    source = row.copy()
    if variant:
        for key in ("material_id", "object_color", "alpha", "roughness", "reflectivity"):
            if key in variant:
                source[key] = variant[key]
    scene["scene_id"] = f"{PROOF_ID}_{row['id']}"
    authoring = scene.setdefault("extensions", {}).setdefault("ray_tracing", {}).setdefault("authoring", {})
    object_materials = authoring.setdefault("object_materials", [])
    for entry in object_materials:
        if entry.get("object_id") != "rear_blue_panel":
            continue
        entry["material_id"] = source["material_id"]
        entry["object_color"] = source["object_color"]
        entry["alpha"] = source["alpha"]
        entry["roughness"] = source["roughness"]
        entry["reflectivity"] = source["reflectivity"]
        entry["emissive_strength"] = 0.0
        break
    else:
        object_materials.append(
            {
                "object_id": "rear_blue_panel",
                "material_id": source["material_id"],
                "object_color": source["object_color"],
                "alpha": source["alpha"],
                "roughness": source["roughness"],
                "reflectivity": source["reflectivity"],
                "emissive_strength": 0.0,
            }
        )
    return scene


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
        handle.write("\n")


def run(cmd: list[str], cwd: Path) -> None:
    subprocess.check_call(cmd, cwd=str(cwd))


def convert_png(path: Path) -> Path | None:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        return None
    png = path.with_suffix(".png")
    try:
        subprocess.check_call([ffmpeg, "-y", "-i", str(path), str(png)],
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError:
        return None
    return png


def read_bmp(path: Path) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"not a BMP: {path}")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    if dib_size < 40:
        raise ValueError(f"unsupported BMP header: {path}")
    width = struct.unpack_from("<i", data, 18)[0]
    height_signed = struct.unpack_from("<i", data, 22)[0]
    planes = struct.unpack_from("<H", data, 26)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if planes != 1 or compression not in (0, 3) or bpp not in (24, 32) or width <= 0 or height_signed == 0:
        raise ValueError(f"unsupported BMP format: {path}")
    height = abs(height_signed)
    top_down = height_signed < 0
    stride = ((width * bpp + 31) // 32) * 4
    pixels: list[tuple[int, int, int, int]] = [(0, 0, 0, 255)] * (width * height)
    for src_row in range(height):
        dest_row = src_row if top_down else height - 1 - src_row
        row_offset = pixel_offset + src_row * stride
        for x in range(width):
            px = row_offset + x * (bpp // 8)
            b = data[px]
            g = data[px + 1]
            r = data[px + 2]
            a = data[px + 3] if bpp == 32 else 255
            pixels[dest_row * width + x] = (r, g, b, a)
    return width, height, pixels


def write_bmp(path: Path, width: int, height: int, pixels: list[tuple[int, int, int, int]]) -> None:
    row_bytes = width * 4
    pixel_bytes = row_bytes * height
    file_size = 14 + 40 + pixel_bytes
    out = bytearray()
    out += b"BM"
    out += struct.pack("<IHHI", file_size, 0, 0, 54)
    out += struct.pack("<IiiHHIIiiII", 40, width, height, 1, 32, 0, pixel_bytes, 2835, 2835, 0, 0)
    for row in range(height - 1, -1, -1):
        for x in range(width):
            r, g, b, a = pixels[row * width + x]
            out += bytes((b, g, r, a))
    path.write_bytes(out)


def stitch_rows(paths: list[Path], output_path: Path) -> tuple[int, int]:
    images = [read_bmp(path) for path in paths]
    widths = {image[0] for image in images}
    if len(widths) != 1:
        raise ValueError("preview row widths do not match")
    width = images[0][0]
    height = sum(image[1] for image in images)
    pixels: list[tuple[int, int, int, int]] = [(32, 34, 38, 255)] * (width * height)
    y_cursor = 0
    for image_width, image_height, image_pixels in images:
        for y in range(image_height):
            dest_start = (y_cursor + y) * width
            src_start = y * image_width
            pixels[dest_start:dest_start + width] = image_pixels[src_start:src_start + image_width]
        y_cursor += image_height
    write_bmp(output_path, width, height, pixels)
    return width, height


def redact_json(root: Path, src: Path, dst: Path) -> None:
    run([sys.executable, str(root / "tools" / "redact_public_json.py"), str(src), str(dst)], cwd=root)


def write_index(output_root: Path, summary: dict) -> None:
    lines = [
        "# M12-S5 Layer Control Preview Grid",
        "",
        "Generated material preview set for the M12 layer-control visual proof promotion.",
        "",
        "![Layer control preview grid](preview.png)",
        "",
        f"- proof id: `{PROOF_ID}`",
        "- rendered preview: `preview.bmp`",
        "- web preview: `preview.png`",
        "- effective summary: `summary.json`",
        "- source command: `make -C ray_tracing test-ray-tracing-material-layer-control-preview-grid`",
        "",
        "## Variant Order",
        "",
        "Contact-sheet cells are ordered left-to-right inside each row.",
        "",
        "| Row | Focus | Variants | Row Artifacts |",
        "| --- | --- | --- | --- |",
    ]
    for index, row in enumerate(summary["rows"], start=1):
        variants = ", ".join(f"`{variant}`" for variant in row["variant_labels"])
        row_id = row["id"]
        lines.append(
            f"| {index} | {row['title']} | {variants} | [BMP]({row_id}_preview.bmp) / [PNG]({row_id}_preview.png) |"
        )
    lines += [
        "",
        "## Request Readback",
        "",
    ]
    for row in summary["rows"]:
        row_id = row["id"]
        lines.append(f"- {row['title']}: `{row_id}_request.json`; summary: `{row_id}_summary.json`")
    lines += [
        "",
        "This set promotes M12 layer-control proof. It demonstrates layer opacity,",
        "placement strength, and signed response influence changes through the",
        "headless Material preview path. It does not promote first-class metallic",
        "or change runtime payload semantics.",
        "",
    ]
    (output_root / "index.md").write_text("\n".join(lines), encoding="utf-8")


def publish_set(root: Path, build_root: Path, docs_root: Path, summary: dict) -> None:
    if docs_root.exists():
        shutil.rmtree(docs_root)
    docs_root.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build_root / "layer_control_preview_grid.bmp", docs_root / "preview.bmp")
    if (build_root / "layer_control_preview_grid.png").exists():
        shutil.copy2(build_root / "layer_control_preview_grid.png", docs_root / "preview.png")
    redact_json(root, build_root / "layer_control_preview_grid_summary.json", docs_root / "summary.json")
    for row in summary["rows"]:
        row_id = row["id"]
        shutil.copy2(build_root / f"{row_id}_preview.bmp", docs_root / f"{row_id}_preview.bmp")
        if (build_root / f"{row_id}_preview.png").exists():
            shutil.copy2(build_root / f"{row_id}_preview.png", docs_root / f"{row_id}_preview.png")
        redact_json(root, build_root / f"{row_id}_request.json", docs_root / f"{row_id}_request.json")
        redact_json(root, build_root / f"{row_id}_summary.json", docs_root / f"{row_id}_summary.json")
    write_index(docs_root, summary)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=repo_root_from_script())
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--publish-docs", action="store_true")
    parser.add_argument("--keep", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    output_root = args.output_root or root / "build" / "agent_runs" / "ray_tracing" / "layer_control_preview_grid"
    output_root = output_root.resolve()
    cli = tool_path(root)
    if not cli.exists():
        subprocess.check_call(["make", "-C", str(root), "ray-tracing-material-preview-headless"])
    if not cli.exists():
        raise SystemExit(f"material preview CLI not found: {cli}")
    if output_root.exists() and not args.keep:
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    fixture = load_fixture(root)
    row_paths: list[Path] = []
    summary = {
        "schema": "ray_tracing_material_layer_control_preview_grid_summary_v1",
        "proof_id": PROOF_ID,
        "output_root": str(output_root),
        "grid_bmp": str(output_root / "layer_control_preview_grid.bmp"),
        "rows": [],
    }
    for row in ROWS:
        scene_path = output_root / f"{row['id']}_scene_runtime.json"
        request_path = output_root / f"{row['id']}_request.json"
        preview_path = output_root / f"{row['id']}_preview.bmp"
        summary_path = output_root / f"{row['id']}_summary.json"
        scene = update_scene(fixture, row)
        write_json(scene_path, scene)
        variants = []
        for variant in row["variants"]:
            variant_copy = json.loads(json.dumps(variant))
            variants.append(variant_copy)
        request = {
            "schema": "ray_tracing_material_preview_request_v1",
            "runtime_scene_path": str(scene_path),
            "object_id": "rear_blue_panel",
            "width": 144,
            "height": 144,
            "columns": 4,
            "background_color": 0x292D35,
            "output_path": str(preview_path),
            "summary_path": str(summary_path),
            "variants": variants,
        }
        write_json(request_path, request)
        run([str(cli), "--request", str(request_path)], cwd=root)
        if not preview_path.exists() or not summary_path.exists():
            raise SystemExit(f"missing preview output for {row['id']}")
        with summary_path.open("r", encoding="utf-8") as handle:
            row_summary = json.load(handle)
        if row_summary.get("variant_count") != len(row["variants"]):
            raise SystemExit(f"unexpected variant count for {row['id']}")
        preview_png = convert_png(preview_path)
        row_paths.append(preview_path)
        summary["rows"].append(
            {
                "id": row["id"],
                "title": row["title"],
                "request_path": str(request_path),
                "preview_bmp": str(preview_path),
                "preview_png": str(preview_png) if preview_png else "",
                "summary_path": str(summary_path),
                "variant_labels": [variant["label"] for variant in row["variants"]],
            }
        )

    grid_bmp = output_root / "layer_control_preview_grid.bmp"
    grid_width, grid_height = stitch_rows(row_paths, grid_bmp)
    grid_png = convert_png(grid_bmp)
    summary["grid_width"] = grid_width
    summary["grid_height"] = grid_height
    summary["grid_png"] = str(grid_png) if grid_png else ""
    write_json(output_root / "layer_control_preview_grid_summary.json", summary)
    write_index(output_root, summary)
    if args.publish_docs:
        publish_set(root, output_root, root / "docs" / "material_preview_sets" / PROOF_ID, summary)
    print(f"layer control preview grid ready: {grid_bmp}")
    if grid_png:
        print(f"layer control preview grid png: {grid_png}")
    print(f"index: {output_root / 'index.md'}")
    if args.publish_docs:
        print(f"published: {root / 'docs' / 'material_preview_sets' / PROOF_ID}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
