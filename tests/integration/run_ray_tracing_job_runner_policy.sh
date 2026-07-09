#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
RUNNER="$(ray_tracing_tool_path ray_tracing_job_runner "$ROOT_DIR")"

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_job_runner_resume_request.json"
JOBS_ROOT="$ROOT_DIR/build/agent_runs/jobs"
RENDER_OUTPUT_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/job_runner_resume_smoke/render_output"
WORK_ROOT="$(ray_tracing_test_reset_work_root job_runner_policy "$ROOT_DIR")"
ERR_DIR="$(ray_tracing_test_diagnostics_dir job_runner_policy "$ROOT_DIR")"
BAD_REQUEST="$WORK_ROOT/job_runner_policy_bad_output_root_request.json"
BAD_BUNDLE_ROOT="$WORK_ROOT/job_runner_policy_bad_bundle"
BAD_BUNDLE_JOB="$BAD_BUNDLE_ROOT/job.json"
BAD_BUNDLE_REQUEST="$BAD_BUNDLE_ROOT/input/run.ray_tracing.json"

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
rm -rf "$RENDER_OUTPUT_ROOT"

if "$RUNNER" submit --request "$REQUEST" --jobs-root ../jobs_escape >"$ERR_DIR/bad_jobs_root.out" 2>"$ERR_DIR/bad_jobs_root.err"; then
  echo "expected relative jobs root to fail" >&2
  exit 1
fi
grep -q 'failed to resolve jobs root' "$ERR_DIR/bad_jobs_root.err"

python3 - "$REQUEST" "$BAD_REQUEST" <<'PY'
import json
import sys

src, dst = sys.argv[1:3]
with open(src, "r", encoding="utf-8") as handle:
    request = json.load(handle)
request["output"]["root"] = "/"
with open(dst, "w", encoding="utf-8") as handle:
    json.dump(request, handle, indent=2)
    handle.write("\n")
PY

if "$RUNNER" submit --request "$BAD_REQUEST" --jobs-root "$JOBS_ROOT" >"$ERR_DIR/bad_output_root.out" 2>"$ERR_DIR/bad_output_root.err"; then
  echo "expected root output path to fail" >&2
  exit 1
fi
grep -q 'invalid detached output root' "$ERR_DIR/bad_output_root.err"

rm -rf "$BAD_BUNDLE_ROOT"
mkdir -p "$BAD_BUNDLE_ROOT/input"
cp "$ROOT_DIR/tests/fixtures/agent_render_job_runner_bundle_request.json" "$BAD_BUNDLE_REQUEST"
cat >"$BAD_BUNDLE_JOB" <<EOF
{
  "schema_family": "codework_job",
  "schema_variant": "headless_bundle_v1",
  "job_id": "../bad",
  "program": "ray_tracing",
  "tool": {
    "name": "ray_tracing",
    "version": "0.1.0",
    "target_os": "linux",
    "target_arch": "x86_64"
  },
  "scene_payload": {
    "schema_family": "codework_scene",
    "schema_variant": "scene_runtime_v1",
    "path": "$ROOT_DIR/config/samples/ps4d_runtime_scene_visual_test.json"
  },
  "run_config": {
    "schema_family": "ray_tracing_request",
    "schema_variant": "ray_tracing_agent_render_request_v1",
    "path": "input/run.ray_tracing.json"
  },
  "outputs": {
    "root": ".",
    "report_path": "output/report.json",
    "logs_dir": ".",
    "artifacts_dir": "output/artifacts"
  },
  "metadata": {
    "title": "Bad bundle id",
    "description": "Invalid bundle job id for policy test",
    "created_by": "codex",
    "created_at": "2026-05-22T00:00:00Z"
  }
}
EOF
if "$RUNNER" submit --request "$BAD_BUNDLE_JOB" --jobs-root "$JOBS_ROOT" >"$ERR_DIR/bad_bundle_id.out" 2>"$ERR_DIR/bad_bundle_id.err"; then
  echo "expected invalid bundle job id to fail" >&2
  exit 1
fi
grep -q 'invalid job id or job path' "$ERR_DIR/bad_bundle_id.err"

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
