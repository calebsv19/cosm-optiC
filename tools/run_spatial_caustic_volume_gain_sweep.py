#!/usr/bin/env python3
"""Run a focused caustic-volume scatter-gain visibility sweep."""

from __future__ import annotations

import argparse
import json
import math
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
RAY_ROOT = SCRIPT_DIR.parent
WORKSPACE_ROOT = RAY_ROOT.parent
INTEGRATION_DIR = RAY_ROOT / "tests" / "integration"
if str(INTEGRATION_DIR) not in sys.path:
    sys.path.insert(0, str(INTEGRATION_DIR))

import run_ray_tracing_spatial_caustic_visual_sphere_mist_matrix as sphere_mist  # noqa: E402


DEFAULT_GAINS = [1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0]


def default_review_root() -> Path:
    return (
        WORKSPACE_ROOT
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "caustic_volume_gain_sweep"
    )


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def gain_slug(gain: float) -> str:
    if abs(gain - round(gain)) < 1.0e-9:
        return f"gain_{int(round(gain)):02d}x"
    return "gain_" + str(gain).replace(".", "p").replace("-", "m") + "x"


def parse_gain_list(raw: str) -> list[float]:
    gains = []
    for part in raw.split(","):
        text = part.strip()
        if not text:
            continue
        value = float(text)
        if not math.isfinite(value) or value <= 0.0:
            raise argparse.ArgumentTypeError(f"invalid gain: {text}")
        if value > 64.0:
            raise argparse.ArgumentTypeError(f"gain exceeds request clamp: {text}")
        gains.append(value)
    if not gains:
        raise argparse.ArgumentTypeError("at least one gain is required")
    return gains


def parse_args() -> argparse.Namespace:
    root = RAY_ROOT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=sphere_mist.default_cli(root))
    parser.add_argument("--review-root", type=Path, default=default_review_root())
    parser.add_argument("--gains", type=parse_gain_list, default=DEFAULT_GAINS)
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--keep-going", action="store_true")
    return parser.parse_args()


def request_specs(gains: list[float]) -> list[dict]:
    specs = [
        {
            "cell_id": "baseline_no_caustic",
            "run_id": "caustic_volume_gain_sweep_baseline",
            "gain": 0.0,
            "inspection": {
                "caustic_mode": "off",
                "caustic_volume_enabled": False,
                "caustic_surface_enabled": False,
                "caustic_sidecar_enabled": False,
                "caustic_sample_budget": 0,
            },
        }
    ]
    for gain in gains:
        specs.append(
            {
                "cell_id": gain_slug(gain),
                "run_id": f"caustic_volume_gain_sweep_{gain_slug(gain)}",
                "gain": gain,
                "inspection": {
                    "caustic_mode": "transport",
                    "caustic_volume_enabled": True,
                    "caustic_surface_enabled": False,
                    "caustic_sidecar_enabled": False,
                    "caustic_sample_budget": 3072,
                    "caustic_max_path_depth": 2,
                    "caustic_surface_receiver_fallback_enabled": False,
                    "caustic_volume_scatter_gain": gain,
                },
            }
        )
    return specs


def sweep_requests(review_root: Path,
                   scene_path: Path,
                   vf3d_path: Path,
                   gains: list[float]) -> list[dict]:
    run_root = review_root / "runs"
    requests = []
    for spec in request_specs(gains):
        cell_id = spec["cell_id"]
        output_root = run_root / cell_id
        summary_path = output_root / "render_summary.json"
        request = sphere_mist.base_request(
            spec["run_id"],
            scene_path,
            output_root,
            summary_path,
            vf3d_path,
        )
        request["inspection"].update(spec["inspection"])
        request_path = review_root / "generated_requests" / f"request_{cell_id}.json"
        write_json(request_path, request)
        requests.append(
            {
                "cell_id": cell_id,
                "gain": spec["gain"],
                "request_path": request_path,
                "summary_path": summary_path,
            }
        )
    return requests


