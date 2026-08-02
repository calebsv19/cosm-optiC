#!/usr/bin/env python3
"""High-resolution PSG-23B native-curve versus triangle-tube diagnostic."""

from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "tools"))
from procedural_surface_visual_proof import (  # noqa: E402
    write_labeled_contact_sheet,
)


DEFAULT_CONTRACT = (
    REPO_ROOT
    / "tests/fixtures/procedural_imported_surface_strands_psg23b/visual_contract.json"
)
DEFAULT_BINARY = (
    REPO_ROOT
    / "build/toolchains/clang/arm64/tests/runtime_curve_blas_psg23b_test"
)
DEFAULT_OUTPUT_ROOT = (
    REPO_ROOT
    / "build/agent_runs/ray_tracing/procedural_solid/"
    "psg23b_native_curve_parity_v1"
)
Pixel = tuple[int, int, int]
ImageRows = list[list[Pixel]]


def read_grid(path: Path, width: int, height: int) -> list[dict[str, float]]:
    rows: list[dict[str, float]] = []
    with path.open(newline="", encoding="utf-8") as handle:
        for raw in csv.DictReader(handle):
            row = {key: float(value) for key, value in raw.items()}
            row["pixel_x"] = int(raw["pixel_x"])
            row["pixel_y"] = int(raw["pixel_y"])
            rows.append(row)
    expected = width * height
    if len(rows) != expected:
        raise RuntimeError(f"grid row count mismatch: {len(rows)} != {expected}")
    return rows


def blank(width: int, height: int, color: Pixel = (14, 17, 21)) -> ImageRows:
    return [[color for _ in range(width)] for _ in range(height)]


def mask_image(
    rows: list[dict[str, float]],
    width: int,
    height: int,
    key: str,
) -> ImageRows:
    image = blank(width, height)
    for row in rows:
        if row[key] > 0.5:
            u = max(0.0, min(1.0, row["curve_u"]))
            image[int(row["pixel_y"])][int(row["pixel_x"])] = (
                int(35 + 45 * u),
                int(155 + 80 * (1.0 - u)),
                int(185 + 55 * u),
            )
    return image


def error_image(
    rows: list[dict[str, float]],
    width: int,
    height: int,
    tolerance: float,
) -> ImageRows:
    image = blank(width, height)
    for row in rows:
        curve_hit = row["curve_hit"] > 0.5
        tube_hit = row["tube_hit"] > 0.5
        if curve_hit != tube_hit:
            color = (255, 0, 255)
        elif curve_hit:
            delta = min(1.0, abs(row["curve_t"] - row["tube_t"]) / tolerance)
            color = (
                int(25 + 225 * delta),
                int(175 - 45 * delta),
                int(70 - 25 * delta),
            )
        else:
            color = (14, 17, 21)
        image[int(row["pixel_y"])][int(row["pixel_x"])] = color
    return image


def draw_line(
    image: ImageRows,
    points: list[tuple[int, int]],
    color: Pixel,
    thickness: int,
) -> None:
    height = len(image)
    width = len(image[0]) if height else 0
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        steps = max(abs(x1 - x0), abs(y1 - y0), 1)
        for step in range(steps + 1):
            x = round(x0 + (x1 - x0) * step / steps)
            y = round(y0 + (y1 - y0) * step / steps)
            for dy in range(-thickness, thickness + 1):
                for dx in range(-thickness, thickness + 1):
                    px = x + dx
                    py = y + dy
                    if 0 <= px < width and 0 <= py < height:
                        image[py][px] = color


