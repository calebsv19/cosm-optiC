#!/usr/bin/env python3
"""Reproduce and verify the unchanged-renderer RT-MIRROR-1 before-state."""

from __future__ import annotations

import hashlib
import json
import platform
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests/fixtures/rt_mirror_1_baseline"
OUTPUT = ROOT / "build/agent_runs/ray_tracing/rt_mirror_1_baseline"
RENDERER = ROOT / "build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless"
EXPECTED_PATH = FIXTURE / "baseline_expected.json"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_equal(label: str, actual: object, expected: object) -> None:
    if actual != expected:
        fail(f"{label}: expected {expected!r}, got {actual!r}")


def render(request_name: str, mode: str) -> tuple[Path, Path]:
    request = FIXTURE / request_name
    summary = OUTPUT / mode / "render_summary.json"
    subprocess.run(
        [
            str(RENDERER),
            "--request",
            str(request),
            "--render",
            "--summary",
            str(summary),
            "--summary-file-only",
        ],
        cwd=ROOT,
        check=True,
    )
    frame = OUTPUT / mode / "frames/frame_0000.bmp"
    if not frame.is_file() or not summary.is_file():
        fail(f"{mode}: renderer did not retain the frame and summary")
    return frame, summary


def object_by_id(summary: dict, object_id: str) -> dict:
    for item in summary.get("object_audit", []):
        if item.get("object_id") == object_id:
            return item
    fail(f"object audit does not contain {object_id!r}")


def validate_summary(mode: str, summary: dict, expected: dict) -> dict:
    contract = expected["render_contract"]
    stats = summary["render_stats"]
    bvh = summary["bvh_summary"]
    subject = object_by_id(summary, "smooth_subject")

    require_equal(f"{mode}.diagnostics", summary["diagnostics"], "ok")
    require_equal(f"{mode}.rendered_frames", summary["rendered_frames"], True)
    require_equal(f"{mode}.integrator", summary["integrator_3d"], contract["integrator_3d"])
    require_equal(f"{mode}.route_family", summary["route_family"], contract["route_family"])
    require_equal(f"{mode}.route_native_3d", summary["route_native_3d"], contract["route_native_3d"])
    require_equal(f"{mode}.scene_triangle_count", bvh["triangle_count"], expected["geometry"]["scene_triangle_count"])
    require_equal(f"{mode}.flat_fallback_calls", bvh["flat_fallback_calls"], contract["route_flat_fallback_calls"])
    require_equal(f"{mode}.overflow_fallback_calls", bvh["overflow_fallback_calls"], contract["route_overflow_fallback_calls"])
    require_equal(f"{mode}.trace_overflows", bvh["trace_overflows"], contract["route_trace_overflows"])
    require_equal(f"{mode}.smooth_subject.primary_hit_pixels", subject["primary_hit_pixels"], contract["smooth_subject_primary_hit_pixels"])
    require_equal(f"{mode}.mirror_reflection_hit_pixels", stats["mirror_reflection_hit_pixels"], contract["mirror_reflection_hit_pixels"])
    require_equal(f"{mode}.mirror_geometry_reflection_pixels", stats["mirror_geometry_reflection_pixels"], contract["mirror_geometry_reflection_pixels"])
    require_equal(f"{mode}.denoise.applied", summary["denoise"]["applied"], contract[f"{mode}_denoise_applied"])
    if mode == "resolved":
        require_equal(
            "resolved.denoise_preserved_mirror_glossy_pixel_count",
            stats["denoise_preserved_mirror_glossy_pixel_count"],
            contract["resolved_preserved_mirror_glossy_pixel_count"],
        )
    return {
        "diagnostics": summary["diagnostics"],
        "integrator_3d": summary["integrator_3d"],
        "route_family": summary["route_family"],
        "route_native_3d": summary["route_native_3d"],
        "scene_triangle_count": bvh["triangle_count"],
        "smooth_subject_primary_hit_pixels": subject["primary_hit_pixels"],
        "mirror_reflection_hit_pixels": stats["mirror_reflection_hit_pixels"],
        "mirror_geometry_reflection_pixels": stats["mirror_geometry_reflection_pixels"],
        "denoise_applied": summary["denoise"]["applied"],
    }


