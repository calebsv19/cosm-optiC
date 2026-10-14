#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"
ROOT_DIR="$(ray_tracing_root_dir)"
CLI="$(ray_tracing_tool_path ray_tracing_render_headless "$ROOT_DIR")"

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_image_export_request.json"
OUT_ROOT="$(ray_tracing_test_reset_work_root image_export_smoke "$ROOT_DIR")"
SUMMARY="$OUT_ROOT/render_summary.json"
FRAME0="$OUT_ROOT/frames/frame_0000.bmp"
FRAME1="$OUT_ROOT/frames/frame_0001.bmp"

python3 - "$REQUEST" "$OUT_ROOT/request.json" "$OUT_ROOT" <<'PY_REQUEST'
import json
from pathlib import Path
import sys
source, destination, work = map(Path, sys.argv[1:])
request = json.loads(source.read_text())
scene = Path(request["scene"]["runtime_scene_path"])
request["scene"]["runtime_scene_path"] = str(scene if scene.is_absolute() else (source.parent / scene).resolve())
request["output"]["root"] = str(work)
request["progress"]["summary_path"] = str(work / "render_summary.json")
request["progress"]["progress_path"] = str(work / "render_progress.json")
destination.write_text(json.dumps(request, indent=2) + "\n")
PY_REQUEST
REQUEST="$OUT_ROOT/request.json"
"$CLI" --request "$REQUEST" --render --summary "$SUMMARY" > "$OUT_ROOT/stdout_summary.json"

grep -q '"schema_version": "ray_tracing_headless_summary_v1"' "$SUMMARY"
grep -q '"scene_applied": true' "$SUMMARY"
grep -q '"route_native_3d": true' "$SUMMARY"
grep -q '"prepared_frame": true' "$SUMMARY"
grep -q '"evaluated_scene_bound": true' "$SUMMARY"
grep -q '"evaluated_scene_first_frame": 0' "$SUMMARY"
grep -q '"evaluated_scene_last_frame": 1' "$SUMMARY"
grep -q '"rendered_frames": true' "$SUMMARY"
grep -q '"frames_rendered": 2' "$SUMMARY"

test -s "$FRAME0"
test -s "$FRAME1"
test "$(dd if="$FRAME0" bs=1 count=2 2>/dev/null)" = "BM"
test "$(dd if="$FRAME1" bs=1 count=2 2>/dev/null)" = "BM"
