#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
LINE_TOOL="$ROOT_DIR/../line_drawing/build/toolchains/clang/bin/agent_scene_tool"
PREVIEW_BIN="$(ray_tracing_tool_path ray_tracing_material_preview_headless "$ROOT_DIR")"
LINE_REQUEST="$ROOT_DIR/../line_drawing/tests/fixtures/agent_detached_lab_emitter_paths_request.json"
WORK_ROOT="$(ray_tracing_test_reset_work_root material_preview_headless "$ROOT_DIR")"
AUTHOR_OUT="$WORK_ROOT/author"
REQUEST_PATH="$WORK_ROOT/material_preview_request.json"
OUTPUT_PATH="$WORK_ROOT/material_preview_output.bmp"
SUMMARY_PATH="$WORK_ROOT/material_preview_summary.json"
NEGATIVE_ROOT="$WORK_ROOT/negative_requests"
DIAG_ROOT="$WORK_ROOT/diagnostics"

mkdir -p "$NEGATIVE_ROOT" "$DIAG_ROOT"

make -C "$ROOT_DIR/../line_drawing" BUILD_TOOLCHAIN=clang agent_scene_tool

"$LINE_TOOL" --request "$LINE_REQUEST" --out "$AUTHOR_OUT" --determinism-check

cat >"$REQUEST_PATH" <<EOF
{
  "schema": "ray_tracing_material_preview_request_v1",
  "runtime_scene_path": "$AUTHOR_OUT/scene_runtime.json",
  "object_id": "left_screen",
  "width": 160,
  "height": 160,
  "columns": 2,
  "output_path": "$OUTPUT_PATH",
  "summary_path": "$SUMMARY_PATH",
  "variants": [
    {
      "label": "base"
    },
    {
      "label": "rough_flow",
      "preview_overlay": {
        "kind": "grime",
        "opacity": 0.72,
        "pattern_mode": 3,
        "coverage": 0.68,
        "grain": 0.61,
        "contrast": 0.79,
        "surface_damage": 0.46,
        "roughness_influence": 0.74,
        "reflectivity_influence": -0.25,
        "specular_influence": -0.20,
        "diffuse_influence": 0.45,
        "transparency_influence": -0.35
      }
    }
  ]
}
EOF

"$PREVIEW_BIN" --request "$REQUEST_PATH"

test -f "$OUTPUT_PATH"
test -f "$SUMMARY_PATH"
rg -q '"variant_count":[[:space:]]*2' "$SUMMARY_PATH"
rg -q '"object_id":[[:space:]]*"left_screen"' "$SUMMARY_PATH"
rg -q '"preview_overlay"' "$SUMMARY_PATH"
rg -q '"kind":[[:space:]]*"grime"' "$SUMMARY_PATH"
rg -q '"roughness_influence":[[:space:]]*0\.73' "$SUMMARY_PATH"
rg -q '"reflectivity_influence":[[:space:]]*-0\.25' "$SUMMARY_PATH"
rg -q '"transparency_influence":[[:space:]]*-0\.34' "$SUMMARY_PATH"

expect_material_reject() {
  local label="$1"
  local request_path="$2"
  local expected="$3"
  set +e
  "$PREVIEW_BIN" --request "$request_path" \
    >"$DIAG_ROOT/$label.out" 2>"$DIAG_ROOT/$label.err"
  status=$?
  set -e
  if [ "$status" -eq 0 ]; then
    echo "expected material preview rejection for $label" >&2
    exit 1
  fi
  grep -q "$expected" "$DIAG_ROOT/$label.err"
}

cat >"$NEGATIVE_ROOT/missing_runtime_scene.json" <<EOF
{
  "schema": "ray_tracing_material_preview_request_v1",
  "runtime_scene_path": "missing_scene_runtime.json",
  "object_id": "left_screen",
  "output_path": "$WORK_ROOT/missing_runtime_scene.bmp",
  "summary_path": "$WORK_ROOT/missing_runtime_scene_summary.json"
}
EOF

cat >"$NEGATIVE_ROOT/invalid_output_path.json" <<EOF
{
  "schema": "ray_tracing_material_preview_request_v1",
  "runtime_scene_path": "$AUTHOR_OUT/scene_runtime.json",
  "object_id": "left_screen",
  "output_path": "",
  "summary_path": "$WORK_ROOT/invalid_output_summary.json"
}
EOF

expect_material_reject \
  missing_runtime_scene \
  "$NEGATIVE_ROOT/missing_runtime_scene.json" \
  "field=runtime_scene_path"

expect_material_reject \
  invalid_output_path \
  "$NEGATIVE_ROOT/invalid_output_path.json" \
  "field=output_path"
