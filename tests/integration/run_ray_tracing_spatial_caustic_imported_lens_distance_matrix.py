#!/usr/bin/env python3
"""Render an imported closed-lens wall-caustic distance/scale matrix."""

from __future__ import annotations

import argparse
import json
import platform
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import run_ray_tracing_spatial_caustic_imported_lens_wall_preview as wall_preview  # noqa: E402
import run_ray_tracing_spatial_caustic_mesh_dielectric_lens_fixture as mesh_fixture  # noqa: E402


LENS_Y_POSITIONS = (-1.85, -1.55, -1.25, -1.05, -0.75, -0.45)
DISTANCE_ENERGY_SCALES = (0.001, 0.0025, 0.005)


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
        / "caustic_imported_lens_distance_matrix"
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


def clean_review_root(review_root: Path) -> None:
    for child in review_root.iterdir() if review_root.exists() else []:
        if child.is_dir():
            shutil.rmtree(child)
        else:
            child.unlink()


def distance_token(lens_y: float) -> str:
    sign = "m" if lens_y < 0.0 else "p"
    return f"lens_y_{sign}{abs(lens_y):.2f}".replace(".", "p")


def scale_token(scale: float) -> str:
    return f"scale_{scale:.4f}".rstrip("0").rstrip(".").replace(".", "p")


def write_distance_scene(review_root: Path, lens_y: float) -> tuple[Path, Path]:
    scene_root = review_root / "generated_scenes" / distance_token(lens_y)
    scene_path, lens_path = wall_preview.write_preview_scene(scene_root)
    scene = load_json(scene_path)
    for obj in scene.get("objects", []):
        if obj.get("object_id") == "imported_biconvex_lens":
            obj.setdefault("transform", {}).setdefault("position", {})["y"] = lens_y
            break
    scene["scene_id"] = f"caustic_imported_lens_distance_{distance_token(lens_y)}"
    write_json(scene_path, scene)
    return scene_path, lens_path


def base_request(run_id: str,
                 scene_path: Path,
                 output_root: Path,
                 summary_path: Path,
                 lens_y: float) -> dict:
    request = wall_preview.base_request(run_id, scene_path, output_root, summary_path)
    request["inspection"]["camera_look_at"] = {"x": 0.0, "y": lens_y, "z": 1.24}
    request["inspection"]["camera_position"] = {"x": 1.95, "y": -3.25, "z": 1.52}
    request["inspection"]["camera_zoom"] = 0.52
    return request


