#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CLI="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"
if [[ ! -x "$CLI" ]]; then
  CLI="$ROOT_DIR/build/$(uname -m)/tools/cli/ray_tracing_render_headless"
fi

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_mesh_asset_spheres_request.json"
TLAS_REQUEST="$ROOT_DIR/tests/fixtures/agent_render_mesh_asset_spheres_tlas_request.json"
FLATTENED_REQUEST="$ROOT_DIR/tests/fixtures/agent_render_mesh_asset_spheres_flattened_request.json"
OUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/mesh_asset_spheres_mrt4"
TLAS_OUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/mesh_asset_spheres_mrt4_tlas"
FLATTENED_OUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/mesh_asset_spheres_mrt4_flattened"
SUMMARY="$OUT_ROOT/render_summary.json"
TLAS_SUMMARY="$TLAS_OUT_ROOT/render_summary.json"
FLATTENED_SUMMARY="$FLATTENED_OUT_ROOT/render_summary.json"
FRAME0="$OUT_ROOT/frames/frame_0000.bmp"
TLAS_FRAME0="$TLAS_OUT_ROOT/frames/frame_0000.bmp"
FLATTENED_FRAME0="$FLATTENED_OUT_ROOT/frames/frame_0000.bmp"

mkdir -p "$OUT_ROOT"
"$CLI" --request "$REQUEST" --render --summary "$SUMMARY" > "$OUT_ROOT/stdout_summary.json"
mkdir -p "$TLAS_OUT_ROOT"
"$CLI" --request "$TLAS_REQUEST" --render --summary "$TLAS_SUMMARY" > "$TLAS_OUT_ROOT/stdout_summary.json"
mkdir -p "$FLATTENED_OUT_ROOT"
"$CLI" --request "$FLATTENED_REQUEST" --render --summary "$FLATTENED_SUMMARY" > "$FLATTENED_OUT_ROOT/stdout_summary.json"

python3 - "$SUMMARY" "$TLAS_SUMMARY" "$FLATTENED_SUMMARY" <<'PY'
import json
import sys

summary_path = sys.argv[1]
tlas_summary_path = sys.argv[2]
flattened_summary_path = sys.argv[3]
with open(summary_path, "r", encoding="utf-8") as f:
    summary = json.load(f)
with open(tlas_summary_path, "r", encoding="utf-8") as f:
    tlas_summary = json.load(f)
with open(flattened_summary_path, "r", encoding="utf-8") as f:
    flattened_summary = json.load(f)

assert summary["schema_version"] == "ray_tracing_headless_summary_v1"
assert summary["scene_applied"] is True
assert summary["route_native_3d"] is True
assert summary["prepared_frame"] is True
assert summary["rendered_frames"] is True
assert summary["frames_rendered"] == 1

bvh = summary["bvh_summary"]
assert bvh["ready"] is True
assert bvh["triangle_count"] == 1238, bvh["triangle_count"]
assert bvh["node_count"] > 1, bvh["node_count"]
assert bvh["leaf_count"] > 1, bvh["leaf_count"]
assert bvh["max_depth"] > 1, bvh["max_depth"]
assert bvh["max_leaf_triangle_count"] <= bvh["leaf_size"], bvh
assert bvh["total_bytes"] > 0, bvh["total_bytes"]
assert bvh["centroid_bytes"] > 0, bvh["centroid_bytes"]
assert bvh["triangle_bounds_min_bytes"] > 0, bvh["triangle_bounds_min_bytes"]
assert bvh["triangle_bounds_max_bytes"] > 0, bvh["triangle_bounds_max_bytes"]
assert bvh["sort_scratch_bytes"] == 0, bvh["sort_scratch_bytes"]
assert bvh["build_scratch_bytes"] == bvh["centroid_bytes"] + bvh["triangle_bounds_min_bytes"] + bvh["triangle_bounds_max_bytes"] + bvh["sort_scratch_bytes"], bvh
assert bvh["trace_calls"] > 0, bvh["trace_calls"]
assert bvh["node_visits"] > 0, bvh["node_visits"]
assert bvh["aabb_tests"] >= bvh["node_visits"], bvh
assert bvh["triangle_tests"] > 0, bvh["triangle_tests"]
assert bvh["trace_overflows"] == 0, bvh["trace_overflows"]
assert bvh["overflow_fallback_calls"] == 0, bvh["overflow_fallback_calls"]

accel = summary["prepared_acceleration"]
assert accel["active_trace_route"] == "tlas_blas", accel["active_trace_route"]
assert accel["requested_trace_route"] == "tlas_blas", accel["requested_trace_route"]
assert accel["route_trace_calls"] > 0, accel["route_trace_calls"]
assert accel["route_tlas_trace_calls"] == accel["route_trace_calls"], accel
assert accel["route_tlas_trace_hits"] > 0, accel["route_tlas_trace_hits"]
assert accel["route_flattened_fallback_calls"] == 0, accel
assert accel["route_parity_mismatches"] == 0, accel["route_parity_mismatches"]

