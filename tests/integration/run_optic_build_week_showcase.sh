#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CLI="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"
REQUEST="$ROOT_DIR/config/samples/optic_build_week_showcase/render_request.json"
OUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/optic_build_week_showcase"
SUMMARY="$OUT_ROOT/render_summary.json"
FRAME0="$OUT_ROOT/frames/frame_0000.bmp"

mkdir -p "$OUT_ROOT"
"$CLI" --request "$REQUEST" --render --summary "$SUMMARY" > "$OUT_ROOT/stdout_summary.json"

python3 - "$SUMMARY" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as stream:
    summary = json.load(stream)

assert summary["scene_applied"] is True
assert summary["route_native_3d"] is True
assert summary["prepared_frame"] is True
assert summary["rendered_frames"] is True
assert summary["frames_rendered"] == 1
assert summary["volume_summary"]["enabled"] is False
assert summary["inspection"]["caustic_state"]["mode"] == "off"
assert summary["inspection"]["caustic_state"]["photon_map_requested"] is False
assert summary["prepared_acceleration"]["active_trace_route"] == "tlas_blas"

objects = {entry["object_id"]: entry for entry in summary["object_audit"]}
expected = {
    "reflection_blob": 20480,
    "grooved_orb": 16128,
    "lattice_shell": 8064,
}
for object_id, triangle_count in expected.items():
    assert objects[object_id]["triangle_count"] == triangle_count
    assert objects[object_id]["primary_hit_pixels"] > 0
PY

test -s "$FRAME0"
test "$(dd if="$FRAME0" bs=1 count=2 2>/dev/null)" = "BM"
echo "optiC Build Week showcase passed: $FRAME0"
