#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
RUNNER="$(ray_tracing_tool_path ray_tracing_job_runner "$ROOT_DIR")"
RUN_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/temporal_checkpoint_phase_c"
JOBS_ROOT="$RUN_ROOT/jobs"
SCENE="$ROOT_DIR/config/samples/ps4d_runtime_scene_visual_test.json"
REFERENCE_OUTPUT="$RUN_ROOT/reference_output"
RESUME_OUTPUT="$RUN_ROOT/resume_output"

rm -rf "$RUN_ROOT"
mkdir -p "$JOBS_ROOT"

write_request() {
  local path="$1"
  local output_root="$2"
  python3 - "$path" "$output_root" "$SCENE" <<'PY'
import json
import sys
path, output_root, scene = sys.argv[1:]
payload = {
    "schema_version": "ray_tracing_agent_render_request_v1",
    "run_id": "temporal_checkpoint_phase_c",
    "scene": {"runtime_scene_path": scene},
    "volume": {"enabled": False},
    "render": {
        "start_frame": 0,
        "frame_count": 1,
        "width": 160,
        "height": 96,
        "normalized_t": 0.0,
        "temporal_frames": 4,
        "use_tiled_renderer": True,
        "tile_size": 32,
        "adaptive_sampling_enabled": True,
        "integrator_3d": "diffuse_bounce",
    },
    "output": {"root": output_root, "overwrite": True},
    "progress": {
        "summary_path": output_root + "/unused_summary.json",
        "progress_path": output_root + "/unused_progress.json",
    },
}
with open(path, "w", encoding="utf-8") as handle:
    json.dump(payload, handle, indent=2)
    handle.write("\n")
PY
}

submit() {
  local request="$1"
  shift
  local output
  output="$("$RUNNER" submit --request "$request" --jobs-root "$JOBS_ROOT" "$@")"
  printf '%s' "$output" | sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p'
}

wait_for_state() {
  local job_id="$1"
  local pattern="$2"
  local status=""
  for _ in $(seq 1 300); do
    status="$("$RUNNER" status --job-id "$job_id" --jobs-root "$JOBS_ROOT")"
    if printf '%s' "$status" | grep -Eq "\"state\": \"($pattern)\""; then
      printf '%s' "$status"
      return 0
    fi
    sleep 0.1
  done
  printf '%s\n' "$status" >&2
  return 1
}

wait_for_event() {
  local job_id="$1"
  local message_type="$2"
  for _ in $(seq 1 120); do
    if grep -q "\"message_type\":\"$message_type\"" \
      "$JOBS_ROOT/$job_id"/worker_events/*.json 2>/dev/null; then
      return 0
    fi
    sleep 0.05
  done
  echo "timed out waiting for worker event: $message_type" >&2
  return 1
}

REFERENCE_REQUEST="$RUN_ROOT/reference_request.json"
RESUME_REQUEST="$RUN_ROOT/resume_request.json"
write_request "$REFERENCE_REQUEST" "$REFERENCE_OUTPUT"
write_request "$RESUME_REQUEST" "$RESUME_OUTPUT"

REFERENCE_JOB_ID="$(submit "$REFERENCE_REQUEST")"
wait_for_state "$REFERENCE_JOB_ID" completed >/dev/null

INTERRUPTED_SUBMIT_OUTPUT="$(
  RAY_TRACING_TEST_EXIT_AFTER_CHECKPOINT_SUBPASS=2 \
    "$RUNNER" submit --request "$RESUME_REQUEST" --jobs-root "$JOBS_ROOT"
)"
INTERRUPTED_JOB_ID="$(
  printf '%s' "$INTERRUPTED_SUBMIT_OUTPUT" |
    sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p'
)"
INTERRUPTED_STATUS="$(wait_for_state "$INTERRUPTED_JOB_ID" interrupted)"
printf '%s' "$INTERRUPTED_STATUS" | grep -q '"stage": "resumable"'
printf '%s' "$INTERRUPTED_STATUS" | grep -q '"resume_available": true'
test "$(find "$RESUME_OUTPUT/checkpoints/frame_0000" \
  -name 'generation_*.rtck' -type f | wc -l | tr -d ' ')" -eq 2
test -s "$RESUME_OUTPUT/checkpoints/frame_0000/current.json"
grep -q '"completed_subpasses": 2' \
  "$RESUME_OUTPUT/checkpoints/frame_0000/current.json"
grep -q '"message_type":"checkpoint_reference"' \
  "$JOBS_ROOT/$INTERRUPTED_JOB_ID"/worker_events/*.json
grep -q '"state":"tile_batch"' \
  "$JOBS_ROOT/$INTERRUPTED_JOB_ID"/worker_events/*.json

# Simulate damage to the newest generation. Schema 2 must retain and recover
# from the prior committed generation rather than treating current.json as
# sufficient proof that the newest payload is usable.
NEWEST_GENERATION="$(
  find "$RESUME_OUTPUT/checkpoints/frame_0000" \
    -name 'generation_*.rtck' -type f | sort | tail -n 1
)"
python3 - "$NEWEST_GENERATION" <<'PY'
import os
import sys
path = sys.argv[1]
size = os.path.getsize(path)
assert size > 64
with open(path, "r+b") as handle:
    handle.truncate(size - 17)
PY

RESUMED_JOB_ID="$(submit "$RESUME_REQUEST" --resume)"
wait_for_state "$RESUMED_JOB_ID" completed >/dev/null
wait_for_event "$RESUMED_JOB_ID" completion

cmp "$REFERENCE_OUTPUT/frames/frame_0000.bmp" \
    "$RESUME_OUTPUT/frames/frame_0000.bmp"
test "$(find "$RESUME_OUTPUT/checkpoints/frame_0000" \
  -name 'generation_*.rtck' -type f | wc -l | tr -d ' ')" -eq 2
grep -q '"completed_subpasses": 4' \
  "$RESUME_OUTPUT/checkpoints/frame_0000/current.json"
grep -q '"message_type":"completion"' \
  "$JOBS_ROOT/$RESUMED_JOB_ID"/worker_events/*.json

echo "ray tracing Phase C temporal checkpoint interruption/resume equivalence passed"
