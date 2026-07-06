#!/usr/bin/env python3
"""Run a small A/B matrix for the improved sphere/mist caustic-funnel fixture."""

from __future__ import annotations

import argparse
import json
import platform
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
import run_ray_tracing_spatial_caustic_visual_sphere_mist_matrix as sphere_mist  # noqa: E402


VARIANTS = [
    {
        "variant_id": "baseline_dark_grazing",
        "label": "Baseline improved dark/grazing view",
        "sphere_z": 1.32,
        "light_z": 3.55,
    },
    {
        "variant_id": "higher_light",
        "label": "Higher light",
        "sphere_z": 1.32,
        "light_z": 4.25,
    },
    {
        "variant_id": "raised_sphere",
        "label": "Raised sphere",
        "sphere_z": 1.52,
        "light_z": 3.55,
    },
    {
        "variant_id": "higher_light_raised_sphere",
        "label": "Higher light and raised sphere",
        "sphere_z": 1.52,
        "light_z": 4.25,
    },
]


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
        / "caustic_funnel_fixture_matrix"
    )


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--review-root", type=Path, default=default_review_root())
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--keep-going", action="store_true")
    parser.add_argument("--debug-export", action="store_true")
    parser.add_argument("--include-analytic-policy", action="store_true")
    parser.add_argument("--variant-id", action="append", default=None)
    return parser.parse_args()


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_variant_scene(variant_root: Path, variant: dict) -> Path:
    scene_path = sphere_mist.write_visual_scene(variant_root)
    scene = load_json(scene_path)
    scene["scene_id"] = f"caustic_funnel_fixture_{variant['variant_id']}"
    for obj in scene.get("objects", []):
        if obj.get("object_id") == "high_quality_glass_sphere":
            obj.setdefault("transform", {}).setdefault("position", {})["z"] = variant["sphere_z"]
    for light in scene.get("lights", []):
        if light.get("light_id") == "overhead_focus_light":
            light.setdefault("position", {})["z"] = variant["light_z"]
    write_json(scene_path, scene)
    return scene_path


def request_for_cell(
    variant_root: Path,
    variant: dict,
    scene_path: Path,
    volume_path: Path,
    cell_id: str,
    debug_export: bool,
) -> dict:
    output_root = variant_root / "runs" / cell_id
    summary_path = output_root / "render_summary.json"
    request = sphere_mist.base_request(
        f"caustic_funnel_{variant['variant_id']}_{cell_id}",
        scene_path,
        output_root,
        summary_path,
        volume_path,
    )
    if cell_id == "mist_no_caustic":
        request["inspection"].update({
            "caustic_mode": "off",
            "caustic_volume_enabled": False,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 0,
        })
    elif cell_id == "volume_caustic_only":
        request["inspection"].update({
            "caustic_mode": "transport",
            "caustic_volume_enabled": True,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 3072,
            "caustic_max_path_depth": 2,
            "caustic_surface_receiver_fallback_enabled": False,
        })
        if debug_export:
            request["inspection"]["caustic_transport_debug_export_enabled"] = True
    elif cell_id == "volume_analytic_sphere_lens":
        request["inspection"].update({
            "caustic_mode": "transport",
            "caustic_volume_enabled": True,
            "caustic_surface_enabled": False,
            "caustic_sidecar_enabled": False,
            "caustic_sample_budget": 3072,
            "caustic_max_path_depth": 2,
            "caustic_surface_receiver_fallback_enabled": False,
            "caustic_transport_emission_policy": "analytic_sphere_lens",
        })
        if debug_export:
            request["inspection"]["caustic_transport_debug_export_enabled"] = True
    else:
        raise ValueError(f"unknown funnel fixture cell: {cell_id}")
    request_path = variant_root / "generated_requests" / f"request_{cell_id}.json"
    write_json(request_path, request)
    return {
        "cell_id": cell_id,
        "request_path": request_path,
        "summary_path": summary_path,
    }


