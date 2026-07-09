#!/usr/bin/env python3
"""Run focused Disney v2 emissive-area stress variants."""

from __future__ import annotations

import argparse
import copy
import json
import platform
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


def ray_tracing_root() -> Path:
    return Path(__file__).resolve().parents[2]


def codework_root() -> Path:
    return Path(__file__).resolve().parents[3]


def default_cli(root: Path) -> Path:
    machine = platform.machine()
    candidate = (
        root
        / "build"
        / "toolchains"
        / "clang"
        / machine
        / "tools"
        / "cli"
        / "ray_tracing_render_headless"
    )
    if candidate.exists():
        return candidate
    return root / "build" / machine / "tools" / "cli" / "ray_tracing_render_headless"


def default_output_root() -> Path:
    return (
        codework_root()
        / "_private_workspace_artifacts"
        / "agent_runs"
        / "ray_tracing"
        / "disney_v2_emissive_stress"
        / "ela12_candidate_cache"
    )


def parse_args() -> argparse.Namespace:
    root = ray_tracing_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=default_cli(root))
    parser.add_argument("--output-root", type=Path, default=default_output_root())
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--many-count", type=int, default=8)
    parser.add_argument("--temporal-frames", type=int, default=12)
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def fixture_root() -> Path:
    return ray_tracing_root() / "tests" / "fixtures" / "disney_v2_visual_matrix" / "high_noise_emitter"


def object_by_id(scene: dict, object_id: str) -> dict:
    for obj in scene.get("objects", []):
        if obj.get("object_id") == object_id:
            return obj
    raise ValueError(f"missing object_id {object_id}")


def material_by_object_id(scene: dict, object_id: str) -> dict:
    materials = (
        scene.get("extensions", {})
        .get("ray_tracing", {})
        .get("authoring", {})
        .get("object_materials", [])
    )
    for material in materials:
        if material.get("object_id") == object_id:
            return material
    raise ValueError(f"missing object material for {object_id}")


def set_emitter_off(scene: dict, object_id: str) -> None:
    material = material_by_object_id(scene, object_id)
    material["material_id"] = 0
    material["emissive_strength"] = 0.0
    material["reflectivity"] = 0.0
    material["roughness"] = 0.75


def append_many_emitters(scene: dict, count: int) -> None:
    if count < 1:
        raise ValueError("--many-count must be at least 1")
    base_object_id = "small_warm_emitter"
    base_object = object_by_id(scene, base_object_id)
    base_material = material_by_object_id(scene, base_object_id)
    objects = scene.setdefault("objects", [])
    materials = (
        scene.setdefault("extensions", {})
        .setdefault("ray_tracing", {})
        .setdefault("authoring", {})
        .setdefault("object_materials", [])
    )
    # Keep the original emitter and add deterministic duplicates around it.
    columns = 4
    for index in range(2, count + 1):
        duplicate = copy.deepcopy(base_object)
        material = copy.deepcopy(base_material)
        duplicate_id = f"small_warm_emitter_{index:02d}"
        duplicate["object_id"] = duplicate_id
        material["object_id"] = duplicate_id
        col = (index - 2) % columns
        row = (index - 2) // columns
        duplicate.setdefault("transform", {}).setdefault("position", {})
        duplicate["transform"]["position"] = {
            "x": -0.95 + 0.55 * col,
            "y": 0.12 + 0.08 * (row % 2),
            "z": 0.28 + 0.32 * row,
        }
        duplicate.setdefault("primitive", {})["width"] = 0.24
        duplicate.setdefault("primitive", {})["height"] = 0.24
        duplicate.setdefault("primitive", {})["depth"] = 0.24
        objects.append(duplicate)
        materials.append(material)


def variant_scene(base_scene: dict, variant: str, many_count: int) -> dict:
    scene = copy.deepcopy(base_scene)
    scene["scene_id"] = f"disney_v2_emissive_stress_{variant}"
    if variant == "emitter_off":
        set_emitter_off(scene, "small_warm_emitter")
    elif variant == "single_emitter":
        pass
    elif variant == "many_emitters":
        append_many_emitters(scene, many_count)
    else:
        raise ValueError(f"unknown variant {variant}")
    return scene


