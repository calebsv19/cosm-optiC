#!/bin/sh
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
LINE_TOOL="$ROOT_DIR/../line_drawing/build/bin/agent_scene_tool"
PREVIEW_BIN="$ROOT_DIR/build/arm64/tools/cli/ray_tracing_material_preview_headless"
LINE_REQUEST="$ROOT_DIR/../line_drawing/tests/fixtures/agent_detached_lab_emitter_paths_request.json"
AUTHOR_OUT="/private/tmp/ray_tracing_material_preview_author"
REQUEST_PATH="/private/tmp/ray_tracing_material_preview_request.json"
OUTPUT_PATH="/private/tmp/ray_tracing_material_preview_output.bmp"
SUMMARY_PATH="/private/tmp/ray_tracing_material_preview_summary.json"

rm -rf "$AUTHOR_OUT"
rm -f "$REQUEST_PATH" "$OUTPUT_PATH" "$SUMMARY_PATH"

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
        "surface_damage": 0.46
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
