#!/usr/bin/env python3
import argparse
import json
import struct
from pathlib import Path


def fail(message):
    raise SystemExit(message)


def read_bmp_pixels(path):
    data = Path(path).read_bytes()
    if len(data) < 54 or data[0:2] != b"BM":
        fail(f"{path}: not a BMP file")

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    header_size = struct.unpack_from("<I", data, 14)[0]
    if header_size < 40:
        fail(f"{path}: unsupported BMP header")

    width = struct.unpack_from("<i", data, 18)[0]
    height_signed = struct.unpack_from("<i", data, 22)[0]
    planes = struct.unpack_from("<H", data, 26)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if width <= 0 or height_signed == 0 or planes != 1:
        fail(f"{path}: invalid BMP dimensions or planes")

    supported_rgb = bpp == 24 and compression == 0
    supported_bgra = bpp == 32 and (
        compression == 0
        or (
            compression == 3
            and header_size >= 56
            and len(data) >= 66
            and struct.unpack_from("<I", data, 54)[0] == 0x00FF0000
            and struct.unpack_from("<I", data, 58)[0] == 0x0000FF00
            and struct.unpack_from("<I", data, 62)[0] == 0x000000FF
        )
    )
    if not (supported_rgb or supported_bgra):
        fail(f"{path}: unsupported BMP layout")

    height = abs(height_signed)
    bytes_per_pixel = bpp // 8
    row_stride = ((width * bpp + 31) // 32) * 4
    required_size = pixel_offset + row_stride * height
    if len(data) < required_size:
        fail(f"{path}: truncated BMP pixel data")

    pixels = []
    for y in range(height):
        row_start = pixel_offset + y * row_stride
        for x in range(width):
            pixel_start = row_start + x * bytes_per_pixel
            b = data[pixel_start]
            g = data[pixel_start + 1]
            r = data[pixel_start + 2]
            pixels.append((r, g, b))

    return width, height, pixels


def write_blank_bmp(path, width=4, height=4):
    row_stride = ((width * 3 + 3) // 4) * 4
    pixel_bytes = bytes(row_stride * height)
    file_size = 54 + len(pixel_bytes)
    header = bytearray()
    header.extend(b"BM")
    header.extend(struct.pack("<IHHI", file_size, 0, 0, 54))
    header.extend(struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, len(pixel_bytes), 2835, 2835, 0, 0))
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(bytes(header) + pixel_bytes)


def metrics_for(path):
    width, height, pixels = read_bmp_pixels(path)
    nonzero_pixels = sum(1 for r, g, b in pixels if r != 0 or g != 0 or b != 0)
    max_channel = max(max(pixel) for pixel in pixels) if pixels else 0
    if nonzero_pixels <= 0 or max_channel <= 0:
        fail(f"{path}: blank BMP artifact")
    return {
        "path": str(Path(path).resolve()),
        "width": width,
        "height": height,
        "pixels": len(pixels),
        "nonzero_pixels": nonzero_pixels,
        "max_channel": max_channel,
    }


def main():
    parser = argparse.ArgumentParser(description="Validate the ray_tracing R6 visual artifact BMP.")
    parser.add_argument("--frame", required=True, help="BMP frame to validate")
    parser.add_argument("--write-metrics", help="Optional JSON metrics output")
    parser.add_argument("--make-blank-probe", help="Write a known blank BMP and verify it is rejected")
    args = parser.parse_args()

    metrics = metrics_for(args.frame)

    if args.make_blank_probe:
        blank_path = Path(args.make_blank_probe)
        write_blank_bmp(blank_path)
        try:
            metrics_for(blank_path)
        except SystemExit:
            pass
        else:
            fail(f"{blank_path}: blank probe unexpectedly passed validation")

    if args.write_metrics:
        metrics_path = Path(args.write_metrics)
        metrics_path.parent.mkdir(parents=True, exist_ok=True)
        metrics_path.write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(
        "ray_tracing visual artifact validation passed: "
        f"{metrics['path']} width={metrics['width']} height={metrics['height']} "
        f"nonzero_pixels={metrics['nonzero_pixels']} max_channel={metrics['max_channel']}"
    )


if __name__ == "__main__":
    main()
