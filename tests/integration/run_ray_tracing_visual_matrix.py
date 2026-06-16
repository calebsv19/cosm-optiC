#!/usr/bin/env python3
"""Render a RayTracing visual-matrix manifest and publish local review artifacts."""

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


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = root / "build" / "toolchains" / "clang" / machine / "tools" / "cli" / "ray_tracing_render_headless"
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--review-root", type=Path, default=None)
    parser.add_argument("--group", action="append", default=[])
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--keep-going", action="store_true")
    parser.add_argument("--contact-columns", type=int, default=3)
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def resolve_path(path_text: str, base: Path) -> Path:
    path = Path(path_text)
    if path.is_absolute():
        return path
    return (base / path).resolve()


def request_cell_id(request_path: Path) -> str:
    stem = request_path.stem
    return stem[len("request_"):] if stem.startswith("request_") else stem


def selected_groups(manifest: dict, wanted: list[str]) -> list[dict]:
    groups = manifest.get("groups", [])
    if not wanted:
        return groups
    wanted_set = set(wanted)
    return [group for group in groups if group.get("id") in wanted_set]


def requested_paths(manifest_path: Path, groups: list[dict]) -> list[Path]:
    paths: list[Path] = []
    seen: set[Path] = set()
    for group in groups:
        for request_name in group.get("requests", []):
            path = (manifest_path.parent / request_name).resolve()
            if path not in seen:
                paths.append(path)
                seen.add(path)
    return paths


def default_review_root(manifest: dict, manifest_path: Path) -> Path:
    value = manifest.get("default_review_root")
    if value:
        return resolve_path(value, manifest_path.parent)
    value = manifest.get("private_workspace_output_root")
    if value:
        return resolve_path(value, manifest_path.parent) / "review"
    matrix_id = manifest.get("matrix_id", manifest_path.parent.name)
    return repo_root() / "build" / "agent_runs" / "ray_tracing" / "visual_matrix_reviews" / matrix_id


def request_output_root(request: dict, request_path: Path) -> Path:
    output = request.get("output", {})
    root_text = output.get("root")
    if not root_text:
        raise ValueError(f"{request_path}: missing output.root")
    return resolve_path(root_text, request_path.parent)


def request_summary_path(request: dict, request_path: Path, out_root: Path) -> Path:
    progress = request.get("progress", {})
    summary_text = progress.get("summary_path")
    if summary_text:
        return resolve_path(summary_text, request_path.parent)
    return out_root / "render_summary.json"


def render_request(cli: Path, request_path: Path, out_root: Path, summary_path: Path) -> float:
    out_root.mkdir(parents=True, exist_ok=True)
    stdout_path = out_root / "stdout_summary.json"
    stderr_path = out_root / "stderr.txt"
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
        raise RuntimeError(
            f"render failed for {request_path} with exit {result.returncode}; stderr: {stderr_path}"
        )
    return elapsed


def bmp_magic_ok(path: Path) -> bool:
    try:
        with path.open("rb") as f:
            return f.read(2) == b"BM"
    except OSError:
        return False


def copy_request_and_summary(request_path: Path,
                             summary_path: Path,
                             review_root: Path,
                             cell_id: str) -> tuple[Path, Path]:
    request_dst = review_root / "requests" / request_path.name
    summary_dst = review_root / "summaries" / f"summary_{cell_id}.json"
    request_dst.parent.mkdir(parents=True, exist_ok=True)
    summary_dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(request_path, request_dst)
    shutil.copy2(summary_path, summary_dst)
    return request_dst, summary_dst