dynamic_accel = summary["dynamic_geometry_acceleration"]
assert dynamic_accel["static_mesh_asset_policy"] == "static_blas_tlas", dynamic_accel
assert dynamic_accel["static_mesh_asset_present"] is True, dynamic_accel
assert dynamic_accel["static_mesh_asset_blas_active"] is True, dynamic_accel
assert dynamic_accel["static_mesh_asset_loaded_assets"] == 3, dynamic_accel
assert dynamic_accel["static_mesh_asset_loaded_instances"] == 3, dynamic_accel
assert dynamic_accel["water_surface_policy"] == "not_present", dynamic_accel
assert dynamic_accel["water_surface_accel_action"] == "none", dynamic_accel
assert dynamic_accel["water_surface_frame_stamp_changed"] is False, dynamic_accel
assert dynamic_accel["water_surface_topology_comparable"] is False, dynamic_accel
assert dynamic_accel["water_surface_topology_stable"] is False, dynamic_accel
assert dynamic_accel["mesh_emissive_policy"] == "not_present", dynamic_accel
assert dynamic_accel["generated_runtime_mesh_policy"] == "file_backed_mesh_asset_static_when_sidecar_resolved", dynamic_accel
assert dynamic_accel["deforming_mesh_policy"] == "not_present_pending_dynamic_accel_contract", dynamic_accel

water_cache = summary["dynamic_water_acceleration_cache"]
assert water_cache["valid"] is False, water_cache
assert water_cache["cache_ready"] is False, water_cache
assert water_cache["last_status"] == "disabled", water_cache
assert water_cache["observed_frames"] == 0, water_cache
assert water_cache["rebuilds"] == 0, water_cache
assert water_cache["refits"] == 0, water_cache
assert water_cache["fallbacks"] == 0, water_cache
assert water_cache["geometry_cache_ready"] is False, water_cache
assert water_cache["geometry_bvh_ready"] is False, water_cache
assert water_cache["geometry_stores"] == 0, water_cache
assert water_cache["geometry_store_failures"] == 0, water_cache
assert water_cache["route_trace_hits"] == 0, water_cache

expected = {
    "obj_sphere_low": 48,
    "obj_sphere_medium": 224,
    "obj_sphere_high": 960,
}
audit = {entry["object_id"]: entry for entry in summary["object_audit"]}
for object_id, triangle_count in expected.items():
    entry = audit[object_id]
    assert entry["triangle_count"] == triangle_count, (object_id, entry["triangle_count"])
    assert entry["primary_hit_pixels"] > 0, (object_id, entry["primary_hit_pixels"])

tlas_accel = tlas_summary["prepared_acceleration"]
assert tlas_summary["rendered_frames"] is True
assert tlas_accel["active_trace_route"] == "tlas_blas", tlas_accel["active_trace_route"]
assert tlas_accel["requested_trace_route"] == "tlas_blas", tlas_accel["requested_trace_route"]
assert tlas_accel["route_trace_calls"] > 0, tlas_accel["route_trace_calls"]
assert tlas_accel["route_tlas_trace_calls"] == tlas_accel["route_trace_calls"], tlas_accel
assert tlas_accel["route_tlas_trace_hits"] > 0, tlas_accel["route_tlas_trace_hits"]
assert tlas_accel["route_flattened_fallback_calls"] == 0, tlas_accel
assert tlas_accel["route_parity_mismatches"] == 0, tlas_accel["route_parity_mismatches"]

flattened_accel = flattened_summary["prepared_acceleration"]
assert flattened_summary["rendered_frames"] is True
assert flattened_accel["active_trace_route"] == "flattened_bvh", flattened_accel["active_trace_route"]
assert flattened_accel["requested_trace_route"] == "flattened_bvh", flattened_accel["requested_trace_route"]
assert flattened_accel["route_trace_calls"] > 0, flattened_accel["route_trace_calls"]
assert flattened_accel["route_flattened_trace_calls"] == flattened_accel["route_trace_calls"], flattened_accel
assert flattened_accel["route_tlas_trace_calls"] == 0, flattened_accel["route_tlas_trace_calls"]
assert flattened_accel["route_parity_mismatches"] == 0, flattened_accel["route_parity_mismatches"]
PY

test -s "$FRAME0"
test "$(dd if="$FRAME0" bs=1 count=2 2>/dev/null)" = "BM"
test -s "$TLAS_FRAME0"
test "$(dd if="$TLAS_FRAME0" bs=1 count=2 2>/dev/null)" = "BM"
test -s "$FLATTENED_FRAME0"
test "$(dd if="$FLATTENED_FRAME0" bs=1 count=2 2>/dev/null)" = "BM"