def payload_image(
    rows: list[dict[str, float]],
    width: int,
    height: int,
) -> ImageRows:
    image = blank(width, height)
    center_rows = [
        row
        for row in rows
        if row["curve_hit"] > 0.5 and abs(row["sample_z"]) < 0.005
    ]
    center_rows.sort(key=lambda row: row["pixel_x"])
    graph_top = 12
    graph_bottom = height - 12
    for fraction in (0.0, 0.25, 0.5, 0.75, 1.0):
        y = int(graph_bottom - fraction * (graph_bottom - graph_top))
        for x in range(width):
            image[y][x] = (42, 48, 58)
    radius_points: list[tuple[int, int]] = []
    parameter_points: list[tuple[int, int]] = []
    tangent_points: list[tuple[int, int]] = []
    span = graph_bottom - graph_top
    for row in center_rows:
        x = int(row["pixel_x"])
        radius_value = max(0.0, min(1.0, row["curve_radius"] / 0.24))
        parameter_value = max(0.0, min(1.0, row["curve_u"]))
        tangent_value = max(0.0, min(1.0, abs(row["tangent_y"])))
        radius_points.append((x, int(graph_bottom - radius_value * span)))
        parameter_points.append((x, int(graph_bottom - parameter_value * span)))
        tangent_points.append((x, int(graph_bottom - tangent_value * span)))
    draw_line(image, radius_points, (255, 177, 64), 1)
    draw_line(image, parameter_points, (74, 205, 255), 1)
    draw_line(image, tangent_points, (132, 240, 135), 0)
    return image


def resize_nearest(image: ImageRows, width: int, height: int) -> ImageRows:
    source_height = len(image)
    source_width = len(image[0])
    return [
        [
            image[min(source_height - 1, y * source_height // height)][
                min(source_width - 1, x * source_width // width)
            ]
            for x in range(width)
        ]
        for y in range(height)
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, default=DEFAULT_CONTRACT)
    parser.add_argument("--test-bin", type=Path, default=DEFAULT_BINARY)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    args = parser.parse_args()

    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    args.output_root.mkdir(parents=True, exist_ok=True)
    review_root = args.output_root / "review"
    review_root.mkdir(parents=True, exist_ok=True)
    grid_path = args.output_root / "parity_grid.csv"
    subprocess.run(
        [str(args.test_bin), "--dump-grid", str(grid_path)],
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )

    grid_width = int(contract["grid_width"])
    grid_height = int(contract["grid_height"])
    rows = read_grid(grid_path, grid_width, grid_height)
    mismatch_count = sum(
        (row["curve_hit"] > 0.5) != (row["tube_hit"] > 0.5) for row in rows
    )
    common_hits = [
        row
        for row in rows
        if row["curve_hit"] > 0.5 and row["tube_hit"] > 0.5
    ]
    maximum_t_delta = max(
        (abs(row["curve_t"] - row["tube_t"]) for row in common_hits),
        default=0.0,
    )
    mismatch_ratio = mismatch_count / len(rows)
    passed = (
        mismatch_ratio <= contract["maximum_hit_state_mismatch_ratio"]
        and maximum_t_delta <= contract["maximum_common_hit_t_delta"]
    )

    panel_width = 760
    panel_height = 390
    cells = [
        (
            "NATIVE CURVE HIT MASK",
            resize_nearest(
                mask_image(rows, grid_width, grid_height, "curve_hit"),
                panel_width,
                panel_height,
            ),
        ),
        (
            "TRIANGLE TUBE ORACLE",
            resize_nearest(
                mask_image(rows, grid_width, grid_height, "tube_hit"),
                panel_width,
                panel_height,
            ),
        ),
        (
            "DEPTH DELTA GREEN TO ORANGE",
            resize_nearest(
                error_image(
                    rows,
                    grid_width,
                    grid_height,
                    float(contract["maximum_common_hit_t_delta"]),
                ),
                panel_width,
                panel_height,
            ),
        ),
        (
            "U CYAN RADIUS ORANGE TANGENT GREEN",
            resize_nearest(
                payload_image(rows, grid_width, grid_height),
                panel_width,
                panel_height,
            ),
        ),
    ]
    image_path = review_root / "psg23b_native_curve_parity_matrix.png"
    write_labeled_contact_sheet(image_path, cells, columns=2)
    output_width = panel_width * 2 + 8
    output_height = (panel_height + 24) * 2 + 8
    summary = {
        "schema": "ray_tracing.procedural_imported_surface_strands_psg23b_visual_proof",
        "schema_version": 1,
        "passed": passed,
        "proof_id": contract["proof_id"],
        "resolution": [output_width, output_height],
        "grid_resolution": [grid_width, grid_height],
        "sample_count": len(rows),
        "common_hit_count": len(common_hits),
        "hit_state_mismatch_count": mismatch_count,
        "hit_state_mismatch_ratio": mismatch_ratio,
        "maximum_common_hit_t_delta": maximum_t_delta,
        "authority": contract["authority"],
        "image": str(image_path),
    }
    summary_path = args.output_root / "proof_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
