#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
RUNNER="$(ray_tracing_tool_path ray_tracing_job_runner "$ROOT_DIR")"

TEMPLATE_REQUEST="$ROOT_DIR/tests/fixtures/agent_render_job_runner_request.json"
WORK_ROOT="$(ray_tracing_test_reset_work_root job_runner_smoke "$ROOT_DIR")"
REQUEST="$WORK_ROOT/request.json"
JOBS_ROOT="$WORK_ROOT/jobs"
RENDER_OUTPUT_ROOT="$WORK_ROOT/render_output"

mkdir -p "$JOBS_ROOT"
python3 - "$TEMPLATE_REQUEST" "$REQUEST" "$RENDER_OUTPUT_ROOT" <<'PY'
import json, sys
from pathlib import Path
source, destination, output = map(Path, sys.argv[1:])
request = json.loads(source.read_text())
request['scene']['runtime_scene_path'] = str((source.parent / request['scene']['runtime_scene_path']).resolve())
request['output']['root'] = str(output)
request['progress']['summary_path'] = str(destination.parent / 'render_summary_unused.json')
request['progress']['progress_path'] = str(destination.parent / 'render_progress_unused.json')
destination.write_text(json.dumps(request, indent=2) + '\n')
PY

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
grep -q '"worker_protocol_version": 1' "$STATUS_FILE"
grep -q '"execution_mode": "worker_protocol"' "$STATUS_FILE"
grep -Eq '"immutable_request_sha256": "[0-9a-f]{64}"' "$STATUS_FILE"
grep -Eq '"renderer_build_sha256": "[0-9a-f]{64}"' "$STATUS_FILE"
grep -q '"schema_version": "ray_tracing_headless_summary_v1"' "$SUMMARY_FILE"
grep -q '"evaluated_scene_bound": true' "$SUMMARY_FILE"
grep -q '"evaluated_scene_first_frame": 0' "$SUMMARY_FILE"
grep -q '"evaluated_scene_last_frame": 0' "$SUMMARY_FILE"
grep -q '"rendered_frames": true' "$SUMMARY_FILE"
grep -q '"stage": "completed"' "$PROGRESS_FILE"
grep -q '"temporal_subpasses_started": 2' "$PROGRESS_FILE"
grep -q '"temporal_subpasses_completed": 2' "$PROGRESS_FILE"
grep -q '"temporal_subpasses_total": 2' "$PROGRESS_FILE"
test -f "$JOB_ROOT/stdout.log"
test -f "$JOB_ROOT/stderr.log"
test -f "$JOB_ROOT/pid.txt"
test -f "$JOB_ROOT/worker_capabilities.json"
test -f "$JOB_ROOT/worker_request.json"
test -d "$JOB_ROOT/worker_events"
grep -q '"message_type":"progress"' "$JOB_ROOT"/worker_events/*.json
grep -q '"message_type":"dirty_region"' "$JOB_ROOT"/worker_events/*.json
grep -q '"message_type":"checkpoint_reference"' "$JOB_ROOT"/worker_events/*.json
grep -q '"message_type":"completion"' "$JOB_ROOT"/worker_events/*.json
test -f "$FRAME_FILE"