def request_for_variant(template: dict,
                        variant: str,
                        scene_path: Path,
                        output_root: Path,
                        repeat_index: int,
                        temporal_frames: int) -> dict:
    request = copy.deepcopy(template)
    run_id = f"disney_v2_emissive_stress_{variant}_r{repeat_index:02d}"
    out_dir = output_root / "runs" / f"repeat_{repeat_index:02d}" / variant
    request["run_id"] = run_id
    request.setdefault("scene", {})["runtime_scene_path"] = str(scene_path)
    request.setdefault("render", {})["integrator_3d"] = "disney_v2"
    request["render"]["denoise_enabled"] = False
    request["render"]["temporal_frames"] = temporal_frames
    request.setdefault("inspection", {})["object_audit_enabled"] = False
    request["output"] = {"root": str(out_dir), "overwrite": True}
    request["progress"] = {
        "summary_path": str(out_dir / "render_summary.json"),
        "progress_path": str(out_dir / "render_progress.json"),
    }
    return request


def bmp_magic_ok(path: Path) -> bool:
    try:
        with path.open("rb") as f:
            return f.read(2) == b"BM"
    except OSError:
        return False


def render(cli: Path, request_path: Path, summary_path: Path) -> tuple[dict, float]:
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    stdout_path = summary_path.parent / "stdout_summary.json"
    stderr_path = summary_path.parent / "stderr.txt"
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
    return load_json(summary_path), elapsed


def run_digest(variant: str, repeat_index: int, summary_path: Path, summary: dict, elapsed: float) -> dict:
    stats = summary.get("render_stats", {})
    bvh = summary.get("bvh_summary", {})
    frame_path = Path(summary.get("outputs", {}).get("first_frame_path", ""))
    return {
        "variant": variant,
        "repeat_index": repeat_index,
        "summary_path": str(summary_path),
        "frame_path": str(frame_path),
        "elapsed_seconds": elapsed,
        "checks": {
            "route_label_ok": summary.get("integrator_3d") == "disney_v2",
            "rendered_ok": summary.get("rendered_frames") is True,
            "frame_ok": bmp_magic_ok(frame_path),
            "nonzero_ok": int(stats.get("nonzero_pixels", 0)) > 0,
            "bvh_ok": bool(bvh.get("ready", False)) and int(bvh.get("trace_overflows", 0)) == 0,
        },
        "render_stats": {
            "visible_pixels": int(stats.get("visible_pixels", 0)),
            "nonzero_pixels": int(stats.get("nonzero_pixels", 0)),
            "secondary_rays": int(stats.get("secondary_rays", 0)),
            "secondary_hits": int(stats.get("secondary_hits", 0)),
            "emissive_area_candidate_count": int(stats.get("emissive_area_candidate_count", 0)),
            "emissive_area_selected_candidates": int(
                stats.get("emissive_area_selected_candidates", 0)
            ),
            "emissive_area_visibility_rays": int(stats.get("emissive_area_visibility_rays", 0)),
            "emissive_area_primary_samples": int(stats.get("emissive_area_primary_samples", 0)),
            "emissive_area_recursive_samples": int(stats.get("emissive_area_recursive_samples", 0)),
            "emissive_area_recursive_policy_skips": int(
                stats.get("emissive_area_recursive_policy_skips", 0)
            ),
            "emissive_area_recursive_candidate_cap_skips": int(
                stats.get("emissive_area_recursive_candidate_cap_skips", 0)
            ),
            "emissive_area_recursive_triangle_cap_skips": int(
                stats.get("emissive_area_recursive_triangle_cap_skips", 0)
            ),
            "emissive_area_recursive_candidate_cap": int(
                stats.get("emissive_area_recursive_candidate_cap", 0)
            ),
            "emissive_area_recursive_triangle_cap": int(
                stats.get("emissive_area_recursive_triangle_cap", 0)
            ),
            "emissive_area_full_scan_fallbacks": int(
                stats.get("emissive_area_full_scan_fallbacks", 0)
            ),
            "max_radiance": float(stats.get("max_radiance", 0.0)),
        },
        "bvh_summary": {
            "triangle_count": int(bvh.get("triangle_count", 0)),
            "trace_calls": int(bvh.get("trace_calls", 0)),
            "trace_overflows": int(bvh.get("trace_overflows", 0)),
        },
    }


