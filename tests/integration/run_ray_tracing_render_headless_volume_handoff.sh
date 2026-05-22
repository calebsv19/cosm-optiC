#!/usr/bin/env bash
set -euo pipefail

RAY_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
CODEWORK_ROOT="$(cd "$RAY_DIR/.." && pwd)"
LINE_DIR="$CODEWORK_ROOT/line_drawing"
PHYSICS_DIR="$CODEWORK_ROOT/physics_sim"

RUN_ROOT="$RAY_DIR/build/agent_runs/physics_trio/volume_handoff_image_export"
LINE_OUT="$RUN_ROOT/line_drawing"
PHYSICS_OUT="$RUN_ROOT/physics_sim"
RAY_OUT="$RUN_ROOT/ray_tracing"
REQUEST="$RUN_ROOT/ray_tracing_request.json"
SUMMARY="$RAY_OUT/render_summary.json"
STDOUT_SUMMARY="$RAY_OUT/stdout_summary.json"

LINE_TOOL="$LINE_DIR/build/bin/agent_scene_tool"
PHYSICS_TOOL="$PHYSICS_DIR/physics_sim_headless"
RAY_TOOL="$RAY_DIR/build/$(uname -m)/tools/cli/ray_tracing_render_headless"
if [[ ! -x "$RAY_TOOL" ]]; then
  RAY_TOOL="$RAY_DIR/build/tools/cli/ray_tracing_render_headless"
fi

mkdir -p "$RUN_ROOT" "$RAY_OUT"

make -C "$LINE_DIR" agent_scene_tool
make -C "$PHYSICS_DIR" physics_sim_headless
make -C "$RAY_DIR" ray-tracing-render-headless

"$LINE_TOOL" \
  --request "$LINE_DIR/tests/fixtures/agent_room_prism_emitter_request.json" \
  --out "$LINE_OUT" \
  --determinism-check

"$PHYSICS_TOOL" \
  --runtime-scene "$LINE_OUT/scene_runtime.json" \
  --frames 10 \
  --output-root "$PHYSICS_OUT" \
  --summary "$PHYSICS_OUT/run_summary.json" \
  --progress "$PHYSICS_OUT/run_progress.json" \
  --progress-interval 5 \
  --overwrite \
  --save-volume-frames

SCENE_BUNDLE="$(find "$PHYSICS_OUT/volume_frames" -name scene_bundle.json -print | sort | head -n 1)"
if [[ -z "$SCENE_BUNDLE" || ! -f "$SCENE_BUNDLE" ]]; then
  echo "missing PhysicsSim scene_bundle.json under $PHYSICS_OUT/volume_frames" >&2
  exit 1
fi

cat > "$REQUEST" <<JSON
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "run_id": "agent_render_volume_handoff_smoke",
  "scene": {
    "runtime_scene_path": "$LINE_OUT/scene_runtime.json"
  },
  "volume": {
    "enabled": true,
    "source_kind": "scene_bundle",
    "source_path": "$SCENE_BUNDLE",
    "affects_lighting": true,
    "debug_overlay": false
  },
  "render": {
    "start_frame": 0,
    "frame_count": 3,
    "width": 640,
    "height": 640,
    "normalized_t": 0.0,
    "temporal_frames": 2,
    "integrator_3d": "direct_light"
  },
  "inspection": {
    "camera_zoom": 0.95,
    "camera_position": { "x": -3.8, "y": -7.2, "z": 2.2 },
    "camera_look_at": { "x": -0.2, "y": 0.8, "z": 1.2 },
    "environment_brightness": 0.0,
    "light_intensity": 2.6,
    "light_radius": 0.10,
    "forward_decay": 220.0,
    "volume_scatter_gain": 3.0,
    "volume_step_scale": 1.0,
    "volume_tint": { "r": 0.35, "g": 0.65, "b": 1.80 }
  },
  "output": {
    "root": "$RAY_OUT",
    "overwrite": true
  },
  "progress": {
    "summary_path": "$SUMMARY",
    "progress_path": "$RAY_OUT/render_progress.json"
  }
}
JSON

"$RAY_TOOL" --request "$REQUEST" --render --summary "$SUMMARY" > "$STDOUT_SUMMARY"

grep -q '"schema_version": "ray_tracing_headless_summary_v1"' "$SUMMARY"
grep -q '"scene_applied": true' "$SUMMARY"
grep -q '"volume_attached": true' "$SUMMARY"
grep -q '"volume_summary_built": true' "$SUMMARY"
grep -q '"route_native_3d": true' "$SUMMARY"
grep -q '"prepared_frame": true' "$SUMMARY"
grep -q '"rendered_frames": true' "$SUMMARY"
grep -q '"frames_rendered": 3' "$SUMMARY"
grep -q '"has_density": true' "$SUMMARY"
grep -Eq '"density_non_zero_cell_count": [1-9][0-9]*' "$SUMMARY"
grep -Eq '"visible_pixels": [1-9][0-9]*' "$SUMMARY"
grep -Eq '"nonzero_pixels": [1-9][0-9]*' "$SUMMARY"
grep -Eq '"max_radiance": 0\.[2-9][0-9]*|\"max_radiance\": [1-9][0-9]*\.[0-9]+' "$SUMMARY"
grep -q '"has_volume_tint_override": true' "$SUMMARY"
grep -q '"has_volume_step_scale_override": true' "$SUMMARY"
grep -Eq '"max_rgb": \[[6-9][0-9], [6-9][0-9], [6-9][0-9]\]|\"max_rgb\": \[[1-9][0-9]{2}, [1-9][0-9]{2}, [1-9][0-9]{2}\]' "$SUMMARY"

test -s "$RAY_OUT/frames/frame_0000.bmp"
test -s "$RAY_OUT/frames/frame_0001.bmp"
test -s "$RAY_OUT/frames/frame_0002.bmp"
test "$(dd if="$RAY_OUT/frames/frame_0000.bmp" bs=1 count=2 2>/dev/null)" = "BM"
test "$(dd if="$RAY_OUT/frames/frame_0001.bmp" bs=1 count=2 2>/dev/null)" = "BM"
test "$(dd if="$RAY_OUT/frames/frame_0002.bmp" bs=1 count=2 2>/dev/null)" = "BM"

echo "ray tracing volume handoff image export passed: $RAY_OUT/frames"
