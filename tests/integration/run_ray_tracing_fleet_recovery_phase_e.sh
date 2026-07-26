#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/integration/lib/common.sh"

WORK_ROOT="$(ray_tracing_test_reset_work_root fleet_recovery_phase_e "$ROOT_DIR")"
JOBS_ROOT="$WORK_ROOT/jobs"
JOB_ID="phase_e_interrupted"
JOB_ROOT="$JOBS_ROOT/$JOB_ID"
OUTPUT_ROOT="$WORK_ROOT/output"
REQUEST_PATH="$JOB_ROOT/job_request.json"
STATUS_PATH="$JOB_ROOT/job_status.json"
DESCRIPTOR_PATH="$JOB_ROOT/ray_tracing_recovery_descriptor.json"
RUNNER="$(ray_tracing_tool_path ray_tracing_job_runner "$ROOT_DIR")"

mkdir -p "$JOB_ROOT/output" "$OUTPUT_ROOT"
printf '{"schema_version":"fixture","run_id":"phase_e"}\n' >"$REQUEST_PATH"
cat >"$STATUS_PATH" <<JSON
{
  "job_id": "$JOB_ID",
  "state": "running",
  "stage": "rendering",
  "request_path": "$REQUEST_PATH",
  "output_root": "$OUTPUT_ROOT",
  "progress_path": "$JOB_ROOT/render_progress.json",
  "summary_path": "$JOB_ROOT/result_summary.json",
  "stdout_path": "$JOB_ROOT/stdout.log",
  "stderr_path": "$JOB_ROOT/stderr.log",
  "pid": 999999,
  "requested_start_frame": 0,
  "requested_frame_count": 2,
  "effective_start_frame": 0,
  "effective_frame_count": 2,
  "temporal_subpasses_total": 1,
  "submitted_at_utc": "2026-07-24T00:00:00Z",
  "started_at_utc": "2026-07-24T00:00:01Z",
  "updated_at_utc": "2026-07-24T00:00:00Z"
}
JSON

RESULT="$("$RUNNER" reconcile --jobs-root "$JOBS_ROOT")"
python3 - "$RESULT" "$STATUS_PATH" "$DESCRIPTOR_PATH" <<'PY'
import hashlib
import json
import pathlib
import sys

result = json.loads(sys.argv[1])
status_path = pathlib.Path(sys.argv[2])
descriptor_path = pathlib.Path(sys.argv[3])
status = json.loads(status_path.read_text())
descriptor = json.loads(descriptor_path.read_text())
expected_request_digest = hashlib.sha256(
    pathlib.Path(status["request_path"]).read_bytes()
).hexdigest()

assert result["status"] == "reconciled"
assert result["jobs_scanned"] == 1
assert result["recovery_descriptors"] == 1
assert status["state"] == "interrupted"
assert status["stage"] == "resumable"
assert status["resume_available"] is True
assert descriptor["schema"] == "ray_tracing_recovery_descriptor_v1"
assert descriptor["recovery_state"] == "resumable"
assert descriptor["request_sha256"] == expected_request_digest
assert descriptor["checkpoint_kind"] == "empty_prefix"
assert descriptor["checkpoint_sha256"] == expected_request_digest
assert descriptor["resume_from_frame"] == 0
PY

if find "$JOBS_ROOT" -name 'resume_authority.receipt.json' -print -quit | grep -q .; then
  echo "boot reconciliation consumed resume authority or started work" >&2
  exit 1
fi

echo "ray tracing fleet recovery phase E boot reconciliation passed"

LIVE_JOBS_ROOT="$WORK_ROOT/live_jobs"
LIVE_OUTPUT="$WORK_ROOT/live_output"
LIVE_REQUEST="$WORK_ROOT/live_request.json"
SCENE="$ROOT_DIR/config/samples/ps4d_runtime_scene_visual_test.json"
mkdir -p "$LIVE_JOBS_ROOT"

python3 - "$LIVE_REQUEST" "$LIVE_OUTPUT" "$SCENE" <<'PY'
import json
import sys
request_path, output_root, scene = sys.argv[1:]
payload = {
    "schema_version": "ray_tracing_agent_render_request_v1",
    "run_id": "fleet_recovery_phase_e",
    "scene": {"runtime_scene_path": scene},
    "volume": {"enabled": False},
    "render": {
        "start_frame": 0,
        "frame_count": 1,
        "width": 64,
        "height": 48,
        "normalized_t": 0.0,
        "temporal_frames": 4,
        "use_tiled_renderer": True,
        "tile_size": 16,
        "adaptive_sampling_enabled": True,
        "integrator_3d": "diffuse_bounce",
    },
    "output": {"root": output_root, "overwrite": True},
    "progress": {
        "summary_path": output_root + "/unused_summary.json",
        "progress_path": output_root + "/unused_progress.json",
    },
}
pathlib = __import__("pathlib")
pathlib.Path(request_path).write_text(json.dumps(payload, indent=2) + "\n")
PY

job_id_from_output() {
  sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p'
}

