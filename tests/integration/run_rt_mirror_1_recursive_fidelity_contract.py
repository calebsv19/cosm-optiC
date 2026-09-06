#!/usr/bin/env python3
"""Capture the RT-MIRROR-1 Phase 1 before-state or evaluate acceptance."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests/fixtures/rt_mirror_1_recursive_fidelity"
OUTPUT = ROOT / "build/agent_runs/ray_tracing/rt_mirror_1_recursive_fidelity"
RENDERER = ROOT / "build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require_equal(label: str, actual: object, expected: object) -> None:
    if actual != expected:
        raise RuntimeError(f"{label}: expected {expected!r}, got {actual!r}")


def render(request_name: str, lane: str) -> tuple[Path, Path]:
    request = FIXTURE / request_name
    summary = OUTPUT / lane / "render_summary.json"
    summary.parent.mkdir(parents=True, exist_ok=True)
    resolved = json.loads(request.read_text())
    resolved["scene"]["runtime_scene_path"] = str((FIXTURE / resolved["scene"]["runtime_scene_path"]).resolve())
    resolved["output"]["root"] = str(summary.parent)
    resolved["progress"]["summary_path"] = str(summary)
    resolved["progress"]["progress_path"] = str(summary.parent / "render_progress.json")
    request = summary.parent / "resolved_request.json"
    request.write_text(json.dumps(resolved, indent=2) + "\n")
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
    frame = OUTPUT / lane / "frames/frame_0000.bmp"
    if not frame.is_file() or not summary.is_file():
        raise RuntimeError(f"{lane}: renderer did not retain frame and summary")
    return frame, summary


def retained(lane: str) -> tuple[Path, Path]:
    frame = OUTPUT / lane / "frames/frame_0000.bmp"
    summary = OUTPUT / lane / "render_summary.json"
    if not frame.is_file() or not summary.is_file():
        raise RuntimeError(f"{lane}: retained output missing; rerun without --reuse-existing")
    return frame, summary


def bmp_metrics(path: Path, definition: dict) -> dict:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise RuntimeError(f"unsupported non-BMP frame: {path}")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    stored_height = struct.unpack_from("<i", data, 22)[0]
    bits_per_pixel = struct.unpack_from("<H", data, 28)[0]
    if width <= 0 or stored_height == 0 or bits_per_pixel != 32:
        raise RuntimeError(f"expected positive-width 32-bit BMP, got {width}x{stored_height}x{bits_per_pixel}")
    height = abs(stored_height)
    row_stride = width * 4
    blue_pixels = 0
    chroma_pixels = 0
    bright_neutral_pixels = 0
    white_pixels = 0
    blue_points: list[tuple[int, int]] = []
    bright_neutral_points: list[tuple[int, int]] = []
    vertical_bands = [
        {"y_min": (height * band) // 4, "y_max": (height * (band + 1)) // 4 - 1,
         "blue_pixels": 0, "chroma_pixels": 0, "bright_neutral_pixels": 0, "white_pixels": 0}
        for band in range(4)
    ]
    blue_red = float(definition["blue_over_red_min"])
    blue_green = float(definition["blue_over_green_min"])
    channel_range = int(definition["channel_range_min"])
    for y in range(height):
        source_y = height - 1 - y if stored_height > 0 else y
        row = pixel_offset + source_y * row_stride
        for x in range(width):
            b, g, r, _ = data[row + x * 4 : row + x * 4 + 4]
            chroma = max(r, g, b) - min(r, g, b)
            band = min(3, (y * 4) // height)
            if chroma >= channel_range:
                chroma_pixels += 1
                vertical_bands[band]["chroma_pixels"] += 1
            if chroma >= channel_range and b > r * blue_red and b > g * blue_green:
                blue_pixels += 1
                blue_points.append((x, y))
                vertical_bands[band]["blue_pixels"] += 1
            if min(r, g, b) >= 200 and chroma < channel_range:
                bright_neutral_pixels += 1
                bright_neutral_points.append((x, y))
                vertical_bands[band]["bright_neutral_pixels"] += 1
            if min(r, g, b) >= 240:
                white_pixels += 1
                vertical_bands[band]["white_pixels"] += 1

    def bounds(points: list[tuple[int, int]]) -> list[int] | None:
        if not points:
            return None
        return [
            min(point[0] for point in points),
            min(point[1] for point in points),
            max(point[0] for point in points),
            max(point[1] for point in points),
        ]

    return {
        "width": width,
        "height": height,
        "pixel_count": width * height,
        "blue_pixels": blue_pixels,
        "chroma_pixels": chroma_pixels,
        "bright_neutral_pixels": bright_neutral_pixels,
        "white_pixels": white_pixels,
        "blue_bounds": bounds(blue_points),
        "bright_neutral_bounds": bounds(bright_neutral_points),
        "vertical_bands": vertical_bands,
    }


def bmp_rgb_at(path: Path, x: int, y: int) -> list[int]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise RuntimeError(f"unsupported non-BMP frame: {path}")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    stored_height = struct.unpack_from("<i", data, 22)[0]
    bits_per_pixel = struct.unpack_from("<H", data, 28)[0]
    height = abs(stored_height)
    if width <= 0 or height <= 0 or bits_per_pixel != 32 or not (0 <= x < width and 0 <= y < height):
        raise RuntimeError(f"invalid BMP probe coordinate ({x}, {y}) for {width}x{height}x{bits_per_pixel}")
    source_y = height - 1 - y if stored_height > 0 else y
    offset = pixel_offset + (source_y * width + x) * 4
    b, g, r, _ = data[offset : offset + 4]
    return [r, g, b]


def radiance_isolation(summary: dict, frame: Path) -> dict:
    probe = summary.get("mirror_recursive_fidelity", {}).get("radiance_isolation_probe", {})
    if not probe.get("valid"):
        raise RuntimeError("radiance isolation probe is missing or invalid")
    pixel = probe.get("pixel", {})
    x = int(pixel.get("x", -1))
    y = int(pixel.get("y", -1))
    predicted = [int(value) for value in probe.get("composed_tonemap_rgb8", [])]
    if len(predicted) != 3:
        raise RuntimeError("radiance isolation probe has no predicted tone-mapped RGB")
    actual = bmp_rgb_at(frame, x, y)
    first_vertex = [float(value) for value in probe.get("first_vertex_linear_rgb", [])]
    composed = [float(value) for value in probe.get("composed_pre_tonemap_linear_rgb", [])]
    energy = probe.get("host_mirror_energy_composition", {})
    if len(first_vertex) != 3 or len(composed) != 3:
        raise RuntimeError("radiance isolation probe has incomplete linear RGB stages")
    classified_names = (
        "local_diffuse_after_attenuation",
        "local_specular_after_attenuation",
        "ambient_after_attenuation",
        "emission",
        "transmission",
        "stochastic_direct",
        "stochastic_bsdf",
        "recursive_direct",
        "recursive_bsdf",
        "unclassified_post_shade_rgb",
    )
    classified = [float(value) for value in probe.get("reflection_after_bsdf_linear_rgb", [])]
    if len(classified) != 3:
        raise RuntimeError("radiance isolation probe has no reflection RGB")
    for name in classified_names:
        values = [float(value) for value in energy.get(name, [])]
        if len(values) != 3:
            raise RuntimeError(f"radiance isolation probe has no {name} RGB")
        classified = [classified[i] + values[i] for i in range(3)]
    composition_delta = [composed[i] - classified[i] for i in range(3)]
    return {
        "probe": probe,
        "final_frame_rgb8": actual,
        "final_minus_single_sample_tonemap_rgb8": [actual[i] - predicted[i] for i in range(3)],
        "stage_metrics": {
            "first_vertex_linear_chroma": max(first_vertex) - min(first_vertex),
            "first_vertex_blue_over_red": first_vertex[2] / first_vertex[0] if first_vertex[0] > 0.0 else 0.0,
            "composed_pre_tonemap_linear_chroma": max(composed) - min(composed),
            "composed_pre_tonemap_blue_over_red": composed[2] / composed[0] if composed[0] > 0.0 else 0.0,
            "final_rgb8_chroma": max(actual) - min(actual),
            "final_blue_over_red": actual[2] / actual[0] if actual[0] > 0 else 0.0,
            "composition_sum_delta_rgb": composition_delta,
        },
    }


def low_chroma_subject_probe(summary: dict, frame: Path) -> dict:
    probe = summary.get("mirror_recursive_fidelity", {}).get(
        "low_chroma_high_luma_subject_probe", {}
    )
    if not probe.get("valid"):
        raise RuntimeError("low-chroma reflected-subject probe is missing or invalid")
    pixel = probe.get("pixel", {})
    x = int(pixel.get("x", -1))
    y = int(pixel.get("y", -1))
    predicted = [int(value) for value in probe.get("composed_tonemap_rgb8", [])]
    if len(predicted) != 3:
        raise RuntimeError("low-chroma reflected-subject probe has no predicted RGB")
    actual = bmp_rgb_at(frame, x, y)
    first_vertex = [float(value) for value in probe.get("first_vertex_linear_rgb", [])]
    reflection = [float(value) for value in probe.get("reflection_after_bsdf_linear_rgb", [])]
    composed = [float(value) for value in probe.get("composed_pre_tonemap_linear_rgb", [])]
    if len(first_vertex) != 3 or len(reflection) != 3 or len(composed) != 3:
        raise RuntimeError("low-chroma reflected-subject probe has incomplete linear RGB stages")
    return {
        "probe": probe,
        "final_frame_rgb8": actual,
        "final_minus_single_sample_tonemap_rgb8": [
            actual[i] - predicted[i] for i in range(3)
        ],
        "stage_metrics": {
            "first_vertex_linear_chroma": max(first_vertex) - min(first_vertex),
            "first_vertex_blue_over_red": (
                first_vertex[2] / first_vertex[0] if first_vertex[0] > 0.0 else 0.0
            ),
            "reflection_after_bsdf_linear_chroma": max(reflection) - min(reflection),
            "composed_pre_tonemap_linear_chroma": max(composed) - min(composed),
            "final_rgb8_chroma": max(actual) - min(actual),
        },
    }


def object_by_id(summary: dict, object_id: str) -> dict:
    for item in summary.get("object_audit", []):
        if item.get("object_id") == object_id:
            return item
    raise RuntimeError(f"object audit does not contain {object_id!r}")


def stable_summary(summary: dict) -> dict:
    require_equal("summary diagnostics", summary.get("diagnostics"), "ok")
    require_equal("summary rendered_frames", summary.get("rendered_frames"), True)
    require_equal("summary route_family", summary.get("route_family"), "native_3d")
    require_equal("summary route_native_3d", summary.get("route_native_3d"), True)
    bvh = summary["bvh_summary"]
    require_equal("summary flat_fallback_calls", bvh["flat_fallback_calls"], 0)
    require_equal("summary overflow_fallback_calls", bvh["overflow_fallback_calls"], 0)
    require_equal("summary trace_overflows", bvh["trace_overflows"], 0)
    stats = summary["render_stats"]
    subject = object_by_id(summary, "smooth_subject")
    return {
        "smooth_subject_primary_hit_pixels": subject["primary_hit_pixels"],
        "mirror_reflection_hit_pixels": stats["mirror_reflection_hit_pixels"],
        "mirror_geometry_reflection_pixels": stats["mirror_geometry_reflection_pixels"],
    }


def nested_value(root: dict, dotted_path: str) -> tuple[bool, object]:
    value: object = root
    for part in dotted_path.split("."):
        if not isinstance(value, dict) or part not in value:
            return False, None
        value = value[part]
    return True, value


def acceptance_failures(
    reflected_metrics: dict,
    reflected_summary: dict,
    raw_isolation: dict | None,
    neutral_subject: dict | None,
    requirements: dict,
) -> list[str]:
    failures: list[str] = []
    coverage = requirements["reflected_signal"]
    for metric, requirement in (("blue_pixels", "blue_pixels_min"), ("chroma_pixels", "chroma_pixels_min")):
        if int(reflected_metrics[metric]) < int(coverage[requirement]):
            failures.append(
                f"reflected {metric} {reflected_metrics[metric]} is below {coverage[requirement]}"
            )
    bounds = reflected_metrics.get("blue_bounds")
    if not bounds:
        failures.append("reflected blue signal has no spatial bounds")
    else:
        width = int(bounds[2]) - int(bounds[0]) + 1
        height = int(bounds[3]) - int(bounds[1]) + 1
        if width < int(coverage["blue_bounds_width_min"]):
            failures.append(f"reflected blue-signal width {width} is below {coverage['blue_bounds_width_min']}")
        if height < int(coverage["blue_bounds_height_min"]):
            failures.append(f"reflected blue-signal height {height} is below {coverage['blue_bounds_height_min']}")

    summary_contract = requirements["required_summary_contract"]
    contract_root = reflected_summary.get(summary_contract["root"])
    if not isinstance(contract_root, dict):
        failures.append(f"missing summary object {summary_contract['root']}")
        return failures
    for field in summary_contract["fields"]:
        present, _ = nested_value(contract_root, field)
        if not present:
            failures.append(f"missing summary field {summary_contract['root']}.{field}")

    probe = contract_root.get("reflected_probe")
    if isinstance(probe, dict):
        identity = requirements["required_probe_identity"]
        for key in ("object_id", "material_id", "normal_provenance"):
            if probe.get(key) != identity[key]:
                failures.append(f"reflected probe {key} expected {identity[key]!r}, got {probe.get(key)!r}")
        if int(probe.get("path_depth", -1)) < int(identity["path_depth_min"]):
            failures.append(f"reflected probe path_depth is below {identity['path_depth_min']}")

    if raw_isolation is None or neutral_subject is None:
        failures.append("raw reflected-subject proof is required for acceptance")
        return failures
    identity = requirements["required_probe_identity"]
    raw_probe = raw_isolation["probe"]
    for key in ("object_id", "material_id", "normal_provenance"):
        if raw_probe.get(key) != identity[key]:
            failures.append(f"raw reflected probe {key} expected {identity[key]!r}, got {raw_probe.get(key)!r}")
    if int(raw_probe.get("path_depth", -1)) < int(identity["path_depth_min"]):
        failures.append(f"raw reflected probe path_depth is below {identity['path_depth_min']}")

    chroma = requirements["raw_chroma"]
    stages = raw_isolation["stage_metrics"]
    for metric, requirement in (
        ("first_vertex_blue_over_red", "first_vertex_blue_over_red_min"),
        ("composed_pre_tonemap_blue_over_red", "composed_blue_over_red_min"),
        ("final_blue_over_red", "final_blue_over_red_min"),
        ("composed_pre_tonemap_linear_chroma", "composed_linear_chroma_min"),
    ):
        if float(stages[metric]) < float(chroma[requirement]):
            failures.append(f"raw {metric} {stages[metric]:.9f} is below {chroma[requirement]}")

    accounting = requirements["energy_accounting"]
    if max(abs(float(value)) for value in stages["composition_sum_delta_rgb"]) > float(
        accounting["composition_delta_abs_max"]
    ):
        failures.append("raw host-mirror terms do not exactly compose the linear pixel")
    if max(abs(int(value)) for value in raw_isolation["final_minus_single_sample_tonemap_rgb8"]) > int(
        accounting["tonemap_byte_delta_abs_max"]
    ):
        failures.append("raw tone-map prediction does not exactly match the exported pixel")

    if min(neutral_subject["final_frame_rgb8"]) > requirements["raw_chroma"]["neutral_subject_channel_floor_max"]:
        failures.append("reflected subject washes out to near-white before reconstruction")

    neutral_energy = neutral_subject["probe"]["host_mirror_energy_composition"]
    neutral_sum = [0.0, 0.0, 0.0]
    for term in (
        "local_diffuse_after_attenuation",
        "local_specular_after_attenuation",
        "ambient_after_attenuation",
        "stochastic_direct",
        "stochastic_bsdf",
        "recursive_direct",
        "recursive_bsdf",
    ):
        values = [float(value) for value in neutral_energy[term]]
        neutral_sum = [neutral_sum[i] + values[i] for i in range(3)]
    reflection = [float(value) for value in neutral_subject["probe"]["reflection_after_bsdf_linear_rgb"]]
    neutral_sum = [neutral_sum[i] + reflection[i] for i in range(3)]
    composed = [float(value) for value in neutral_subject["probe"]["composed_pre_tonemap_linear_rgb"]]
    if max(abs(composed[i] - neutral_sum[i]) for i in range(3)) > float(
        accounting["neutral_composition_delta_abs_max"]
    ):
        failures.append("low-chroma reflected-subject pixel is not exactly single-accounted")
    return failures


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--mode",
        choices=("before-state", "acceptance", "radiance-isolation"),
        default="before-state",
    )
    parser.add_argument("--reuse-existing", action="store_true")
    parser.add_argument("--output-root", type=Path, help="Separate retained evidence directory inside this checkout")
    return parser.parse_args()


def main() -> int:
    global OUTPUT
    args = parse_args()
    if args.output_root:
        OUTPUT = (ROOT / args.output_root).resolve()
        OUTPUT.relative_to(ROOT)  # Reports intentionally use portable checkout-relative paths.
    OUTPUT.mkdir(parents=True, exist_ok=True)
    expected = json.loads((FIXTURE / "before_state_expected.json").read_text())
    requirements = json.loads((FIXTURE / "acceptance_requirements.json").read_text())
    if not RENDERER.is_file():
        raise RuntimeError(f"renderer missing: {RENDERER}")
    input_contract = expected["inputs"] if args.mode == "before-state" else requirements["inputs"]
    for relative, expected_hash in input_contract.items():
        require_equal(f"input hash {relative}", sha256(FIXTURE / relative), expected_hash)

    if args.reuse_existing:
        direct_frame, direct_summary_path = retained("direct")
        reflected_frame, reflected_summary_path = retained("reflected")
    else:
        direct_frame, direct_summary_path = render("request_direct_probe.json", "direct")
        reflected_frame, reflected_summary_path = render("request_reflected_probe.json", "reflected")
    raw_frame = None
    raw_summary_path = None
    if args.mode in ("acceptance", "radiance-isolation"):
        if args.reuse_existing:
            raw_frame, raw_summary_path = retained("reflected_raw")
        else:
            raw_frame, raw_summary_path = render("request_reflected_raw_probe.json", "reflected_raw")

    direct_summary = json.loads(direct_summary_path.read_text())
    reflected_summary = json.loads(reflected_summary_path.read_text())
    raw_summary = json.loads(raw_summary_path.read_text()) if raw_summary_path else None
    direct_stable = stable_summary(direct_summary)
    reflected_stable = stable_summary(reflected_summary)
    definition = requirements["reflected_signal"]["pixel_definition"]
    direct_metrics = bmp_metrics(direct_frame, definition)
    reflected_metrics = bmp_metrics(reflected_frame, definition)
    raw_isolation = radiance_isolation(raw_summary, raw_frame) if raw_frame and raw_summary else None
    neutral_subject = low_chroma_subject_probe(raw_summary, raw_frame) if raw_frame and raw_summary else None
    failures = acceptance_failures(
        reflected_metrics, reflected_summary, raw_isolation, neutral_subject, requirements
    )
    fixed_pixel = requirements.get("fixed_raw_subject_pixel")
    fixed_rgb = None
    if fixed_pixel and raw_frame:
        fixed_rgb = bmp_rgb_at(raw_frame, fixed_pixel["x"], fixed_pixel["y"])
        if min(fixed_rgb) > fixed_pixel["channel_floor_max"] or fixed_rgb[2] - fixed_rgb[0] < fixed_pixel["blue_minus_red_min"]:
            failures.append("fixed reflected-subject pixel loses blue response or washes out")
    ratio = reflected_metrics["blue_pixels"] / direct_metrics["blue_pixels"] if direct_metrics["blue_pixels"] else 0.0

    if args.mode == "before-state":
        require_equal("direct frame hash", sha256(direct_frame), expected["frames"]["direct_sha256"])
        require_equal("reflected frame hash", sha256(reflected_frame), expected["frames"]["reflected_sha256"])
        require_equal("direct stable summary", direct_stable, expected["stable_summaries"]["direct"])
        require_equal("reflected stable summary", reflected_stable, expected["stable_summaries"]["reflected"])
        for lane, metrics in (("direct", direct_metrics), ("reflected", reflected_metrics)):
            require_equal(f"{lane} blue pixels", metrics["blue_pixels"], expected["image_metrics"][f"{lane}_blue_pixels"])
            require_equal(f"{lane} chroma pixels", metrics["chroma_pixels"], expected["image_metrics"][f"{lane}_chroma_pixels"])
        if not failures:
            raise RuntimeError("before-state unexpectedly satisfies the Phase 1 acceptance contract")

    report = {
        "schema": "rt_mirror_1_recursive_fidelity_report_v2",
        "mode": args.mode,
        "before_state_status": "pass" if args.mode == "before-state" else "not_evaluated",
        "acceptance_status": "pass" if not failures else "fail",
        "acceptance_failures": failures,
        "acceptance_contract": requirements,
        "fixed_raw_subject_pixel_rgb8": fixed_rgb,
        "frames": {
            "direct": {"path": str(direct_frame.relative_to(ROOT)), "sha256": sha256(direct_frame)},
            "reflected": {"path": str(reflected_frame.relative_to(ROOT)), "sha256": sha256(reflected_frame)},
        },
        "image_metrics": {
            "direct": direct_metrics,
            "reflected": reflected_metrics,
            "reflected_to_direct_blue_pixel_ratio": ratio,
        },
        "stable_summaries": {"direct": direct_stable, "reflected": reflected_stable},
        "mirror_environment_miss_audit": reflected_summary.get("mirror_recursive_fidelity", {}).get(
            "environment_miss_radiance", {}
        ),
    }
    if raw_frame and raw_summary:
        reconstructed_isolation = radiance_isolation(reflected_summary, reflected_frame)
        require_equal(
            "raw isolation probe object",
            raw_isolation["probe"].get("object_id"),
            "smooth_subject",
        )
        require_equal(
            "raw isolation probe material",
            raw_isolation["probe"].get("material_id"),
            2,
        )
        require_equal(
            "raw final byte versus tone-map prediction",
            raw_isolation["final_minus_single_sample_tonemap_rgb8"],
            [0, 0, 0],
        )
        require_equal(
            "neutral probe object",
            neutral_subject["probe"].get("object_id"),
            "smooth_subject",
        )
        require_equal(
            "neutral probe material",
            neutral_subject["probe"].get("material_id"),
            2,
        )
        require_equal(
            "neutral probe final byte versus tone-map prediction",
            neutral_subject["final_minus_single_sample_tonemap_rgb8"],
            [0, 0, 0],
        )
        neutral_energy = neutral_subject["probe"]["host_mirror_energy_composition"]
        neutral_composed = [
            float(value)
            for value in neutral_subject["probe"]["composed_pre_tonemap_linear_rgb"]
        ]
        neutral_reflection = [
            float(value)
            for value in neutral_subject["probe"]["reflection_after_bsdf_linear_rgb"]
        ]
        neutral_disjoint_terms = [0.0, 0.0, 0.0]
        for term in (
            "local_diffuse_after_attenuation",
            "local_specular_after_attenuation",
            "ambient_after_attenuation",
            "stochastic_direct",
            "stochastic_bsdf",
            "recursive_direct",
            "recursive_bsdf",
        ):
            values = [float(value) for value in neutral_energy[term]]
            neutral_disjoint_terms = [
                neutral_disjoint_terms[channel] + values[channel]
                for channel in range(3)
            ]
        neutral_disjoint_terms = [
            neutral_disjoint_terms[channel] + neutral_reflection[channel]
            for channel in range(3)
        ]
        if max(
            abs(neutral_composed[channel] - neutral_disjoint_terms[channel])
            for channel in range(3)
        ) > 1e-6:
            raise RuntimeError("neutral reflected-subject pixel is not single-accounted")
        if raw_isolation["stage_metrics"]["first_vertex_blue_over_red"] <= 1.15:
            raise RuntimeError("raw first reflected vertex is not blue-biased")
        if raw_isolation["stage_metrics"]["composed_pre_tonemap_blue_over_red"] <= 1.15:
            raise RuntimeError("pre-tone-map composed reflection is not blue-biased")
        if raw_isolation["stage_metrics"]["final_blue_over_red"] <= 1.15:
            raise RuntimeError("tone-mapped probe loses the required blue/red ratio")
        if max(abs(value) for value in raw_isolation["stage_metrics"]["composition_sum_delta_rgb"]) > 1e-6:
            raise RuntimeError("host mirror energy terms do not sum to the composed linear pixel")
        energy = raw_isolation["probe"]["host_mirror_energy_composition"]
        attenuation = float(energy["base_attenuation"])
        for term in ("direct_lighting_input", "local_diffuse", "local_specular", "ambient"):
            before = [float(value) for value in energy[f"{term}_before_attenuation"]]
            after = [float(value) for value in energy[f"{term}_after_attenuation"]]
            for channel in range(3):
                expected_after = before[channel] * attenuation
                if abs(after[channel] - expected_after) > 1e-6:
                    raise RuntimeError(
                        f"{term} channel {channel} bypasses mirror base attenuation: "
                        f"expected {expected_after}, got {after[channel]}"
                    )
        environment_miss = raw_summary.get("mirror_recursive_fidelity", {}).get(
            "environment_miss_radiance", {}
        )
        first_environment_count = int(
            environment_miss.get("first_reflection_contributing_pixels", 0)
        )
        first_environment_rgb = [
            float(value) for value in environment_miss.get("first_reflection_rgb", [])
        ]
        if first_environment_count <= 0 or len(first_environment_rgb) != 3 or max(first_environment_rgb) <= 0.0:
            raise RuntimeError("enabled environment does not contribute to first-reflection misses")
        first_depth = raw_summary["mirror_recursive_fidelity"]["path_depths"][0]
        if first_environment_count > int(first_depth["termination_reasons"]["no_hit"]):
            raise RuntimeError("environment miss contributions exceed first-reflection no-hit outcomes")
        report["radiance_isolation"] = {
            "classification": "host_mirror_energy_budget_preserves_reflected_chroma_through_tonemap",
            "raw_single_sample_no_reconstruction": raw_isolation,
            "temporal_reconstructed": reconstructed_isolation,
            "low_chroma_high_luma_reflected_subject": neutral_subject,
            "reconstruction_probe_rgb8_delta_from_raw": [
                reconstructed_isolation["final_frame_rgb8"][i]
                - raw_isolation["final_frame_rgb8"][i]
                for i in range(3)
            ],
            "denoise_preserved_mirror_glossy_pixels": reflected_summary["render_stats"][
                "denoise_preserved_mirror_glossy_pixel_count"
            ],
            "denoise_radiance_luma_delta": reflected_summary["render_stats"][
                "denoise_radiance_luma_delta"
            ],
            "raw_frame": {
                "path": str(raw_frame.relative_to(ROOT)),
                "sha256": sha256(raw_frame),
            },
        }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    report_name = {
        "before-state": "before_state_report.json",
        "acceptance": "acceptance_report.json",
        "radiance-isolation": "radiance_isolation_report.json",
    }[args.mode]
    report_path = OUTPUT / report_name
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")

    if args.mode == "acceptance" and failures:
        print(f"RT-MIRROR-1 recursive fidelity acceptance: FAIL ({len(failures)} unmet requirements)", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    if args.mode == "radiance-isolation":
        print(f"RT-MIRROR-1 radiance isolation: PASS ({report_path.relative_to(ROOT)})")
        return 0
    print(f"RT-MIRROR-1 recursive fidelity {args.mode}: PASS ({report_path.relative_to(ROOT)})")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, RuntimeError, struct.error, subprocess.CalledProcessError) as exc:
        print(f"RT-MIRROR-1 recursive fidelity: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
