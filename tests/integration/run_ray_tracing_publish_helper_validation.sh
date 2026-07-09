#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
WORK_ROOT="$(ray_tracing_test_reset_work_root publish_helper_validation "$ROOT_DIR")"
RUN_ROOT="$WORK_ROOT/run_safe"
FRAME_DIR="$RUN_ROOT/ray_tracing/output/frames"
DROP_ID="ray-tracing--render-review--20260621T000000Z--r4s2safe"
DROP_ROOT="$(ray_tracing_visualizer_drop_root "$DROP_ID" "$ROOT_DIR")"

rm -rf "$DROP_ROOT"
mkdir -p "$FRAME_DIR" "$RUN_ROOT/ray_tracing"

cat > "$RUN_ROOT/ray_request.json" <<'JSON'
{"schema_version":"ray_tracing_agent_render_request_v1"}
JSON
cat > "$RUN_ROOT/ray_tracing/render_summary.json" <<'JSON'
{"schema_version":"ray_tracing_headless_summary_v1","stage":"completed"}
JSON
python3 - "$FRAME_DIR/frame_0000.png" <<'PY'
import base64
import sys

png = (
    b"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8"
    b"/x8AAwMCAO+/p9sAAAAASUVORK5CYII="
)
with open(sys.argv[1], "wb") as handle:
    handle.write(base64.b64decode(png))
PY

"$ROOT_DIR/tools/publish_render_outputs.sh" \
  --run-root "$RUN_ROOT" \
  --set-id r4s2_safe \
  --title "R4 S2 Safe" \
  --frame frame_0000.png \
  --mode visualizer \
  --drop-id "$DROP_ID" \
  --stage-only

test -f "$DROP_ROOT/manifest.json"
test -f "$DROP_ROOT/outputs/frame_0000.png"
test -f "$DROP_ROOT/inputs/request.json"

expect_reject() {
  label="$1"
  shift
  set +e
  "$@" >"$WORK_ROOT/$label.out" 2>"$WORK_ROOT/$label.err"
  status=$?
  set -e
  if [ "$status" -eq 0 ]; then
    echo "expected rejection for $label" >&2
    exit 1
  fi
}

expect_reject bad_set_id \
  "$ROOT_DIR/tools/publish_render_outputs.sh" \
    --run-root "$RUN_ROOT" \
    --set-id ../bad \
    --frame frame_0000.png \
    --stage-only

expect_reject bad_frame \
  "$ROOT_DIR/tools/publish_render_review_set.sh" \
    --run-root "$RUN_ROOT" \
    --set-id r4s2_safe \
    --frame ../frame_0000.png

expect_reject bad_latest_job_type \
  "$ROOT_DIR/tools/publish_latest_render_run.sh" \
    --run-root "$RUN_ROOT" \
    --set-id r4s2_safe \
    --frame frame_0000.png \
    --job-type ../bad \
    --stage-only

expect_reject bad_material_set_id \
  "$ROOT_DIR/tools/publish_material_preview_set.sh" \
    --request "$RUN_ROOT/ray_request.json" \
    --set-id ../bad

grep -q "invalid --set-id" "$WORK_ROOT/bad_set_id.err"
grep -q "invalid --frame" "$WORK_ROOT/bad_frame.err"
grep -q "invalid --job-type" "$WORK_ROOT/bad_latest_job_type.err"
grep -q "invalid --set-id" "$WORK_ROOT/bad_material_set_id.err"

echo "ray tracing publish helper validation passed"
