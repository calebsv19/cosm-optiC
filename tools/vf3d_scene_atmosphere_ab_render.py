#!/usr/bin/env python3
"""Render the S3 VF3D atmosphere A/B proof matrix for a runtime scene."""

from __future__ import annotations

import argparse
import copy
import json
import subprocess
import time
from pathlib import Path
from typing import Any

from vf3d_initial_state_preset_tool import (
    PresetError,
    draw_text,
    generate_preset,
    paste,
    read_bmp,
    scaled_image,
    write_json,
    write_rgb_png,
)
from vf3d_scene_atmosphere_resolver import resolve_specs


RAY_ROOT = Path(__file__).resolve().parents[1]
WORKSPACE_ROOT = RAY_ROOT.parent
DEFAULT_RESOLVER_ROOT = (
    WORKSPACE_ROOT
    / "_private_workspace_artifacts"
    / "ambient_air_probe"
    / "vf3d_scene_atmosphere_resolver"
    / "generated_functional_room_s9r"
)
DEFAULT_REQUEST = (
    WORKSPACE_ROOT
    / "_private_workspace_artifacts"
    / "scene_iterations"
    / "generated-functional-room-s9r"
    / "local_probe"
    / "extracted_payload"
    / "request.json"
)
DEFAULT_OUTPUT_ROOT = (
    WORKSPACE_ROOT
    / "_private_workspace_artifacts"
    / "ambient_air_probe"
    / "vf3d_scene_atmosphere_ab"
    / "generated_functional_room_s9r"
)
RENDER_CANDIDATES = (
    RAY_ROOT / "build" / "toolchains" / "clang" / "arm64" / "tools" / "cli" / "ray_tracing_render_headless",
    RAY_ROOT / "build" / "arm64" / "tools" / "cli" / "ray_tracing_render_headless",
    RAY_ROOT / "build" / "tools" / "cli" / "ray_tracing_render_headless",
)
MODES = ("small_patch", "scene_fit", "scene_overscan")
VARIANCE_MODES = (
    "scene_fit_shift_left",
    "scene_fit_shift_right",
    "scene_fit_shift_up",
    "scene_fit_rotated_z25",
)


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text())


def render_cli() -> Path:
    for candidate in RENDER_CANDIDATES:
        if candidate.exists() and candidate.is_file():
            return candidate
    raise PresetError("missing ray_tracing_render_headless binary; run make -C ray_tracing ray-tracing-render-headless")


def mode_generation_summary(resolver_root: Path, mode: str) -> dict[str, Any]:
    matches = sorted((resolver_root / "generated").glob(f"*_{mode}_seed*/generation_summary.json"))
    if not matches:
        raise PresetError(f"missing generation summary for {mode} under {resolver_root}")
    return read_json(matches[0])


def ensure_mode_generated(
    *,
    resolver_root: Path,
    runtime_scene_path: Path,
    mode: str,
    seed: int,
    preset: str,
) -> None:
    try:
        mode_generation_summary(resolver_root, mode)
        return
    except PresetError:
        pass

    summary = resolve_specs(
        runtime_scene_path,
        output_root=resolver_root.parent,
        seed=seed,
        mode=mode,
        preset=preset,
    )
    for spec_entry in summary["specs"]:
        generate_preset(read_json(Path(spec_entry["spec_path"])), render=False)


def base_request(request_path: Path) -> dict[str, Any]:
    request = read_json(request_path)
    request.setdefault("render", {})
    request["render"]["width"] = 320
    request["render"]["height"] = 210
    request["render"]["frame_count"] = 1
    request["render"]["temporal_frames"] = 1
    request["render"]["integrator_3d"] = "direct_light"
    request.setdefault("inspection", {})
    request["inspection"]["object_audit_enabled"] = True
    request["inspection"]["object_audit_max_dimension"] = 160
    return request


def production_inspection() -> dict[str, Any]:
    return {
        "volume_density_scale": 0.82,
        "volume_density_gamma": 0.72,
        "volume_scatter_gain": 2.45,
        "volume_absorption_gain": 0.0,
        "volume_opacity_clamp": 1.25,
        "volume_step_scale": 0.75,
        "volume_albedo": {"r": 0.95, "g": 0.97, "b": 1.0},
    }


def debug_inspection() -> dict[str, Any]:
    return {
        "volume_density_scale": 1.05,
        "volume_density_gamma": 0.58,
        "volume_scatter_gain": 3.4,
        "volume_absorption_gain": 0.0,
        "volume_opacity_clamp": 2.0,
        "volume_step_scale": 0.75,
        "volume_albedo": {"r": 0.92, "g": 0.98, "b": 1.0},
    }