def run_variant(
    cli: Path,
    review_root: Path,
    variant: dict,
    skip_render: bool,
    debug_export: bool,
    include_analytic_policy: bool,
) -> dict:
    variant_root = review_root / variant["variant_id"]
    scene_path = write_variant_scene(variant_root, variant)
    volume_path = variant_root / "generated_volume" / "dark_grazing_mist.vf3d"
    sphere_mist.write_soft_mist_vf3d(volume_path)

    runs = []
    runs_by_cell = {}
    failures = []
    cell_ids = ["mist_no_caustic", "volume_caustic_only"]
    if include_analytic_policy:
        cell_ids.append("volume_analytic_sphere_lens")
    for cell_id in cell_ids:
        item = request_for_cell(variant_root, variant, scene_path, volume_path, cell_id, debug_export)
        elapsed = sphere_mist.render_request(cli, item["request_path"], item["summary_path"], skip_render)
        summary = load_json(item["summary_path"])
        frame_path, png_path = sphere_mist.copy_frame_png(summary, variant_root, cell_id)
        summary_copy = variant_root / "summaries" / f"summary_{cell_id}.json"
        summary_copy.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(item["summary_path"], summary_copy)
        digest = sphere_mist.caustic_digest(summary)
        validation_cell_id = "volume_caustic_only" if cell_id == "volume_analytic_sphere_lens" else cell_id
        cell_failures = sphere_mist.validate_cell(validation_cell_id, digest)
        if cell_id == "volume_analytic_sphere_lens":
            if digest.get("transport_analytic_sphere_lens_resolved_count", 0) <= 0:
                cell_failures.append("analytic sphere-lens cell did not resolve a sphere descriptor")
            if digest.get("transport_analytic_sphere_lens_emitted_paths", 0) <= 0:
                cell_failures.append("analytic sphere-lens cell did not emit analytic paths")
            if digest.get("transport_analytic_sphere_lens_rejected_count", 0) != 0:
                cell_failures.append("analytic sphere-lens cell reported descriptor rejection")
        failures.extend([f"{cell_id}: {failure}" for failure in cell_failures])
        run = {
            "cell_id": cell_id,
            "request_path": str(item["request_path"]),
            "summary_path": str(item["summary_path"]),
            "summary_copy": str(summary_copy),
            "frame_path": str(frame_path),
            "png_path": str(png_path),
            "elapsed_seconds": elapsed,
            "caustic": digest,
            "failures": cell_failures,
        }
        runs.append(run)
        runs_by_cell[cell_id] = run

    frame_deltas = sphere_mist.frame_delta_metrics(runs_by_cell)
    readback = sphere_mist.phase11k_readback(runs_by_cell, frame_deltas)
    delta_artifacts = sphere_mist.write_delta_artifacts(variant_root, runs_by_cell)
    side_by_side = delta_artifacts.get("volume_caustic_only", {}).get("side_by_side_diff16_path", "")
    contact_sheet_path = variant_root / "contact_sheet.png"
    sphere_mist.write_contact_sheet(contact_sheet_path, runs)
    return {
        "variant_id": variant["variant_id"],
        "label": variant["label"],
        "sphere_z": variant["sphere_z"],
        "light_z": variant["light_z"],
        "scene_path": str(scene_path),
        "volume_path": str(volume_path),
        "runs": runs,
        "frame_deltas_vs_off": frame_deltas,
        "readback": readback,
        "delta_artifacts": delta_artifacts,
        "side_by_side_diff16_path": side_by_side,
        "contact_sheet_path": str(contact_sheet_path),
        "failures": failures,
    }


def write_aggregate_sheet(path: Path, variants: list[dict]) -> None:
    rows = []
    row_separator = 8
    sep_pixel = (34, 34, 34)
    for variant in variants:
        runs = {run.get("cell_id"): run for run in variant.get("runs", [])}
        baseline = runs.get("mist_no_caustic")
        volume = runs.get("volume_caustic_only")
        if not baseline or not volume:
            continue
        before_w, before_h, before = review_artifacts.read_bmp_rgb(Path(baseline["frame_path"]))
        after_w, after_h, after = review_artifacts.read_bmp_rgb(Path(volume["frame_path"]))
        if (before_w, before_h) != (after_w, after_h):
            continue
        diff = review_artifacts.abs_diff_pixels(before, after, 16)
        width, height, pixels = review_artifacts.side_by_side(before, after, diff)
        if rows:
            rows.extend([[sep_pixel] * width for _ in range(row_separator)])
        rows.extend(pixels)
    if not rows:
        return
    review_artifacts.write_png_rgb(path, len(rows[0]), len(rows), rows)


