#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_ROOT="$ROOT_DIR/build/$(uname -m)"
RUNNER="$BUILD_ROOT/tools/cli/ray_tracing_job_runner"
if [[ ! -x "$RUNNER" ]]; then
  RUNNER="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_job_runner"
fi

JOBS_ROOT="$ROOT_DIR/build/agent_runs/jobs"
BUNDLE_ROOT="/private/tmp/ray_tracing_job_runner_bundle_smoke"
JOB_ID="ray-tracing--bundle-smoke--20260522T000000Z--smoke01"
JOB_ROOT="$JOBS_ROOT/$JOB_ID"
JOB_JSON="$BUNDLE_ROOT/job.json"
RUN_REQUEST="$BUNDLE_ROOT/input/run.ray_tracing.json"
STATUS_FILE="$JOB_ROOT/job_status.json"
SHARED_JOB_FILE="$JOB_ROOT/job.json"
SHARED_REPORT_FILE="$JOB_ROOT/output/report.json"
SUMMARY_FILE="$JOB_ROOT/result_summary.json"
FRAME_FILE="$JOB_ROOT/output/artifacts/frames/frame_0000.bmp"

mkdir -p "$JOBS_ROOT"
rm -rf "$BUNDLE_ROOT" "$JOB_ROOT"
mkdir -p "$BUNDLE_ROOT/input"

cp "$ROOT_DIR/tests/fixtures/agent_render_job_runner_bundle_request.json" "$RUN_REQUEST"

cat >"$JOB_JSON" <<EOF
{
  "schema_family": "codework_job",
  "schema_variant": "headless_bundle_v1",
  "job_id": "$JOB_ID",
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
    "title": "Bundle smoke",
    "description": "Outer bundle smoke for detached ray tracing runner",
    "created_by": "codex",
    "created_at": "2026-05-22T00:00:00Z"
  }
}
EOF

SUBMIT_OUTPUT="$("$RUNNER" submit --request "$JOB_JSON" --jobs-root "$JOBS_ROOT")"
PARSED_JOB_ID="$(printf '%s' "$SUBMIT_OUTPUT" | sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p')"
if [[ "$PARSED_JOB_ID" != "$JOB_ID" ]]; then
  echo "expected job id $JOB_ID but got $PARSED_JOB_ID" >&2
  exit 1
fi

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

grep -q '"schema_family": "codework_job"' "$SHARED_JOB_FILE"
grep -q '"schema_variant": "headless_bundle_v1"' "$SHARED_JOB_FILE"
grep -q '"job_id": "'"$JOB_ID"'"' "$SHARED_JOB_FILE"
grep -q '"run_config"' "$SHARED_JOB_FILE"
grep -q '"schema_family": "codework_job_report"' "$SHARED_REPORT_FILE"
grep -q '"schema_variant": "headless_report_v1"' "$SHARED_REPORT_FILE"
grep -q '"state": "succeeded"' "$SHARED_REPORT_FILE"
grep -q '"type": "frame_sequence"' "$SHARED_REPORT_FILE"
grep -q '"schema_version": "ray_tracing_detached_job_status_v1"' "$STATUS_FILE"
grep -q '"state": "completed"' "$STATUS_FILE"
grep -q '"schema_version": "ray_tracing_headless_summary_v1"' "$SUMMARY_FILE"
test -f "$FRAME_FILE"