def render_stats_digest(summary: dict) -> dict:
    stats = summary.get("render_stats", {})
    bvh = summary.get("bvh_summary", {})
    return {
        "visible_pixels": int(stats.get("visible_pixels", 0)),
        "nonzero_pixels": int(stats.get("nonzero_pixels", 0)),
        "secondary_rays": int(stats.get("secondary_rays", 0)),
        "secondary_hits": int(stats.get("secondary_hits", 0)),
        "mirror_dominant_pixels": int(stats.get("mirror_dominant_pixels", 0)),
        "mirror_base_attenuated_pixels": int(stats.get("mirror_base_attenuated_pixels", 0)),
        "mirror_reflection_hit_pixels": int(stats.get("mirror_reflection_hit_pixels", 0)),
        "mirror_emitter_reflection_pixels": int(
            stats.get("mirror_emitter_reflection_pixels", 0)
        ),
        "mirror_geometry_reflection_pixels": int(
            stats.get("mirror_geometry_reflection_pixels", 0)
        ),
        "max_mirror_dominance": float(stats.get("max_mirror_dominance", 0.0)),
        "max_mirror_specular_reflection_radiance": float(
            stats.get("max_mirror_specular_reflection_radiance", 0.0)
        ),
        "total_mirror_specular_reflection_radiance": float(
            stats.get("total_mirror_specular_reflection_radiance", 0.0)
        ),
        "temporal_committed_subpasses": int(stats.get("temporal_committed_subpasses", 0)),
        "max_radiance": float(stats.get("max_radiance", 0.0)),
        "max_rgb": stats.get("max_rgb", []),
        "denoise_reconstructed_pixel_count": int(stats.get("denoise_reconstructed_pixel_count", 0)),
        "denoise_stable_interior_sample_count": int(
            stats.get("denoise_stable_interior_sample_count", 0)
        ),
        "denoise_rejected_edge_sample_count": int(stats.get("denoise_rejected_edge_sample_count", 0)),
        "denoise_preserved_transparent_pixel_count": int(
            stats.get("denoise_preserved_transparent_pixel_count", 0)
        ),
        "denoise_preserved_mirror_glossy_pixel_count": int(
            stats.get("denoise_preserved_mirror_glossy_pixel_count", 0)
        ),
        "bvh_triangle_count": int(bvh.get("triangle_count", 0)),
        "bvh_trace_overflows": int(bvh.get("trace_overflows", 0)),
    }


def validate_run(summary: dict, frame_path: Path, requested_integrator: str) -> dict:
    bvh = summary.get("bvh_summary", {})
    stats = summary.get("render_stats", {})
    return {
        "route_label_ok": summary.get("integrator_3d") == requested_integrator,
        "rendered_ok": summary.get("rendered_frames") is True and int(summary.get("frames_rendered", 0)) > 0,
        "frame_ok": frame_path.exists() and bmp_magic_ok(frame_path),
        "nonzero_ok": int(stats.get("nonzero_pixels", 0)) > 0,
        "bvh_ok": bool(bvh.get("ready", False)) and int(bvh.get("trace_overflows", 0)) == 0,
    }


def process_request(cli: Path,
                    request_path: Path,
                    review_root: Path,
                    skip_render: bool) -> tuple[dict, list[list[tuple[int, int, int]]]]:
    request = load_json(request_path)
    cell_id = request_cell_id(request_path)
    out_root = request_output_root(request, request_path)
    summary_path = request_summary_path(request, request_path, out_root)
    elapsed = None
    if not skip_render:
        elapsed = render_request(cli, request_path, out_root, summary_path)
    summary = load_json(summary_path)
    frame_path = Path(summary.get("outputs", {}).get("first_frame_path", ""))
    if not frame_path.is_absolute():
        frame_path = (out_root / "frames" / "frame_0000.bmp").resolve()

    width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
    png_path = review_root / "frames" / f"{cell_id}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(png_path, width, height, pixels)
    request_copy, summary_copy = copy_request_and_summary(request_path, summary_path, review_root, cell_id)
    requested_integrator = request.get("render", {}).get("integrator_3d", "")
    checks = validate_run(summary, frame_path, requested_integrator)
    run = {
        "cell_id": cell_id,
        "request_path": str(request_path),
        "request_copy": str(request_copy),
        "summary_path": str(summary_path),
        "summary_copy": str(summary_copy),
        "frame_path": str(frame_path),
        "png_path": str(png_path),
        "elapsed_seconds": elapsed,
        "integrator_3d": summary.get("integrator_3d"),
        "run_id": summary.get("run_id"),
        "render": summary.get("render", {}),
        "denoise": summary.get("denoise", {}),
        "checks": checks,
        "stats": render_stats_digest(summary),
    }
    return run, pixels


