#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_ROOT="$ROOT_DIR/build/$(uname -m)"
RUNNER="$BUILD_ROOT/tools/cli/ray_tracing_job_runner"
if [[ ! -x "$RUNNER" ]]; then
  RUNNER="$ROOT_DIR/build/tools/cli/ray_tracing_job_runner"
fi

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_job_runner_request.json"
JOBS_ROOT="$ROOT_DIR/build/agent_runs/jobs"
RENDER_OUTPUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/job_runner_smoke/render_output"

mkdir -p "$JOBS_ROOT"
rm -rf "$RENDER_OUTPUT_ROOT"

SUBMIT_OUTPUT="$("$RUNNER" submit --request "$REQUEST" --jobs-root "$JOBS_ROOT")"
JOB_ID="$(printf '%s' "$SUBMIT_OUTPUT" | sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p')"

if [[ -z "$JOB_ID" ]]; then
  echo "failed to parse job id from submit output: $SUBMIT_OUTPUT" >&2
  exit 1
fi

JOB_ROOT="$JOBS_ROOT/$JOB_ID"
STATUS_FILE="$JOB_ROOT/job_status.json"
SUMMARY_FILE="$JOB_ROOT/result_summary.json"
PROGRESS_FILE="$JOB_ROOT/render_progress.json"
FRAME_FILE="$RENDER_OUTPUT_ROOT/frames/frame_0000.bmp"

for _ in $(seq 1 60); do
  STATUS_JSON="$("$RUNNER" status --job-id "$JOB_ID" --jobs-root "$JOBS_ROOT")"
  if printf '%s' "$STATUS_JSON" | grep -q '"state": "completed"'; then
    break
  fi
  if printf '%s' "$STATUS_JSON" | grep -q '"state": "failed"'; then
    echo "$STATUS_JSON" >&2
    exit 1
  fi
  sleep 1
done

grep -q '"state": "completed"' "$STATUS_FILE"
grep -q '"schema_version": "ray_tracing_detached_job_status_v1"' "$STATUS_FILE"
grep -q '"temporal_subpasses_started": 2' "$STATUS_FILE"
grep -q '"temporal_subpasses_completed": 2' "$STATUS_FILE"
grep -q '"temporal_subpasses_total": 2' "$STATUS_FILE"
grep -q '"progress_ratio": 1.000000' "$STATUS_FILE"
grep -q '"schema_version": "ray_tracing_headless_summary_v1"' "$SUMMARY_FILE"
grep -q '"rendered_frames": true' "$SUMMARY_FILE"
grep -q '"stage": "completed"' "$PROGRESS_FILE"
grep -q '"temporal_subpasses_started": 2' "$PROGRESS_FILE"
grep -q '"temporal_subpasses_completed": 2' "$PROGRESS_FILE"
grep -q '"temporal_subpasses_total": 2' "$PROGRESS_FILE"
test -f "$JOB_ROOT/stdout.log"
test -f "$JOB_ROOT/stderr.log"
test -f "$JOB_ROOT/pid.txt"
test -f "$FRAME_FILE"
