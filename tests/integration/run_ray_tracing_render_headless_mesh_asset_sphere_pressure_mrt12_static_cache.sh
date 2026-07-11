#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CLI="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"
if [[ ! -x "$CLI" ]]; then
  CLI="$ROOT_DIR/build/$(uname -m)/tools/cli/ray_tracing_render_headless"
fi

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_mesh_asset_sphere_pressure_mrt12_static_cache_request.json"
OUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/mesh_asset_sphere_pressure_mrt12_static_cache"
SUMMARY="$OUT_ROOT/render_summary.json"
FRAME0="$OUT_ROOT/frames/frame_0000.bmp"
FRAME2="$OUT_ROOT/frames/frame_0002.bmp"

mkdir -p "$OUT_ROOT"
"$CLI" --request "$REQUEST" --render --summary "$SUMMARY" > "$OUT_ROOT/stdout_summary.json"

python3 - "$SUMMARY" <<'PY'
import json
import sys

summary_path = sys.argv[1]
with open(summary_path, "r", encoding="utf-8") as f:
    summary = json.load(f)

assert summary["schema_version"] == "ray_tracing_headless_summary_v1"
assert summary["scene_applied"] is True
assert summary["route_native_3d"] is True
assert summary["prepared_frame"] is True
assert summary["rendered_frames"] is True
assert summary["frames_rendered"] == 3

bvh = summary["bvh_summary"]
assert bvh["triangle_count"] == 65030, bvh["triangle_count"]
assert bvh["trace_calls"] > 0, bvh["trace_calls"]
assert bvh["trace_overflows"] == 0, bvh["trace_overflows"]

accel = summary["prepared_acceleration"]
assert accel["active_trace_route"] == "tlas_blas", accel["active_trace_route"]
assert accel["requested_trace_route"] == "tlas_blas", accel["requested_trace_route"]
assert bvh["ready"] is False, bvh
assert bvh["node_count"] == 0, bvh["node_count"]
assert bvh["leaf_count"] == 0, bvh["leaf_count"]
assert accel["route_trace_calls"] > 0, accel["route_trace_calls"]
assert accel["route_tlas_trace_calls"] == accel["route_trace_calls"], accel
assert accel["route_tlas_trace_hits"] > 0, accel
assert accel["route_flattened_fallback_calls"] == 0, accel

cache = summary["prepared_scene_cache"]
assert cache["valid"] is True, cache
assert cache["static_geometry_reuse_enabled"] is True, cache
assert cache["hits"] == 3, cache
assert cache["misses"] == 1, cache
assert cache["stores"] == 1, cache
assert cache["time_independent_hits"] == 2, cache
assert cache["cached_triangle_count"] == 65030, cache
assert cache["cached_bvh_node_count"] in (0, bvh["node_count"]), cache
assert cache["cached_bvh_leaf_count"] in (0, bvh["leaf_count"]), cache
assert abs(cache["cached_normalized_t"] - 0.0) < 1e-9, cache
assert abs(cache["last_requested_normalized_t"] - 1.0) < 1e-9, cache

audit = {entry["object_id"]: entry for entry in summary["object_audit"]}
entry = audit["obj_sphere_pressure_mrt10"]
assert entry["triangle_count"] == 65024, entry["triangle_count"]
assert entry["primary_hit_pixels"] > 0, entry["primary_hit_pixels"]
PY

test -s "$FRAME0"
test -s "$FRAME2"
test "$(dd if="$FRAME0" bs=1 count=2 2>/dev/null)" = "BM"
test "$(dd if="$FRAME2" bs=1 count=2 2>/dev/null)" = "BM"