def request_for_variant(
    base: dict[str, Any],
    *,
    slug: str,
    output_root: Path,
    scene_bundle_path: str | None,
    debug_overlay: bool,
) -> dict[str, Any]:
    request = copy.deepcopy(base)
    variant_root = output_root / slug
    request["run_id"] = f"vf3d_scene_atmosphere_ab_{slug}"
    request["output"] = {"root": str(variant_root.resolve()), "overwrite": True}
    request["progress"] = {
        "summary_path": str((variant_root / "render_summary.json").resolve()),
        "progress_path": str((variant_root / "render_progress.json").resolve()),
    }
    if scene_bundle_path:
        request["volume"] = {
            "enabled": True,
            "source_kind": "scene_bundle",
            "source_path": scene_bundle_path,
            "visible": True,
            "affects_lighting": True,
            "debug_overlay": debug_overlay,
        }
        request.setdefault("inspection", {}).update(debug_inspection() if debug_overlay else production_inspection())
    else:
        request["volume"] = {"enabled": False, "source_kind": "none", "visible": False}
    return request


def run_render(request_path: Path) -> tuple[dict[str, Any], float]:
    start = time.monotonic()
    subprocess.run([str(render_cli()), "--request", str(request_path), "--render"], check=True)
    elapsed = time.monotonic() - start
    request = read_json(request_path)
    summary_path = Path(request["progress"]["summary_path"])
    if not summary_path.exists():
        raise PresetError(f"missing render summary: {summary_path}")
    return read_json(summary_path), elapsed


def convert_frame(output_root: Path) -> Path:
    bmp_path = output_root / "frames" / "frame_0000.bmp"
    if not bmp_path.exists():
        raise PresetError(f"missing rendered BMP frame: {bmp_path}")
    width, height, pixels = read_bmp(bmp_path)
    png_path = output_root / "frames" / "frame_0000.png"
    write_rgb_png(png_path, width, height, pixels)
    return png_path


def pixel_delta(a_path: Path, b_path: Path) -> dict[str, Any]:
    aw, ah, ap = read_bmp(a_path)
    bw, bh, bp = read_bmp(b_path)
    if (aw, ah) != (bw, bh):
        raise PresetError(f"cannot diff frames with different dimensions: {a_path} {b_path}")
    changed = 0
    max_channel_delta = 0
    sum_abs = 0
    for a, b in zip(ap, bp):
        delta = abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])
        if delta:
            changed += 1
            max_channel_delta = max(max_channel_delta, abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2]))
            sum_abs += delta
    return {
        "width": aw,
        "height": ah,
        "changed_pixels": changed,
        "total_pixels": aw * ah,
        "changed_fraction": changed / float(aw * ah),
        "mean_abs_channel_delta": sum_abs / float(aw * ah * 3),
        "max_channel_delta": max_channel_delta,
    }


