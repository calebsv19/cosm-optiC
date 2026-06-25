#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CLI="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"
if [[ ! -x "$CLI" ]]; then
  CLI="$ROOT_DIR/build/$(uname -m)/tools/cli/ray_tracing_render_headless"
fi

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_mesh_asset_spheres_request.json"
OUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/mesh_asset_spheres_mrt4"
SUMMARY="$OUT_ROOT/render_summary.json"
FRAME0="$OUT_ROOT/frames/frame_0000.bmp"

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
PY

test -s "$FRAME0"
test "$(dd if="$FRAME0" bs=1 count=2 2>/dev/null)" = "BM"
