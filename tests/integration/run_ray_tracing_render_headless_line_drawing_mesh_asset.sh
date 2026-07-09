#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CLI="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"
if [[ ! -x "$CLI" ]]; then
  CLI="$ROOT_DIR/build/$(uname -m)/tools/cli/ray_tracing_render_headless"
fi

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_line_drawing_high_quality_mesh_request.json"
OUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/line_drawing_high_quality_mesh_asset_parity"
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
assert bvh["ready"] is True, bvh
assert bvh["triangle_count"] == 16130, bvh["triangle_count"]
assert bvh["node_count"] > 1, bvh["node_count"]
assert bvh["leaf_count"] > 1, bvh["leaf_count"]
assert bvh["trace_calls"] > 0, bvh["trace_calls"]
assert bvh["trace_overflows"] == 0, bvh["trace_overflows"]

cache = summary["prepared_scene_cache"]
assert cache["valid"] is True, cache
assert cache["cached_triangle_count"] == 16130, cache
assert cache["cached_bvh_node_count"] in (0, bvh["node_count"]), cache
assert cache["cached_bvh_leaf_count"] in (0, bvh["leaf_count"]), cache

audit = {entry["object_id"]: entry for entry in summary["object_audit"]}
entry = audit["high_quality_sphere"]
assert entry["triangle_count"] == 16128, entry["triangle_count"]
assert entry["primary_hit_pixels"] > 0, entry["primary_hit_pixels"]
PY

test -s "$FRAME0"
test "$(dd if="$FRAME0" bs=1 count=2 2>/dev/null)" = "BM"