def make_contact_sheet(output_root: Path, variants: list[dict[str, Any]]) -> Path:
    core_columns = [
        ("BASELINE", "baseline_production"),
        ("SMALL", "small_patch_production"),
        ("FIT", "scene_fit_production"),
        ("OVERSCAN", "scene_overscan_production"),
    ]
    core_debug_columns = [
        ("BASELINE", "baseline_production"),
        ("SMALL DBG", "small_patch_debug"),
        ("FIT DBG", "scene_fit_debug"),
        ("OVER DBG", "scene_overscan_debug"),
    ]
    rows = [core_columns, core_debug_columns]
    by_slug = {v["slug"]: v for v in variants}
    if "scene_fit_shift_left_production" in by_slug:
        rows.extend(
            [
                [
                    ("SHIFT LEFT", "scene_fit_shift_left_production"),
                    ("SHIFT RIGHT", "scene_fit_shift_right_production"),
                    ("SHIFT UP", "scene_fit_shift_up_production"),
                    ("ROT Z25", "scene_fit_rotated_z25_production"),
                ],
                [
                    ("LEFT DBG", "scene_fit_shift_left_debug"),
                    ("RIGHT DBG", "scene_fit_shift_right_debug"),
                    ("UP DBG", "scene_fit_shift_up_debug"),
                    ("ROT DBG", "scene_fit_rotated_z25_debug"),
                ],
            ]
        )
    cell_w = 320
    cell_h = 210
    label_h = 22
    margin = 10
    column_count = 4
    sheet_w = margin * (column_count + 1) + cell_w * column_count
    sheet_h = margin * (len(rows) + 1) + (cell_h + label_h) * len(rows)
    canvas = [(18, 18, 18)] * (sheet_w * sheet_h)
    for row_i, row in enumerate(rows):
        for col_i, (label, slug) in enumerate(row):
            variant = by_slug[slug]
            bmp_path = Path(variant["frame_bmp"])
            width, height, pixels = read_bmp(bmp_path)
            scaled = scaled_image(width, height, pixels, cell_w, cell_h)
            x0 = margin + col_i * (cell_w + margin)
            y0 = margin + row_i * (cell_h + label_h + margin)
            paste(canvas, sheet_w, sheet_h, x0, y0 + label_h, cell_w, cell_h, scaled)
            draw_text(canvas, sheet_w, sheet_h, x0, y0 + 6, label)
    out = output_root / "vf3d_scene_atmosphere_ab_contact.png"
    write_rgb_png(out, sheet_w, sheet_h, canvas)
    return out


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-request", type=Path, default=DEFAULT_REQUEST)
    parser.add_argument("--resolver-root", type=Path, default=DEFAULT_RESOLVER_ROOT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--runtime-scene", type=Path, help="Runtime scene path used when generating missing variance modes.")
    parser.add_argument("--seed", type=int, default=12)
    parser.add_argument("--preset", choices=("mist_patch_v1",), default="mist_patch_v1")
    parser.add_argument("--include-variance", action="store_true", help="Add shifted/rotated scene-fit variance rows.")
    parser.add_argument("--generate-missing", action="store_true", help="Generate missing resolver VF3D artifacts before rendering.")
    parser.add_argument("--render", action="store_true", help="Run the headless renderer for the matrix.")
    return parser


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()
    try:
        base = base_request(args.base_request.resolve())
        output_root = args.output_root.resolve()
        output_root.mkdir(parents=True, exist_ok=True)

        variants: list[dict[str, Any]] = []
        request_specs: list[tuple[str, str | None, bool]] = [("baseline_production", None, False)]
        resolver_root = args.resolver_root.resolve()
        modes = (*MODES, *VARIANCE_MODES) if args.include_variance else MODES
        runtime_scene = args.runtime_scene
        if runtime_scene is None:
            runtime_scene_value = base.get("scene", {}).get("runtime_scene_path") if isinstance(base.get("scene"), dict) else None
            runtime_scene = Path(runtime_scene_value) if isinstance(runtime_scene_value, str) and runtime_scene_value else None
        for mode in modes:
            if args.generate_missing:
                if runtime_scene is None:
                    raise PresetError("--generate-missing requires --runtime-scene or scene.runtime_scene_path in the base request")
                ensure_mode_generated(
                    resolver_root=resolver_root,
                    runtime_scene_path=runtime_scene.resolve(),
                    mode=mode,
                    seed=args.seed,
                    preset=args.preset,
                )
            summary = mode_generation_summary(resolver_root, mode)
            scene_bundle_path = str(Path(summary["scene_bundle_path"]).resolve())
            request_specs.append((f"{mode}_production", scene_bundle_path, False))
            request_specs.append((f"{mode}_debug", scene_bundle_path, True))

        baseline_bmp: Path | None = None
        for slug, scene_bundle_path, debug_overlay in request_specs:
            request = request_for_variant(
                base,
                slug=slug,
                output_root=output_root,
                scene_bundle_path=scene_bundle_path,
                debug_overlay=debug_overlay,
            )
            request_path = output_root / f"{slug}_request.json"
            write_json(request_path, request)
            variant = {
                "slug": slug,
                "request_path": str(request_path),
                "scene_bundle_path": scene_bundle_path,
                "debug_overlay": debug_overlay,
            }
            if args.render:
                render_summary, elapsed = run_render(request_path)
                frame_png = convert_frame(Path(request["output"]["root"]))
                frame_bmp = Path(request["output"]["root"]) / "frames" / "frame_0000.bmp"
                variant.update(
                    {
                        "render_summary_path": request["progress"]["summary_path"],
                        "render_seconds": elapsed,
                        "frame_bmp": str(frame_bmp),
                        "frame_png": str(frame_png),
                        "diagnostics": render_summary.get("diagnostics"),
                        "volume_attached": render_summary.get("volume_attached"),
                        "volume_summary_built": render_summary.get("volume_summary_built"),
                        "volume_visible": render_summary.get("volume_visible"),
                    }
                )
                if slug == "baseline_production":
                    baseline_bmp = frame_bmp
                elif baseline_bmp is not None and not debug_overlay:
                    variant["pixel_delta_from_baseline"] = pixel_delta(baseline_bmp, frame_bmp)
            variants.append(variant)

        summary = {
            "schema_version": "vf3d_scene_atmosphere_ab_render_summary_v1",
            "base_request": str(args.base_request.resolve()),
            "resolver_root": str(resolver_root),
            "output_root": str(output_root),
            "include_variance": bool(args.include_variance),
            "generate_missing": bool(args.generate_missing),
            "variants": variants,
        }
        if args.render:
            summary["contact_sheet_path"] = str(make_contact_sheet(output_root, variants))
        summary_path = output_root / "ab_render_summary.json"
        write_json(summary_path, summary)
        print(json.dumps({"summary_path": str(summary_path), **summary}, indent=2))
        return 0
    except (PresetError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"vf3d_scene_atmosphere_ab_render: error: {exc}\n")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