def delta_metrics_against_baseline(runs_by_cell: dict[str, dict]) -> dict:
    if "baseline_no_caustic" not in runs_by_cell:
        return {}
    aliased = {
        "mist_no_caustic": runs_by_cell["baseline_no_caustic"],
    }
    aliased.update(runs_by_cell)
    deltas = sphere_mist.frame_delta_metrics(aliased)
    deltas.pop("mist_no_caustic", None)
    return deltas


def write_delta_sheet(path: Path, runs: list[dict]) -> None:
    baseline = next((run for run in runs if run["cell_id"] == "baseline_no_caustic"), None)
    if not baseline:
        return
    base_w, base_h, base_pixels = sphere_mist.review_artifacts.read_bmp_rgb(
        Path(baseline["frame_path"])
    )
    images = []
    for run in runs:
        width, height, pixels = sphere_mist.review_artifacts.read_bmp_rgb(Path(run["frame_path"]))
        if (width, height) != (base_w, base_h):
            raise ValueError("sweep frames must have matching dimensions")
        diff = sphere_mist.review_artifacts.abs_diff_pixels(base_pixels, pixels, 16)
        images.append((run["cell_id"], pixels, diff))
    separator = 8
    row_separator = 10
    sheet_width = base_w * len(images) + separator * (len(images) - 1)
    sheet_height = base_h * 2 + row_separator
    column_sep = [(34, 34, 34)] * separator
    row_sep = [(26, 26, 26)] * sheet_width
    rows = []
    for y in range(base_h):
        row = []
        for idx, (_, pixels, _) in enumerate(images):
            if idx:
                row.extend(column_sep)
            row.extend(pixels[y])
        rows.append(row)
    for _ in range(row_separator):
        rows.append(list(row_sep))
    for y in range(base_h):
        row = []
        for idx, (_, _, diff) in enumerate(images):
            if idx:
                row.extend(column_sep)
            row.extend(diff[y])
        rows.append(row)
    sphere_mist.review_artifacts.write_png_rgb(path, sheet_width, sheet_height, rows)


def classify_gain(run: dict, delta: dict) -> dict:
    digest = run["caustic"]
    positive = int(delta.get("positive_pixel_count", 0))
    max_luma = float(delta.get("max_luma_delta", 0.0))
    p95 = float(delta.get("positive_luma_delta_p95", 0.0))
    near_white = int(delta.get("near_white_pixel_count", 0))
    scatter_ratio = float(digest.get("volume_scatter_to_direct_radiance_ratio", 0.0))
    scatter_pixels = int(digest.get("volume_scatter_contributing_pixel_count", 0))
    visible = (
        positive >= 64
        and max_luma >= 4.0
        and float(digest.get("volume_scatter_radiance_sum", 0.0)) >= 0.05
    )
    reviewable = (
        visible
        and near_white == 0
        and scatter_pixels >= 64
        and p95 < 80.0
        and scatter_ratio < 4.0
    )
    overdriven = near_white > 0 or p95 >= 80.0 or scatter_ratio >= 4.0
    return {
        "cell_id": run["cell_id"],
        "gain": run["gain"],
        "visible": visible,
        "reviewable": reviewable,
        "overdriven": overdriven,
        "positive_pixel_count": positive,
        "max_luma_delta": max_luma,
        "positive_luma_delta_p95": p95,
        "near_white_pixel_count": near_white,
        "scatter_pixels": scatter_pixels,
        "volume_scatter_radiance_sum": float(digest.get("volume_scatter_radiance_sum", 0.0)),
        "volume_scatter_to_direct_radiance_ratio": scatter_ratio,
    }


