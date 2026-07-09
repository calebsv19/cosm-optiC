#!/usr/bin/env python3
import argparse
import json
import math
import struct
import zlib
from pathlib import Path


def read_bmp_rgb(path):
    data = Path(path).read_bytes()
    if len(data) < 54 or data[0:2] != b"BM":
        raise ValueError(f"{path}: not a BMP file")

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    header_size = struct.unpack_from("<I", data, 14)[0]
    if header_size < 40:
        raise ValueError(f"{path}: unsupported BMP header")

    width = struct.unpack_from("<i", data, 18)[0]
    height_signed = struct.unpack_from("<i", data, 22)[0]
    planes = struct.unpack_from("<H", data, 26)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    supported_rgb = bpp == 24 and compression == 0
    supported_bgra = bpp == 32 and (
        compression == 0 or
        (compression == 3 and header_size >= 56 and
         struct.unpack_from("<I", data, 54)[0] == 0x00ff0000 and
         struct.unpack_from("<I", data, 58)[0] == 0x0000ff00 and
         struct.unpack_from("<I", data, 62)[0] == 0x000000ff)
    )
    if width <= 0 or height_signed == 0 or planes != 1 or not (supported_rgb or supported_bgra):
        raise ValueError(f"{path}: unsupported BMP layout")

    height = abs(height_signed)
    top_down = height_signed < 0
    row_stride = ((width * bpp + 31) // 32) * 4
    pixels = []
    for y in range(height):
        source_y = y if top_down else height - 1 - y
        row_base = pixel_offset + source_y * row_stride
        row = []
        for x in range(width):
            base = row_base + x * (bpp // 8)
            b, g, r = data[base], data[base + 1], data[base + 2]
            row.append((r, g, b))
        pixels.append(row)
    return width, height, pixels


def write_png_rgb(path, width, height, pixels):
    raw = bytearray()
    for row in pixels:
        raw.append(0)
        for r, g, b in row:
            raw.extend((r, g, b))

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) +
                kind +
                payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    Path(path).write_bytes(png)


def clamp_u8(value):
    return max(0, min(255, int(value)))


def abs_diff_pixels(left, right, amplification):
    height = len(left)
    width = len(left[0]) if height else 0
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            lr, lg, lb = left[y][x]
            rr, rg, rb = right[y][x]
            row.append((
                clamp_u8(abs(rr - lr) * amplification),
                clamp_u8(abs(rg - lg) * amplification),
                clamp_u8(abs(rb - lb) * amplification),
            ))
        rows.append(row)
    return rows


def side_by_side(left, right, diff, separator=8):
    height = len(left)
    width = len(left[0]) if height else 0
    sep = [(32, 32, 32)] * separator
    rows = []
    for y in range(height):
        rows.append(left[y] + sep + right[y] + sep + diff[y])
    return width * 3 + separator * 2, height, rows


def diff_metrics(left, right):
    height = len(left)
    width = len(left[0]) if height else 0
    pixels = width * height
    sum_abs = [0, 0, 0]
    sum_sq = 0.0
    changed_pixels = 0
    changed_gt_2 = 0
    changed_gt_8 = 0
    max_abs = 0
    max_luma = 0.0
    sum_abs_luma = 0.0
    for y in range(height):
        for x in range(width):
            diffs = [abs(right[y][x][i] - left[y][x][i]) for i in range(3)]
            local_max = max(diffs)
            if local_max > 0:
                changed_pixels += 1
            if local_max > 2:
                changed_gt_2 += 1
            if local_max > 8:
                changed_gt_8 += 1
            for i, value in enumerate(diffs):
                sum_abs[i] += value
                sum_sq += float(value * value)
            max_abs = max(max_abs, local_max)
            luma = 0.2126 * diffs[0] + 0.7152 * diffs[1] + 0.0722 * diffs[2]
            sum_abs_luma += luma
            max_luma = max(max_luma, luma)

    denom = float(max(1, pixels))
    return {
        "pixels": pixels,
        "changed_pixels": changed_pixels,
        "changed_pixel_ratio": changed_pixels / denom,
        "changed_pixels_gt_2": changed_gt_2,
        "changed_pixels_gt_2_ratio": changed_gt_2 / denom,
        "changed_pixels_gt_8": changed_gt_8,
        "changed_pixels_gt_8_ratio": changed_gt_8 / denom,
        "mean_abs_rgb": [value / denom for value in sum_abs],
        "mean_abs_all_channels": sum(sum_abs) / (denom * 3.0),
        "mean_abs_luma": sum_abs_luma / denom,
        "rms_rgb_error": math.sqrt(sum_sq / (denom * 3.0)),
        "max_abs_channel_delta": max_abs,
        "max_luma_delta": max_luma,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--before-bmp", required=True)
    parser.add_argument("--after-bmp", required=True)
    parser.add_argument("--before-label", default="before")
    parser.add_argument("--after-label", default="after")
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()

    before_w, before_h, before = read_bmp_rgb(args.before_bmp)
    after_w, after_h, after = read_bmp_rgb(args.after_bmp)
    if (before_w, before_h) != (after_w, after_h):
        raise ValueError("input images must have matching dimensions")

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    before_png = out_dir / f"{args.before_label}.png"
    after_png = out_dir / f"{args.after_label}.png"
    diff4_png = out_dir / "diff_abs_amplified4x.png"
    diff8_png = out_dir / "diff_abs_amplified8x.png"
    side_png = out_dir / f"side_by_side_{args.before_label}_{args.after_label}_diff4x.png"
    metrics_json = out_dir / "diff_metrics.json"

    diff4 = abs_diff_pixels(before, after, 4)
    diff8 = abs_diff_pixels(before, after, 8)
    side_w, side_h, side = side_by_side(before, after, diff4)
    metrics = diff_metrics(before, after)
    metrics.update({
        "before_bmp": str(Path(args.before_bmp).resolve()),
        "after_bmp": str(Path(args.after_bmp).resolve()),
        "before_png": str(before_png.resolve()),
        "after_png": str(after_png.resolve()),
        "diff_png": str(diff4_png.resolve()),
        "diff_8x_png": str(diff8_png.resolve()),
        "side_by_side_png": str(side_png.resolve()),
        "diff_amplification": 4,
        "diff8": {"diff_amplification": 8},
    })

    write_png_rgb(before_png, before_w, before_h, before)
    write_png_rgb(after_png, after_w, after_h, after)
    write_png_rgb(diff4_png, before_w, before_h, diff4)
    write_png_rgb(diff8_png, before_w, before_h, diff8)
    write_png_rgb(side_png, side_w, side_h, side)
    metrics_json.write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(metrics, indent=2))


if __name__ == "__main__":
    main()
