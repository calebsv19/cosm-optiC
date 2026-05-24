#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CLI="$ROOT_DIR/build/$(uname -m)/tools/cli/ray_tracing_render_headless"
if [[ ! -x "$CLI" ]]; then
  CLI="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"
fi

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_image_export_request.json"
OUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/image_export_smoke"
SUMMARY="$OUT_ROOT/render_summary.json"
FRAME0="$OUT_ROOT/frames/frame_0000.bmp"
FRAME1="$OUT_ROOT/frames/frame_0001.bmp"

mkdir -p "$OUT_ROOT"
"$CLI" --request "$REQUEST" --render --summary "$SUMMARY" > "$OUT_ROOT/stdout_summary.json"

grep -q '"schema_version": "ray_tracing_headless_summary_v1"' "$SUMMARY"
grep -q '"scene_applied": true' "$SUMMARY"
grep -q '"route_native_3d": true' "$SUMMARY"
grep -q '"prepared_frame": true' "$SUMMARY"
grep -q '"rendered_frames": true' "$SUMMARY"
grep -q '"frames_rendered": 2' "$SUMMARY"

test -s "$FRAME0"
test -s "$FRAME1"
test "$(dd if="$FRAME0" bs=1 count=2 2>/dev/null)" = "BM"
test "$(dd if="$FRAME1" bs=1 count=2 2>/dev/null)" = "BM"