def choose_knee(classifications: list[dict]) -> dict:
    reviewable = [item for item in classifications if item["reviewable"]]
    if reviewable:
        target_p95 = 40.0
        default = min(
            reviewable,
            key=lambda item: (
                abs(item["positive_luma_delta_p95"] - target_p95),
                abs(item["max_luma_delta"] - 56.0),
                item["gain"],
            ),
        )
        strong = max(reviewable, key=lambda item: item["gain"])
        first = min(reviewable, key=lambda item: item["gain"])
        overdriven = [item for item in classifications if item["overdriven"]]
        first_overdriven = min(overdriven, key=lambda item: item["gain"]) if overdriven else None
        shortlist = {
            first["gain"],
            default["gain"],
            strong["gain"],
        }
        if first_overdriven:
            shortlist.add(first_overdriven["gain"])
        return {
            "first_visible_gain": first["gain"],
            "default_candidate_gain": default["gain"],
            "default_candidate_cell_id": default["cell_id"],
            "strong_proof_gain": strong["gain"],
            "overdrive_starts_at_gain": first_overdriven["gain"] if first_overdriven else 0.0,
            "selected_gain": default["gain"],
            "selected_cell_id": default["cell_id"],
            "reason": "closest safe/reviewable gain to the target visual p95 delta",
            "remote_shortlist": sorted(shortlist),
        }
    visible = [item for item in classifications if item["visible"]]
    if visible:
        best = min(visible, key=lambda item: (item["overdriven"], item["gain"]))
        return {
            "first_visible_gain": best["gain"],
            "default_candidate_gain": best["gain"],
            "default_candidate_cell_id": best["cell_id"],
            "strong_proof_gain": best["gain"],
            "overdrive_starts_at_gain": best["gain"] if best["overdriven"] else 0.0,
            "selected_gain": best["gain"],
            "selected_cell_id": best["cell_id"],
            "reason": "visible but failed at least one reviewability guard",
            "remote_shortlist": [best["gain"]],
        }
    return {
        "first_visible_gain": 0.0,
        "default_candidate_gain": 0.0,
        "default_candidate_cell_id": "",
        "strong_proof_gain": 0.0,
        "overdrive_starts_at_gain": 0.0,
        "selected_gain": 0.0,
        "selected_cell_id": "",
        "reason": "no gain produced a visible caustic-volume delta",
        "remote_shortlist": [],
    }


