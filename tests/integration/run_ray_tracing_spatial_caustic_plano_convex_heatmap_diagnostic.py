#!/usr/bin/env python3
"""Render caustic-only heatmap diagnostics for the plano-convex lens proof."""

from __future__ import annotations

import argparse
import json
import math
import platform
import shutil
import struct
import subprocess
import sys
import time
import zlib
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import run_ray_tracing_spatial_caustic_imported_lens_wall_preview as wall_preview  # noqa: E402
import run_ray_tracing_spatial_caustic_plano_convex_lens_distance_matrix as plano_matrix  # noqa: E402


DIAGNOSTIC_LENS_Y_POSITIONS = (-1.85, -1.25, -0.85, -0.50)
DIAGNOSTIC_FOOTPRINT_SCALES = (1.0, 2.0, 5.0)
DIAGNOSTIC_ENERGY_SCALE = 0.0025
RECEIVER_X_RANGE = (-0.85, 0.55)
RECEIVER_Z_RANGE = (0.62, 1.86)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def workspace_root() -> Path:
    return repo_root().parent


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = root / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless"
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def default_review_root() -> Path:
    return (
        workspace_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "caustic_plano_convex_heatmap_diagnostic"
    )


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--review-root", type=Path, default=default_review_root())
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--debug-export", action="store_true")
    return parser.parse_args()


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def read_png_rgb(path: Path) -> tuple[int, int, list[list[tuple[int, int, int]]]]:
    data = path.read_bytes()
    if len(data) < 8 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG file")
    offset = 8
    width = 0
    height = 0
    idat = bytearray()
    while offset + 8 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if bit_depth != 8 or color_type != 2 or compression != 0 or filter_method != 0 or interlace != 0:
                raise ValueError(f"{path}: unsupported PNG layout")
        elif kind == b"IDAT":
            idat.extend(payload)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(bytes(idat))
    stride = width * 3
    rows = []
    pos = 0
    prior = [0] * stride
    for _y in range(height):
        filter_type = raw[pos]
        pos += 1
        scanline = list(raw[pos:pos + stride])
        pos += stride
        if filter_type == 1:
            for i in range(stride):
                scanline[i] = (scanline[i] + (scanline[i - 3] if i >= 3 else 0)) & 0xff
        elif filter_type == 2:
            for i in range(stride):
                scanline[i] = (scanline[i] + prior[i]) & 0xff
        elif filter_type == 3:
            for i in range(stride):
                left = scanline[i - 3] if i >= 3 else 0
                scanline[i] = (scanline[i] + ((left + prior[i]) // 2)) & 0xff
        elif filter_type == 4:
            for i in range(stride):
                left = scanline[i - 3] if i >= 3 else 0
                up = prior[i]
                up_left = prior[i - 3] if i >= 3 else 0
                p = left + up - up_left
                pa = abs(p - left)
                pb = abs(p - up)
                pc = abs(p - up_left)
                predict = left if pa <= pb and pa <= pc else (up if pb <= pc else up_left)
                scanline[i] = (scanline[i] + predict) & 0xff
        elif filter_type != 0:
            raise ValueError(f"{path}: unsupported PNG filter {filter_type}")
        row = []
        for x in range(width):
            base = x * 3
            row.append((scanline[base], scanline[base + 1], scanline[base + 2]))
        rows.append(row)
        prior = scanline
    return width, height, rows


def read_rgb_image(path: Path) -> tuple[int, int, list[list[tuple[int, int, int]]]]:
    if path.suffix.lower() == ".png":
        return read_png_rgb(path)
    return review_artifacts.read_bmp_rgb(path)


def clean_review_root(review_root: Path) -> None:
    for child in review_root.iterdir() if review_root.exists() else []:
        if child.is_dir():
            shutil.rmtree(child)
        else:
            child.unlink()


def footprint_token(scale: float) -> str:
    return f"footprint_{scale:.2f}".rstrip("0").rstrip(".").replace(".", "p")


def cell_id(lens_y: float, footprint_scale: float | None) -> str:
    distance = plano_matrix.lens_y_token(lens_y)
    if footprint_scale is None:
        return f"{distance}_off"
    return f"{distance}_{footprint_token(footprint_scale)}"


def request_for_cell(review_root: Path,
                     scene_path: Path,
                     lens_y: float,
                     footprint_scale: float | None,
                     debug_export: bool) -> dict:
    cid = cell_id(lens_y, footprint_scale)
    output_root = review_root / "runs" / cid
    summary_path = output_root / "render_summary.json"
    request = wall_preview.base_request(
        f"caustic_plano_convex_heatmap_diagnostic_{cid}",
        scene_path,
        output_root,
        summary_path,
    )
    request["inspection"]["camera_look_at"] = {"x": 0.0, "y": lens_y, "z": 1.24}
    request["inspection"]["camera_position"] = {"x": 1.95, "y": -3.25, "z": 1.52}
    request["inspection"]["camera_zoom"] = 0.52
    if footprint_scale is None:
        request["inspection"].update({
            "caustic_mode": "off",
            "caustic_volume_enabled": False,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 0,
        })
    else:
        request["inspection"].update({
            "caustic_mode": "transport",
            "caustic_volume_enabled": False,
            "caustic_surface_enabled": True,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 2048,
            "caustic_max_path_depth": 2,
            "caustic_surface_energy_scale": DIAGNOSTIC_ENERGY_SCALE,
            "caustic_surface_footprint_scale": footprint_scale,
            "caustic_surface_receiver_fallback_enabled": False,
            "caustic_transport_emission_policy": "mesh_dielectric_lens",
            "caustic_lens_traversal_preset": "dense_glass",
        })
        if debug_export:
            request["inspection"]["caustic_transport_debug_export_enabled"] = True
    request_path = review_root / "generated_requests" / f"request_{cid}.json"
    write_json(request_path, request)
    return {
        "cell_id": cid,
        "request_path": request_path,
        "summary_path": summary_path,
    }


def render_request(cli: Path, request_path: Path, summary_path: Path, skip_render: bool) -> float | None:
    if skip_render:
        return None
    stdout_path = summary_path.parent / "stdout_summary.json"
    stderr_path = summary_path.parent / "stderr.txt"
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    with stdout_path.open("w", encoding="utf-8") as stdout:
        result = subprocess.run(
            [str(cli), "--request", str(request_path), "--render", "--summary", str(summary_path)],
            stdout=stdout,
            stderr=subprocess.PIPE,
            text=True,
        )
    elapsed = time.perf_counter() - start
    stderr_path.write_text(result.stderr or "", encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(f"render failed for {request_path} with exit {result.returncode}; stderr: {stderr_path}")
    return elapsed


def copy_frame_png(summary: dict, review_root: Path, cid: str) -> tuple[Path, Path]:
    frame_path = wall_preview.first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{cid}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return frame_path, png_path


def luma(pixel: tuple[int, int, int]) -> float:
    return 0.2126 * float(pixel[0]) + 0.7152 * float(pixel[1]) + 0.0722 * float(pixel[2])


def heat_color(t: float) -> tuple[int, int, int]:
    t = max(0.0, min(1.0, t))
    if t <= 0.0:
        return (0, 0, 0)
    if t < 0.25:
        u = t / 0.25
        return (0, int(28.0 * u), int(180.0 + 60.0 * u))
    if t < 0.50:
        u = (t - 0.25) / 0.25
        return (0, int(28.0 + 210.0 * u), 255)
    if t < 0.75:
        u = (t - 0.50) / 0.25
        return (int(255.0 * u), 255, int(255.0 * (1.0 - u)))
    u = (t - 0.75) / 0.25
    return (255, 255, int(255.0 * u))


def positive_delta_field(baseline_frame: Path, caustic_frame: Path) -> tuple[int, int, list[list[float]], dict]:
    width, height, base_pixels = review_artifacts.read_bmp_rgb(baseline_frame)
    caustic_width, caustic_height, caustic_pixels = review_artifacts.read_bmp_rgb(caustic_frame)
    if width != caustic_width or height != caustic_height:
        raise ValueError("frame dimensions do not match")
    deltas: list[list[float]] = []
    values = []
    weighted_x = 0.0
    weighted_y = 0.0
    weighted_total = 0.0
    max_delta = 0.0
    for y in range(height):
        row = []
        for x in range(width):
            delta = max(0.0, luma(caustic_pixels[y][x]) - luma(base_pixels[y][x]))
            row.append(delta)
            if delta > 0.0:
                values.append(delta)
                weighted_x += float(x) * delta
                weighted_y += float(y) * delta
                weighted_total += delta
                max_delta = max(max_delta, delta)
        deltas.append(row)
    values.sort()
    p95 = values[min(len(values) - 1, int(0.95 * (len(values) - 1)))] if values else 0.0
    p99 = values[min(len(values) - 1, int(0.99 * (len(values) - 1)))] if values else 0.0
    metrics = {
        "positive_pixel_count": len(values),
        "positive_luma_sum": weighted_total,
        "positive_luma_p95": p95,
        "positive_luma_p99": p99,
        "positive_luma_max": max_delta,
        "positive_delta_centroid": {
            "x": weighted_x / weighted_total if weighted_total > 0.0 else 0.0,
            "y": weighted_y / weighted_total if weighted_total > 0.0 else 0.0,
        },
    }
    return width, height, deltas, metrics


def write_heatmap_png(path: Path, width: int, height: int, deltas: list[list[float]], normalization: float) -> None:
    norm = max(1.0e-9, normalization)
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            row.append(heat_color(deltas[y][x] / norm))
        rows.append(row)
    path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(path, width, height, rows)


def vec_luma(v: dict) -> float:
    return 0.2126 * float(v.get("x", 0.0)) + 0.7152 * float(v.get("y", 0.0)) + 0.0722 * float(v.get("z", 0.0))


def read_debug_hits(paths_path: Path) -> tuple[list[dict], dict]:
    hits = []
    target_offsets = []
    if not paths_path.exists():
        return hits, {}
    for line in paths_path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        record = json.loads(line)
        crossing = record.get("lens_receiver_crossing") or {}
        target = record.get("target_position") or {}
        x = float(crossing.get("x", 0.0))
        y = float(crossing.get("y", 0.0))
        z = float(crossing.get("z", 0.0))
        if not all(math.isfinite(v) for v in (x, y, z)):
            continue
        hits.append({
            "x": x,
            "y": y,
            "z": z,
            "weight": max(0.0, vec_luma(record.get("throughput") or {})),
        })
        tx = float(target.get("x", 0.0))
        tz = float(target.get("z", 0.0)) - 1.25
        if math.isfinite(tx) and math.isfinite(tz):
            target_offsets.append((tx, tz))
    if not hits:
        return hits, {}
    weight_sum = sum(hit["weight"] for hit in hits)
    centroid_x = sum(hit["x"] * hit["weight"] for hit in hits) / weight_sum if weight_sum > 0.0 else 0.0
    centroid_z = sum(hit["z"] * hit["weight"] for hit in hits) / weight_sum if weight_sum > 0.0 else 0.0
    positive_x = sum(1 for hit in hits if hit["x"] >= 0.0)
    negative_x = len(hits) - positive_x
    high_z = sum(1 for hit in hits if hit["z"] >= 1.25)
    low_z = len(hits) - high_z
    target_positive_x = sum(1 for x, _z in target_offsets if x >= 0.0)
    target_negative_x = len(target_offsets) - target_positive_x
    target_high_z = sum(1 for _x, z in target_offsets if z >= 0.0)
    target_low_z = len(target_offsets) - target_high_z
    stats = {
        "count": len(hits),
        "x_min": min(hit["x"] for hit in hits),
        "x_max": max(hit["x"] for hit in hits),
        "z_min": min(hit["z"] for hit in hits),
        "z_max": max(hit["z"] for hit in hits),
        "weighted_centroid": {"x": centroid_x, "z": centroid_z},
        "receiver_x_symmetry_error": (
            abs(positive_x - negative_x) / float(len(hits)) if hits else 0.0
        ),
        "receiver_z_symmetry_error": (
            abs(high_z - low_z) / float(len(hits)) if hits else 0.0
        ),
        "receiver_quadrants": {
            "x_positive": positive_x,
            "x_negative": negative_x,
            "z_high": high_z,
            "z_low": low_z,
        },
        "aperture_sample_count": len(target_offsets),
        "aperture_x_symmetry_error": (
            abs(target_positive_x - target_negative_x) / float(len(target_offsets))
            if target_offsets else 0.0
        ),
        "aperture_z_symmetry_error": (
            abs(target_high_z - target_low_z) / float(len(target_offsets))
            if target_offsets else 0.0
        ),
        "aperture_quadrants": {
            "x_positive": target_positive_x,
            "x_negative": target_negative_x,
            "z_high": target_high_z,
            "z_low": target_low_z,
        },
    }
    return hits, stats


def write_hit_map_png(path: Path, width: int, height: int, hits: list[dict]) -> None:
    accum = [[0.0 for _ in range(width)] for _ in range(height)]
    x0, x1 = RECEIVER_X_RANGE
    z0, z1 = RECEIVER_Z_RANGE
    for hit in hits:
        u = (hit["x"] - x0) / (x1 - x0)
        v = (hit["z"] - z0) / (z1 - z0)
        px = int(round(u * float(width - 1)))
        py = int(round((1.0 - v) * float(height - 1)))
        if px < 0 or py < 0 or px >= width or py >= height:
            continue
        for oy in (-1, 0, 1):
            for ox in (-1, 0, 1):
                qx = px + ox
                qy = py + oy
                if 0 <= qx < width and 0 <= qy < height:
                    falloff = 1.0 if ox == 0 and oy == 0 else 0.35
                    accum[qy][qx] += max(0.1, hit["weight"]) * falloff
    max_value = max(max(row) for row in accum) if hits else 0.0
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            row.append(heat_color(accum[y][x] / max_value if max_value > 0.0 else 0.0))
        rows.append(row)
    path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(path, width, height, rows)


def write_contact_sheet(path: Path, rows_data: list[dict]) -> None:
    cells = []
    for row in rows_data:
        cells.append(read_rgb_image(Path(row["visual_png_path"])))
        for result in row["footprint_results"]:
            cells.append(read_rgb_image(Path(result["heatmap_png_path"])))
        cells.append(read_rgb_image(Path(row["hit_map_png_path"])))
    if not cells:
        return
    cell_width, cell_height = cells[0][0], cells[0][1]
    columns = 2 + len(DIAGNOSTIC_FOOTPRINT_SCALES)
    separator = 4
    sheet_width = cell_width * columns + separator * (columns - 1)
    sheet_height = cell_height * len(rows_data) + separator * (len(rows_data) - 1)
    sheet_rows = [[(20, 20, 22)] * sheet_width for _ in range(sheet_height)]
    cell_index = 0
    for row_i in range(len(rows_data)):
        for col_i in range(columns):
            _width, _height, pixels = cells[cell_index]
            cell_index += 1
            ox = col_i * (cell_width + separator)
            oy = row_i * (cell_height + separator)
            for y in range(cell_height):
                sheet_rows[oy + y][ox:ox + cell_width] = pixels[y]
    path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(path, sheet_width, sheet_height, sheet_rows)


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Plano-Convex Caustic Heatmap Diagnostic",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- contact sheet: `{report['contact_sheet_path']}`",
        f"- energy scale: `{report['energy_scale']}`",
        f"- footprint scales: `{report['footprint_scales']}`",
        f"- lens y positions: `{report['lens_y_positions']}`",
        "",
        "## Readback",
        "",
    ]
    for row in report.get("diagnostic_rows", []):
        lines.append(f"### Lens y `{row['lens_y']}`")
        hit_stats = row.get("debug_hit_stats") or {}
        lines.append(
            f"- debug hit cloud: count `{hit_stats.get('count', 0)}`, "
            f"x `[ {hit_stats.get('x_min', 0.0):.4f}, {hit_stats.get('x_max', 0.0):.4f} ]`, "
            f"z `[ {hit_stats.get('z_min', 0.0):.4f}, {hit_stats.get('z_max', 0.0):.4f} ]`, "
            f"receiver symmetry x/z "
            f"`{hit_stats.get('receiver_x_symmetry_error', 0.0):.4f}`/"
            f"`{hit_stats.get('receiver_z_symmetry_error', 0.0):.4f}`, "
            f"aperture symmetry x/z "
            f"`{hit_stats.get('aperture_x_symmetry_error', 0.0):.4f}`/"
            f"`{hit_stats.get('aperture_z_symmetry_error', 0.0):.4f}`"
        )
        for result in row.get("footprint_results", []):
            metrics = result["heatmap_metrics"]
            caustic = result["caustic"]
            lines.append(
                f"- footprint `{result['footprint_scale']}`: emitted "
                f"`{caustic.get('transport_mesh_dielectric_lens_emitted_path_count', 0)}`, "
                f"positive `{metrics.get('positive_pixel_count', 0)}`, "
                f"p95 `{metrics.get('positive_luma_p95', 0.0):.4f}`, "
                f"p99 `{metrics.get('positive_luma_p99', 0.0):.4f}`, "
                f"max `{metrics.get('positive_luma_max', 0.0):.4f}`"
            )
        lines.append("")
    if report.get("failures"):
        lines.extend(["## Failures", ""])
        lines.extend([f"- {failure}" for failure in report["failures"]])
    if report.get("warnings"):
        lines.extend(["", "## Warnings", ""])
        lines.extend([f"- {warning}" for warning in report["warnings"]])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    cli = args.cli.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2
    review_root.mkdir(parents=True, exist_ok=True)
    clean_review_root(review_root)

    failures = []
    warnings = []
    diagnostic_rows = []
    heatmap_cells = []
    for lens_y in DIAGNOSTIC_LENS_Y_POSITIONS:
        scene_path, lens_path = plano_matrix.write_distance_scene(review_root, lens_y)
        baseline_request = request_for_cell(review_root, scene_path, lens_y, None, args.debug_export)
        baseline_elapsed = render_request(
            cli,
            baseline_request["request_path"],
            baseline_request["summary_path"],
            args.skip_render,
        )
        baseline_summary = load_json(baseline_request["summary_path"])
        baseline_frame, baseline_png = copy_frame_png(baseline_summary, review_root, baseline_request["cell_id"])

        footprint_results = []
        visual_png_path = None
        debug_hits = []
        debug_hit_stats = {}
        for footprint_scale in DIAGNOSTIC_FOOTPRINT_SCALES:
            request = request_for_cell(review_root, scene_path, lens_y, footprint_scale, True)
            elapsed = render_request(cli, request["request_path"], request["summary_path"], args.skip_render)
            summary = load_json(request["summary_path"])
            frame_path, png_path = copy_frame_png(summary, review_root, request["cell_id"])
            if footprint_scale == DIAGNOSTIC_FOOTPRINT_SCALES[-1]:
                visual_png_path = png_path
            width, height, deltas, metrics = positive_delta_field(baseline_frame, frame_path)
            heatmap_cells.append((width, height, deltas, metrics))
            caustic = wall_preview.caustic_digest(summary)
            debug_path = Path(request["summary_path"]).parent / "caustic_transport_debug_paths.jsonl"
            if footprint_scale == DIAGNOSTIC_FOOTPRINT_SCALES[0]:
                debug_hits, debug_hit_stats = read_debug_hits(debug_path)
            result = {
                "cell_id": request["cell_id"],
                "footprint_scale": footprint_scale,
                "request_path": str(request["request_path"]),
                "summary_path": str(request["summary_path"]),
                "frame_path": str(frame_path),
                "png_path": str(png_path),
                "elapsed_seconds": elapsed,
                "caustic": caustic,
                "heatmap_metrics": metrics,
                "heatmap_png_path": "",
            }
            if caustic.get("transport_mesh_dielectric_lens_emitted_path_count", 0) <= 0:
                failures.append(f"{request['cell_id']} emitted zero mesh-dielectric paths")
            if caustic.get("surface_cache_record_count", 0) <= 0:
                failures.append(f"{request['cell_id']} recorded zero surface-cache deposits")
            if metrics.get("positive_pixel_count", 0) <= 0:
                warnings.append(f"{request['cell_id']} had no positive caustic-only delta")
            footprint_results.append(result)

        hit_map_path = review_root / "heatmaps" / f"{plano_matrix.lens_y_token(lens_y)}_debug_hit_map.png"
        write_hit_map_png(hit_map_path, width, height, debug_hits)
        if debug_hit_stats.get("aperture_x_symmetry_error", 0.0) > 0.50:
            warnings.append(
                f"{plano_matrix.lens_y_token(lens_y)} aperture x symmetry error "
                f"{debug_hit_stats.get('aperture_x_symmetry_error', 0.0):.3f}"
            )
        if debug_hit_stats.get("aperture_z_symmetry_error", 0.0) > 0.50:
            warnings.append(
                f"{plano_matrix.lens_y_token(lens_y)} aperture z symmetry error "
                f"{debug_hit_stats.get('aperture_z_symmetry_error', 0.0):.3f}"
            )
        if debug_hit_stats.get("receiver_x_symmetry_error", 0.0) > 0.50:
            warnings.append(
                f"{plano_matrix.lens_y_token(lens_y)} receiver x symmetry error "
                f"{debug_hit_stats.get('receiver_x_symmetry_error', 0.0):.3f}"
            )
        if debug_hit_stats.get("receiver_z_symmetry_error", 0.0) > 0.50:
            warnings.append(
                f"{plano_matrix.lens_y_token(lens_y)} receiver z symmetry error "
                f"{debug_hit_stats.get('receiver_z_symmetry_error', 0.0):.3f}"
            )
        diagnostic_rows.append({
            "lens_y": lens_y,
            "scene_path": str(scene_path),
            "lens_mesh_path": str(lens_path),
            "baseline": {
                "cell_id": baseline_request["cell_id"],
                "request_path": str(baseline_request["request_path"]),
                "summary_path": str(baseline_request["summary_path"]),
                "frame_path": str(baseline_frame),
                "png_path": str(baseline_png),
                "elapsed_seconds": baseline_elapsed,
            },
            "visual_png_path": str(visual_png_path),
            "hit_map_png_path": str(hit_map_path),
            "debug_hit_stats": debug_hit_stats,
            "footprint_results": footprint_results,
        })

    all_p99 = [
        cell[3]["positive_luma_p99"]
        for cell in heatmap_cells
        if cell[3].get("positive_luma_p99", 0.0) > 0.0
    ]
    normalization = max(all_p99) if all_p99 else 1.0
    heatmap_index = 0
    for row in diagnostic_rows:
        for result in row["footprint_results"]:
            width, height, deltas, _metrics = heatmap_cells[heatmap_index]
            heatmap_index += 1
            heatmap_path = review_root / "heatmaps" / f"{result['cell_id']}_caustic_only_heatmap.png"
            write_heatmap_png(heatmap_path, width, height, deltas, normalization)
            result["heatmap_png_path"] = str(heatmap_path)

    contact_sheet_path = review_root / "plano_convex_caustic_only_heatmap_diagnostic_sheet.png"
    write_contact_sheet(contact_sheet_path, diagnostic_rows)
    report = {
        "schema_version": "ray_tracing_plano_convex_heatmap_diagnostic_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "lens_y_positions": list(DIAGNOSTIC_LENS_Y_POSITIONS),
        "footprint_scales": list(DIAGNOSTIC_FOOTPRINT_SCALES),
        "energy_scale": DIAGNOSTIC_ENERGY_SCALE,
        "heatmap_global_p99_normalization": normalization,
        "contact_sheet_path": str(contact_sheet_path),
        "diagnostic_rows": diagnostic_rows,
        "warnings": warnings,
        "failures": failures,
        "passed": len(failures) == 0,
    }
    report_path = review_root / "plano_convex_caustic_only_heatmap_diagnostic_report.json"
    write_json(report_path, report)
    write_index(review_root / "plano_convex_caustic_only_heatmap_diagnostic_index.md", report)
    print(report_path)
    print(contact_sheet_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