def pad_pixels(pixels: list[list[tuple[int, int, int]]],
               width: int,
               height: int,
               fill: tuple[int, int, int] = (20, 20, 20)) -> list[list[tuple[int, int, int]]]:
    source_h = len(pixels)
    source_w = len(pixels[0]) if source_h else 0
    rows = []
    for y in range(height):
        if y < source_h:
            row = list(pixels[y])
            if source_w < width:
                row.extend([fill] * (width - source_w))
        else:
            row = [fill] * width
        rows.append(row)
    return rows


def write_contact_sheet(review_root: Path,
                        name: str,
                        cells: list[tuple[str, list[list[tuple[int, int, int]]]]],
                        columns: int) -> str:
    if not cells:
        return ""
    columns = max(1, columns)
    cell_w = max(len(pixels[0]) if pixels else 0 for _, pixels in cells)
    cell_h = max(len(pixels) for _, pixels in cells)
    sep = 8
    rows = []
    for row_start in range(0, len(cells), columns):
        row_cells = cells[row_start:row_start + columns]
        padded = [pad_pixels(pixels, cell_w, cell_h) for _, pixels in row_cells]
        while len(padded) < columns:
            padded.append(pad_pixels([], cell_w, cell_h))
        for y in range(cell_h):
            row = []
            for index, pixels in enumerate(padded):
                if index > 0:
                    row.extend([(32, 32, 32)] * sep)
                row.extend(pixels[y])
            rows.append(row)
        if row_start + columns < len(cells):
            rows.extend([[(32, 32, 32)] * (columns * cell_w + (columns - 1) * sep)] * sep)

    out_path = review_root / f"{name}.png"
    review_artifacts.write_png_rgb(out_path, len(rows[0]), len(rows), rows)
    return str(out_path)


def request_key(value: str) -> str:
    return request_cell_id(Path(value))


def comparison_specs(manifest: dict, selected_group_ids: set[str]) -> list[dict]:
    specs = []
    for spec in manifest.get("comparisons", []):
        group_id = spec.get("group")
        if selected_group_ids and group_id and group_id not in selected_group_ids:
            continue
        specs.append(spec)
    return specs


def write_comparison(review_root: Path,
                     spec: dict,
                     pixels_by_cell: dict[str, list[list[tuple[int, int, int]]]]) -> dict:
    comparison_id = spec["id"]
    before_key = request_key(spec["before"])
    after_key = request_key(spec["after"])
    before = pixels_by_cell[before_key]
    after = pixels_by_cell[after_key]
    width = len(before[0]) if before else 0
    height = len(before)
    diff4 = review_artifacts.abs_diff_pixels(before, after, 4)
    diff8 = review_artifacts.abs_diff_pixels(before, after, 8)
    side_w, side_h, side = review_artifacts.side_by_side(before, after, diff4)
    out_dir = review_root / "comparisons" / comparison_id
    out_dir.mkdir(parents=True, exist_ok=True)
    diff4_path = out_dir / "diff_abs_amplified4x.png"
    diff8_path = out_dir / "diff_abs_amplified8x.png"
    side_path = out_dir / f"side_by_side_{before_key}_{after_key}_diff4x.png"
    metrics_path = out_dir / "diff_metrics.json"
    review_artifacts.write_png_rgb(diff4_path, width, height, diff4)
    review_artifacts.write_png_rgb(diff8_path, width, height, diff8)
    review_artifacts.write_png_rgb(side_path, side_w, side_h, side)
    metrics = review_artifacts.diff_metrics(before, after)
    metrics.update({
        "comparison_id": comparison_id,
        "before": before_key,
        "after": after_key,
        "diff_png": str(diff4_path),
        "diff_8x_png": str(diff8_path),
        "side_by_side_png": str(side_path),
        "purpose": spec.get("purpose", ""),
    })
    write_json(metrics_path, metrics)
    return metrics


def rel_link(path: str, root: Path) -> str:
    try:
        return Path(path).resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return path


