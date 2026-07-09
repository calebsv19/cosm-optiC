#!/usr/bin/env python3
"""Generate a headless material-family preview grid for Glass, Mirror, and Metal."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path


FAMILIES = [
    {
        "id": "glass",
        "title": "Glass",
        "material_id": 5,
        "object_color": 0xB6D7FF,
        "alpha": 0.62,
        "roughness": 0.08,
        "reflectivity": 0.02,
        "variants": [
            {"label": "clear_a062", "alpha": 0.62, "roughness": 0.04, "reflectivity": 0.02},
            {"label": "frosted_a062", "alpha": 0.62, "roughness": 0.72, "reflectivity": 0.02},
            {"label": "dense_a082", "alpha": 0.82, "roughness": 0.18, "reflectivity": 0.02},
            {
                "label": "fog_overlay",
                "alpha": 0.68,
                "roughness": 0.24,
                "reflectivity": 0.02,
                "preview_overlay": {
                    "kind": "fog",
                    "opacity": 0.68,
                    "coverage": 0.56,
                    "grain": 0.36,
                    "contrast": 0.44,
                    "surface_damage": 0.52,
                    "color_depth": 0.70,
                    "seed": 53,
                },
            },
        ],
    },
    {
        "id": "mirror",
        "title": "Mirror",
        "material_id": 1,
        "object_color": 0xD8E0EE,
        "alpha": 1.0,
        "roughness": 0.02,
        "reflectivity": 0.96,
        "variants": [
            {"label": "polished_ref096", "roughness": 0.02, "reflectivity": 0.96},
            {"label": "soft_ref088", "roughness": 0.18, "reflectivity": 0.88},
            {"label": "rough_ref072", "roughness": 0.42, "reflectivity": 0.72},
            {
                "label": "oil_overlay",
                "roughness": 0.08,
                "reflectivity": 0.92,
                "preview_overlay": {
                    "kind": "oil",
                    "opacity": 0.70,
                    "coverage": 0.60,
                    "grain": 0.48,
                    "contrast": 0.70,
                    "surface_damage": 0.72,
                    "flow": 0.82,
                    "seed": 67,
                },
            },
        ],
    },
    {
        "id": "metal",
        "title": "Rough Metal",
        "material_id": 2,
        "object_color": 0xB88946,
        "alpha": 1.0,
        "roughness": 0.60,
        "reflectivity": 0.70,
        "variants": [
            {"label": "rough_ref070", "roughness": 0.60, "reflectivity": 0.70},
            {"label": "polished_ref082", "roughness": 0.18, "reflectivity": 0.82},
            {
                "label": "rust_overlay",
                "roughness": 0.74,
                "reflectivity": 0.54,
                "preview_overlay": {
                    "kind": "rust",
                    "opacity": 0.82,
                    "coverage": 0.62,
                    "grain": 0.52,
                    "contrast": 0.72,
                    "surface_damage": 0.70,
                    "color_depth": 0.82,
                    "seed": 31,
                },
            },
            {
                "label": "grime_overlay",
                "roughness": 0.68,
                "reflectivity": 0.58,
                "preview_overlay": {
                    "kind": "grime",
                    "opacity": 0.72,
                    "coverage": 0.58,
                    "grain": 0.64,
                    "contrast": 0.78,
                    "surface_damage": 0.68,
                    "flow": 0.42,
                    "seed": 43,
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
    with fixture.open("r", encoding="utf-8") as f:
        return json.load(f)


def update_family_scene(scene: dict, family: dict) -> dict:
    scene = json.loads(json.dumps(scene))
    scene["scene_id"] = f"material_family_preview_{family['id']}"
    authoring = scene.setdefault("extensions", {}).setdefault("ray_tracing", {}).setdefault("authoring", {})
    object_materials = authoring.setdefault("object_materials", [])
    for entry in object_materials:
        if entry.get("object_id") != "rear_blue_panel":
            continue
        entry["material_id"] = family["material_id"]
        entry["object_color"] = family["object_color"]
        entry["alpha"] = family["alpha"]
        entry["roughness"] = family["roughness"]
        entry["reflectivity"] = family["reflectivity"]
        entry["emissive_strength"] = 0.0
        break
    else:
        object_materials.append(
            {
                "object_id": "rear_blue_panel",
                "material_id": family["material_id"],
                "object_color": family["object_color"],
                "alpha": family["alpha"],
                "roughness": family["roughness"],
                "reflectivity": family["reflectivity"],
                "emissive_strength": 0.0,
            }
        )
    return scene


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


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
    if compression == 3 and bpp != 32:
        raise ValueError(f"unsupported BMP bitfield depth: {path}")
    if compression == 3:
        red_mask = struct.unpack_from("<I", data, 54)[0]
        green_mask = struct.unpack_from("<I", data, 58)[0]
        blue_mask = struct.unpack_from("<I", data, 62)[0]
        alpha_mask = struct.unpack_from("<I", data, 66)[0]
        if (red_mask, green_mask, blue_mask, alpha_mask) != (
            0x00FF0000,
            0x0000FF00,
            0x000000FF,
            0xFF000000,
        ):
            raise ValueError(f"unsupported BMP bitfield masks: {path}")
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
        raise ValueError("family preview row widths do not match")
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


def write_index(output_root: Path, summary: dict) -> None:
    grid_png = Path(summary["grid_png"]).name if summary.get("grid_png") else ""
    lines = [
        "# Material Family Preview Grid",
        "",
        "Generated by `tests/integration/run_ray_tracing_material_family_preview_grid.py`.",
        "",
    ]
    if grid_png:
        lines += [f"![Material family preview grid]({grid_png})", ""]
    lines += [
        "- grid BMP: `material_family_preview_grid.bmp`",
        "- grid summary: `material_family_preview_grid_summary.json`",
        "",
        "## Rows",
        "",
        "| Row | Family | Material Preset | Variants | Artifacts |",
        "| --- | --- | --- | --- | --- |",
    ]
    for index, family in enumerate(summary["families"], start=1):
        variants = ", ".join(f"`{variant}`" for variant in family["variant_labels"])
        artifacts = f"[BMP]({Path(family['preview_bmp']).name})"
        if family.get("preview_png"):
            artifacts += f" / [PNG]({Path(family['preview_png']).name})"
        lines.append(
            f"| {index} | {family['title']} | `{family['material_id']}` | {variants} | {artifacts} |"
        )
    lines.append("")
    (output_root / "index.md").write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=repo_root_from_script())
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--keep", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    output_root = args.output_root or root / "build" / "agent_runs" / "ray_tracing" / "material_family_preview_grid"
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
        "schema": "ray_tracing_material_family_preview_grid_summary_v1",
        "output_root": str(output_root),
        "grid_bmp": str(output_root / "material_family_preview_grid.bmp"),
        "families": [],
    }

    for family in FAMILIES:
        scene_path = output_root / f"{family['id']}_scene_runtime.json"
        request_path = output_root / f"{family['id']}_material_preview_request.json"
        preview_path = output_root / f"{family['id']}_preview.bmp"
        summary_path = output_root / f"{family['id']}_summary.json"
        scene = update_family_scene(fixture, family)
        write_json(scene_path, scene)
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
            "variants": family["variants"],
        }
        write_json(request_path, request)
        run([str(cli), "--request", str(request_path)], cwd=root)
        if not preview_path.exists() or not summary_path.exists():
            raise SystemExit(f"missing preview output for {family['id']}")
        with summary_path.open("r", encoding="utf-8") as f:
            family_summary = json.load(f)
        if family_summary.get("variant_count") != len(family["variants"]):
            raise SystemExit(f"unexpected variant count for {family['id']}")
        preview_png = convert_png(preview_path)
        row_paths.append(preview_path)
        summary["families"].append(
            {
                "id": family["id"],
                "title": family["title"],
                "material_id": family["material_id"],
                "scene_path": str(scene_path),
                "request_path": str(request_path),
                "preview_bmp": str(preview_path),
                "preview_png": str(preview_png) if preview_png else "",
                "summary_path": str(summary_path),
                "variant_labels": [variant["label"] for variant in family["variants"]],
            }
        )

    grid_bmp = output_root / "material_family_preview_grid.bmp"
    grid_width, grid_height = stitch_rows(row_paths, grid_bmp)
    grid_png = convert_png(grid_bmp)
    summary["grid_width"] = grid_width
    summary["grid_height"] = grid_height
    summary["grid_png"] = str(grid_png) if grid_png else ""
    write_json(output_root / "material_family_preview_grid_summary.json", summary)
    write_index(output_root, summary)
    print(f"material family preview grid ready: {grid_bmp}")
    if grid_png:
        print(f"material family preview grid png: {grid_png}")
    print(f"index: {output_root / 'index.md'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