def mean(values: list[float]) -> float:
    return sum(values) / len(values) if values else 0.0


def aggregate_runs(runs: list[dict]) -> dict:
    by_variant: dict[str, list[dict]] = {}
    for run in runs:
        by_variant.setdefault(run["variant"], []).append(run)
    variants = {}
    for variant, values in sorted(by_variant.items()):
        elapsed = [float(run["elapsed_seconds"]) for run in values]
        stats = [run["render_stats"] for run in values]
        variants[variant] = {
            "repeat_count": len(values),
            "elapsed_mean_seconds": mean(elapsed),
            "elapsed_min_seconds": min(elapsed),
            "elapsed_max_seconds": max(elapsed),
            "emissive_area_candidate_count_mean": mean(
                [float(item["emissive_area_candidate_count"]) for item in stats]
            ),
            "emissive_area_selected_candidates_mean": mean(
                [float(item["emissive_area_selected_candidates"]) for item in stats]
            ),
            "emissive_area_visibility_rays_mean": mean(
                [float(item["emissive_area_visibility_rays"]) for item in stats]
            ),
            "emissive_area_primary_samples_mean": mean(
                [float(item["emissive_area_primary_samples"]) for item in stats]
            ),
            "emissive_area_recursive_samples_mean": mean(
                [float(item["emissive_area_recursive_samples"]) for item in stats]
            ),
            "emissive_area_recursive_policy_skips_mean": mean(
                [float(item["emissive_area_recursive_policy_skips"]) for item in stats]
            ),
            "emissive_area_recursive_candidate_cap_skips_mean": mean(
                [float(item["emissive_area_recursive_candidate_cap_skips"]) for item in stats]
            ),
            "emissive_area_recursive_triangle_cap_skips_mean": mean(
                [float(item["emissive_area_recursive_triangle_cap_skips"]) for item in stats]
            ),
            "emissive_area_recursive_candidate_cap_mean": mean(
                [float(item["emissive_area_recursive_candidate_cap"]) for item in stats]
            ),
            "emissive_area_recursive_triangle_cap_mean": mean(
                [float(item["emissive_area_recursive_triangle_cap"]) for item in stats]
            ),
            "emissive_area_full_scan_fallbacks_mean": mean(
                [float(item["emissive_area_full_scan_fallbacks"]) for item in stats]
            ),
        }
    return variants


