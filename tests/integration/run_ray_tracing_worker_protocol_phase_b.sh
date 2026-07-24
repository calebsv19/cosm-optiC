#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
RUNNER="$(ray_tracing_tool_path ray_tracing_job_runner "$ROOT_DIR")"
WORKER="$(ray_tracing_tool_path ray_tracing_worker_runtime "$ROOT_DIR")"
RUN_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/worker_protocol_phase_b"
JOBS_ROOT="$RUN_ROOT/jobs"
SCENE="$ROOT_DIR/config/samples/ps4d_runtime_scene_visual_test.json"

rm -rf "$RUN_ROOT"
mkdir -p "$JOBS_ROOT"

write_request() {
  local request_path="$1"
  local output_root="$2"
  local temporal_frames="$3"
  local width="$4"
  local height="$5"
  python3 - "$request_path" "$output_root" "$SCENE" "$temporal_frames" "$width" "$height" <<'PY'
import json
import sys
request_path, output_root, scene, temporal, width, height = sys.argv[1:]
payload = {
    "schema_version": "ray_tracing_agent_render_request_v1",
    "run_id": "worker_protocol_phase_b",
    "scene": {"runtime_scene_path": scene},
    "volume": {"enabled": False},
    "render": {
        "start_frame": 0,
        "frame_count": 1,
        "width": int(width),
        "height": int(height),
        "normalized_t": 0.0,
        "temporal_frames": int(temporal),
        "integrator_3d": "diffuse_bounce",
    },
    "output": {"root": output_root, "overwrite": True},
    "progress": {
        "summary_path": output_root + "/unused_summary.json",
        "progress_path": output_root + "/unused_progress.json",
    },
}
with open(request_path, "w", encoding="utf-8") as handle:
    json.dump(payload, handle, indent=2)
    handle.write("\n")
PY
}

submit_job() {
  local request_path="$1"
  local mode="$2"
  local submit_output
  if [[ "$mode" == "direct" ]]; then
    submit_output="$(RAY_TRACING_WORKER_PROTOCOL_MODE=direct "$RUNNER" submit \
      --request "$request_path" --jobs-root "$JOBS_ROOT")"
  else
    submit_output="$("$RUNNER" submit --request "$request_path" --jobs-root "$JOBS_ROOT")"
  fi
  printf '%s' "$submit_output" | sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p'
}

wait_terminal() {
  local job_id="$1"
  local expected="$2"
  local status_json=""
  for _ in $(seq 1 240); do
    status_json="$("$RUNNER" status --job-id "$job_id" --jobs-root "$JOBS_ROOT")"
    if printf '%s' "$status_json" | grep -q "\"state\": \"$expected\""; then
      printf '%s' "$status_json"
      return 0
    fi
    if printf '%s' "$status_json" | grep -Eq '"state": "(failed|recovery_required)"'; then
      printf '%s\n' "$status_json" >&2
      return 1
    fi
    sleep 0.25
  done
  printf '%s\n' "$status_json" >&2
  return 1
}

