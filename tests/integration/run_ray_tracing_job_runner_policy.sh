#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_ROOT="$ROOT_DIR/build/$(uname -m)"
RUNNER="$BUILD_ROOT/tools/cli/ray_tracing_job_runner"
if [[ ! -x "$RUNNER" ]]; then
  RUNNER="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_job_runner"
fi

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_job_runner_resume_request.json"
JOBS_ROOT="$ROOT_DIR/build/agent_runs/jobs"
RENDER_OUTPUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/job_runner_resume_smoke/render_output"
ERR_DIR="/private/tmp/ray_tracing_job_runner_policy"

wait_for_job() {
  local job_id="$1"
  local status_json
  for _ in $(seq 1 60); do
    status_json="$("$RUNNER" status --job-id "$job_id" --jobs-root "$JOBS_ROOT")"
    if printf '%s' "$status_json" | grep -q '"state": "completed"'; then
      return 0
    fi
    if printf '%s' "$status_json" | grep -q '"state": "failed"'; then
      echo "$status_json" >&2
      return 1
    fi
    sleep 1
  done
  echo "timed out waiting for job $job_id" >&2
  return 1
}

submit_job() {
  local extra_args=("$@")
  local submit_output
  submit_output="$("$RUNNER" submit --request "$REQUEST" --jobs-root "$JOBS_ROOT" "${extra_args[@]}")"
  printf '%s' "$submit_output" | sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p'
}

mkdir -p "$JOBS_ROOT"
mkdir -p "$ERR_DIR"
rm -rf "$RENDER_OUTPUT_ROOT"

JOB_ID="$(submit_job)"
[[ -n "$JOB_ID" ]]
wait_for_job "$JOB_ID"

grep -q '"overwrite_policy": "fail_if_exists"' "$JOBS_ROOT/$JOB_ID/job_status.json"
test -f "$RENDER_OUTPUT_ROOT/frames/frame_0000.bmp"
test -f "$RENDER_OUTPUT_ROOT/frames/frame_0001.bmp"
test -f "$RENDER_OUTPUT_ROOT/frames/frame_0002.bmp"

if "$RUNNER" submit --request "$REQUEST" --jobs-root "$JOBS_ROOT" >"$ERR_DIR/submit.out" 2>"$ERR_DIR/submit.err"; then
  echo "expected submit without overwrite/resume to fail on existing outputs" >&2
  exit 1
fi
grep -q 'existing output frames found; use --overwrite or --resume' "$ERR_DIR/submit.err"

rm -f "$RENDER_OUTPUT_ROOT/frames/frame_0002.bmp"
JOB_ID="$(submit_job --resume)"
[[ -n "$JOB_ID" ]]
wait_for_job "$JOB_ID"
grep -q '"overwrite_policy": "resume"' "$JOBS_ROOT/$JOB_ID/job_status.json"
grep -q '"requested_frame_count": 3' "$JOBS_ROOT/$JOB_ID/job_status.json"
grep -q '"effective_frame_count": 1' "$JOBS_ROOT/$JOB_ID/job_status.json"
grep -q '"temporal_subpasses_started": 2' "$JOBS_ROOT/$JOB_ID/job_status.json"
grep -q '"temporal_subpasses_total": 2' "$JOBS_ROOT/$JOB_ID/job_status.json"
test -f "$RENDER_OUTPUT_ROOT/frames/frame_0002.bmp"

JOB_ID="$(submit_job --overwrite)"
[[ -n "$JOB_ID" ]]
wait_for_job "$JOB_ID"
grep -q '"overwrite_policy": "overwrite"' "$JOBS_ROOT/$JOB_ID/job_status.json"

FAKE_JOB_ID="rtjob_fake_stalled"
FAKE_JOB_ROOT="$JOBS_ROOT/$FAKE_JOB_ID"
rm -rf "$FAKE_JOB_ROOT"
mkdir -p "$FAKE_JOB_ROOT"

sleep 30 &
FAKE_PID=$!
trap 'kill "$FAKE_PID" 2>/dev/null || true' EXIT

cat >"$FAKE_JOB_ROOT/pid.txt" <<EOF
$FAKE_PID
EOF

cat >"$FAKE_JOB_ROOT/render_progress.json" <<'EOF'
{
  "schema_version": "ray_tracing_render_progress_v1",
  "run_id": "fake_stalled_run",
  "stage": "rendering_frame",
  "state": "running",
  "frame_index": 0,
  "frames_completed": 0,
  "frame_count": 1,
  "temporal_subpasses_started": 6,
  "temporal_subpasses_completed": 5,
  "temporal_subpasses_total": 6,
  "progress_ratio": 0.833333,
  "updated_at_utc": "2026-05-20T00:00:00Z",
  "diagnostics": "rendering frame (subpass 6/6 active)"
}
EOF

cat >"$FAKE_JOB_ROOT/job_status.json" <<EOF
{
  "schema_version": "ray_tracing_detached_job_status_v1",
  "program": "ray_tracing",
  "tool": "ray_tracing_render_headless",
  "job_id": "$FAKE_JOB_ID",
  "state": "running",
  "stage": "rendering_frame",
  "request_path": "$ROOT_DIR/tests/fixtures/agent_render_job_runner_request.json",
  "output_root": "$RENDER_OUTPUT_ROOT",
  "progress_path": "$FAKE_JOB_ROOT/render_progress.json",
  "summary_path": "$FAKE_JOB_ROOT/result_summary.json",
  "stdout_path": "$FAKE_JOB_ROOT/stdout.log",
  "stderr_path": "$FAKE_JOB_ROOT/stderr.log",
  "pid_path": "$FAKE_JOB_ROOT/pid.txt",
  "pid": $FAKE_PID,
  "exit_code": -1,
  "overwrite_policy": "overwrite",
  "requested_start_frame": 0,
  "requested_frame_count": 1,
  "effective_start_frame": 0,
  "effective_frame_count": 1,
  "frame_index": 0,
  "frames_completed": 0,
  "temporal_subpasses_started": 6,
  "temporal_subpasses_completed": 5,
  "temporal_subpasses_total": 6,
  "progress_ratio": 0.833333,
  "submitted_at_utc": "2026-05-20T00:00:00Z",
  "started_at_utc": "2026-05-20T00:00:00Z",
  "finished_at_utc": "",
  "updated_at_utc": "2026-05-20T00:00:00Z",
  "diagnostics": "rendering frame (subpass 6/6 active)"
}
EOF

STATUS_JSON="$("$RUNNER" status --job-id "$FAKE_JOB_ID" --jobs-root "$JOBS_ROOT")"
printf '%s' "$STATUS_JSON" | grep -q '"state": "stalled"'
printf '%s' "$STATUS_JSON" | grep -q '"temporal_subpasses_started": 6'
printf '%s' "$STATUS_JSON" | grep -q 'no progress update for'

kill "$FAKE_PID" 2>/dev/null || true
trap - EXIT
