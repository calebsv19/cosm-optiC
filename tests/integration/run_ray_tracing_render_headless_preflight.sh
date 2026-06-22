#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
CLI="$(ray_tracing_tool_path ray_tracing_render_headless "$ROOT_DIR")"

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_preflight_request.json"
WORK_ROOT="$(ray_tracing_test_reset_work_root preflight_smoke "$ROOT_DIR")"
SUMMARY="$WORK_ROOT/render_summary.json"
STDOUT_SUMMARY="$WORK_ROOT/stdout_summary.json"
NEGATIVE_ROOT="$WORK_ROOT/negative_requests"
DIAG_ROOT="$WORK_ROOT/diagnostics"

mkdir -p "$NEGATIVE_ROOT" "$DIAG_ROOT"
"$CLI" --request "$REQUEST" --preflight --summary "$SUMMARY" > "$STDOUT_SUMMARY"

grep -q '"schema_version": "ray_tracing_headless_summary_v1"' "$SUMMARY"
grep -q '"scene_applied": true' "$SUMMARY"
grep -q '"route_native_3d": true' "$SUMMARY"
grep -q '"prepared_frame": true' "$SUMMARY"
grep -q '"integrator_3d": "emission_transparency"' "$SUMMARY"
grep -q '"preset": "glass_preview"' "$SUMMARY"
grep -q '"has_secondary_diffuse_samples_3d_override": true' "$SUMMARY"
grep -q '"secondary_diffuse_samples_3d": 8' "$SUMMARY"
grep -q '"has_transmission_samples_3d_override": true' "$SUMMARY"
grep -q '"transmission_samples_3d": 4' "$SUMMARY"
grep -q '"has_background_brightness_override": true' "$SUMMARY"
grep -q '"background_brightness": 0.420000000' "$SUMMARY"
grep -q '"environment_lighting": {' "$SUMMARY"
grep -q '"mode": "ambient"' "$SUMMARY"
grep -q '"preset": "sky"' "$SUMMARY"
grep -q '"background_brightness_source": "background_brightness"' "$SUMMARY"
grep -q '"background_miss_contributes": true' "$SUMMARY"

expect_preflight_reject() {
  local label="$1"
  local request_path="$2"
  local expected="$3"
  set +e
  "$CLI" --request "$request_path" --preflight \
    >"$DIAG_ROOT/$label.out" 2>"$DIAG_ROOT/$label.err"
  status=$?
  set -e
  if [ "$status" -eq 0 ]; then
    echo "expected preflight rejection for $label" >&2
    exit 1
  fi
  grep -q "$expected" "$DIAG_ROOT/$label.err"
}

cat >"$NEGATIVE_ROOT/missing_scene_path.json" <<'JSON'
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "scene": {},
  "render": {
    "frame_count": 1,
    "width": 64,
    "height": 64
  },
  "output": {
    "root": "../negative_output"
  }
}
JSON

cat >"$NEGATIVE_ROOT/invalid_output_root.json" <<'JSON'
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "scene": {
    "runtime_scene_path": "../../../config/samples/ps4d_runtime_scene_visual_test.json"
  },
  "render": {
    "frame_count": 1,
    "width": 64,
    "height": 64
  },
  "output": {
    "root": ""
  }
}
JSON

expect_preflight_reject \
  missing_scene_path \
  "$NEGATIVE_ROOT/missing_scene_path.json" \
  "field=scene.runtime_scene_path"

expect_preflight_reject \
  invalid_output_root \
  "$NEGATIVE_ROOT/invalid_output_root.json" \
  "field=output.root"
