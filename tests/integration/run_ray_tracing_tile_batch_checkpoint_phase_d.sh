#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
RUNNER="$(ray_tracing_tool_path ray_tracing_job_runner "$ROOT_DIR")"
RUN_ROOT="$ROOT_DIR/build/agent_runs/ray_tracing/tile_batch_checkpoint_phase_d"
JOBS_ROOT="$RUN_ROOT/jobs"
SCENE="$ROOT_DIR/config/samples/ps4d_runtime_scene_visual_test.json"
REFERENCE_OUTPUT="$RUN_ROOT/reference_output"

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
    "run_id": "tile_batch_checkpoint_phase_d",
    "scene": {"runtime_scene_path": scene},
    "volume": {"enabled": False},
    "render": {
        "start_frame": 0,
        "frame_count": 1,
        "width": 160,
        "height": 96,
        "normalized_t": 0.0,
        "temporal_frames": 3,
        "use_tiled_renderer": True,
        "tile_size": 32,
        "adaptive_sampling_enabled": True,
        "integrator_3d": "diffuse_bounce",
    },
    "checkpoint": {
        "enabled": True,
        "resume": False,
        "root": output_root + "/checkpoints",
        "tile_batch_size": 1,
        "max_tile_batch_size": 4,
        "max_interval_ms": 250,
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
  for _ in $(seq 1 400); do
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

REFERENCE_REQUEST="$RUN_ROOT/reference_request.json"
write_request "$REFERENCE_REQUEST" "$REFERENCE_OUTPUT"
REFERENCE_JOB_ID="$(submit "$REFERENCE_REQUEST")"
wait_for_state "$REFERENCE_JOB_ID" completed >/dev/null

for stage in \
  before_write \
  during_temporary_write \
  after_file_sync \
  after_rename \
  before_directory_sync
do
  stage_root="$RUN_ROOT/$stage"
  request="$RUN_ROOT/${stage}_request.json"
  write_request "$request" "$stage_root"
  submit_output="$(
    RAY_TRACING_TEST_CHECKPOINT_EXIT_STAGE="$stage" \
    RAY_TRACING_TEST_CHECKPOINT_EXIT_GENERATION=2 \
      "$RUNNER" submit --request "$request" --jobs-root "$JOBS_ROOT"
  )"
  interrupted_job="$(
    printf '%s' "$submit_output" |
      sed -n 's/.*"job_id":"\([^"]*\)".*/\1/p'
  )"
  wait_for_state "$interrupted_job" interrupted >/dev/null

  committed_count="$(
    find "$stage_root/checkpoints/frame_0000" \
      -name 'generation_*.rtck' -type f | wc -l | tr -d ' '
  )"
  test "$committed_count" -ge 1
  test "$committed_count" -le 2
  test -s "$stage_root/checkpoints/frame_0000/generation_00000000000000000001.rtck"

  resumed_job="$(submit "$request" --resume)"
  wait_for_state "$resumed_job" completed >/dev/null
  cmp "$REFERENCE_OUTPUT/frames/frame_0000.bmp" \
      "$stage_root/frames/frame_0000.bmp"
  test "$(find "$stage_root/checkpoints/frame_0000" \
    -name 'generation_*.rtck' -type f | wc -l | tr -d ' ')" -eq 2
  test -s "$stage_root/checkpoints/frame_0000/current.json"
  grep -q '"checkpoint_schema_version": 2' \
    "$stage_root/checkpoints/frame_0000/current.json"
  python3 - "$JOBS_ROOT/$resumed_job/result_summary.json" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as handle:
    checkpoint = json.load(handle)["checkpoint"]
assert checkpoint["schema_version"] == 2
assert checkpoint["resumed"] is True
assert checkpoint["resumed_subpasses"] == 0
assert checkpoint["resumed_tiles_in_subpass"] > 0
assert checkpoint["tile_batch_generations_written"] > 0
assert checkpoint["total_write_ms"] > 0.0
assert checkpoint["maximum_write_ms"] > 0.0
PY
  if grep -q '"reference_path":"[^"]*\\.tmp\\.' \
    "$JOBS_ROOT/$resumed_job"/worker_events/*.json 2>/dev/null; then
    echo "partial checkpoint generation was selected for stage $stage" >&2
    exit 1
  fi
done

echo "ray tracing Phase D tile-batch checkpoint transaction matrix passed"
