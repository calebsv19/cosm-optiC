#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
CLI="$(ray_tracing_tool_path ray_tracing_render_headless "$ROOT_DIR")"
REQUEST="$ROOT_DIR/tests/fixtures/agent_render_visual_artifact_request.json"
OUT_ROOT="$ROOT_DIR/visual_artifacts/source_first_frame"
SUMMARY="$OUT_ROOT/render_summary.json"
STDOUT_SUMMARY="$OUT_ROOT/stdout_summary.json"
FRAME0="$OUT_ROOT/frames/frame_0000.bmp"
VALIDATION_METRICS="$OUT_ROOT/artifact_validation.json"
BLANK_PROBE="$OUT_ROOT/diagnostics/blank_probe.bmp"

rm -rf "$OUT_ROOT"
mkdir -p "$OUT_ROOT"

"$CLI" --request "$REQUEST" --render --summary "$SUMMARY" > "$STDOUT_SUMMARY"

grep -q '"schema_version": "ray_tracing_headless_summary_v1"' "$SUMMARY"
grep -q '"scene_applied": true' "$SUMMARY"
grep -q '"route_native_3d": true' "$SUMMARY"
grep -q '"prepared_frame": true' "$SUMMARY"
grep -q '"rendered_frames": true' "$SUMMARY"
grep -q '"frames_rendered": 1' "$SUMMARY"

test -s "$FRAME0"
test "$(dd if="$FRAME0" bs=1 count=2 2>/dev/null)" = "BM"
python3 "$ROOT_DIR/tests/integration/validate_ray_tracing_visual_artifact.py" \
  --frame "$FRAME0" \
  --write-metrics "$VALIDATION_METRICS" \
  --make-blank-probe "$BLANK_PROBE"

printf 'ray_tracing visual artifact ready: %s\n' "$FRAME0"
