#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CLI="$ROOT_DIR/build/toolchains/clang/$(uname -m)/tools/cli/ray_tracing_render_headless"
if [[ ! -x "$CLI" ]]; then
  CLI="$ROOT_DIR/build/$(uname -m)/tools/cli/ray_tracing_render_headless"
fi

REQUEST="$ROOT_DIR/tests/fixtures/agent_render_preflight_request.json"
SUMMARY="$ROOT_DIR/build/agent_runs/ray_tracing/preflight_smoke/render_summary.json"

mkdir -p "$(dirname "$SUMMARY")"
"$CLI" --request "$REQUEST" --preflight --summary "$SUMMARY" > "$ROOT_DIR/build/agent_runs/ray_tracing/preflight_smoke/stdout_summary.json"

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
