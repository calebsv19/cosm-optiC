#!/usr/bin/env python3
"""Render a bounded temporal review of the accepted pool-caustic optics."""

from __future__ import annotations

import argparse
import json
import math
import platform
import shutil
import subprocess
from pathlib import Path

import run_ray_tracing_animated_water_photon_caustics as lane


def pairwise(values: list[list[float]], metric) -> list[float]:
    return [metric(values[index - 1], values[index]) for index in range(1, len(values))]


def water_motion_metrics(water_root: Path) -> dict:
    frames = [
        json.loads((water_root / f"water_surface_{frame:06d}.json").read_text())
        for frame in range(lane.FRAMES)
    ]
    rms, maximum = [], []
    for previous, current in zip(frames, frames[1:]):
        delta = [
            float(b) - float(a)
            for a, b in zip(previous["heights_y"], current["heights_y"])
        ]
        rms.append(math.sqrt(sum(value * value for value in delta) / len(delta)))
        maximum.append(max(abs(value) for value in delta))
    return {
        "playback_frame_indices": [frame["frame_index"] for frame in frames],
        "source_frame_indices": [frame["source_frame_index"] for frame in frames],
        "source_times_seconds": [frame["source_time_seconds"] for frame in frames],
        "capillary_times_seconds": [
            frame["summary"]["photon_fixture_capillary_time_seconds"]
            for frame in frames
        ],
        "pairwise_height_rms_m": rms,
        "pairwise_height_max_delta_m": maximum,
        "max_slopes": [frame["summary"]["max_slope"] for frame in frames],
    }