wait_event() {
  local event_directory="$1"
  local message_type="$2"
  for _ in $(seq 1 120); do
    if grep -q "\"message_type\":\"$message_type\"" \
      "$event_directory"/*.json 2>/dev/null; then
      return 0
    fi
    sleep 0.05
  done
  echo "timed out waiting for worker event: $message_type" >&2
  return 1
}

PROTOCOL_REQUEST="$RUN_ROOT/protocol_request.json"
DIRECT_REQUEST="$RUN_ROOT/direct_request.json"
PROTOCOL_OUTPUT="$RUN_ROOT/protocol_output"
DIRECT_OUTPUT="$RUN_ROOT/direct_output"
write_request "$PROTOCOL_REQUEST" "$PROTOCOL_OUTPUT" 2 160 96
write_request "$DIRECT_REQUEST" "$DIRECT_OUTPUT" 2 160 96

PROTOCOL_JOB_ID="$(submit_job "$PROTOCOL_REQUEST" protocol)"
PROTOCOL_STATUS="$(wait_terminal "$PROTOCOL_JOB_ID" completed)"
PROTOCOL_JOB_ROOT="$JOBS_ROOT/$PROTOCOL_JOB_ID"
printf '%s' "$PROTOCOL_STATUS" | grep -q '"worker_protocol_version": 1'
printf '%s' "$PROTOCOL_STATUS" | grep -q '"execution_mode": "worker_protocol"'
wait_event "$PROTOCOL_JOB_ROOT/worker_events" completion
grep -q '"message_type":"completion"' "$PROTOCOL_JOB_ROOT"/worker_events/*.json
grep -q '"message_type":"checkpoint_reference"' "$PROTOCOL_JOB_ROOT"/worker_events/*.json

DIRECT_JOB_ID="$(submit_job "$DIRECT_REQUEST" direct)"
DIRECT_STATUS="$(wait_terminal "$DIRECT_JOB_ID" completed)"
DIRECT_JOB_ROOT="$JOBS_ROOT/$DIRECT_JOB_ID"
printf '%s' "$DIRECT_STATUS" | grep -q '"worker_protocol_version": 0'
printf '%s' "$DIRECT_STATUS" | grep -q '"execution_mode": "direct_fallback"'
test ! -e "$DIRECT_JOB_ROOT/worker_request.json"

cmp "$PROTOCOL_OUTPUT/frames/frame_0000.bmp" "$DIRECT_OUTPUT/frames/frame_0000.bmp"
python3 - "$PROTOCOL_JOB_ROOT/result_summary.json" "$DIRECT_JOB_ROOT/result_summary.json" <<'PY'
import json
import sys
left = json.load(open(sys.argv[1], encoding="utf-8"))
right = json.load(open(sys.argv[2], encoding="utf-8"))
assert left["rendered_frames"] is True and right["rendered_frames"] is True
assert left["frames_rendered"] == right["frames_rendered"] == 1
assert left["diagnostics"] == right["diagnostics"] == "ok"
assert set(left["timing_breakdown"]) == set(right["timing_breakdown"])
left_numeric = [value for value in left["timing_breakdown"].values()
                if isinstance(value, (int, float))]
right_numeric = [value for value in right["timing_breakdown"].values()
                 if isinstance(value, (int, float))]
assert left_numeric and right_numeric
assert all(value >= 0 for value in left_numeric)
assert all(value >= 0 for value in right_numeric)
PY

TAMPER_ROOT="$RUN_ROOT/tamper"
mkdir -p "$TAMPER_ROOT/events"
python3 - "$PROTOCOL_JOB_ROOT/worker_request.json" "$TAMPER_ROOT/request.json" \
  "$TAMPER_ROOT/events" <<'PY'
import json
import sys
source, target, events = sys.argv[1:]
payload = json.load(open(source, encoding="utf-8"))
payload["event_directory"] = events
payload["request_sha256"] = "0" * 64
with open(target, "w", encoding="utf-8") as handle:
    json.dump(payload, handle, indent=2)
    handle.write("\n")
PY
set +e
"$WORKER" run --message "$TAMPER_ROOT/request.json" \
  >"$TAMPER_ROOT/stdout.log" 2>"$TAMPER_ROOT/stderr.log"
TAMPER_EXIT=$?
set -e
test "$TAMPER_EXIT" -eq 21
grep -q '"message_type":"interruption"' "$TAMPER_ROOT"/events/*.json
grep -q 'request digest mismatch' "$TAMPER_ROOT"/events/*.json

CANCEL_REQUEST="$RUN_ROOT/cancel_request.json"
CANCEL_OUTPUT="$RUN_ROOT/cancel_output"
write_request "$CANCEL_REQUEST" "$CANCEL_OUTPUT" 64 640 360
CANCEL_JOB_ID="$(submit_job "$CANCEL_REQUEST" protocol)"
CANCEL_JOB_ROOT="$JOBS_ROOT/$CANCEL_JOB_ID"
for _ in $(seq 1 80); do
  if test -e "$CANCEL_JOB_ROOT/worker_events/00000001_progress.json"; then
    break
  fi
  sleep 0.05
done
"$RUNNER" cancel --job-id "$CANCEL_JOB_ID" --jobs-root "$JOBS_ROOT" >/dev/null
grep -q '"message_type":"cancellation"' "$CANCEL_JOB_ROOT/worker_cancel.json"
for _ in $(seq 1 80); do
  if grep -q '"message_type":"cancellation"' "$CANCEL_JOB_ROOT"/worker_events/*.json 2>/dev/null; then
    break
  fi
  sleep 0.05
done
grep -q '"state": "cancelled"' "$CANCEL_JOB_ROOT/job_status.json"
grep -q '"message_type":"cancellation"' "$CANCEL_JOB_ROOT"/worker_events/*.json

DEATH_REQUEST="$RUN_ROOT/death_request.json"
DEATH_OUTPUT="$RUN_ROOT/death_output"
write_request "$DEATH_REQUEST" "$DEATH_OUTPUT" 64 640 360
DEATH_JOB_ID="$(submit_job "$DEATH_REQUEST" protocol)"
DEATH_JOB_ROOT="$JOBS_ROOT/$DEATH_JOB_ID"
for _ in $(seq 1 80); do
  if test -e "$DEATH_JOB_ROOT/worker_events/00000001_progress.json"; then
    break
  fi
  sleep 0.05
done
DEATH_PID="$(tr -d '[:space:]' < "$DEATH_JOB_ROOT/pid.txt")"
kill -KILL -- "-$DEATH_PID"
for _ in $(seq 1 80); do
  set +e
  DEATH_STATUS="$("$RUNNER" status --job-id "$DEATH_JOB_ID" --jobs-root "$JOBS_ROOT" 2>/dev/null)"
  STATUS_EXIT=$?
  set -e
  if [[ "$STATUS_EXIT" -eq 0 ]] &&
     printf '%s' "$DEATH_STATUS" | grep -q '"state": "interrupted"'; then
    break
  fi
  sleep 0.05
done
printf '%s' "$DEATH_STATUS" | grep -q '"state": "interrupted"'
printf '%s' "$DEATH_STATUS" | grep -Eq '"stage": "(resumable|recovery_required)"'
printf '%s' "$DEATH_STATUS" | grep -q '"execution_mode": "worker_protocol"'

echo "ray tracing Phase B worker protocol integration passed"