def write_analytic_review_sheet(path: Path, variants: list[dict]) -> None:
    rows = []
    row_separator = 8
    column_separator = 8
    sep_pixel = (34, 34, 34)
    for variant in variants:
        runs = {run.get("cell_id"): run for run in variant.get("runs", [])}
        baseline = runs.get("mist_no_caustic")
        triangle = runs.get("volume_caustic_only")
        analytic = runs.get("volume_analytic_sphere_lens")
        if not baseline or not triangle or not analytic:
            continue
        base_w, base_h, base_pixels = review_artifacts.read_bmp_rgb(Path(baseline["frame_path"]))
        tri_w, tri_h, tri_pixels = review_artifacts.read_bmp_rgb(Path(triangle["frame_path"]))
        ana_w, ana_h, ana_pixels = review_artifacts.read_bmp_rgb(Path(analytic["frame_path"]))
        if (tri_w, tri_h) != (base_w, base_h) or (ana_w, ana_h) != (base_w, base_h):
            continue
        analytic_diff = review_artifacts.abs_diff_pixels(base_pixels, ana_pixels, 16)
        row_images = [base_pixels, tri_pixels, ana_pixels, analytic_diff]
        width = base_w * len(row_images) + column_separator * (len(row_images) - 1)
        if rows:
            rows.extend([[sep_pixel] * width for _ in range(row_separator)])
        column_sep = [sep_pixel] * column_separator
        for y in range(base_h):
            row = []
            for idx, pixels in enumerate(row_images):
                if idx:
                    row.extend(column_sep)
                row.extend(pixels[y])
            rows.append(row)
    if not rows:
        return
    review_artifacts.write_png_rgb(path, len(rows[0]), len(rows), rows)


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Spatial Caustic Funnel Fixture Matrix",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- review root: `{report['review_root']}`",
        f"- aggregate sheet: `{report['aggregate_contact_sheet_path']}`",
        f"- analytic review sheet: `{report.get('analytic_review_contact_sheet_path', '')}`",
        "",
        "## Variants",
        "",
    ]
    for variant in report["variants"]:
        rb = variant.get("readback", {})
        delta = variant.get("frame_deltas_vs_off", {}).get("volume_caustic_only", {})
        lines.append(f"### {variant['variant_id']}")
        lines.append("")
        lines.append(f"- label: {variant['label']}")
        lines.append(f"- sphere z: `{variant['sphere_z']}`")
        lines.append(f"- light z: `{variant['light_z']}`")
        lines.append(f"- positive pixels: `{delta.get('positive_pixel_count', 0)}`")
        lines.append(f"- max luma delta: `{delta.get('max_luma_delta', 0.0)}`")
        lines.append(f"- mean luma delta: `{delta.get('mean_luma_delta', 0.0)}`")
        lines.append(f"- cache hit ratio: `{rb.get('volume_cache_hit_ratio', 0.0)}`")
        lines.append(f"- scatter pixels: `{rb.get('volume_scatter_pixels', 0)}`")
        lines.append(f"- screen span: `{rb.get('volume_screen_span', {})}`")
        lines.append(f"- side-by-side diff: `{variant.get('side_by_side_diff16_path', '')}`")
        if variant.get("failures"):
            lines.append("- failures:")
            for failure in variant["failures"]:
                lines.append(f"  - {failure}")
        lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    cli = args.cli.resolve()
    review_root = args.review_root.resolve()
    if not args.skip_render and not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2
    review_root.mkdir(parents=True, exist_ok=True)

    requested_variant_ids = set(args.variant_id or [])
    selected_variants = [
        variant for variant in VARIANTS
        if not requested_variant_ids or variant["variant_id"] in requested_variant_ids
    ]
    if requested_variant_ids and not selected_variants:
        print(f"no matching variants for: {sorted(requested_variant_ids)}", file=sys.stderr)
        return 2

    variants = []
    failures = []
    for variant in selected_variants:
        try:
            result = run_variant(cli,
                                 review_root,
                                 variant,
                                 args.skip_render,
                                 args.debug_export,
                                 args.include_analytic_policy)
            variants.append(result)
            failures.extend([f"{variant['variant_id']}: {failure}" for failure in result["failures"]])
        except Exception as exc:
            failures.append(f"{variant['variant_id']}: {exc}")
            if not args.keep_going:
                break

    aggregate_path = review_root / "funnel_fixture_matrix_contact_sheet.png"
    analytic_review_path = review_root / "funnel_fixture_matrix_analytic_review_sheet.png"
    write_aggregate_sheet(aggregate_path, variants)
    write_analytic_review_sheet(analytic_review_path, variants)
    report = {
        "schema_version": "ray_tracing_spatial_caustic_funnel_fixture_matrix_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "aggregate_contact_sheet_path": str(aggregate_path),
        "analytic_review_contact_sheet_path": str(analytic_review_path),
        "variants": variants,
        "failures": failures,
        "passed": len(failures) == 0 and len(variants) == len(selected_variants),
    }
    report_path = review_root / "funnel_fixture_matrix_report.json"
    write_json(report_path, report)
    write_index(review_root / "funnel_fixture_matrix_index.md", report)
    print(report_path)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