def main() -> int:
    expected = json.loads(EXPECTED_PATH.read_text())
    if not RENDERER.is_file():
        fail(f"renderer is missing: {RENDERER}; run make ray-tracing-render-headless")

    for relative, expected_hash in expected["inputs"].items():
        require_equal(f"input hash {relative}", sha256(FIXTURE / relative), expected_hash)

    mesh = json.loads((FIXTURE / "assets/mesh_assets/rt_mirror_1_smooth_organic.runtime.json").read_text())
    mesh_data = mesh["mesh"]
    geometry = expected["geometry"]
    require_equal("mesh vertex_count", mesh_data["vertex_count"], geometry["mesh_vertex_count"])
    require_equal("mesh triangle_count", mesh_data["triangle_count"], geometry["mesh_triangle_count"])
    require_equal("mesh normal_count", mesh_data["normal_count"], geometry["mesh_normal_count"])
    require_equal("mesh normal_provenance", mesh_data["normal_provenance"], geometry["mesh_normal_provenance"])

    contract = expected["render_contract"]
    for request_name, denoise_enabled in (("request_raw.json", False), ("request_resolved.json", True)):
        request = json.loads((FIXTURE / request_name).read_text())
        render_settings = request["render"]
        inspection = request["inspection"]
        require_equal(f"{request_name}.width", render_settings["width"], contract["width"])
        require_equal(f"{request_name}.height", render_settings["height"], contract["height"])
        require_equal(f"{request_name}.temporal_frames", render_settings["temporal_frames"], contract["temporal_frames"])
        require_equal(f"{request_name}.denoise_enabled", render_settings["denoise_enabled"], denoise_enabled)
        require_equal(f"{request_name}.camera_zoom", inspection["camera_zoom"], contract["camera_zoom"])
        require_equal(
            f"{request_name}.secondary_diffuse_samples_3d",
            inspection["secondary_diffuse_samples_3d"],
            contract["secondary_diffuse_samples_3d"],
        )
        require_equal(
            f"{request_name}.transmission_samples_3d",
            inspection["transmission_samples_3d"],
            contract["transmission_samples_3d"],
        )

    config = (ROOT / "include/config/config_manager.h").read_text()
    for name, value in (
        ("RUNTIME_3D_BOUNCE_DEPTH_DEFAULT", expected["render_contract"]["bounce_depth_default"]),
        ("RUNTIME_3D_SPECULAR_DEPTH_DEFAULT", expected["render_contract"]["specular_depth_default"]),
        ("RUNTIME_3D_TRANSMISSION_DEPTH_DEFAULT", expected["render_contract"]["transmission_depth_default"]),
    ):
        if f"#define {name} {value}" not in config:
            fail(f"default-depth contract changed: expected #define {name} {value}")

    OUTPUT.mkdir(parents=True, exist_ok=True)
    for request_name in ("request_raw.json", "request_resolved.json"):
        shutil.copy2(FIXTURE / request_name, OUTPUT / request_name)

    raw_frame, raw_summary_path = render("request_raw.json", "raw")
    raw_hash = sha256(raw_frame)
    raw_summary = json.loads(raw_summary_path.read_text())
    raw_contract = validate_summary("raw", raw_summary, expected)

    resolved_frame, resolved_summary_path = render("request_resolved.json", "resolved")
    resolved_hash = sha256(resolved_frame)
    resolved_summary = json.loads(resolved_summary_path.read_text())
    resolved_contract = validate_summary("resolved", resolved_summary, expected)

    require_equal("raw frame hash", raw_hash, expected["frames"]["raw_sha256"])
    require_equal("resolved frame hash", resolved_hash, expected["frames"]["resolved_sha256"])
    if expected["frames"]["raw_and_resolved_must_differ"] and raw_hash == resolved_hash:
        fail("raw and resolved frame hashes unexpectedly match")

    repeat_frame, repeat_summary_path = render("request_raw.json", "raw")
    repeat_hash = sha256(repeat_frame)
    repeat_summary = json.loads(repeat_summary_path.read_text())
    repeat_contract = validate_summary("raw", repeat_summary, expected)
    require_equal("repeat raw frame hash", repeat_hash, raw_hash)
    require_equal("repeat raw stable summary contract", repeat_contract, raw_contract)

    machine = f"{platform.system().lower()}-{platform.machine().lower()}"
    report = {
        "schema": "rt_mirror_1_baseline_report_v1",
        "status": "pass",
        "baseline_renderer_commit": expected["baseline_renderer_commit"],
        "reference_platform": expected["reference_platform"],
        "executed_platform": machine,
        "inputs": expected["inputs"],
        "geometry": geometry,
        "render_contract": expected["render_contract"],
        "sampling_identity": expected["sampling_identity"],
        "frames": {
            "raw_sha256": raw_hash,
            "resolved_sha256": resolved_hash,
            "repeat_raw_sha256": repeat_hash,
            "repeat_raw_matches": repeat_hash == raw_hash,
            "raw_and_resolved_differ": raw_hash != resolved_hash,
        },
        "stable_summaries": {"raw": raw_contract, "resolved": resolved_contract},
        "retained_outputs": {
            "raw_frame": str(raw_frame.relative_to(ROOT)),
            "raw_summary": str(raw_summary_path.relative_to(ROOT)),
            "resolved_frame": str(resolved_frame.relative_to(ROOT)),
            "resolved_summary": str(resolved_summary_path.relative_to(ROOT)),
        },
    }
    report_path = OUTPUT / "baseline_report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(f"RT-MIRROR-1 baseline contract: PASS ({report_path.relative_to(ROOT)})")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"RT-MIRROR-1 baseline contract: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