def write_index(path: Path, report: dict) -> None:
    lines = [
        "# Caustic Volume Gain Sweep",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- first visible gain: `{report['knee'].get('first_visible_gain', 0.0)}`",
        f"- default candidate gain: `{report['knee'].get('default_candidate_gain', 0.0)}`",
        f"- strong proof gain: `{report['knee'].get('strong_proof_gain', 0.0)}`",
        f"- overdrive starts at gain: `{report['knee'].get('overdrive_starts_at_gain', 0.0)}`",
        f"- selected gain: `{report['knee']['selected_gain']}`",
        f"- selection reason: {report['knee']['reason']}",
        f"- remote shortlist: `{report['knee'].get('remote_shortlist', [])}`",
        f"- contact sheet: `{Path(report['contact_sheet_path']).name}`",
        f"- overlay sheet: `{Path(report['overlay_sheet_path']).name}`",
        f"- delta sheet: `{Path(report['delta_sheet_path']).name}`",
        "",
        "## Gain Readback",
        "",
    ]
    for item in report["gain_classifications"]:
        lines.append(
            f"- `{item['cell_id']}` gain `{item['gain']:.3f}`: "
            f"visible `{item['visible']}`, reviewable `{item['reviewable']}`, "
            f"overdriven `{item['overdriven']}`, positive `{item['positive_pixel_count']}`, "
            f"max `{item['max_luma_delta']:.3f}`, p95 `{item['positive_luma_delta_p95']:.3f}`, "
            f"near-white `{item['near_white_pixel_count']}`, scatter pixels `{item['scatter_pixels']}`, "
            f"caustic radiance `{item['volume_scatter_radiance_sum']:.6f}`, "
            f"caustic/direct `{item['volume_scatter_to_direct_radiance_ratio']:.6f}`"
        )
    lines.extend(["", "## Artifacts", ""])
    for run in report["runs"]:
        lines.append(
            f"- `{run['cell_id']}` gain `{run['gain']:.3f}`: "
            f"`{Path(run['png_path']).name}`, summary `{Path(run['summary_copy']).name}`"
        )
    if report["failures"]:
        lines.extend(["", "## Failures", ""])
        for failure in report["failures"]:
            lines.append(f"- {failure}")
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
    scene_path = sphere_mist.write_visual_scene(review_root)
    vf3d_path = review_root / "generated_volume" / "low_density_uniform_mist.vf3d"
    sphere_mist.write_soft_mist_vf3d(vf3d_path)
    requests = sweep_requests(review_root, scene_path, vf3d_path, args.gains)

    runs = []
    runs_by_cell: dict[str, dict] = {}
    failures: list[str] = []
    for item in requests:
        cell_id = item["cell_id"]
        try:
            elapsed = sphere_mist.render_request(
                cli, item["request_path"], item["summary_path"], args.skip_render
            )
            summary = sphere_mist.load_json(item["summary_path"])
            frame_path, png_path = sphere_mist.copy_frame_png(summary, review_root, cell_id)
            summary_copy = review_root / "summaries" / f"summary_{cell_id}.json"
            summary_copy.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item["summary_path"], summary_copy)
            digest = sphere_mist.caustic_digest(summary)
            run = {
                "cell_id": cell_id,
                "gain": item["gain"],
                "request_path": str(item["request_path"]),
                "summary_path": str(item["summary_path"]),
                "summary_copy": str(summary_copy),
                "frame_path": str(frame_path),
                "png_path": str(png_path),
                "elapsed_seconds": elapsed,
                "caustic": digest,
            }
            runs.append(run)
            runs_by_cell[cell_id] = run
        except Exception as exc:
            failures.append(f"{cell_id}: {exc}")
            if not args.keep_going:
                break

    frame_deltas = delta_metrics_against_baseline(runs_by_cell) if runs_by_cell else {}
    gain_classifications = [
        classify_gain(run, frame_deltas.get(run["cell_id"], {}))
        for run in runs
        if run["cell_id"] != "baseline_no_caustic"
    ]
    knee = choose_knee(gain_classifications)

    contact_sheet_path = review_root / "caustic_volume_gain_sweep_contact_sheet.png"
    sphere_mist.write_contact_sheet(contact_sheet_path, runs)
    overlay_sheet_path = review_root / "caustic_volume_gain_sweep_overlay_sheet.png"
    sphere_mist.write_overlay_sheet(overlay_sheet_path, runs)
    delta_sheet_path = review_root / "caustic_volume_gain_sweep_delta16x_sheet.png"
    write_delta_sheet(delta_sheet_path, runs)

    report = {
        "schema_version": "ray_tracing_spatial_caustic_volume_gain_sweep_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "cli": str(cli),
        "review_root": str(review_root),
        "scene_path": str(scene_path),
        "vf3d_path": str(vf3d_path),
        "gains": args.gains,
        "contact_sheet_path": str(contact_sheet_path),
        "overlay_sheet_path": str(overlay_sheet_path),
        "delta_sheet_path": str(delta_sheet_path),
        "runs": runs,
        "frame_deltas_vs_baseline": frame_deltas,
        "gain_classifications": gain_classifications,
        "knee": knee,
        "failures": failures,
        "passed": len(failures) == 0 and len(runs) == len(requests) and knee["selected_gain"] > 0.0,
    }
    write_json(review_root / "caustic_volume_gain_sweep_report.json", report)
    write_index(review_root / "caustic_volume_gain_sweep_index.md", report)
    print(review_root / "caustic_volume_gain_sweep_report.json")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