def request_for_cell(review_root: Path,
                     scene_path: Path,
                     lens_y: float,
                     scale: float | None,
                     debug_export: bool) -> dict:
    distance_id = distance_token(lens_y)
    cell_id = f"{distance_id}_off" if scale is None else f"{distance_id}_{scale_token(scale)}"
    output_root = review_root / "runs" / cell_id
    summary_path = output_root / "render_summary.json"
    request = base_request(
        f"caustic_imported_lens_distance_matrix_{cell_id}",
        scene_path,
        output_root,
        summary_path,
        lens_y,
    )
    if scale is None:
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
            "caustic_surface_energy_scale": scale,
            "caustic_surface_footprint_scale": 5.0,
            "caustic_surface_receiver_fallback_enabled": False,
            "caustic_transport_emission_policy": "mesh_dielectric_lens",
            "caustic_lens_traversal_preset": "dense_glass",
        })
        if debug_export:
            request["inspection"]["caustic_transport_debug_export_enabled"] = True
    request_path = review_root / "generated_requests" / f"request_{cell_id}.json"
    write_json(request_path, request)
    return {
        "cell_id": cell_id,
        "lens_y": lens_y,
        "scale": scale,
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


def copy_frame_png(summary: dict, review_root: Path, cell_id: str) -> tuple[Path, Path]:
    frame_path = wall_preview.first_frame_path(summary)
    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{cell_id}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    return frame_path, png_path


def footprint_radius(metrics: dict) -> float:
    centroid = metrics.get("positive_delta_centroid") or {}
    cx = centroid.get("x")
    cy = centroid.get("y")
    if cx is None or cy is None:
        return 0.0
    area = float(metrics.get("positive_pixel_count", 0))
    if area <= 0.0:
        return 0.0
    return (area / 3.141592653589793) ** 0.5


def write_matrix_sheet(path: Path, matrix_rows: list[dict]) -> None:
    cells = []
    for row in matrix_rows:
        for item in row["scale_results"]:
            width, height, pixels = review_artifacts.read_bmp_rgb(Path(item["frame_path"]))
            cells.append((width, height, pixels))
    if not cells:
        return
    cell_width, cell_height = cells[0][0], cells[0][1]
    separator = 4
    columns = len(DISTANCE_ENERGY_SCALES)
    rows_count = len(matrix_rows)
    sheet_width = cell_width * columns + separator * (columns - 1)
    sheet_height = cell_height * rows_count + separator * (rows_count - 1)
    rows = [[(20, 20, 22)] * sheet_width for _ in range(sheet_height)]
    cell_i = 0
    for row_i in range(rows_count):
        for col_i in range(columns):
            _width, _height, pixels = cells[cell_i]
            cell_i += 1
            offset_x = col_i * (cell_width + separator)
            offset_y = row_i * (cell_height + separator)
            for y in range(cell_height):
                rows[offset_y + y][offset_x:offset_x + cell_width] = pixels[y]
    review_artifacts.write_png_rgb(path, sheet_width, sheet_height, rows)


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Imported Lens Distance Matrix",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- passed: `{report['passed']}`",
        f"- matrix sheet: `{report['matrix_sheet_path']}`",
        f"- scales: `{report['energy_scales']}`",
        f"- lens y positions: `{report['lens_y_positions']}`",
        "",
        "## Results",
        "",
    ]
    for row in report.get("matrix_rows", []):
        lines.append(f"### Lens y `{row['lens_y']}`")
        for item in row.get("scale_results", []):
            metrics = item["metrics"]
            caustic = item["caustic"]
            lines.append(
                f"- scale `{item['energy_scale']}`: emitted "
                f"`{caustic.get('transport_mesh_dielectric_lens_emitted_path_count', 0)}`, "
                f"positive `{metrics.get('positive_pixel_count', 0)}`, "
                f"saturated `{metrics.get('saturated_pixel_count', 0)}`, "
                f"p95 `{metrics.get('positive_delta_p95', 0.0):.4f}`, "
                f"max `{metrics.get('max_luma_delta', 0.0):.4f}`, "
                f"radius `{item.get('footprint_radius', 0.0):.4f}`"
            )
        lines.append("")
    if report.get("failures"):
        lines.extend(["## Failures", ""])
        lines.extend([f"- {failure}" for failure in report["failures"]])
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
    matrix_rows = []
    first_lens_path: Path | None = None
    for lens_y in LENS_Y_POSITIONS:
        scene_path, lens_path = write_distance_scene(review_root, lens_y)
        if first_lens_path is None:
            first_lens_path = lens_path
            topology = mesh_fixture.audit_runtime_mesh_topology(lens_path)
            failures.extend([
                f"lens_topology: {failure}"
                for failure in mesh_fixture.validate_mesh_topology_audit(topology)
            ])
        baseline_request = request_for_cell(review_root, scene_path, lens_y, None, args.debug_export)
        elapsed = render_request(cli, baseline_request["request_path"], baseline_request["summary_path"], args.skip_render)
        baseline_summary = load_json(baseline_request["summary_path"])
        baseline_frame, baseline_png = copy_frame_png(baseline_summary, review_root, baseline_request["cell_id"])
        baseline_run = {
            "cell_id": baseline_request["cell_id"],
            "request_path": str(baseline_request["request_path"]),
            "summary_path": str(baseline_request["summary_path"]),
            "frame_path": str(baseline_frame),
            "png_path": str(baseline_png),
            "elapsed_seconds": elapsed,
        }
        scale_results = []
        for scale in DISTANCE_ENERGY_SCALES:
            item = request_for_cell(review_root, scene_path, lens_y, scale, args.debug_export)
            elapsed = render_request(cli, item["request_path"], item["summary_path"], args.skip_render)
            summary = load_json(item["summary_path"])
            frame_path, png_path = copy_frame_png(summary, review_root, item["cell_id"])
            caustic = wall_preview.caustic_digest(summary)
            metrics = wall_preview.wall_delta_metrics(Path(baseline_frame), frame_path)
            result = {
                "cell_id": item["cell_id"],
                "lens_y": lens_y,
                "energy_scale": scale,
                "request_path": str(item["request_path"]),
                "summary_path": str(item["summary_path"]),
                "frame_path": str(frame_path),
                "png_path": str(png_path),
                "elapsed_seconds": elapsed,
                "caustic": caustic,
                "metrics": metrics,
                "footprint_radius": footprint_radius(metrics),
            }
            if caustic.get("transport_mesh_dielectric_lens_emitted_path_count", 0) <= 0:
                failures.append(f"{item['cell_id']} emitted zero mesh-dielectric paths")
            if caustic.get("surface_cache_record_count", 0) <= 0:
                failures.append(f"{item['cell_id']} recorded zero surface-cache deposits")
            if metrics.get("positive_pixel_count", 0) <= 0:
                failures.append(f"{item['cell_id']} brightened zero receiver pixels")
            scale_results.append(result)
        matrix_rows.append({
            "lens_y": lens_y,
            "scene_path": str(scene_path),
            "baseline": baseline_run,
            "scale_results": scale_results,
        })

    matrix_sheet_path = review_root / "imported_lens_distance_matrix_sheet.png"
    write_matrix_sheet(matrix_sheet_path, matrix_rows)
    report = {
        "schema_version": "ray_tracing_imported_lens_distance_matrix_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "lens_mesh_path": str(first_lens_path) if first_lens_path else None,
        "lens_y_positions": list(LENS_Y_POSITIONS),
        "energy_scales": list(DISTANCE_ENERGY_SCALES),
        "matrix_sheet_path": str(matrix_sheet_path),
        "matrix_rows": matrix_rows,
        "failures": failures,
        "passed": len(failures) == 0,
    }
    report_path = review_root / "imported_lens_distance_matrix_report.json"
    write_json(report_path, report)
    write_index(review_root / "imported_lens_distance_matrix_index.md", report)
    print(report_path)
    print(matrix_sheet_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
