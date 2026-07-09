#!/usr/bin/env python3
"""Analyze whether an A/B visual diff clusters around tiled-renderer borders."""

import argparse
import json
from pathlib import Path

import generate_ray_tracing_denoise_review_artifacts as review_artifacts


def luma_delta(a, b):
    return (
        0.2126 * abs(a[0] - b[0]) +
        0.7152 * abs(a[1] - b[1]) +
        0.0722 * abs(a[2] - b[2])
    )


def max_delta(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2]))


def is_tile_border_pixel(x, y, width, height, tile_size, margin):
    if tile_size <= 0:
        return False
    near_vertical = False
    near_horizontal = False
    for boundary_x in range(tile_size, width, tile_size):
        if abs(x - boundary_x) <= margin:
            near_vertical = True
            break
    for boundary_y in range(tile_size, height, tile_size):
        if abs(y - boundary_y) <= margin:
            near_horizontal = True
            break
    return near_vertical or near_horizontal


def init_bucket():
    return {
        "pixels": 0,
        "changed_pixels": 0,
        "changed_pixels_gt_2": 0,
        "changed_pixels_gt_8": 0,
        "max_abs_channel_delta": 0,
        "max_luma_delta": 0.0,
        "sum_abs_luma": 0.0,
    }


def note_pixel(bucket, delta, luma):
    bucket["pixels"] += 1
    bucket["sum_abs_luma"] += luma
    if delta > 0:
        bucket["changed_pixels"] += 1
    if delta > 2:
        bucket["changed_pixels_gt_2"] += 1
    if delta > 8:
        bucket["changed_pixels_gt_8"] += 1
    if delta > bucket["max_abs_channel_delta"]:
        bucket["max_abs_channel_delta"] = delta
    if luma > bucket["max_luma_delta"]:
        bucket["max_luma_delta"] = luma


def finish_bucket(bucket):
    pixels = max(1, bucket["pixels"])
    bucket["changed_pixel_ratio"] = bucket["changed_pixels"] / pixels
    bucket["changed_pixels_gt_2_ratio"] = bucket["changed_pixels_gt_2"] / pixels
    bucket["changed_pixels_gt_8_ratio"] = bucket["changed_pixels_gt_8"] / pixels
    bucket["mean_abs_luma"] = bucket["sum_abs_luma"] / pixels
    del bucket["sum_abs_luma"]
    return bucket


def overlay_pixels(before, after, tile_size, margin):
    height = len(after)
    width = len(after[0]) if height else 0
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            pixel = after[y][x]
            delta = max_delta(before[y][x], after[y][x])
            border = is_tile_border_pixel(x, y, width, height, tile_size, margin)
            on_grid_line = (
                (tile_size > 0 and x > 0 and x % tile_size == 0) or
                (tile_size > 0 and y > 0 and y % tile_size == 0)
            )
            if delta > 2:
                row.append((255, 0, 255))
            elif delta > 0:
                row.append((255, 224, 0) if border else (255, 255, 255))
            elif on_grid_line:
                row.append((0, 192, 255))
            elif border:
                r, g, b = pixel
                row.append((min(255, int(r * 0.65) + 20),
                            min(255, int(g * 0.65) + 40),
                            min(255, int(b * 0.65) + 60)))
            else:
                row.append(pixel)
        rows.append(row)
    return rows


def analyze(before, after, tile_size, margin):
    height = len(before)
    width = len(before[0]) if height else 0
    buckets = {
        "all": init_bucket(),
        "tile_border_band": init_bucket(),
        "tile_interior": init_bucket(),
    }
    changed_on_border = 0
    changed_interior = 0

    for y in range(height):
        for x in range(width):
            delta = max_delta(before[y][x], after[y][x])
            luma = luma_delta(before[y][x], after[y][x])
            border = is_tile_border_pixel(x, y, width, height, tile_size, margin)
            note_pixel(buckets["all"], delta, luma)
            note_pixel(buckets["tile_border_band" if border else "tile_interior"], delta, luma)
            if delta > 0:
                if border:
                    changed_on_border += 1
                else:
                    changed_interior += 1

    for bucket in buckets.values():
        finish_bucket(bucket)

    all_changed = max(1, buckets["all"]["changed_pixels"])
    border_pixels = max(1, buckets["tile_border_band"]["pixels"])
    interior_pixels = max(1, buckets["tile_interior"]["pixels"])
    border_changed_ratio = buckets["tile_border_band"]["changed_pixels"] / border_pixels
    interior_changed_ratio = buckets["tile_interior"]["changed_pixels"] / interior_pixels
    return {
        "schema_version": "ray_tracing_tile_border_diff_audit_v1",
        "width": width,
        "height": height,
        "tile_size": tile_size,
        "border_margin_pixels": margin,
        "buckets": buckets,
        "changed_pixel_distribution": {
            "changed_on_border_band": changed_on_border,
            "changed_interior": changed_interior,
            "changed_on_border_band_ratio_of_changed": changed_on_border / all_changed,
            "changed_interior_ratio_of_changed": changed_interior / all_changed,
        },
        "border_clustering": {
            "border_pixel_ratio": buckets["tile_border_band"]["pixels"] /
                                  max(1, buckets["all"]["pixels"]),
            "border_changed_pixel_ratio": border_changed_ratio,
            "interior_changed_pixel_ratio": interior_changed_ratio,
            "border_to_interior_changed_ratio":
                border_changed_ratio / max(1.0e-12, interior_changed_ratio),
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--before-bmp", required=True)
    parser.add_argument("--after-bmp", required=True)
    parser.add_argument("--tile-size", type=int, required=True)
    parser.add_argument("--margin", type=int, default=1)
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()

    before_w, before_h, before = review_artifacts.read_bmp_rgb(args.before_bmp)
    after_w, after_h, after = review_artifacts.read_bmp_rgb(args.after_bmp)
    if (before_w, before_h) != (after_w, after_h):
        raise ValueError("input images must have matching dimensions")

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    audit = analyze(before, after, args.tile_size, args.margin)
    overlay = overlay_pixels(before, after, args.tile_size, args.margin)

    audit_path = out_dir / "tile_border_diff_audit.json"
    overlay_path = out_dir / "tile_border_changed_overlay.png"
    audit["overlay_png"] = str(overlay_path.resolve())
    audit["before_bmp"] = str(Path(args.before_bmp).resolve())
    audit["after_bmp"] = str(Path(args.after_bmp).resolve())
    audit_path.write_text(json.dumps(audit, indent=2) + "\n", encoding="utf-8")
    review_artifacts.write_png_rgb(overlay_path, before_w, before_h, overlay)
    print(json.dumps(audit, indent=2))


if __name__ == "__main__":
    main()
