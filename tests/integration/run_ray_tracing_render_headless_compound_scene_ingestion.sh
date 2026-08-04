#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"
ROOT_DIR="$(ray_tracing_root_dir)"
CLI="$(ray_tracing_tool_path ray_tracing_render_headless "$ROOT_DIR")"
FIXTURE="$ROOT_DIR/tests/fixtures/compound_scene_handoff"
WORK_ROOT="$(ray_tracing_test_reset_work_root compound_scene_ingestion "$ROOT_DIR")"
SUMMARY="$WORK_ROOT/summary.json"

"$CLI" --request "$FIXTURE/s9i_agent_request.json" --render --summary "$SUMMARY" >/dev/null
grep -q '"prepared_frame": true' "$SUMMARY"
grep -q '"rendered_frames": true' "$SUMMARY"
grep -q '"frames_rendered": 1' "$SUMMARY"
grep -q '"object_count": 7' "$SUMMARY"
grep -q '"prepared_frame_triangle_count": 58' "$SUMMARY"
grep -q '^  "diagnostics": "ok"' "$SUMMARY"

python3 - "$FIXTURE/s9i_ingestion.json" "$WORK_ROOT/bad_ingestion.json" \
  "$FIXTURE/s9i_agent_request.json" "$WORK_ROOT/bad_request.json" <<'PY'
import json, pathlib, sys
source, bad, request, bad_request = map(pathlib.Path, sys.argv[1:])
descriptor = json.loads(source.read_text())
descriptor["tick"] = 721
descriptor["handoff_path"] = str((source.parent / descriptor["handoff_path"]).resolve())
descriptor["room_path"] = str((source.parent / descriptor["room_path"]).resolve())
bad.write_text(json.dumps(descriptor))
payload = json.loads(pathlib.Path(request).read_text())
payload["scene"]["runtime_scene_path"] = str((pathlib.Path(request).parent /
                                                payload["scene"]["runtime_scene_path"]).resolve())
payload["scene"]["compound_scene_ingestion_path"] = str(bad)
bad_request.write_text(json.dumps(payload))
PY
if "$CLI" --request "$WORK_ROOT/bad_request.json" --preflight --summary "$WORK_ROOT/bad_summary.json" >/dev/null; then
  echo "invalid compound-scene descriptor unexpectedly prepared" >&2
  exit 1
fi
grep -q 'compound ingestion resolution failed' "$WORK_ROOT/bad_summary.json"
