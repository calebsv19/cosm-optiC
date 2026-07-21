#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CLI="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"
if [[ ! -x "$CLI" ]]; then
  CLI="$ROOT_DIR/build/$(uname -m)/tools/cli/ray_tracing_render_headless"
fi

MATRIX_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/disney_v2_visual_matrix/primitive_glass_corridor"
REQUEST_ROOT="$ROOT_DIR/tests/fixtures/disney_v2_visual_matrix/primitive_glass_corridor"
OFF_REQUEST="$REQUEST_ROOT/request_disney_v2_denoise_off_12.json"
ON_REQUEST="$REQUEST_ROOT/request_disney_v2_denoise_on_12.json"
OFF_OUT="$MATRIX_ROOT/disney_v2_denoise_off_12"
ON_OUT="$MATRIX_ROOT/disney_v2_denoise_on_12"

mkdir -p "$OFF_OUT" "$ON_OUT"
"$CLI" --request "$OFF_REQUEST" --render --summary "$OFF_OUT/render_summary.json" > "$OFF_OUT/stdout_summary.json"
"$CLI" --request "$ON_REQUEST" --render --summary "$ON_OUT/render_summary.json" > "$ON_OUT/stdout_summary.json"

python3 - "$OFF_OUT/render_summary.json" "$ON_OUT/render_summary.json" <<'PY'
import json
import sys

off_path, on_path = sys.argv[1], sys.argv[2]

def load(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def common(summary, run_id, denoise_enabled):
    assert summary["schema_version"] == "ray_tracing_headless_summary_v1"
    assert summary["run_id"] == run_id
    assert summary["integrator_3d"] == "disney_v2"
    assert summary["render"]["temporal_frames"] == 12
    assert summary["render"]["has_denoise_enabled_override"] is True
    assert summary["render"]["denoise_enabled"] is denoise_enabled
    assert summary["denoise"]["has_request_override"] is True
    assert summary["denoise"]["enabled"] is denoise_enabled
    assert summary["scene_applied"] is True
    assert summary["route_native_3d"] is True
    assert summary["prepared_frame"] is True
    assert summary["rendered_frames"] is True
    assert summary["frames_rendered"] == 1
    assert summary["render_stats"]["temporal_committed_subpasses"] == 12
    assert summary["render_stats"]["visible_pixels"] > 0
    assert summary["render_stats"]["nonzero_pixels"] > 0
    prepared_accel = summary["prepared_acceleration"]
    assert prepared_accel["enabled"] is True
    assert prepared_accel["active_trace_route"] == "tlas_blas"
    assert prepared_accel["route_tlas_trace_calls"] > 0
    assert prepared_accel["route_acceleration_failure_calls"] == 0
    assert prepared_accel["route_flattened_fallback_calls"] == 0

off = load(off_path)
on = load(on_path)
common(off, "visual_matrix_glass_corridor_disney_v2_denoise_off_12", False)
common(on, "visual_matrix_glass_corridor_disney_v2_denoise_on_12", True)

off_stats = off["render_stats"]
on_stats = on["render_stats"]

assert off["denoise"]["applied"] is False
assert off_stats["denoise_temporal_frame_count"] == 0
assert off_stats["denoise_raw_pixel_count"] == 0
assert off_stats["denoise_reconstructed_pixel_count"] == 0

assert on["denoise"]["applied"] is True
assert on_stats["denoise_temporal_frame_count"] == 12
assert on_stats["denoise_raw_pixel_count"] > 0
assert on_stats["denoise_reconstructed_pixel_count"] > 0
assert on_stats["denoise_stable_interior_sample_count"] > 0
assert on_stats["denoise_rejected_edge_sample_count"] > 0
assert on_stats["denoise_preserved_transparent_pixel_count"] > 0
assert "denoise_radiance_luma_delta" in on_stats
PY

test -s "$OFF_OUT/frames/frame_0000.bmp"
test -s "$ON_OUT/frames/frame_0000.bmp"
test "$(dd if="$OFF_OUT/frames/frame_0000.bmp" bs=1 count=2 2>/dev/null)" = "BM"
test "$(dd if="$ON_OUT/frames/frame_0000.bmp" bs=1 count=2 2>/dev/null)" = "BM"
