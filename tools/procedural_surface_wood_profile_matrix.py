#!/usr/bin/env python3
"""Build a texture-only versus physical wood-height review matrix."""
from __future__ import annotations

import argparse
from pathlib import Path

from procedural_surface_feature_relief_visual_proof import review_artifacts


def resized(image, width: int, height: int):
    source_width, source_height, pixels = image
    return [[pixels[min(source_height - 1, y * source_height // height)]
             [min(source_width - 1, x * source_width // width)]
             for x in range(width)] for y in range(height)]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--texture-only-bmp", type=Path, required=True)
    parser.add_argument("--physical-height-bmp", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    texture = review_artifacts.read_bmp_rgb(args.texture_only_bmp)
    physical = review_artifacts.read_bmp_rgb(args.physical_height_bmp)
    cell_width, cell_height, header = 720, 480, 0
    canvas = [[[32, 34, 37] for _ in range(cell_width * 2 + 24)]
              for _ in range(cell_height + header)]
    for offset, image in ((0, texture), (cell_width + 24, physical)):
        scaled = resized(image, cell_width, cell_height)
        for y, row in enumerate(scaled):
            canvas[y + header][offset:offset + cell_width] = row
    args.out.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(args.out, len(canvas[0]), len(canvas), canvas)
    print(args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