def encode_videos(diagnostics: Path) -> dict:
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        return {"encoded": False, "reason": "ffmpeg unavailable"}
    animation = diagnostics / "animation"
    outputs = {
        "beauty": diagnostics / "animated_beauty.mp4",
        "floor_aov": diagnostics / "animated_floor_light_aov.mp4",
        "comparison": diagnostics / "animated_beauty_floor_aov_comparison.mp4",
    }
    common = ["-y", "-hide_banner", "-loglevel", "error", "-framerate", "6"]
    subprocess.run(
        [
            ffmpeg, *common, "-i", str(animation / "beauty_%04d.png"),
            "-frames:v", str(lane.FRAMES), "-c:v", "libx264", "-pix_fmt", "yuv420p",
            str(outputs["beauty"]),
        ],
        check=True,
    )
    subprocess.run(
        [
            ffmpeg, *common, "-i",
            str(animation / "reconstructed_surface_gather_%04d.png"),
            "-frames:v", str(lane.FRAMES), "-c:v", "libx264", "-pix_fmt", "yuv420p",
            str(outputs["floor_aov"]),
        ],
        check=True,
    )
    subprocess.run(
        [
            ffmpeg, *common,
            "-i", str(animation / "beauty_%04d.png"),
            "-framerate", "6",
            "-i", str(animation / "reconstructed_surface_gather_%04d.png"),
            "-filter_complex",
            "[0:v]scale=640:384:force_original_aspect_ratio=decrease,"
            "pad=640:384:(ow-iw)/2:(oh-ih)/2:black[left];"
            "[1:v]scale=384:384[right];[left][right]hstack=inputs=2[out]",
            "-map", "[out]", "-frames:v", str(lane.FRAMES),
            "-c:v", "libx264", "-pix_fmt", "yuv420p",
            str(outputs["comparison"]),
        ],
        check=True,
    )
    return {"encoded": True, "outputs": {key: str(value) for key, value in outputs.items()}}


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--water-manifest", type=Path, required=True)
    parser.add_argument(
        "--cli",
        type=Path,
        default=root / "build/toolchains/clang" / platform.machine()
        / "tools/cli/ray_tracing_render_headless",
    )
    parser.add_argument(
        "--review-root",
        type=Path,
        default=root / "build/agent_runs/ray_tracing/pool_caustic_temporal_review",
    )
    parser.add_argument("--photon-budget", type=int, default=32768)
    args = parser.parse_args()
    args.cli = args.cli.resolve()
    args.review_root = args.review_root.resolve()
    if args.review_root.exists():
        shutil.rmtree(args.review_root)

    generated = args.review_root / "generated"
    scene_path = generated / "scene_runtime.json"
    bundle, source = lane.water_bundle(
        generated / "water",
        args.water_manifest.resolve(),
        capillary_review=True,
    )
    lane.scene(scene_path, pool_review=True)

    specs = (
        ("static_population_only", 1, False),
        ("animated_photon_contribution", lane.FRAMES, True),
    )
    summaries = {}
    for run_id, count, enabled in specs:
        out = args.review_root / "runs" / run_id
        out.mkdir(parents=True)
        request_path = args.review_root / "requests" / f"{run_id}.json"
        lane.dump(
            request_path,
            lane.render_request(
                scene_path,
                bundle,
                out,
                count,
                enabled,
                args.photon_budget,
                pool_review=True,
            ),
        )
        summaries[run_id] = lane.run(args.cli, request_path, out)

    diagnostics = args.review_root / "diagnostics"
    animation = diagnostics / "animation"
    animation.mkdir(parents=True)
    animated = args.review_root / "runs/animated_photon_contribution"
    raw_bins, gather_bins, beauty_pixels = [], [], []
    raw_counts, gather_counts, raw_pixels, gather_pixels = [], [], [], []
    for frame in range(lane.FRAMES):
        raw = lane.jsonl(animated / f"photon_surface_records_{frame:04d}.jsonl")
        gather = lane.jsonl(animated / f"photon_surface_queries_{frame:04d}.jsonl")
        raw_counts.append(len(raw))
        gather_counts.append(len(gather))
        raw_bins.append(lane.spatial_bins(raw, "flux"))
        gather_bins.append(lane.spatial_bins(gather, "physical_flux"))
        lane.heatmap(
            raw, "flux", animation / f"raw_photon_landings_{frame:04d}.png", raw_pixels
        )
        lane.heatmap(
            gather,
            "physical_flux",
            animation / f"reconstructed_surface_gather_{frame:04d}.png",
            gather_pixels,
        )
        _, _, pixels = lane.bmp_png(
            animated / "frames" / f"frame_{frame:04d}.bmp",
            animation / f"beauty_{frame:04d}.png",
        )
        beauty_pixels.append(pixels)

    lane.contact_sheet(
        beauty_pixels, diagnostics / "animated_beauty_contact_sheet.png"
    )
    lane.contact_sheet(
        gather_pixels, diagnostics / "animated_floor_light_aov_contact_sheet.png"
    )
    lane.contact_sheet(
        raw_pixels, diagnostics / "animated_raw_photon_landings_contact_sheet.png"
    )
    control = lane.difference(
        args.review_root / "runs/static_population_only/frames/frame_0000.bmp",
        animated / "frames/frame_0000.bmp",
        diagnostics / "frame_0000_beauty_photon_difference_8x.png",
    )

    raw_l1 = pairwise(raw_bins, lane.spatial_l1)
    gather_l1 = pairwise(gather_bins, lane.spatial_l1)
    raw_cosine = pairwise(raw_bins, lane.spatial_cosine)
    gather_cosine = pairwise(gather_bins, lane.spatial_cosine)
    beauty_delta = pairwise(beauty_pixels, lane.bmp_frame_delta)
    record_stability = min(raw_counts) / max(raw_counts)
    coherent = (
        min(raw_l1) > 0.003
        and min(gather_l1) > 0.003
        and min(gather_cosine) > 0.25
        and record_stability > 0.70
        and min(beauty_delta) > 0.01
    )
    report = {
        "status": "pass" if coherent else "review_required",
        "profile": "distant_point_plus_physics_macro_capillary",
        "promotion_eligible": False,
        "frame_count": lane.FRAMES,
        "photon_budget": args.photon_budget,
        "water_source": source,
        "water_motion": water_motion_metrics(generated / "water"),
        "raw_record_counts": raw_counts,
        "gather_query_counts": gather_counts,
        "raw_pairwise_spatial_l1": raw_l1,
        "gather_pairwise_spatial_l1": gather_l1,
        "raw_pairwise_spatial_cosine": raw_cosine,
        "gather_pairwise_spatial_cosine": gather_cosine,
        "beauty_pairwise_mean_rgb_delta": beauty_delta,
        "record_count_stability_ratio": record_stability,
        "frame_0000_control": control,
        "coherent_temporal_motion": coherent,
        "videos": encode_videos(diagnostics),
        "summary_paths": {
            key: str(args.review_root / "runs" / key / "render_summary.json")
            for key in summaries
        },
    }
    lane.dump(args.review_root / "temporal_review_report.json", report)
    print(args.review_root / "temporal_review_report.json")
    return 0 if coherent else 1


if __name__ == "__main__":
    raise SystemExit(main())