def write_index(path: Path, report: dict) -> None:
    root = path.parent
    lines = [
        f"# {report['title']}",
        "",
        f"- matrix id: `{report['matrix_id']}`",
        f"- generated: `{report['generated_at_utc']}`",
        f"- manifest: `{report['manifest_path']}`",
        "",
    ]
    if report.get("contact_sheet"):
        contact = rel_link(report["contact_sheet"], root)
        lines.extend([f"![Matrix contact sheet]({contact})", ""])
    lines.extend(["## Runs", ""])
    for run in report["runs"]:
        stats = run["stats"]
        png = rel_link(run["png_path"], root)
        lines.append(
            f"- `{run['cell_id']}` / `{run['integrator_3d']}`: "
            f"visible `{stats['visible_pixels']}`, nonzero `{stats['nonzero_pixels']}`, "
            f"mirror dom `{stats['mirror_dominant_pixels']}`, "
            f"mirror hits `{stats['mirror_reflection_hit_pixels']}`, "
            f"temporal `{stats['temporal_committed_subpasses']}`, "
            f"denoise recon `{stats['denoise_reconstructed_pixel_count']}`, "
            f"PNG `{png}`"
        )
    if report.get("comparisons"):
        lines.extend(["", "## Comparisons", ""])
        for comparison in report["comparisons"]:
            side = rel_link(comparison["side_by_side_png"], root)
            lines.append(
                f"- `{comparison['comparison_id']}`: changed "
                f"`{comparison['changed_pixels']}/{comparison['pixels']}`, "
                f"max delta `{comparison['max_abs_channel_delta']}`, side-by-side `{side}`"
            )
            lines.extend(["", f"![{comparison['comparison_id']}]({side})", ""])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    manifest_path = args.manifest.resolve()
    manifest = load_json(manifest_path)
    cli = args.cli.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2

    groups = selected_groups(manifest, args.group)
    selected_group_ids = {group.get("id", "") for group in groups}
    requests = requested_paths(manifest_path, groups)
    review_root = (args.review_root.resolve()
                   if args.review_root
                   else default_review_root(manifest, manifest_path).resolve())
    review_root.mkdir(parents=True, exist_ok=True)

    runs = []
    pixels_by_cell: dict[str, list[list[tuple[int, int, int]]]] = {}
    contact_cells = []
    failed = False
    for request_path in requests:
        try:
            run, pixels = process_request(cli, request_path, review_root, args.skip_render)
            runs.append(run)
            pixels_by_cell[run["cell_id"]] = pixels
            contact_cells.append((run["cell_id"], pixels))
            if not all(run["checks"].values()):
                failed = True
                print(f"checks failed for {request_path}: {run['checks']}", file=sys.stderr)
                if not args.keep_going:
                    break
        except Exception as exc:  # noqa: BLE001 - CLI/report tooling should surface context.
            failed = True
            print(str(exc), file=sys.stderr)
            if not args.keep_going:
                break

    comparisons = []
    if not failed or args.keep_going:
        for spec in comparison_specs(manifest, selected_group_ids):
            try:
                comparisons.append(write_comparison(review_root, spec, pixels_by_cell))
            except Exception as exc:  # noqa: BLE001
                failed = True
                print(f"comparison failed for {spec.get('id')}: {exc}", file=sys.stderr)
                if not args.keep_going:
                    break

    contact_sheet = write_contact_sheet(review_root,
                                        "matrix_contact_sheet",
                                        contact_cells,
                                        args.contact_columns)
    report = {
        "schema_version": "ray_tracing_visual_matrix_report_v1",
        "matrix_id": manifest.get("matrix_id", manifest_path.parent.name),
        "title": manifest.get("title", manifest.get("matrix_id", manifest_path.parent.name)),
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "manifest_path": str(manifest_path),
        "review_root": str(review_root),
        "selected_groups": [group.get("id", "") for group in groups],
        "contact_sheet": contact_sheet,
        "runs": runs,
        "comparisons": comparisons,
        "passed": not failed and all(all(run["checks"].values()) for run in runs),
    }
    write_json(review_root / "matrix_report.json", report)
    write_index(review_root / "index.md", report)
    print(review_root / "matrix_report.json")
    return 1 if failed or not report["passed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
