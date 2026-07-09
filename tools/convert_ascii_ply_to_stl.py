#!/usr/bin/env python3
"""Convert a simple ASCII PLY triangle mesh to binary STL.

This is a private stress-lane helper for Stanford-style scanned meshes. It
expects an ASCII PLY with vertex positions and triangular faces.
"""

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path


def parse_header(handle) -> tuple[int, int, list[str]]:
    vertex_count = None
    face_count = None
    header_lines: list[str] = []
    first = handle.readline()
    if first.strip() != "ply":
        raise ValueError("not a PLY file")
    header_lines.append(first.rstrip("\n"))
    for line in handle:
        stripped = line.strip()
        header_lines.append(stripped)
        if stripped.startswith("format ") and stripped != "format ascii 1.0":
            raise ValueError(f"unsupported PLY format: {stripped}")
        if stripped.startswith("element vertex "):
            vertex_count = int(stripped.split()[2])
        elif stripped.startswith("element face "):
            face_count = int(stripped.split()[2])
        elif stripped == "end_header":
            break
    if vertex_count is None or face_count is None:
        raise ValueError("PLY header missing vertex or face count")
    return vertex_count, face_count, header_lines


def normal(a: tuple[float, float, float],
           b: tuple[float, float, float],
           c: tuple[float, float, float]) -> tuple[float, float, float]:
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    length = math.sqrt(nx * nx + ny * ny + nz * nz)
    if length <= 0.0:
        return 0.0, 0.0, 0.0
    return nx / length, ny / length, nz / length


def convert(input_path: Path, output_path: Path) -> dict[str, int | str]:
    vertices: list[tuple[float, float, float]] = []
    triangles_written = 0
    skipped_faces = 0
    with input_path.open("r", encoding="utf-8", errors="strict") as src:
        vertex_count, face_count, _ = parse_header(src)
        for _ in range(vertex_count):
            parts = src.readline().split()
            if len(parts) < 3:
                raise ValueError("vertex row has fewer than 3 coordinates")
            vertices.append((float(parts[0]), float(parts[1]), float(parts[2])))

        output_path.parent.mkdir(parents=True, exist_ok=True)
        with output_path.open("wb") as dst:
            header = f"CodeWork ascii PLY to STL: {input_path.name}".encode("ascii", errors="replace")
            dst.write(header[:80].ljust(80, b" "))
            dst.write(struct.pack("<I", face_count))
            count_pos = dst.tell()
            for _ in range(face_count):
                parts = src.readline().split()
                if not parts:
                    skipped_faces += 1
                    continue
                n = int(parts[0])
                indices = [int(value) for value in parts[1:1 + n]]
                if n != 3 or len(indices) != 3:
                    skipped_faces += 1
                    continue
                if any(index < 0 or index >= len(vertices) for index in indices):
                    skipped_faces += 1
                    continue
                a, b, c = vertices[indices[0]], vertices[indices[1]], vertices[indices[2]]
                nx, ny, nz = normal(a, b, c)
                dst.write(struct.pack("<12fH", nx, ny, nz, *a, *b, *c, 0))
                triangles_written += 1
            if triangles_written != face_count:
                dst.seek(count_pos - 4)
                dst.write(struct.pack("<I", triangles_written))
    return {
        "input": str(input_path),
        "output": str(output_path),
        "vertices": len(vertices),
        "faces_declared": face_count,
        "triangles_written": triangles_written,
        "skipped_faces": skipped_faces,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = convert(args.input.expanduser().resolve(), args.output.expanduser().resolve())
    print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