wait_for_state() {
  local job_id="$1"
  local state="$2"
  local status=""
  # The full stable lane can leave the host under sustained compile/render
  # pressure before Phase E starts. Allow the detached worker up to 60 seconds
  # to reach its expected state; standalone runs normally finish in seconds.
  for _ in $(seq 1 600); do
    status="$("$RUNNER" status --job-id "$job_id" --jobs-root "$LIVE_JOBS_ROOT")"
    if printf '%s' "$status" | grep -q "\"state\": \"$state\""; then
      printf '%s' "$status"
      return 0
    fi
    sleep 0.1
  done
  printf '%s\n' "$status" >&2
  return 1
}

wait_for_file() {
  local path="$1"
  for _ in $(seq 1 600); do
    if [[ -s "$path" ]]; then
      return 0
    fi
    sleep 0.1
  done
  echo "timed out waiting for file: $path" >&2
  return 1
}

wait_for_event_message() {
  local event_dir="$1"
  local message_type="$2"
  for _ in $(seq 1 600); do
    if grep -q "\"message_type\":\"$message_type\"" \
         "$event_dir"/*.json 2>/dev/null; then
      return 0
    fi
    sleep 0.1
  done
  echo "timed out waiting for worker event: $message_type in $event_dir" >&2
  return 1
}

INTERRUPTED_OUTPUT="$(
  RAY_TRACING_TEST_EXIT_AFTER_CHECKPOINT_SUBPASS=2 \
    "$RUNNER" submit --request "$LIVE_REQUEST" --jobs-root "$LIVE_JOBS_ROOT"
)"
INTERRUPTED_ID="$(printf '%s' "$INTERRUPTED_OUTPUT" | job_id_from_output)"
wait_for_state "$INTERRUPTED_ID" interrupted >/dev/null
LIVE_DESCRIPTOR="$LIVE_JOBS_ROOT/$INTERRUPTED_ID/ray_tracing_recovery_descriptor.json"
test -s "$LIVE_DESCRIPTOR"

AUTHORITY_PATH="$WORK_ROOT/resume_authority.json"
FENCE_PATH="$WORK_ROOT/output_fence.json"
RECEIPT_PATH="$WORK_ROOT/recovery_receipts/phase_e_token_1.receipt.json"
mkdir -p "$(dirname "$RECEIPT_PATH")"
python3 - "$LIVE_DESCRIPTOR" "$AUTHORITY_PATH" "$FENCE_PATH" "$RECEIPT_PATH" <<'PY'
import json
import pathlib
import sys
descriptor_path, authority_path, fence_path, receipt_path = map(pathlib.Path, sys.argv[1:])
descriptor = json.loads(descriptor_path.read_text())
authority = {
    "schema": "ray_tracing_resume_authority_v1",
    "token_id": "phase_e_token_1",
    "source_job_id": descriptor["job_id"],
    "worker_id": "phase_e_worker_a",
    "lease_id": "phase_e_lease_7",
    "lease_generation": 7,
    "output_generation": 11,
    "request_sha256": descriptor["request_sha256"],
    "checkpoint_sha256": descriptor["checkpoint_sha256"],
    "fence_relpath": str(fence_path.relative_to(authority_path.parent)),
    "receipt_relpath": str(receipt_path.relative_to(authority_path.parent)),
    "expires_at_utc": "2999-01-01T00:00:00Z",
}
fence = {
    "schema": "ray_tracing_output_fence_v1",
    "token_id": authority["token_id"],
    "worker_id": authority["worker_id"],
    "lease_id": authority["lease_id"],
    "lease_generation": authority["lease_generation"],
    "output_generation": authority["output_generation"],
    "active": True,
}
authority_path.write_text(json.dumps(authority, indent=2) + "\n")
fence_path.write_text(json.dumps(fence, indent=2) + "\n")
PY

RESUMED_OUTPUT="$(
  RAY_TRACING_FLEET_JOB=1 \
  RAY_TRACING_RECOVERY_DESCRIPTOR_PATH="$LIVE_DESCRIPTOR" \
  RAY_TRACING_RESUME_AUTHORITY_PATH="$AUTHORITY_PATH" \
  RAY_TRACING_RECOVERY_WORKER_ID="phase_e_worker_a" \
    "$RUNNER" submit --request "$LIVE_REQUEST" --jobs-root "$LIVE_JOBS_ROOT" --resume
)"
RESUMED_ID="$(printf '%s' "$RESUMED_OUTPUT" | job_id_from_output)"
wait_for_state "$RESUMED_ID" completed >/dev/null
wait_for_file "$RECEIPT_PATH"
grep -q '"recovery_authorized":true' \
  "$LIVE_JOBS_ROOT/$RESUMED_ID/worker_request.json"
wait_for_event_message \
  "$LIVE_JOBS_ROOT/$RESUMED_ID/worker_events" \
  completion

if RAY_TRACING_FLEET_JOB=1 \
   "$RUNNER" submit --request "$LIVE_REQUEST" --jobs-root "$LIVE_JOBS_ROOT" --resume \
     >"$WORK_ROOT/unauthorized.out" 2>"$WORK_ROOT/unauthorized.err"; then
  echo "fleet resume unexpectedly succeeded without coordinator authority" >&2
  exit 1
fi
grep -q "fleet resume requires coordinator-issued recovery authority" \
  "$WORK_ROOT/unauthorized.err"

echo "ray tracing fleet recovery phase E manual authority resume passed"