def write_markdown(path: Path, report: dict) -> None:
    lines = [
        "# Disney V2 Emissive Stress Report",
        "",
        f"- generated: `{report['generated_at_utc']}`",
        f"- repeat count: `{report['repeat_count']}`",
        f"- many emitter count: `{report['many_count']}`",
        f"- passed: `{str(report['passed']).lower()}`",
        "",
        "## Variants",
        "",
    ]
    for variant, summary in report["variant_summary"].items():
        lines.append(
            "- `{variant}`: elapsed mean `{elapsed:.3f}s`, candidates `{candidates:.0f}`, "
            "selected `{selected:.0f}`, visibility `{visibility:.0f}`, primary `{primary:.0f}`, "
            "recursive `{recursive:.0f}`, recursive skips `{recursive_skips:.0f}`, "
            "fallbacks `{fallbacks:.0f}`".format(
                variant=variant,
                elapsed=summary["elapsed_mean_seconds"],
                candidates=summary["emissive_area_candidate_count_mean"],
                selected=summary["emissive_area_selected_candidates_mean"],
                visibility=summary["emissive_area_visibility_rays_mean"],
                primary=summary["emissive_area_primary_samples_mean"],
                recursive=summary["emissive_area_recursive_samples_mean"],
                recursive_skips=summary["emissive_area_recursive_policy_skips_mean"],
                fallbacks=summary["emissive_area_full_scan_fallbacks_mean"],
            )
        )
    lines.extend(["", "## Runs", ""])
    for run in report["runs"]:
        stats = run["render_stats"]
        lines.append(
            "- repeat `{repeat}` `{variant}`: elapsed `{elapsed:.3f}s`, candidates `{candidates}`, "
            "selected `{selected}`, visibility `{visibility}`, recursive skips `{recursive_skips}`, "
            "fallbacks `{fallbacks}`, summary `{summary}`".format(
                repeat=run["repeat_index"],
                variant=run["variant"],
                elapsed=run["elapsed_seconds"],
                candidates=stats["emissive_area_candidate_count"],
                selected=stats["emissive_area_selected_candidates"],
                visibility=stats["emissive_area_visibility_rays"],
                recursive_skips=stats["emissive_area_recursive_policy_skips"],
                fallbacks=stats["emissive_area_full_scan_fallbacks"],
                summary=run["summary_path"],
            )
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    if args.repeat < 1:
        print("--repeat must be at least 1", file=sys.stderr)
        return 2
    root = ray_tracing_root()
    cli = args.cli.resolve()
    output_root = args.output_root.resolve()
    if not cli.exists():
        print(f"missing headless CLI: {cli}", file=sys.stderr)
        return 2

    fixture = fixture_root()
    base_scene = load_json(fixture / "scene_runtime.json")
    template = load_json(fixture / "request_disney_v2_denoise_off_12.json")
    generated_root = output_root / "generated"
    variants = ("emitter_off", "single_emitter", "many_emitters")
    runs: list[dict] = []

    for repeat_index in range(1, args.repeat + 1):
        for variant in variants:
            scene = variant_scene(base_scene, variant, args.many_count)
            scene_path = generated_root / f"repeat_{repeat_index:02d}" / f"scene_{variant}.json"
            write_json(scene_path, scene)
            request = request_for_variant(
                template,
                variant,
                scene_path,
                output_root,
                repeat_index,
                args.temporal_frames,
            )
            request_path = generated_root / f"repeat_{repeat_index:02d}" / f"request_{variant}.json"
            write_json(request_path, request)
            summary_path = Path(request["progress"]["summary_path"])
            summary, elapsed = render(cli, request_path, summary_path)
            runs.append(run_digest(variant, repeat_index, summary_path, summary, elapsed))

    variant_summary = aggregate_runs(runs)
    off_candidates = variant_summary.get("emitter_off", {}).get(
        "emissive_area_candidate_count_mean", 0.0
    )
    single_candidates = variant_summary.get("single_emitter", {}).get(
        "emissive_area_candidate_count_mean", 0.0
    )
    many_candidates = variant_summary.get("many_emitters", {}).get(
        "emissive_area_candidate_count_mean", 0.0
    )
    fallback_sum = sum(
        run["render_stats"]["emissive_area_full_scan_fallbacks"] for run in runs
    )
    passed = (
        all(all(run["checks"].values()) for run in runs)
        and off_candidates == 0.0
        and single_candidates > 0.0
        and many_candidates > single_candidates
        and fallback_sum == 0
    )
    report = {
        "schema_version": "ray_tracing_disney_v2_emissive_stress_report_v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "output_root": str(output_root),
        "cli": str(cli),
        "repeat_count": args.repeat,
        "many_count": args.many_count,
        "temporal_frames": args.temporal_frames,
        "passed": passed,
        "variant_summary": variant_summary,
        "runs": runs,
    }
    write_json(output_root / "emissive_stress_report.json", report)
    write_markdown(output_root / "emissive_stress_report.md", report)
    print(output_root / "emissive_stress_report.json")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
